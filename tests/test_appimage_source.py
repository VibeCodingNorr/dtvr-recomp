# SPDX-License-Identifier: GPL-3.0-only
"""Packaging exports one pinned commit even if another agent advances HEAD."""
import io
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from build_appimage import export_source


class SourceExportTests(unittest.TestCase):
    def archive(self, name="VERSION", mode=0o644):
        buffer = io.BytesIO()
        with tarfile.open(fileobj=buffer, mode="w") as archive:
            entry = tarfile.TarInfo(name)
            content = b"0.0.1\n"
            entry.size = len(content)
            entry.mode = mode
            archive.addfile(entry, io.BytesIO(content))
        return buffer.getvalue()

    def test_export_and_metadata_share_the_pinned_commit(self):
        revision = "a" * 40
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "source"
            with patch("build_appimage.subprocess.check_output",
                       side_effect=[revision + "\n", self.archive()]) as git:
                self.assertEqual(export_source(destination), revision)
            self.assertEqual(git.call_args_list[1].args[0][-1], revision)
            self.assertEqual((destination / "VERSION").read_text(), "0.0.1\n")

    def test_export_rejects_media_and_escaping_paths(self):
        for name in ("disc.iso", "../escape", "/absolute"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temporary:
                with patch("build_appimage.subprocess.check_output",
                           side_effect=["a" * 40 + "\n", self.archive(name)]):
                    with self.assertRaises(ValueError):
                        export_source(Path(temporary) / "source")


if __name__ == "__main__":
    unittest.main()
