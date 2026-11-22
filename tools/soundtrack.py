# SPDX-License-Identifier: GPL-3.0-only
"""Build and validate complete local Ogg cue packs from user-supplied MIDI/banks."""

import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from inspect_music import read_midi

REPO = Path(__file__).resolve().parents[1]
CUES = ("a1", *(f"e{i}" for i in range(1, 12)), *(f"f{i}" for i in range(1, 4)),
        *(f"g{i}" for i in range(6)), "i", "s", "z1", "z2")
BACKGROUND = [f"g{i}" for i in range(6)]
SETTINGS = {"sample_rate": 44100, "channels": 2, "vorbis_quality": 6,
            "gain": 0.2, "tail_seconds": 2.0, "polyphony": 256,
            "timing_source": "sample", "cpu_cores": 1,
            "reverb_room_size": 0.2, "reverb_damp": 0.0, "reverb_width": 0.5,
            "reverb_level": 0.3, "chorus_voices": 3, "chorus_level": 2.0,
            "chorus_speed": 0.3, "chorus_depth": 8.0}


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def external_destination(path):
    path = path.expanduser().resolve()
    if path == REPO or REPO in path.parents:
        raise ValueError("Soundtrack output must be outside the checkout")
    if path.exists() or path.is_symlink():
        raise ValueError(f"Soundtrack destination already exists: {path}")
    return path


def validate_identity(pack_id, name, style):
    if not isinstance(pack_id, str) or not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,47}", pack_id):
        raise ValueError("Pack ID must contain 1–48 lowercase letters, digits or hyphens")
    if not isinstance(name, str) or not name or len(name) > 80 or not name.isprintable():
        raise ValueError("Pack name must contain 1–80 printable characters")
    if style not in ("classic", "dry"):
        raise ValueError("Music style must be classic or dry")


def soundfont_info(path):
    path = path.expanduser().resolve(strict=True)
    with path.open("rb") as source:
        header = source.read(12)
    if len(header) != 12 or header[:4] != b"RIFF" or header[8:] != b"sfbk":
        raise ValueError(f"Not a SoundFont bank: {path.name}")
    return {"name": path.name, "sha256": digest(path)}


def run(command):
    result = subprocess.run(command, check=True, capture_output=True, text=True, timeout=300)
    return result


def tool_versions():
    for tool in ("fluidsynth", "ffmpeg", "ffprobe"):
        if not shutil.which(tool):
            raise ValueError(f"Soundtrack rendering requires {tool}")
    synth = run(["fluidsynth", "--version"])
    version = next((line for line in synth.stdout.splitlines() if "runtime version" in line), "unknown")
    return {"fluidsynth": version, "ffmpeg": run(["ffmpeg", "-version"]).stdout.splitlines()[0]}


def probe_ogg(path, expected=None):
    result = json.loads(run(["ffprobe", "-v", "error", "-show_streams", "-of", "json", str(path)]).stdout)
    streams = result.get("streams", [])
    if len(streams) != 1:
        raise ValueError(f"Expected one Ogg audio stream: {path.name}")
    stream = streams[0]
    duration = float(stream.get("duration", "nan"))
    if (stream.get("codec_name") != "vorbis" or stream.get("channels") != 2
            or stream.get("sample_rate") != "44100" or not math.isfinite(duration) or duration <= 0
            or (expected is not None and abs(duration - expected) > 0.05)):
        raise ValueError(f"Invalid Ogg properties or duration: {path.name}")
    run(["ffmpeg", "-nostdin", "-v", "error", "-xerror", "-i", str(path), "-f", "null", "-"])
    return duration


def validate_catalog(catalog):
    if not isinstance(catalog, dict) or catalog.get("schema_version") != 1 or catalog.get("source_kind") != "soundfont-render":
        raise ValueError("Unsupported soundtrack catalog")
    validate_identity(catalog.get("id", ""), catalog.get("name", ""), catalog.get("style", ""))
    if (not isinstance(catalog.get("cues"), dict) or set(catalog["cues"]) != set(CUES)
            or catalog.get("background_order") != BACKGROUND):
        raise ValueError("Incomplete soundtrack cue catalog")
    for cue, entry in catalog["cues"].items():
        if not isinstance(entry, dict) or entry.get("file") != f"cues/{cue}.ogg":
            raise ValueError("Unsafe soundtrack cue path")
        for key in ("midi_sha256", "ogg_sha256"):
            if not isinstance(entry.get(key), str) or not re.fullmatch(r"[a-f0-9]{64}", entry[key]):
                raise ValueError("Missing soundtrack cue identity")
        logical, media = entry.get("nominal_end_seconds"), entry.get("media_duration_seconds")
        if (type(logical) not in (float, int) or type(media) not in (float, int)
                or not math.isfinite(logical) or not math.isfinite(media)
                or not 0 <= logical <= 3600 or media < logical
                or abs(media - logical - SETTINGS["tail_seconds"]) > 0.05):
            raise ValueError("Invalid soundtrack timeline")


