# SPDX-License-Identifier: GPL-3.0-only
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from installer_dependencies import dependency_report, distribution, package_command
from installer_diagnostics import DiagnosticLog, redact
from installer_folders import child_directories, nearest_directory


class DependencyReportTests(unittest.TestCase):
    def test_collects_all_missing_tools_and_libraries(self):
        with patch("installer_dependencies.shutil.which", return_value="tool"), patch(
                "installer_dependencies.subprocess.run", return_value=subprocess.CompletedProcess([], 1)):
            report = dependency_report(None, audio=True, render=True,
                                       system=distribution({"ID": "ubuntu"}))
        self.assertFalse(report["ready"])
        self.assertTrue({"sdl2", "SDL2_ttf", "vorbisfile", "json-c", "fonts", "vorbis-encoder"} <= set(report["missing"]))
        self.assertIn("libsdl2-dev", report["command"])
        self.assertEqual(report["command"].count("ffmpeg"), 1)

    def test_no_pkg_config_does_not_guess_installed_libraries(self):
        with patch("installer_dependencies.shutil.which", return_value=None), patch(
                "installer_dependencies.subprocess.run") as run:
            report = dependency_report("Arial", system=distribution({"ID": "cachyos", "ID_LIKE": "arch"}))
        run.assert_not_called()
        self.assertIn("pkgconf", report["command"])
        self.assertNotIn("sdl2-compat", report["command"])
        self.assertEqual(next(item for item in report["items"] if item["id"] == "sdl2")["state"], "unknown")

    def test_distribution_values_never_become_shell_code(self):
        unknown = distribution({"ID": "$(malicious)", "PRETTY_NAME": "Unknown\nSystem"})
        self.assertEqual(package_command(["cmake"], unknown), "")
        self.assertNotIn("\n", unknown["name"])
        arch = distribution({"ID": "custom", "ID_LIKE": "arch"})
        self.assertEqual(package_command(["sdl2", "fonts"], arch),
                         "sudo pacman -S --needed sdl2-compat ttf-liberation")
        self.assertEqual(package_command([], arch), "")


class DiagnosticTests(unittest.TestCase):
    def test_paths_with_spaces_quotes_and_escape_sequences_are_removed(self):
        private = "/private/test user/my disc.iso"
        source = ("Tool missing: cmake\n" + private + "\n"
                  'error: "/unknown/user data/folder/file"\n'
                  r"C:\private\user\file" + "\n"
                  r"quoted \\u002ftmp\\u002fprivate" + "\n"
                  "\x1b[31mBuild failed\x1b[0m")
        cleaned = redact(source, [private])
        self.assertNotIn(private, cleaned)
        self.assertNotIn("user data", cleaned)
        self.assertNotIn("private", cleaned)
        self.assertIn("Tool missing: cmake", cleaned)
        self.assertIn("Build failed", cleaned)

    def test_export_contains_bounded_logs_without_private_paths(self):
        log = DiagnosticLog()
        for number in range(1600):
            log.append(f"line {number}")
        log.append("failed at /unknown/private/project")
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "diagnostic.txt"
            log.export(target)
            text = target.read_text()
        self.assertNotIn("line 0\n", text)
        self.assertIn("line 1599", text)
        self.assertNotIn("/unknown", text)
        self.assertIn("Version:", text)


class FolderModelTests(unittest.TestCase):
    def test_immediate_directories_hidden_toggle_and_parent_fallback(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name in ("Zulu", "alpha", ".hidden"):
                (root / name).mkdir()
            (root / "file.txt").write_text("not a directory")
            self.assertEqual([p.name for p in child_directories(root)], ["alpha", "Zulu"])
            self.assertEqual([p.name for p in child_directories(root, True)], [".hidden", "alpha", "Zulu"])
            self.assertEqual(nearest_directory(root / "missing/deeper"), root.resolve())
            with self.assertRaises(OSError):
                child_directories(root / "file.txt")


if __name__ == "__main__":
    unittest.main()
