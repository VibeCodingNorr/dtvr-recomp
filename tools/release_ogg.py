# SPDX-License-Identifier: GPL-3.0-only
"""Accept a release's Ogg music tracks as an explicitly NON-ORIGINAL CD audio source.

Some releases of the US English retail edition ship the CD music as a data image
(`track01.iso`), a cue sheet and lossy Ogg Vorbis files (`track02.ogg`..`track07.ogg`)
re-encoded by whoever made that release. Those files are not the disc's CD-DA. This
module accepts them only for the hash-pinned English retail image, only when every
Ogg file and the cue sheet match the pinned SHA-256, and copies them byte for byte
(no transcoding) into the installation as `segment-01.ogg`..`segment-06.ogg`.
See docs/cd-audio.md ("Release-OGG") for provenance and the measured silence.
"""

import json
from pathlib import Path
import re
import shutil
import tempfile

from soundtrack import digest

SOURCE_KIND = "release-ogg"
LABEL = "Release-OGG (nicht original)"
PROFILE_VERSION = 1
# Silence threshold of the measurements below: |sample| > 16 of s16 after decoding.
SILENCE_THRESHOLD = 16

# US English retail ("Dr. Drago's Madcap Chase"), release with cue sheet and Ogg tracks.
# Track k+1 = observed German region k (docs/cd-audio.md) plus the zero separator at
# its end; the leading silence is the encoder/disc start and is not trimmed.
ENGLISH_RETAIL = {
    "profile_version": PROFILE_VERSION,
    "edition": "en-us-retail",
    "iso_sha256": "26260860553361b356a8884b290c659ab6cedf2e29645fbcf2f8f0c511551fc7",
    "executable_sha256": "a8374ca9e39d70b4508efefa782b7135c650792be36600289356a51de819225a",
    "language": "e",
    "cue": {"file": "track01.cue",
            "sha256": "11084c6c22bfa4a61cf184443753353c5049a0bd2f90743f9f3ac05c204484d9"},
    # (segment, cue track, file, SHA-256, samples per channel, leading s, trailing s)
    "tracks": [
        ("segment-01", 2, "track02.ogg", "89b3ec1c1c297fba257c68b3edd0ffe1417b8a5a399d8975c2709b3646827917",
         7975044, 0.039, 1.964),
        ("segment-02", 3, "track03.ogg", "a129fe4e8d0a27895f04f29a48239583a8114915ccc36d10ce31668b14be2f8e",
         8534820, 0.039, 1.965),
        ("segment-03", 4, "track04.ogg", "7111a2b40236f2236e5a3913004e59d24ef3839629e611c089593b86600ce819",
         8755908, 0.039, 1.968),
        ("segment-04", 5, "track05.ogg", "66f41e296c78a8f019368f4bc44b9518a8be0a150f50d484bbce2742115d5d49",
         6993672, 0.039, 1.961),
        ("segment-05", 6, "track06.ogg", "924fb31f515753583647d16a73421a5376bb976314a4553fe5a47d93f6cb9ab9",
         7802172, 0.039, 1.962),
        ("segment-06", 7, "track07.ogg", "73c3794edc8c952491e5432fcee1bc22177d8f9e9c401dc5577154d892472781",
         4986828, 0.039, 4.177),
    ],
}
PROFILES = (ENGLISH_RETAIL,)


def release_profile(iso_hash, profiles=None):
    """The release-Ogg profile pinned to this data image, or None."""
    return next((profile for profile in (PROFILES if profiles is None else profiles)
                 if profile["iso_sha256"] == iso_hash), None)


def language_letter(program):
    """The first byte of dat/lan.inf (case-insensitive path), lowercase, as src/data_paths.cpp."""
    directory = next((p for p in Path(program).iterdir() if p.name.lower() == "dat" and p.is_dir()), None)
    file = next((p for p in directory.iterdir() if p.name.lower() == "lan.inf"), None) if directory else None
    if file is None or file.is_symlink() or not file.is_file():
        raise ValueError("dat/lan.inf missing; cannot verify the edition for release Ogg audio")
    data = file.read_bytes()[:1]
    if not data or not data.isalpha():
        raise ValueError("dat/lan.inf does not start with a language letter")
    return data.decode("ascii").lower()


