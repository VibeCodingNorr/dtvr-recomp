# SPDX-License-Identifier: GPL-3.0-only
"""Preflight checks use synthetic media metadata and never write installation data."""

import hashlib
from pathlib import Path, PurePosixPath
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import MagicMock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from installer_checks import preflight, validate_output


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
        image.read.return_value = b"test"
        image.files.return_value = [(PurePosixPath(name), i, 4) for i, name in enumerate(
            ["program/rally.exe", "program/butt/1/map.bmp", "program/map/0/part0000.sbb", "program/sbs/d.sbs"])]
        for target, replacement in [
                ("installer_checks.IsoImage", MagicMock(return_value=image)),
                ("installer_checks.KNOWN_EXE", hashlib.sha256(b"test").hexdigest()),
                ("installer_checks.shutil.which", MagicMock(return_value="tool")),
                ("installer_checks.subprocess.run", MagicMock(return_value=subprocess.CompletedProcess([], 0))),
                ("installer_checks.shutil.disk_usage", MagicMock(return_value=SimpleNamespace(free=2**32)))]:
            manager = patch(target, replacement)
            manager.start()
            self.addCleanup(manager.stop)

    def test_success_is_read_only(self):
        report = preflight(self.iso, self.output)
        self.assertEqual(report["program_files"], 4)
        self.assertIn("ctest", report["dependencies"])
        self.assertFalse(self.output.parent.exists())
        self.assertEqual(list(self.root.iterdir()), [self.iso])

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


if __name__ == "__main__":
    unittest.main()
