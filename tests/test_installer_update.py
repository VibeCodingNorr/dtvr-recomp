# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import json
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from installation_manifest import write_manifest, verify_installation
from installer_update import preserve_files, publish, update_info, publication_lock
from project_version import read_version


def installation(root, content="old", packs=()):
    for name in ("play", "bin/dtvr-recomp", "data/program/butt/1/map.bmp"):
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        path.chmod(0o755)
    return write_manifest(root, {"project_version": read_version(), "soundtrack_packs": list(packs)})


class UpdateTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.old = self.root / "installed game"
        self.stage = self.root / "staging"
        self.previous = installation(self.old)
        installation(self.stage, "new")

    def test_update_retains_user_files_and_complete_backup(self):
        save = self.old / "saves/slot0.sav"
        save.parent.mkdir()
        save.write_text("user progress")
        (self.old / "notes.txt").write_text("keep this too")
        packs, excluded = preserve_files(self.old, self.stage, self.previous, [])
        write_manifest(self.stage, {"project_version": read_version()}, excluded)
        backup = publish(self.stage, self.old, self.previous)
        self.assertEqual((self.old / "play").read_text(), "new")
        self.assertEqual(save.read_text(), "user progress")
        self.assertEqual((self.old / "notes.txt").read_text(), "keep this too")
        self.assertEqual((backup / "play").read_text(), "old")
        self.assertEqual((backup / "saves/slot0.sav").read_text(), "user progress")
        save.write_text("later progress")
        self.assertEqual(verify_installation(self.old)["files_verified"], 3)
        self.assertNotIn("saves/slot0.sav", update_info(self.old)["files"])

    def test_rename_failure_restores_original(self):
        rename = Path.rename

        def fail_stage(path, target):
            if path == self.stage:
                raise OSError("simulated publication error")
            return rename(path, target)

        with patch.object(Path, "rename", fail_stage):
            with self.assertRaisesRegex(OSError, "publication error"):
                publish(self.stage, self.old, self.previous)
        self.assertEqual((self.old / "play").read_text(), "old")
        self.assertEqual(list(self.root.glob("*.backup-*")), [])

    def test_changed_installation_and_downgrade_are_rejected(self):
        (self.old / "play").write_text("modified")
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            publish(self.stage, self.old, self.previous)
        self.assertTrue(self.stage.exists())
        installation(self.old)
        with patch("installer_update.read_version", return_value="0.0.0"):
            with self.assertRaisesRegex(ValueError, "downgrades"):
                update_info(self.old)

    def test_unmanaged_links_and_file_conflicts_are_rejected(self):
        (self.old / "external").symlink_to(self.stage)
        with self.assertRaisesRegex(ValueError, "linked"):
            update_info(self.old)
        (self.old / "external").unlink()
        (self.old / "custom").write_text("user")
        (self.stage / "custom").write_text("new runtime file")
        with self.assertRaisesRegex(ValueError, "conflicts"):
            preserve_files(self.old, self.stage, self.previous, [])

    def test_preserves_existing_ogg_pack_unless_replaced(self):
        pack_file = self.old / "data/music/packs/custom/a1.ogg"
        pack_file.parent.mkdir(parents=True)
        pack_file.write_bytes(b"synthetic audio fixture")
        pack = {"id": "custom", "directory": "data/music/packs/custom", "cue_count": 1}
        previous = installation(self.old, packs=[pack])
        packs, excluded = preserve_files(self.old, self.stage, previous, [])
        self.assertEqual(packs, [pack])
        self.assertEqual((self.stage / "data/music/packs/custom/a1.ogg").read_bytes(), pack_file.read_bytes())
        self.assertNotIn("data/music/packs/custom/a1.ogg", excluded)
        # Re-rendering the same ID keeps only the new pack, even if it has new files.
        packs, excluded = preserve_files(self.old, self.stage, previous, [pack])
        self.assertEqual(packs, [pack])

    def test_installers_cannot_publish_concurrently(self):
        with publication_lock(self.old):
            with self.assertRaisesRegex(ValueError, "Another installer"):
                with publication_lock(self.old):
                    self.fail("lock must reject a second publisher")

    def test_user_files_in_retained_music_packs_remain_mutable(self):
        pack_file = self.old / "data/music/packs/custom/a1.ogg"
        pack_file.parent.mkdir(parents=True)
        pack_file.write_bytes(b"synthetic audio fixture")
        pack = {"id": "custom", "directory": "data/music/packs/custom", "cue_count": 1}
        previous = installation(self.old, packs=[pack])
        notes = pack_file.with_name("notes.txt")
        notes.write_text("personal music notes")

        packs, excluded = preserve_files(self.old, self.stage, previous, [])
        manifest = write_manifest(self.stage, {"project_version": read_version(),
                                             "soundtrack_packs": packs}, excluded)
        publish(self.stage, self.old, previous)

        self.assertEqual(notes.read_text(), "personal music notes")
        self.assertNotIn("data/music/packs/custom/notes.txt", manifest["files"])
        self.assertIn("data/music/packs/custom/a1.ogg", manifest["files"])
        notes.write_text("updated personal music notes")
        updated = update_info(self.old)
        next_stage = self.root / "next staging"
        installation(next_stage, "next")
        preserve_files(self.old, next_stage, updated, [])
        self.assertEqual((next_stage / "data/music/packs/custom/notes.txt").read_text(),
                         "updated personal music notes")

    def test_invalid_pack_metadata_is_rejected_before_building(self):
        manifest = self.old / "manifest.json"
        for catalog in (None, "not a list", [{"id": "../escape"}],
                        [{"id": "same"}, {"id": "same"}]):
            with self.subTest(catalog=catalog):
                manifest.write_text(json.dumps({**self.previous, "soundtrack_packs": catalog}))
                with self.assertRaisesRegex(ValueError, "soundtrack"):
                    update_info(self.old)

    def test_failed_rollback_reports_the_preserved_backup(self):
        rename = Path.rename

        def fail_publication_and_restore(path, target):
            if path == self.stage or path.name == "installation":
                raise PermissionError("simulated filesystem failure")
            return rename(path, target)

        with patch.object(Path, "rename", fail_publication_and_restore):
            with self.assertRaisesRegex(OSError, "backup.*installation"):
                publish(self.stage, self.old, self.previous)
        backups = list(self.root.glob("*.backup-*/installation"))
        self.assertEqual(len(backups), 1)
        self.assertEqual((backups[0] / "play").read_text(), "old")
        self.assertTrue(self.stage.exists())


if __name__ == "__main__":
    unittest.main()
