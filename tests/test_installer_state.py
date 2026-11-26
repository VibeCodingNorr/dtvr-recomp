# SPDX-License-Identifier: GPL-3.0-only
"""Only local GUI selections are remembered; validation remains authoritative."""

import json
import os
from pathlib import Path
import stat
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from installer_state import load_paths, save_paths, state_path


class StateTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="dtvr-installer-state-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        environment = patch.dict(os.environ, {"XDG_STATE_HOME": str(self.root / "state"),
                                              "XDG_DATA_HOME": str(self.root / "data")})
        environment.start()
        self.addCleanup(environment.stop)

    def installation(self, target, legacy=False):
        target.mkdir(parents=True)
        (target / "manifest.json").write_text(json.dumps({"schema_version": 2,
            "format": "dtvr-recomb-installation" if legacy else "dtvr-recomp-installation"}))

    def test_private_atomic_roundtrip_and_custom_upgrade(self):
        source = self.root / "Disc $ ä.iso"
        target = self.root / "Custom game"
        save_paths(source, target, "iso", self.root / "bank.sf2", False, True)
        self.assertEqual(stat.S_IMODE(state_path().stat().st_mode), 0o600)
        self.assertEqual(list(state_path().parent.glob(".installer-*")), [])
        self.assertFalse(load_paths()["update"])
        self.installation(target)
        restored = load_paths()
        self.assertEqual(restored["source"], str(source))
        self.assertEqual(restored["output"], str(target))
        self.assertTrue(restored["update"])
        self.assertFalse(restored["music"])
        self.assertTrue(restored["dry"])
        self.assertFalse(source.exists())  # No source probing, import or implicit acceptance.
        self.assertFalse((self.root / "config").exists())

    def test_directory_selection_and_defaults_including_legacy_name(self):
        target = self.root / "data/dtvr-recomb"
        self.installation(target, legacy=True)
        self.assertEqual(load_paths()["output"], str(target))
        save_paths(self.root / "Original folder", target, "directory")
        self.assertEqual(load_paths()["source_kind"], "directory")
        self.assertTrue(load_paths()["update"])

    def test_invalid_state_is_ignored_and_does_not_hide_default_installation(self):
        target = self.root / "data/dtvr-recomp"
        self.installation(target)
        state_path().parent.mkdir(parents=True)
        for content in ("{", "[]", '"string"', 'x' * 65537,
                        '{"schema_version": 1, "output": "relative", "source": 12}',
                        '{"schema_version": 999, "output": "/ignored"}'):
            state_path().write_text(content)
            with self.subTest(content=content[:40]):
                self.assertTrue(load_paths()["update"])
                self.assertEqual(load_paths()["output"], str(target))
                self.assertNotIn("source", load_paths())

    def test_different_target_does_not_inherit_wrong_source(self):
        save_paths(self.root / "disc.iso", self.root / "deleted game", "iso")
        self.installation(self.root / "data/dtvr-recomp")
        self.assertNotIn("source", load_paths())

    def test_malformed_manifest_is_not_recognized(self):
        target = self.root / "data/dtvr-recomp"
        target.mkdir(parents=True)
        for value in ([], {"schema_version": 2, "format": []}, {"schema_version": 1}):
            (target / "manifest.json").write_text(json.dumps(value))
            self.assertFalse(load_paths()["update"])

    def test_relative_xdg_state_falls_back_to_home(self):
        with patch.dict(os.environ, {"XDG_STATE_HOME": "relative"}), patch("installer_state.Path.home", return_value=self.root):
            self.assertEqual(state_path(), self.root / ".local/state/dtvr-recomp/installer.json")


if __name__ == "__main__":
    unittest.main()
