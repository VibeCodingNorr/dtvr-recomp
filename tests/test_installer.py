# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import os
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from install import REPO, default_output, validate_output, install
from test_iso9660 import fixture


class InstallerTests(unittest.TestCase):
    def test_output_outside_checkout(self):
        with self.assertRaises(ValueError):
            validate_output(REPO)
        with self.assertRaises(ValueError):
            validate_output(REPO / "local/new-install")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.assertEqual(validate_output(root / "game"), root.resolve() / "game")
            with patch.dict(os.environ, {"XDG_DATA_HOME": str(root)}):
                self.assertEqual(default_output(), root / "dtvr-recomp")
            link = root / "checkout"
            link.symlink_to(REPO, target_is_directory=True)
            with self.assertRaises(ValueError):
                validate_output(link / "data")

    def test_invalid_original_is_cleaned_up(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image = root / "invalid-game.iso"
            image.write_bytes(fixture())
            with self.assertRaises(ValueError):
                install(image, root / "game", 1)
            self.assertFalse((root / "game").exists())
            self.assertEqual(sorted(p.name for p in root.iterdir()), ["invalid-game.iso"])


if __name__ == "__main__":
    unittest.main()
