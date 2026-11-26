# SPDX-License-Identifier: GPL-3.0-only
"""Release Ogg tracks as a non-original CD music source: generated tones only, no release media."""

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import MagicMock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import release_ogg
from release_ogg import (ENGLISH_RETAIL, LABEL, check_release_audio, import_release_audio, language_letter,
                         parse_cue, release_profile, require_edition)
from cd_audio import KNOWN_ISO as GERMAN_ISO
from installer_checks import KNOWN_EXE as GERMAN_EXE, preflight

FFMPEG = bool(shutil.which("ffmpeg") and shutil.which("ffprobe"))
CUE = "".join(['FILE "track01.iso" BINARY\n  TRACK 01 MODE1/2048\n    INDEX 01 00:00:00\n',
               *(f'FILE "track{n:02}.ogg" MP3\n  TRACK {n:02} AUDIO\n'
                 + ("    PREGAP 00:02:00\n" if n == 2 else "") + "    INDEX 01 00:00:00\n" for n in range(2, 8))])


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def release_dir(root):
    """A generated release folder (cue + six tone Ogg files) and its pinned profile."""
    folder = root / "release"
    folder.mkdir()
    (folder / "track01.cue").write_text(CUE)
    tracks = []
    for index, track in enumerate(range(2, 8), 1):
        name = f"track{track:02}.ogg"
        subprocess.run(["ffmpeg", "-nostdin", "-v", "error", "-f", "lavfi", "-i",
                        f"sine=frequency={220 * index}:sample_rate=44100:duration=1",
                        "-af", "apad=pad_dur=0.5", "-ac", "2", "-c:a", "libvorbis", str(folder / name)], check=True)
        samples = int(json.loads(subprocess.run(
            ["ffprobe", "-v", "error", "-show_streams", "-of", "json", str(folder / name)],
            check=True, capture_output=True, text=True).stdout)["streams"][0]["duration_ts"])
        tracks.append((f"segment-{index:02}", track, name, sha256(folder / name), samples, 0.0, 0.5))
    profile = {**ENGLISH_RETAIL, "iso_sha256": "1" * 64, "tracks": tracks,
               "cue": {"file": "track01.cue", "sha256": sha256(folder / "track01.cue")}}
    return folder, profile


class ProfileTests(unittest.TestCase):
    def test_pinned_english_profile(self):
        self.assertIs(release_profile(ENGLISH_RETAIL["iso_sha256"]), ENGLISH_RETAIL)
        self.assertIsNone(release_profile(GERMAN_ISO))
        tracks = ENGLISH_RETAIL["tracks"]
        self.assertEqual([(t[0], t[1], t[2]) for t in tracks],
                         [(f"segment-{i:02}", i + 1, f"track{i + 1:02}.ogg") for i in range(1, 7)])
        for *_, samples, leading, trailing in tracks:
            self.assertTrue(0 < leading < 0.1 and 1.9 < trailing < 4.5 and samples > 44100 * 100)

    def test_edition_gate(self):
        require_edition(ENGLISH_RETAIL, ENGLISH_RETAIL["executable_sha256"], "e")
        for exe, letter in [(GERMAN_EXE, "d"), (GERMAN_EXE, "e"), (ENGLISH_RETAIL["executable_sha256"], "d")]:
            with self.subTest(exe=exe[:8], letter=letter), self.assertRaisesRegex(ValueError, "nicht original"):
                require_edition(ENGLISH_RETAIL, exe, letter)

    def test_language_letter_is_case_insensitive(self):
        with tempfile.TemporaryDirectory() as temporary:
            program = Path(temporary)
            (program / "DAT").mkdir()
            (program / "DAT/LAN.INF").write_bytes(b"E\r\n")
            self.assertEqual(language_letter(program), "e")
            (program / "DAT/LAN.INF").write_bytes(b"\r\n")
            with self.assertRaises(ValueError):
                language_letter(program)
            (program / "DAT/LAN.INF").unlink()
            with self.assertRaises(ValueError):
                language_letter(program)

    def test_cue_mapping_and_malformed_sheets(self):
        sheet = parse_cue(CUE)
        self.assertEqual(sheet[1], ("track01.iso", "MODE1/2048"))
        self.assertEqual({n: sheet[n] for n in range(2, 8)}, {n: (f"track{n:02}.ogg", "AUDIO") for n in range(2, 8)})
        for bad in ['FILE "../x.ogg" MP3\n  TRACK 02 AUDIO\n', "  TRACK 02 AUDIO\n",
                    'FILE "a.ogg" MP3\n  TRACK 02 AUDIO\n  TRACK 02 AUDIO\n', "x" * 20000]:
            with self.subTest(bad=bad[:30]), self.assertRaises(ValueError):
                parse_cue(bad)


