# SPDX-License-Identifier: GPL-3.0-only
"""Soundtrack completeness, atomic failure and self-contained verification checks."""

import copy
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from soundtrack import CUES, REPO, external_destination, render_pack, validate_catalog, validate_identity, verify_pack


MIDI = (b"MThd\x00\x00\x00\x06\x00\x00\x00\x01\x01\xe0"
        b"MTrk\x00\x00\x00\x0d\x00\x90\x3c\x64\x83\x60\x80\x3c\x00\x00\xff\x2f\x00")


def inputs(root):
    midi = root / "midi"
    midi.mkdir()
    for cue in CUES:
        (midi / cue).write_bytes(MIDI)
    bank = root / "fake.sf2"
    bank.write_bytes(b"RIFF\x04\x00\x00\x00sfbk")
    return midi, bank


def fake_run(command):
    if command[0] == "fluidsynth":
        Path(command[command.index("-F") + 1]).write_bytes(b"synthetic PCM placeholder")
    elif command[0] == "ffmpeg":
        Path(command[-1]).write_bytes(b"synthetic Ogg placeholder")
    return subprocess.CompletedProcess(command, 0, "", "")


class SoundtrackTests(unittest.TestCase):
    def test_identity_and_external_output(self):
        for pack_id in ("../escape", "a/b", "UPPER", "", "x" * 49):
            with self.assertRaises(ValueError):
                validate_identity(pack_id, "Test", "classic")
        with self.assertRaises(ValueError):
            validate_identity("test", "Test\n", "classic")
        with self.assertRaises(ValueError):
            external_destination(REPO / "local/music")

    def test_complete_pack_and_no_runtime_source_dependency(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            midi, bank = inputs(root)
            with patch("soundtrack.tool_versions", return_value={"test": "synthetic"}), \
                    patch("soundtrack.run", side_effect=fake_run) as process, \
                    patch("soundtrack.probe_ogg", return_value=2.5):
                catalog = render_pack(midi, [bank], root / "pack")
                self.assertEqual(len(catalog["cues"]), 25)
                self.assertEqual(len(list((root / "pack/cues").glob("*.ogg"))), 25)
                self.assertEqual(process.call_count, 2)  # identical MIDI files render only once
                for cue in CUES:
                    (midi / cue).unlink()
                bank.unlink()
                self.assertEqual(verify_pack(root / "pack")["id"], "rendered-gm")
                self.assertFalse(list((root / "pack").rglob("*.sf2")))
                self.assertFalse(list((root / "pack").rglob("*.mid")))
                bad = copy.deepcopy(catalog)
                del bad["cues"]["g2"]
                with self.assertRaises(ValueError):
                    validate_catalog(bad)
                bad = copy.deepcopy(catalog)
                bad["cues"]["g2"]["file"] = "../elsewhere.ogg"
                with self.assertRaises(ValueError):
                    validate_catalog(bad)
                bad = copy.deepcopy(catalog)
                bad["cues"]["g2"]["media_duration_seconds"] = float("nan")
                with self.assertRaises(ValueError):
                    validate_catalog(bad)
                for malformed in (None, [], {**catalog, "cues": None}, {**catalog, "id": None}):
                    with self.assertRaises(ValueError):
                        validate_catalog(malformed)
                (root / "pack/cues/g2.ogg").write_bytes(b"changed")
                with self.assertRaisesRegex(ValueError, "hash mismatch"):
                    verify_pack(root / "pack")

    def test_failed_render_is_not_published(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            midi, bank = inputs(root)
            with patch("soundtrack.tool_versions", return_value={}), \
                    patch("soundtrack.run", side_effect=subprocess.CalledProcessError(1, ["renderer"])):
                with self.assertRaises(subprocess.CalledProcessError):
                    render_pack(midi, [bank], root / "pack")
            self.assertFalse((root / "pack").exists())
            self.assertFalse(list(root.glob(".dtvr-soundtrack-*")))

    def test_missing_cue_is_rejected_before_rendering(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            midi, bank = inputs(root)
            (midi / "g2").unlink()
            with self.assertRaisesRegex(ValueError, "g2"):
                render_pack(midi, [bank], root / "pack")
            self.assertFalse((root / "pack").exists())

    @unittest.skipUnless(os.environ.get("DTVR_TEST_SOUNDFONT"), "No test SoundFont explicitly supplied")
    def test_real_render_of_synthetic_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            midi, _ = inputs(root)
            render_pack(midi, [Path(os.environ["DTVR_TEST_SOUNDFONT"])], root / "real")
            self.assertEqual(len(verify_pack(root / "real")["cues"]), 25)


if __name__ == "__main__":
    unittest.main()
