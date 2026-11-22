#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the native Ogg decoder with generated tones; no original music needed."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

CUES = ["a1", *(f"e{i}" for i in range(1, 12)), *(f"f{i}" for i in range(1, 4)),
        *(f"g{i}" for i in range(6)), "i", "s", "z1", "z2"]


def main():
    with tempfile.TemporaryDirectory(prefix="dtvr-music-test-") as temporary:
        root = Path(temporary)
        tone = root / "tone.ogg"
        subprocess.run([sys.argv[2], "-nostdin", "-v", "error", "-f", "lavfi", "-i",
                        "sine=frequency=440:sample_rate=44100:duration=2.12", "-ac", "2",
                        "-c:a", "libvorbis", str(tone)], check=True)
        for pack_id in ("tone", "dry", "missing", "linked", "corrupt", "unsafe"):
            pack = root / "packs" / pack_id
            (pack / "cues").mkdir(parents=True)
            catalog = {"schema_version": 1, "id": pack_id, "name": pack_id,
                       "source_kind": "soundfont-render", "asset_complete": True,
                       "background_order": [f"g{i}" for i in range(6)],
                       "cues": {}}
            for cue in CUES:
                shutil.copyfile(tone, pack / "cues" / f"{cue}.ogg")
                catalog["cues"][cue] = {"file": f"cues/{cue}.ogg", "nominal_end_seconds": 0.12,
                                        "media_duration_seconds": 2.12, "loop": False}
            if pack_id == "missing":
                (pack / "cues/g2.ogg").unlink()
            if pack_id == "linked":
                (pack / "cues/g0.ogg").unlink()
                (pack / "cues/g0.ogg").symlink_to(tone)
            if pack_id == "corrupt":
                (pack / "cues/g0.ogg").write_bytes(b"not ogg")
            if pack_id == "unsafe":
                catalog["cues"]["g0"]["file"] = "../tone.ogg"
            (pack / "pack.json").write_text(json.dumps(catalog))
        subprocess.run([sys.argv[1], str(root / "packs")], check=True)


if __name__ == "__main__":
    main()
