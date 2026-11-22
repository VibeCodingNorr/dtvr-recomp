# SPDX-License-Identifier: GPL-3.0-only
"""Music research parser checks using a generated, two-note MIDI fixture."""

from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from inspect_music import read_midi


def midi(track):
    return b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480) + b"MTrk" + struct.pack(">I", len(track)) + track


class MusicTests(unittest.TestCase):
    def test_tempo_changes_and_running_status(self):
        track = (b"\x00\x90\x3c\x64"          # note at 0
                 b"\x83\x60\x3c\x00"       # running-status off after 480 ticks / .5 s
                 b"\x00\xff\x51\x03\x0f\x42\x40"  # one second per quarter
                 b"\x00\x90\x40\x64"
                 b"\x83\x60\x80\x40\x00"  # off at 1.5 s
                 b"\x00\xff\x2f\x00")
        result = read_midi(midi(track))
        self.assertAlmostEqual(result["duration_seconds"], 1.5)
        self.assertEqual([e[0] for e in result["events"]], [0, 0.5, 0.5, 1.5])

    def test_track_end_silence(self):
        result = read_midi(midi(b"\x83\x60\xff\x2f\x00"))
        self.assertAlmostEqual(result["duration_seconds"], 0.5)
        self.assertEqual(result["events"], [])

    def test_invalid_inputs(self):
        good = midi(b"\x00\xff\x2f\x00")
        for data in [good[:n] for n in range(len(good))] + [
                good + b"extra", midi(b"\x00\x3c\x64"),
                midi(b"\x00\xff\x51\x03\x00\x00\x00"),
                midi(b"\xff\xff\xff\xff\x00"), midi(b"\x00\x90\x80\x00")]:
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                read_midi(data)


if __name__ == "__main__":
    unittest.main()