@unittest.skipUnless(FFMPEG, "FFmpeg not installed")
class ImportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.folder, self.profile = release_dir(self.root)

    def test_copy_is_byte_identical_and_marked(self):
        record = import_release_audio(self.folder, self.root / "music", self.profile)
        for segment, track, name, digest, samples, _, trailing in self.profile["tracks"]:
            copied = self.root / "music" / f"{segment}.ogg"
            self.assertEqual(copied.read_bytes(), (self.folder / name).read_bytes())
            region = next(r for r in record["regions"] if r["name"] == segment)
            self.assertEqual((region["source_file"], region["cue_track"], region["ogg_sha256"]), (name, track, digest))
            self.assertAlmostEqual(region["music_end_seconds"], samples / 44100 - trailing, places=5)
        stored = json.loads((self.root / "music/audio.json").read_text())
        self.assertEqual(stored, record)
        self.assertEqual((stored["label"], stored["source_kind"], stored["original"]), (LABEL, "release-ogg", False))
        self.assertTrue(stored["lossy_reencode_by_release"] and stored["copied_unmodified"])
        self.assertFalse(stored["original_track_numbers_known"])
        with self.assertRaisesRegex(ValueError, "already exists"):
            import_release_audio(self.folder, self.root / "music", self.profile)

    def test_hash_mismatches_are_refused_and_leave_nothing(self):
        data = bytearray((self.folder / "track05.ogg").read_bytes())
        data[-1] ^= 1
        (self.folder / "track05.ogg").write_bytes(bytes(data))
        with self.assertRaisesRegex(ValueError, "hash mismatch: track05.ogg"):
            import_release_audio(self.folder, self.root / "music", self.profile)
        self.assertFalse((self.root / "music").exists())
        self.assertEqual([p.name for p in self.root.iterdir() if p.name.startswith(".")], [])
        (self.folder / "track01.cue").write_text(CUE.replace("PREGAP 00:02:00", "PREGAP 00:02:01"))
        with self.assertRaisesRegex(ValueError, "cue sheet hash mismatch"):
            check_release_audio(self.folder, self.profile)

    def test_missing_and_linked_files(self):
        (self.folder / "track07.ogg").rename(self.root / "elsewhere.ogg")
        with self.assertRaisesRegex(ValueError, "track07.ogg missing"):
            check_release_audio(self.folder, self.profile)
        (self.folder / "track07.ogg").symlink_to(self.root / "elsewhere.ogg")
        with self.assertRaisesRegex(ValueError, "track07.ogg missing or linked"):
            check_release_audio(self.folder, self.profile)
        os.symlink(self.folder, self.root / "linked")
        with self.assertRaises(ValueError):
            check_release_audio(self.root / "linked", self.profile)

    def test_hybrid_catalog_maps_cue_tracks_to_segments(self):
        from cd_soundtrack import RECORDINGS, build_cd_hybrid
        from soundtrack import render_pack
        from test_soundtrack import fake_run, inputs
        import_release_audio(self.folder, self.root / "music", self.profile)
        midi, bank = inputs(self.root)
        samples = {sha: count for *_, sha, count, _, _ in self.profile["tracks"]}
        with patch("soundtrack.tool_versions", return_value={"test": "synthetic"}), \
                patch("soundtrack.run", side_effect=fake_run), patch("soundtrack.probe_ogg", return_value=2.5), \
                patch("cd_soundtrack.probe_ogg", side_effect=lambda path: samples[sha256(path)] / 44100):
            render_pack(midi, [bank], self.root / "rendered")
            catalog = build_cd_hybrid(self.root / "music", self.root / "rendered", self.root / "packs/release-ogg-hybrid")
        self.assertEqual((catalog["id"], catalog["source_kind"]), ("release-ogg-hybrid", "release-ogg-hybrid"))
        self.assertTrue(catalog["name"].startswith(LABEL))
        self.assertEqual((catalog["cd_audio"]["label"], catalog["cd_audio"]["original"]), (LABEL, False))
        tracks = {t[0]: t for t in self.profile["tracks"]}
        for cue, (segment, _) in RECORDINGS.items():
            entry = catalog["cues"][cue]
            self.assertEqual(entry["source"], "release-ogg-recording")
            self.assertEqual((entry["release_file"], entry["cue_track"]), (tracks[segment][2], tracks[segment][1]))
            self.assertAlmostEqual(entry["media_duration_seconds"] - entry["nominal_end_seconds"], 0.5, places=5)
            self.assertEqual((self.root / "packs/release-ogg-hybrid" / entry["file"]).read_bytes(),
                             (self.folder / tracks[segment][2]).read_bytes())
        self.assertEqual(catalog["cues"]["i"]["source"], "soundfont-render")


