# SPDX-License-Identifier: GPL-3.0-only
"""The cd-hybrid sound set: recordings for g0, g1, g3, g4, g5, renderings for the rest."""

import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from cd_soundtrack import RECORDINGS, build_cd_hybrid
from soundtrack import CUES, render_pack
from test_soundtrack import fake_run, inputs


def music_dir(root, segments=("segment-01", "segment-02", "segment-03", "segment-04", "segment-05", "segment-06")):
    music = root / "music"
    music.mkdir()
    regions = []
    for name in segments:
        (music / f"{name}.ogg").write_bytes(f"synthetic recording {name}".encode())
        regions.append({"name": name, "file": f"{name}.ogg", "source_pcm_sha256": "0" * 64,
                        "duration_seconds": 100.0})
    (music / "audio.json").write_text(json.dumps({
        "profile_version": 1, "boundary_source": "observed-nonzero-regions-not-original-toc",
        "original_track_numbers_known": False, "pcm_format": "s16be", "regions": regions}))
    return music


class CdSoundtrackTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        midi, bank = inputs(self.root)
        self.patches = [patch("soundtrack.tool_versions", return_value={"test": "synthetic"}),
                        patch("soundtrack.run", side_effect=fake_run),
                        patch("soundtrack.probe_ogg", return_value=2.5),
                        patch("cd_soundtrack.probe_ogg", return_value=178.25)]
        for p in self.patches:
            p.start()
            self.addCleanup(p.stop)
        render_pack(midi, [bank], self.root / "rendered")

    def test_recordings_and_marked_supplements(self):
        catalog = build_cd_hybrid(music_dir(self.root), self.root / "rendered", self.root / "hybrid")
        self.assertEqual(catalog["source_kind"], "cd-hybrid")
        self.assertEqual(set(catalog["cues"]), set(CUES))
        recorded = {cue for cue, e in catalog["cues"].items() if e["source"] == "cd-recording"}
        self.assertEqual(recorded, {"g0", "g1", "g3", "g4", "g5"})
        for cue, (segment, _) in RECORDINGS.items():
            entry = catalog["cues"][cue]
            self.assertEqual(entry["region"], segment)
            self.assertEqual(entry["nominal_end_seconds"], entry["media_duration_seconds"])  # no rendered tail
            self.assertEqual((self.root / "hybrid" / entry["file"]).read_bytes(),
                             (self.root / "music" / f"{segment}.ogg").read_bytes())
        # The intro excerpt (segment-06) does not replace the complete intro.
        self.assertEqual(catalog["cues"]["i"]["source"], "soundfont-render")
        self.assertEqual(catalog["cues"]["g2"]["source"], "soundfont-render")
        self.assertEqual(catalog["supplement"]["pack_id"], "rendered-gm")
        self.assertEqual(json.loads((self.root / "hybrid/pack.json").read_text())["name"], "CD-Aufnahmen + MIDI-Ergänzungen")

    def test_refusals_leave_nothing_behind(self):
        with self.assertRaisesRegex(ValueError, "segment-04"):
            build_cd_hybrid(music_dir(self.root, ("segment-01", "segment-02", "segment-03", "segment-05")),
                            self.root / "rendered", self.root / "hybrid")
        self.assertFalse((self.root / "hybrid").exists())
        (self.root / "taken").mkdir()
        with self.assertRaisesRegex(ValueError, "already exists"):
            build_cd_hybrid(self.root / "music", self.root / "rendered", self.root / "taken")
        # A changed supplement is refused by the rendering's own hashes.
        (self.root / "rendered/cues/e1.ogg").write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            build_cd_hybrid(self.root / "music", self.root / "rendered", self.root / "hybrid")


if __name__ == "__main__":
    unittest.main()
