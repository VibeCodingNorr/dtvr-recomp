# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import json
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from installation_manifest import verify_installation, write_manifest


class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for name in ["play", "bin/dtvr-recomp", "data/program/butt/1/map.bmp"]:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"synthetic test file")
            if name in ("play", "bin/dtvr-recomp"):
                path.chmod(0o755)
        self.manifest = write_manifest(self.root, {})

    def test_every_runtime_file_including_launcher_is_verified(self):
        self.assertEqual(verify_installation(self.root)["files_verified"], 3)
        self.assertIn("play", self.manifest["files"])
        (self.root / "play").write_text("changed launcher")
        with self.assertRaisesRegex(ValueError, "hash mismatch: play"):
            verify_installation(self.root)

    def test_missing_file_and_symlink(self):
        path = self.root / "play"
        path.unlink()
        with self.assertRaises(ValueError):
            verify_installation(self.root)
        path.symlink_to(self.root / "bin/dtvr-recomp")
        with self.assertRaises(ValueError):
            verify_installation(self.root)
        with self.assertRaises(ValueError):
            write_manifest(self.root, {})

    def test_execute_permission(self):
        (self.root / "play").chmod(0o644)
        with self.assertRaisesRegex(ValueError, "not executable: play"):
            verify_installation(self.root)

    def test_untrusted_manifest_paths_and_legacy(self):
        self.manifest["files"]["../escape"] = "0" * 64
        path = self.root / "manifest.json"
        path.write_text(json.dumps(self.manifest))
        with self.assertRaisesRegex(ValueError, "Unsafe manifest path"):
            verify_installation(self.root)
        path.write_text(json.dumps({"schema_version": 1}))
        with self.assertRaisesRegex(ValueError, "legacy"):
            verify_installation(self.root)

    def test_installation_of_the_former_name_stays_verifiable(self):
        # DTVR Recomb installations (bin/dtvr-recomb) must remain updatable.
        new = self.root / "bin/dtvr-recomp"
        old = self.root / "bin/dtvr-recomb"
        new.rename(old)
        files = {name.replace("dtvr-recomp", "dtvr-recomb"): value for name, value in self.manifest["files"].items()}
        legacy = {**self.manifest, "format": "dtvr-recomb-installation", "files": files}
        (self.root / "manifest.json").write_text(json.dumps(legacy))
        self.assertEqual(verify_installation(self.root)["files_verified"], 3)
        old.chmod(0o644)
        with self.assertRaisesRegex(ValueError, "not executable: bin/dtvr-recomb"):
            verify_installation(self.root)
        old.chmod(0o755)
        # The new format requires the new binary name.
        (self.root / "manifest.json").write_text(json.dumps({**legacy, "format": "dtvr-recomp-installation"}))
        with self.assertRaisesRegex(ValueError, "missing required runtime"):
            verify_installation(self.root)


if __name__ == "__main__":
    unittest.main()