def verify_pack(root):
    root = root.resolve(strict=True)
    catalog = json.loads((root / "pack.json").read_text())
    validate_catalog(catalog)
    for cue, entry in catalog["cues"].items():
        path = root / entry["file"]
        if path.is_symlink() or path.resolve() != root / "cues" / f"{cue}.ogg":
            raise ValueError("Soundtrack cue must be a local file inside the pack")
        if digest(path) != entry["ogg_sha256"]:
            raise ValueError(f"Soundtrack hash mismatch: {cue}")
        probe_ogg(path, entry["media_duration_seconds"])
    return catalog


def render_pack(midi_dir, soundfonts, output, pack_id="rendered-gm", name="Rendered SoundFont", style="classic"):
    validate_identity(pack_id, name, style)
    output = external_destination(output)
    if not soundfonts:
        raise ValueError("Supply at least one SoundFont bank")
    banks = [Path(p).expanduser().resolve(strict=True) for p in soundfonts]
    bank_info = [soundfont_info(p) for p in banks]
    midi_dir = midi_dir.resolve(strict=True)
    originals = {}
    for cue in CUES:
        path = midi_dir / cue
        if not path.is_file() or path.stat().st_size > 8 * 1024 * 1024:
            raise ValueError(f"Required original MIDI missing or too large: {cue}")
        info = read_midi(path.read_bytes())
        if not 0 <= info["duration_seconds"] <= 3600:
            raise ValueError(f"Unsupported MIDI duration: {cue}")
        originals[cue] = info
    versions = tool_versions()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".dtvr-soundtrack-", dir=output.parent) as temporary:
        work = Path(temporary)
        stage = work / "pack"
        (stage / "cues").mkdir(parents=True)
        config = work / "empty.conf"
        config.write_text("")  # Do not inherit the developer/user's synth configuration.
        catalog = {"schema_version": 1, "id": pack_id, "name": name, "style": style,
                   "source_kind": "soundfont-render", "asset_complete": True,
                   "original_hardware_verified": False, "original_playback_timing_verified": False,
                   "background_order": BACKGROUND, "settings": {**SETTINGS, "effects": style == "classic"},
                   "tools": versions, "soundfonts": bank_info, "cues": {}}
        cache = {}
        for cue, info in originals.items():
            output_ogg = stage / "cues" / f"{cue}.ogg"
            logical_end = info["duration_seconds"]
            desired_end = logical_end + SETTINGS["tail_seconds"]
            cache_key = info["sha256"]
            if cache_key in cache:
                previous, duration, warnings = cache[cache_key]
                shutil.copyfile(previous, output_ogg)
                print(f"Soundtrack {pack_id}: {cue} (identical source reused)", flush=True)
            else:
                print(f"Soundtrack {pack_id}: rendering {cue}", flush=True)
                wav = work / "render.wav"
                command = ["fluidsynth", "-ni", "-q", "-f", str(config), "-F", str(wav),
                           "-T", "wav", "-O", "float", "-r", "44100", "-g", "0.2",
                           "-R", "1" if style == "classic" else "0",
                           "-C", "1" if style == "classic" else "0",
                           "-o", "player.timing-source=sample", "-o", "synth.cpu-cores=1",
                           "-o", "synth.polyphony=256", "-o", "player.reset-synth=1"]
                for setting, value in [("reverb.room-size", 0.2), ("reverb.damp", 0.0),
                                       ("reverb.width", 0.5), ("reverb.level", 0.3),
                                       ("chorus.nr", 3), ("chorus.level", 2.0),
                                       ("chorus.speed", 0.3), ("chorus.depth", 8.0)]:
                    command += ["-o", f"synth.{setting}={value}"]
                result = run([*command, *(str(p) for p in banks), str(midi_dir / cue)])
                messages = [line for line in result.stderr.splitlines() if line.startswith("fluidsynth:")]
                if any("error:" in line or "No preset found" in line for line in messages):
                    raise ValueError(f"SoundFont rendering failed or lacks an instrument for {cue}")
                warnings = ["missing_note_off_at_eof"] if any("note off event" in line for line in messages) else []
                run(["ffmpeg", "-nostdin", "-v", "error", "-n", "-i", str(wav), "-map", "0:a:0",
                     "-af", "apad", "-t", f"{desired_end:.9f}", "-ar", "44100", "-ac", "2",
                     "-c:a", "libvorbis", "-q:a", "6", "-map_metadata", "-1", str(output_ogg)])
                duration = probe_ogg(output_ogg, desired_end)
                cache[cache_key] = (output_ogg, duration, warnings)
            catalog["cues"][cue] = {"file": f"cues/{cue}.ogg", "midi_sha256": info["sha256"],
                                     "ogg_sha256": digest(output_ogg), "ppqn": info["ppqn"],
                                     "nominal_end_seconds": logical_end, "media_duration_seconds": duration,
                                     "nominal_end_source": "smf-tempo-map-and-end-of-track",
                                     "render_warnings": warnings, "loop": False}
        validate_catalog(catalog)
        (stage / "pack.json").write_text(json.dumps(catalog, indent=2) + "\n")
        if output.exists() or output.is_symlink():
            raise ValueError("Soundtrack destination appeared during rendering")
        stage.rename(output)
    return catalog
