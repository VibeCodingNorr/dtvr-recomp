# SPDX-License-Identifier: GPL-3.0-only
"""Preflight checks use synthetic media metadata and never write installation data."""

import hashlib
import os
from pathlib import Path, PurePosixPath
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import MagicMock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from installer_checks import preflight, validate_output, interface_font
from installer_scenario import synthetic_profiles


class PreflightTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.iso = self.root / "test.iso"
        self.iso.write_bytes(b"synthetic disc metadata")
        self.output = self.root / "nested/install"
        image = MagicMock()
        image.__enter__.return_value = image
        image.sector_size = 2048
        # Entry 4 is dat/lan.inf ("D"), every other read is the synthetic executable.
        image.read.side_effect = lambda block, size: b"D\r\n"[:size] if block == 4 else b"test"
        image.files.return_value = [(PurePosixPath(name), i, 4) for i, name in enumerate(
            ["program/rally.exe", "program/butt/1/map.bmp", "program/map/0/part0000.sbb", "program/sbs/d.sbs",
             "program/dat/lan.inf"])]
        for target, replacement in [
                ("installer_checks.IsoImage", MagicMock(return_value=image)),
                ("editions.PROFILES", synthetic_profiles(hashlib.sha256(b"test").hexdigest())),
                ("installer_checks.interface_font", MagicMock(return_value="Liberation Sans")),
                ("installer_checks.shutil.which", MagicMock(return_value="tool")),
                ("installer_checks.subprocess.run", MagicMock(return_value=subprocess.CompletedProcess([], 0))),
                ("installer_checks.shutil.disk_usage", MagicMock(return_value=SimpleNamespace(free=2**32)))]:
            manager = patch(target, replacement)
            manager.start()
            self.addCleanup(manager.stop)

    def test_success_is_read_only(self):
        report = preflight(self.iso, self.output)
        self.assertEqual(report["program_files"], 5)
        self.assertEqual(report["edition"], "de-retail")
        self.assertIn("ctest", report["dependencies"])
        self.assertFalse(self.output.parent.exists())
        self.assertEqual(list(self.root.iterdir()), [self.iso])

    def test_demo_on_a_cover_disc_is_refused_as_demo(self):
        # A cover disc with the demo below rally/program/ instead of program/.
        image = MagicMock()
        image.__enter__.return_value = image
        image.read.return_value = b"demo"
        image.files.return_value = [(PurePosixPath("rally/program/rally.exe"), 1, 4),
                                    (PurePosixPath("rally/readme.wri"), 2, 4)]
        demo = hashlib.sha256(b"demo").hexdigest()
        with patch("installer_checks.IsoImage", MagicMock(return_value=image)), \
                patch.dict("editions.KNOWN_DEMOS", {demo: "Demoversion von Test"}):
            with self.assertRaisesRegex(ValueError, "^Demoversion von Test – nicht unterstützt"):
                preflight(self.iso, self.output)
            image.files.return_value = [(PurePosixPath("other/program/rally.exe"), 1, 4)]
            image.read.return_value = b"unknown"
            with self.assertRaisesRegex(ValueError, "Unsupported original executable"):
                preflight(self.iso, self.output)
        self.assertFalse(self.output.parent.exists())

    def test_missing_dependencies_are_reported_together(self):
        with patch("installer_checks.shutil.which", return_value=None):
            with self.assertRaisesRegex(ValueError, "cmake, ctest, c\\+\\+, pkg-config"):
                preflight(self.iso, self.output)
        self.assertFalse(self.output.parent.exists())

    def test_sdl_and_space_errors(self):
        with patch("installer_checks.subprocess.run", return_value=subprocess.CompletedProcess([], 1)):
            with self.assertRaisesRegex(ValueError, "SDL2 development"):
                preflight(self.iso, self.output)
        with patch("installer_checks.shutil.disk_usage", return_value=SimpleNamespace(free=0)):
            with self.assertRaisesRegex(ValueError, "disk space"):
                preflight(self.iso, self.output)

    def test_invalid_job_count_and_link(self):
        for jobs in (0, -1, 257, True):
            with self.assertRaises(ValueError):
                preflight(self.iso, self.output, jobs)
        link = self.root / "dangling"
        link.symlink_to(self.output)
        with self.assertRaises(ValueError):
            validate_output(link)

    def test_ttf_and_explicit_update_selection(self):
        with patch("installer_checks.subprocess.run", side_effect=lambda command, **kwargs:
                subprocess.CompletedProcess(command, int("SDL2_ttf" in command))):
            with self.assertRaisesRegex(ValueError, "SDL2_ttf"):
                preflight(self.iso, self.output)
        self.output.mkdir(parents=True)
        with self.assertRaisesRegex(ValueError, "already exists"):
            preflight(self.iso, self.output)
        with self.assertRaises(OSError):
            preflight(self.iso, self.output, update=True)

    def test_font_family_requires_all_faces(self):
        fonts = self.root / "fonts"
        fonts.mkdir()
        for face in ("Regular", "Bold", "Italic"):
            (fonts / f"LiberationSans-{face}.ttf").touch()
        self.assertIsNone(interface_font([fonts]))
        (fonts / "LiberationSans-BoldItalic.ttf").touch()
        self.assertEqual(interface_font([fonts]), "Liberation Sans")
        with patch("installer_checks.interface_font", return_value=None):
            with self.assertRaisesRegex(ValueError, "font family"):
                preflight(self.iso, self.output)

    def test_native_ogg_dependencies(self):
        with patch("installer_checks.subprocess.run", side_effect=lambda command, **kwargs:
                subprocess.CompletedProcess(command, int("vorbisfile" in command))):
            with self.assertRaisesRegex(ValueError, "vorbisfile"):
                preflight(self.iso, self.output)

    def test_unwritable_destination_does_not_create_files(self):
        with patch("installer_checks.os.access", return_value=False):
            with self.assertRaisesRegex(ValueError, "not writable"):
                preflight(self.iso, self.output)
        self.assertFalse(self.output.parent.exists())

    def test_update_cannot_move_its_own_source_iso(self):
        from test_installer_update import installation
        installation(self.output)
        inside = self.output / "original.iso"
        self.iso.rename(inside)
        with self.assertRaisesRegex(ValueError, "ISO.*outside"):
            preflight(inside, self.output, update=True)
        self.assertEqual(inside.read_bytes(), b"synthetic disc metadata")

    @unittest.skipIf(os.geteuid() == 0, "root bypasses directory write permissions")
    def test_actual_directory_permissions(self):
        blocked = self.root / "read-only parent"
        blocked.mkdir()
        blocked.chmod(0o500)
        try:
            with self.assertRaisesRegex(ValueError, "not writable"):
                preflight(self.iso, blocked / "installation")
            self.assertFalse((blocked / "installation").exists())
        finally:
            blocked.chmod(0o700)


if __name__ == "__main__":
    unittest.main()
