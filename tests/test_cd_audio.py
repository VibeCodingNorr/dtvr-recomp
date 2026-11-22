# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic CD PCM tests; no original music in the repository."""

import copy
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from cd_audio import audio_layout, convert_audio, validate_layout, KNOWN_ISO


def layout(fmt="s16be"):
    return {"pcm_format": fmt, "sample_rate": 44100, "channels": 2,
            "sector_bytes": 2352,
            "regions": [{"name": "segment-01", "start_sector": 2, "end_sector_exclusive": 77}]}


class AudioTests(unittest.TestCase):
    def test_known_profile_and_unknown_image(self):
        profile = audio_layout(KNOWN_ISO, 114479 * 2352)
        self.assertEqual(len(profile["regions"]), 6)
        self.assertFalse(profile["original_track_numbers_known"])
        validate_layout(profile, 114479 * 2352)
        self.assertIsNone(audio_layout("unknown", 114479 * 2352))
        with self.assertRaises(ValueError):
            audio_layout(KNOWN_ISO, 123)

    def test_invalid_regions(self):
        bad = []
        for field, value in [("name", "../escape"), ("start_sector", -1),
                             ("end_sector_exclusive", 1000), ("start_sector", 2.5),
                             ("end_sector_exclusive", 2)]:
            case = layout()
            case["regions"][0][field] = value
            bad.append(case)
        overlap = layout()
        overlap["regions"].append({"name": "segment-02", "start_sector": 76, "end_sector_exclusive": 80})
        bad.append(overlap)
        duplicate = layout()
        duplicate["regions"].append(copy.deepcopy(duplicate["regions"][0]))
        bad.append(duplicate)
        for case in bad:
            with self.subTest(case=case), self.assertRaises(ValueError):
                validate_layout(case, 80 * 2352)
        with self.assertRaises(ValueError):
            validate_layout(layout(), 80 * 2352 - 1)

    def test_missing_encoder_leaves_no_destination(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image = root / "test.iso"
            image.write_bytes(bytes(80 * 2352))
            with patch("cd_audio.shutil.which", return_value=None), self.assertRaises(ValueError):
                convert_audio(image, root / "music", layout())
            self.assertFalse((root / "music").exists())

    @unittest.skipUnless(shutil.which("ffmpeg") and shutil.which("ffprobe"), "FFmpeg not installed")
    def test_tone_conversion_and_endianness(self):
        for fmt, endian in [("s16be", ">"), ("s16le", "<")]:
            with self.subTest(fmt=fmt), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                image = root / "test.iso"
                pcm = bytearray()
                for sample in range(44100):
                    value = int(8000 * math.sin(2 * math.pi * 440 * sample / 44100))
                    pcm += struct.pack(endian + "hh", value, value)
                image.write_bytes(bytes(2 * 2352) + pcm + bytes(3 * 2352))
                report = convert_audio(image, root / "music", layout(fmt))
                self.assertEqual(report["regions"][0]["duration_seconds"], 1)
                self.assertEqual(json.loads((root / "music/audio.json").read_text())["codec"], "vorbis")
                decoded = subprocess.run(["ffmpeg", "-v", "error", "-i", str(root / "music/segment-01.ogg"),
                                          "-f", "s16le", "-"], check=True, capture_output=True).stdout
                samples = struct.unpack("<" + str(len(decoded) // 2) + "h", decoded)
                rms = math.sqrt(sum(s * s for s in samples) / len(samples))
                self.assertLess(abs(rms - 8000 / math.sqrt(2)), 600)
                with self.assertRaises(ValueError):
                    convert_audio(image, root / "music", layout(fmt))


if __name__ == "__main__":
    unittest.main()