def require_edition(profile, executable_sha256, language):
    """Release Ogg audio belongs to one edition; refuse it for every other executable/language."""
    if executable_sha256 != profile["executable_sha256"] or language != profile["language"]:
        raise ValueError(f"{LABEL}: only accepted for edition {profile['edition']} "
                         f"(rally.exe {profile['executable_sha256'][:8]}…, lan.inf "
                         f"{profile['language'].upper()}); this installation is a different edition")


def parse_cue(text):
    """Map cue track numbers to files; bounded and strict, for the pinned sheet's structure."""
    if len(text) > 16384:
        raise ValueError("Cue sheet too large")
    tracks, current = {}, None
    for line in text.splitlines():
        words = line.split()
        if words[:1] == ["FILE"]:
            match = re.fullmatch(r'FILE "([A-Za-z0-9_.-]+)" [A-Z0-9]+', line.strip())
            if not match:
                raise ValueError("Unsupported cue FILE line")
            current = match[1]
        elif words[:1] == ["TRACK"]:
            if current is None or len(words) != 3 or not words[1].isdigit() or int(words[1]) in tracks:
                raise ValueError("Unsupported cue TRACK line")
            tracks[int(words[1])] = (current, words[2])
    return tracks


def _source_file(directory, name):
    path = directory / name
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"{LABEL}: {name} missing or linked in {directory}")
    return path


def check_release_audio(directory, profile):
    """Verify the cue sheet and every Ogg file against the pinned hashes (read-only)."""
    directory = Path(directory).expanduser()
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError(f"{LABEL}: choose the directory with the cue sheet and the Ogg tracks")
    directory = directory.resolve(strict=True)
    cue = _source_file(directory, profile["cue"]["file"])
    if digest(cue) != profile["cue"]["sha256"]:
        raise ValueError(f"{LABEL}: cue sheet hash mismatch: {cue.name}")
    sheet = parse_cue(cue.read_text(encoding="ascii"))
    for segment, track, name, sha256, *_ in profile["tracks"]:
        if sheet.get(track) != (name, "AUDIO"):
            raise ValueError(f"{LABEL}: cue track {track} is not {name}")
        if digest(_source_file(directory, name)) != sha256:
            raise ValueError(f"{LABEL}: hash mismatch: {name} (expected the pinned release file)")
    return directory


def import_release_audio(directory, target, profile):
    """Copy the six release Ogg files 1:1 into a new music directory; return the manifest record."""
    directory = check_release_audio(directory, profile)
    if target.exists() or target.is_symlink():
        raise ValueError(f"Audio destination already exists: {target}")
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".dtvr-release-ogg-", dir=target.parent) as temporary:
        stage = Path(temporary) / "audio"
        stage.mkdir()
        regions = []
        for segment, track, name, sha256, samples, leading, trailing in profile["tracks"]:
            output = stage / f"{segment}.ogg"
            print(f"Copying {LABEL}: {name} -> {output.name}", flush=True)
            shutil.copyfile(_source_file(directory, name), output)
            # The pinned hash fixes the content, so the pinned length and silences hold too.
            if digest(output) != sha256:
                raise ValueError(f"{LABEL}: copy changed or source changed during import: {name}")
            duration = round(samples / 44100, 6)
            regions.append({"name": segment, "file": output.name, "source_file": name, "cue_track": track,
                            "ogg_sha256": sha256, "media_duration_seconds": duration,
                            "leading_silence_seconds": leading, "trailing_silence_seconds": trailing,
                            "music_end_seconds": round(duration - trailing, 6)})
        result = {"profile_version": profile["profile_version"], "source_kind": SOURCE_KIND, "label": LABEL,
                  "original": False, "lossy_reencode_by_release": True, "edition": profile["edition"],
                  "iso_sha256": profile["iso_sha256"], "cue_sha256": profile["cue"]["sha256"],
                  "boundary_source": "release-cue-sheet-not-factory-toc",
                  "original_track_numbers_known": False, "copied_unmodified": True,
                  "codec": "vorbis", "sample_rate": 44100, "channels": 2,
                  "silence_threshold": SILENCE_THRESHOLD, "regions": regions,
                  "playback_mapping": "unverified"}
        (stage / "audio.json").write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n")
        if target.exists() or target.is_symlink():
            raise ValueError(f"Audio destination appeared during import: {target}")
        stage.rename(target)
    return result
