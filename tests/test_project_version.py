# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from project_version import read_version, validate_release, version_tuple


class VersionTests(unittest.TestCase):
    def test_version_and_release_increase(self):
        self.assertEqual(len(version_tuple(read_version())), 3)
        validate_release("v0.0.2", "0.0.2", "0.0.1")
        for tag, version, previous in [("v0.0.1", "0.0.1", "0.0.1"),
                                       ("v0.0.1", "0.0.1", "0.0.2"),
                                       ("v0.0.2", "0.0.1", None)]:
            with self.assertRaises(ValueError):
                validate_release(tag, version, previous)

    def test_invalid_versions(self):
        for value in ("0.1", "00.0.1", "1.0.0-beta", "-1.0.0", None):
            with self.assertRaises(ValueError):
                version_tuple(value)


if __name__ == "__main__":
    unittest.main()
