from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_repository import check_file


class PolicyTests(unittest.TestCase):
    def test_reject_original_content_and_private_paths(self):
        for name in ("track.OGG", "image.iso", "map/europe.map", "segment.asm", "original.exe"):
            self.assertIsNotNone(check_file(name, b"example"))
        self.assertIsNotNone(check_file("hidden.txt", b"\x00game"))
        self.assertIsNotNone(check_file("binary.txt", b"\xff"))
        self.assertIsNotNone(check_file("link", b"somewhere", True))
        private = "/" + "/".join(("media", "disk", "game.iso"))
        self.assertIsNotNone(check_file("README.md", private.encode()))

    def test_allow_technical_names_and_generic_paths(self):
        self.assertIsNone(check_file("README.md", b"program/rally.exe; use /path/to/game.iso"))
        self.assertIsNone(check_file("src/reader.cpp", b"// Our format reader\n"))
        self.assertIsNone(check_file("README.md", b"source/media/path checks"))


if __name__ == "__main__":
    unittest.main()
