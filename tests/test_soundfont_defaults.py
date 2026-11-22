# SPDX-License-Identifier: GPL-3.0-only
"""Music is an installer default; silence must be an explicit choice."""

from contextlib import redirect_stderr, redirect_stdout
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import install as installer
from soundfont_defaults import default_soundfont, soundfonts_for_install


class SoundfontDefaultsTests(unittest.TestCase):
    def test_known_bank_probe_checks_headers_and_preserves_priority(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            invalid = root / "invalid.sf2"
            invalid.write_bytes(b"not a soundfont")
            first = root / "first.sf2"
            first.write_bytes(b"RIFF\x04\x00\x00\x00sfbk")
            second = root / "second.sf3"
            second.write_bytes(first.read_bytes())
            self.assertEqual(default_soundfont([root / "missing", root, invalid, first, second]), first.resolve())
            self.assertEqual(default_soundfont([second, first]), second.resolve())
            self.assertIsNone(default_soundfont([root, invalid, root / "missing"]))

    def test_unreadable_bank_is_skipped(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "bank.sf2"
            path.touch()
            with patch.object(Path, "open", side_effect=PermissionError("denied")):
                self.assertIsNone(default_soundfont([path]))

    def test_original_music_uses_local_bank_by_default(self):
        bank = Path("/path/to/system-gm.sf2")
        with patch("soundfont_defaults.default_soundfont", return_value=bank):
            self.assertEqual(soundfonts_for_install(), [bank])

    def test_explicit_banks_keep_user_order_and_never_probe_system_banks(self):
        banks = [Path("/path/base.sf2"), Path("/path/overlay.sf3")]
        with patch("soundfont_defaults.default_soundfont") as discover:
            self.assertEqual(soundfonts_for_install(banks), banks)
            self.assertEqual(soundfonts_for_install(no_music=True), [])
            discover.assert_not_called()
        with self.assertRaises(ValueError):
            soundfonts_for_install(banks, no_music=True)

    def test_missing_bank_never_silently_turns_music_off(self):
        with patch("soundfont_defaults.default_soundfont", return_value=None):
            with self.assertRaisesRegex(ValueError, "--soundfont.*--no-music"):
                soundfonts_for_install()

    def test_cli_default_and_explicit_opt_out_reach_preflight(self):
        bank = Path("/path/to/system-gm.sf2")
        for extra, expected in (([], [bank]), (["--no-music"], []),
                                (["--soundfont", "/path/to/custom.sf2"], [Path("/path/to/custom.sf2")])):
            with self.subTest(extra=extra), patch.object(sys, "argv", ["install.py", "--iso", "/path/disc.iso", "--check", *extra]), \
                    patch("soundfont_defaults.default_soundfont", return_value=bank), \
                    patch.object(installer, "preflight", return_value={}) as preflight, redirect_stdout(io.StringIO()):
                installer.main()
                self.assertEqual(preflight.call_args.args[3], expected)

    def test_cli_install_uses_same_default_as_preflight(self):
        bank = Path("/path/to/system-gm.sf2")
        with patch.object(sys, "argv", ["install.py", "--iso", "/path/disc.iso"]), \
                patch("soundfont_defaults.default_soundfont", return_value=bank), \
                patch.object(installer, "install") as install:
            installer.main()
            self.assertEqual(install.call_args.args[3], [bank])

    def test_dependency_report_includes_rendering_unless_music_declined(self):
        for extra, rendering in (([], True), (["--no-music"], False)):
            with patch.object(sys, "argv", ["install.py", "--dependencies", *extra]), \
                    patch.object(installer, "interface_font", return_value="Test"), \
                    patch.object(installer, "dependency_report", return_value={}) as report, \
                    redirect_stdout(io.StringIO()):
                installer.main()
                self.assertEqual(report.call_args.kwargs["render"], rendering)

    def test_cli_missing_bank_fails_cleanly_before_writing(self):
        error = io.StringIO()
        with patch.object(sys, "argv", ["install.py", "--iso", "/path/disc.iso"]), \
                patch("soundfont_defaults.default_soundfont", return_value=None), \
                patch.object(installer, "install") as install, redirect_stderr(error), \
                self.assertRaises(SystemExit) as stopped:
            installer.main()
        self.assertEqual(stopped.exception.code, 1)
        self.assertIn("Installation failed:", error.getvalue())
        self.assertIn("--no-music", error.getvalue())
        install.assert_not_called()


if __name__ == "__main__":
    unittest.main()
