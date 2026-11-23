#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compose the sound set "CD-Aufnahmen + MIDI-Ergänzungen" (source kind cd-hybrid).

The five background cues that have a recording on the original disc come from the
imported CD audio (`data/music/segment-*.ogg`, `audio.json`, docs/cd-audio.md); every
other cue — g2, the complete intro, events, destinations, finish and victory — is
copied from a complete SoundFont rendering and marked as such. The mapping is the
reproducible pitch-class comparison of tools/inspect_music.py (docs/music-research.md
"Recording-to-cue comparison"); the user recognised region 01 (g0) as the music
remembered from the original. Original CD track numbers and playback through CD
audio are not established; rally.exe plays its music from MIDI files.

Usage:
    python3 tools/cd_soundtrack.py --music-dir INSTALLATION/data/music \
        --render-pack INSTALLATION/data/music/packs/rendered-gm \
        --output INSTALLATION/data/music/packs/cd-hybrid
"""

import argparse
import json
from pathlib import Path
import re
import shutil
import tempfile

from soundtrack import BACKGROUND, CUES, digest, external_destination, probe_ogg, verify_pack

PACK_ID = "cd-hybrid"
PACK_NAME = "CD-Aufnahmen + MIDI-Ergänzungen"
# Recording -> cue with the similarity of tools/inspect_music.py (time scale 1.0 each).
RECORDINGS = {
    "g0": ("segment-01", 0.456),
    "g1": ("segment-02", 0.454),
    "g3": ("segment-03", 0.587),
    "g4": ("segment-04", 0.736),
    "g5": ("segment-05", 0.647),
}
# segment-06 is only the first 109 s of the intro `i` (434 s): not a replacement.


def load_regions(music_dir):
    """The imported CD regions of audio.json, checked against their Ogg files."""
    layout = json.loads((music_dir / "audio.json").read_text())
    regions = {}
    for region in layout.get("regions", []) if isinstance(layout, dict) else []:
        name, file = region.get("name"), region.get("file")
        if (not isinstance(name, str) or not re.fullmatch(r"segment-0[1-9]", name)
                or file != f"{name}.ogg" or not isinstance(region.get("source_pcm_sha256"), str)):
            raise ValueError("Invalid CD audio region record")
        regions[name] = region
    missing = sorted({segment for segment, _ in RECORDINGS.values()} - set(regions))
    if missing:
        raise ValueError(f"CD audio regions missing: {', '.join(missing)}")
    return layout, regions


def build_cd_hybrid(music_dir, render_pack, output, pack_id=PACK_ID, name=PACK_NAME):
    music_dir = Path(music_dir).resolve(strict=True)
    render_root = Path(render_pack).resolve(strict=True)
    output = external_destination(Path(output))
    render = verify_pack(render_root)  # complete soundfont-render set with matching hashes
    layout, regions = load_regions(music_dir)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".dtvr-cd-hybrid-", dir=output.parent) as temporary:
        stage = Path(temporary) / "pack"
        (stage / "cues").mkdir(parents=True)
        catalog = {"schema_version": 1, "id": pack_id, "name": name, "style": render["style"],
                   "source_kind": "cd-hybrid", "asset_complete": True,
                   "original_hardware_verified": False, "original_playback_timing_verified": False,
                   "background_order": BACKGROUND,
                   "cd_audio": {key: layout.get(key) for key in
                                ("profile_version", "boundary_source", "original_track_numbers_known", "pcm_format")},
                   "supplement": {"pack_id": render["id"], "name": render["name"],
                                  "settings": render.get("settings"), "soundfonts": render.get("soundfonts"),
                                  "tools": render.get("tools")},
                   "cues": {}}
        for cue in CUES:
            target = stage / "cues" / f"{cue}.ogg"
            if cue in RECORDINGS:
                segment, similarity = RECORDINGS[cue]
                source = music_dir / f"{segment}.ogg"
                if source.is_symlink() or not source.is_file():
                    raise ValueError(f"CD audio file missing or linked: {source.name}")
                shutil.copyfile(source, target)
                duration = probe_ogg(target)
                catalog["cues"][cue] = {
                    "file": f"cues/{cue}.ogg", "source": "cd-recording", "region": segment,
                    "source_pcm_sha256": regions[segment]["source_pcm_sha256"], "ogg_sha256": digest(target),
                    "nominal_end_seconds": duration, "media_duration_seconds": duration,
                    "nominal_end_source": "cd-recording-end",
                    "mapping": {"method": "tools/inspect_music.py pitch-class comparison",
                                "similarity": similarity, "time_scale": 1.0},
                    "loop": False}
            else:
                entry = render["cues"][cue]
                shutil.copyfile(render_root / entry["file"], target)
                if digest(target) != entry["ogg_sha256"]:
                    raise ValueError(f"Supplement changed while copying: {cue}")
                catalog["cues"][cue] = {**entry, "source": "soundfont-render"}
        (stage / "pack.json").write_text(json.dumps(catalog, indent=2, ensure_ascii=False) + "\n")
        stage.rename(output)
    return catalog


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--music-dir", type=Path, required=True, help="data/music with audio.json and segment-*.ogg")
    parser.add_argument("--render-pack", type=Path, required=True, help="a complete SoundFont rendering")
    parser.add_argument("--output", type=Path, required=True, help="new directory, e.g. data/music/packs/cd-hybrid")
    args = parser.parse_args()
    try:
        catalog = build_cd_hybrid(args.music_dir, args.render_pack, args.output)
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"CD sound set failed: {error}\n")
    recorded = sorted(cue for cue, entry in catalog["cues"].items() if entry["source"] == "cd-recording")
    print(f"{catalog['name']}: CD recordings {', '.join(recorded)}; "
          f"{len(catalog['cues']) - len(recorded)} cues from {catalog['supplement']['pack_id']}")


if __name__ == "__main__":
    main()