class PreflightTests(unittest.TestCase):
    """--cd-audio-dir in the read-only preflight, with synthetic disc metadata."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.iso = self.root / "test.iso"
        self.iso.write_bytes(b"synthetic disc metadata")
        self.output = self.root / "nested/install"
        self.audio = self.root / "audio"
        self.audio.mkdir()
        image = MagicMock()
        image.__enter__.return_value = image
        image.sector_size = 2048
        image.read.return_value = b"test"
        image.files.return_value = [(PurePosixPath(name), i, 4) for i, name in enumerate(
            ["program/rally.exe", "program/butt/1/map.bmp", "program/map/0/part0000.sbb", "program/sbs/d.sbs"])]
        self.checked = MagicMock(side_effect=lambda directory, profile: directory.resolve())
        for target, replacement in [
                ("installer_checks.IsoImage", MagicMock(return_value=image)),
                ("installer_checks.KNOWN_EXE", hashlib.sha256(b"test").hexdigest()),
                ("installer_checks.interface_font", MagicMock(return_value="Liberation Sans")),
                ("installer_checks.shutil.which", MagicMock(return_value="tool")),
                ("installer_checks.subprocess.run", MagicMock(return_value=subprocess.CompletedProcess([], 0))),
                ("installer_checks.shutil.disk_usage", MagicMock(return_value=SimpleNamespace(free=2**32))),
                ("installer_checks.check_release_audio", self.checked)]:
            manager = patch(target, replacement)
            manager.start()
            self.addCleanup(manager.stop)

    def test_other_editions_and_directories_are_refused(self):
        # The synthetic image is not the pinned English retail image (like the German disc).
        with self.assertRaisesRegex(ValueError, "English retail"):
            preflight(self.iso, self.output, cd_audio_dir=self.audio)
        with self.assertRaisesRegex(ValueError, "--iso"):
            preflight(None, self.output, directory=self.root, cd_audio_dir=self.audio)
        self.checked.assert_not_called()
        self.assertIsNone(preflight(self.iso, self.output)["cd_audio_source"])

    def test_pinned_image_reports_the_non_original_source(self):
        profile = {**ENGLISH_RETAIL, "iso_sha256": sha256(self.iso)}
        with patch.object(release_ogg, "PROFILES", (profile,)):
            report = preflight(self.iso, self.output, cd_audio_dir=self.audio)
            self.assertEqual((report["cd_audio_source"], report["cd_audio_supported"]), (LABEL, True))
            self.assertEqual(report["cd_audio_dir"], str(self.audio.resolve()))
            self.checked.assert_called_once()
            with self.assertRaisesRegex(ValueError, "overlap"):
                preflight(self.iso, self.audio / "install", cd_audio_dir=self.audio)
        self.assertFalse(self.output.parent.exists())


if __name__ == "__main__":
    unittest.main()
