# SPDX-License-Identifier: GPL-3.0-only
"""Original-folder imports use synthetic files, never an installed game."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import install
from installation_manifest import verify_installation
from installer_checks import preflight
from installer_directory import inspect_directory, import_directory, inventory
from installer_process import installer_command

SCENARIO = Path(__file__).with_name("installer_scenario.py")
EXE_HASH = hashlib.sha256(b"synthetic executable").hexdigest()


class DirectoryTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="dtvr-directory-tests-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "original files ü $(literal)"
        self.program = self.source / "PROGRAM"
        for name, data in {"RALLY.EXE": b"synthetic executable",
                           "BUTT/1/MAP.BMP": b"synthetic bitmap",
                           "MAP/0/PART0000.SBB": b"synthetic tiles",
                           "SBS/D.SBS": b"synthetic sprites"}.items():
            path = self.program / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        self.output = self.root / "installed game"

    def report(self):
        return inspect_directory(self.source, EXE_HASH)

    def test_case_normalization_hashes_and_read_only_source(self):
        before = inventory(self.program)
        report = self.report()
        self.assertEqual(report, inspect_directory(self.program, EXE_HASH))
        relocated = self.root / "another copy"
        shutil.copytree(self.source, relocated)
        self.assertEqual(report["source_sha256"],
                         inspect_directory(relocated, EXE_HASH)["source_sha256"])
        data = self.root / "staging/data"
        self.assertEqual(import_directory(report, data), 4)
        self.assertEqual(inventory(self.program), before)
        self.assertEqual(inventory(data / "program"), before)
        self.assertFalse(self.output.exists())

    def test_rejects_unknown_version_missing_assets_and_case_collisions(self):
        with self.assertRaisesRegex(ValueError, "Unsupported original"):
            inspect_directory(self.source, "0" * 64)
        bitmap = self.program / "BUTT/1/MAP.BMP"
        bitmap.unlink()
        with self.assertRaisesRegex(ValueError, "missing required") as missing:
            self.report()
        self.assertIn("program/butt/1/map.bmp", str(missing.exception))
        self.assertIn("--iso", str(missing.exception))
        bitmap.write_bytes(b"synthetic bitmap")
        (self.program / "rally.exe").write_bytes(b"duplicate")
        with self.assertRaisesRegex(ValueError, "case-colliding"):
            self.report()

    def test_rejects_links_special_files_and_oversized_inputs(self):
        entry = self.program / "extra"
        entry.symlink_to(self.program / "RALLY.EXE")
        with self.assertRaisesRegex(ValueError, "linked or special"):
            self.report()
        entry.unlink()
        os.mkfifo(entry)
        with self.assertRaisesRegex(ValueError, "linked or special"):
            self.report()
        entry.unlink()
        with entry.open("wb") as stream:
            stream.truncate(64 * 1024 * 1024 + 1)
        with self.assertRaisesRegex(ValueError, "size"):
            self.report()
        entry.unlink()
        link = self.root / "linked source"
        link.symlink_to(self.source, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symbolic link"):
            inspect_directory(link, EXE_HASH)

    def test_rejects_excessive_depth_and_unsafe_names(self):
        deep = self.program.joinpath(*[str(i) for i in range(17)])
        deep.mkdir(parents=True)
        with self.assertRaisesRegex(ValueError, "depth"):
            self.report()
        shutil.rmtree(self.program / "0")
        (self.program / "unsafe\\name").write_bytes(b"test")
        with self.assertRaisesRegex(ValueError, "Unsafe"):
            self.report()

    def test_changed_source_before_and_during_copy_is_rejected(self):
        report = self.report()
        bitmap = self.program / "BUTT/1/MAP.BMP"
        bitmap.write_bytes(b"changed bitmap")
        with self.assertRaisesRegex(ValueError, "changed after validation"):
            import_directory(report, self.root / "stage1")
        report = self.report()
        original_copy = shutil.copyfile

        def changed_copy(source, target):
            original_copy(source, target)
            Path(target).write_bytes(b"changed during import")

        with patch("installer_directory.shutil.copyfile", side_effect=changed_copy):
            with self.assertRaisesRegex(ValueError, "changed during import"):
                import_directory(report, self.root / "stage2")

    def test_preflight_is_read_only_and_requires_disjoint_destination(self):
        dependencies = {"ready": True, "requirements": []}
        with patch("installer_checks.KNOWN_EXE", EXE_HASH), \
                patch("installer_checks.dependency_report", return_value=dependencies) as probe:
            report = preflight(None, self.output, directory=self.source)
            self.assertEqual(report["source_kind"], "directory")
            self.assertIsNone(report["iso_sha256"])
            self.assertFalse(report["cd_audio_supported"])
            self.assertFalse(probe.call_args.kwargs["audio"])
            self.assertFalse(self.output.exists())
            for destination in (self.source, self.source / "new", self.root):
                with self.subTest(destination=destination):
                    with self.assertRaisesRegex(ValueError, "overlap"):
                        preflight(None, destination, directory=self.source)
        with self.assertRaisesRegex(ValueError, "exactly one"):
            preflight(self.root / "disc.iso", self.output, directory=self.source)

    def test_directory_command_and_mutually_exclusive_cli(self):
        command = installer_command(self.source, self.output, source_kind="directory", update=True)
        self.assertIn("--directory", command)
        self.assertNotIn("--iso", command)
        self.assertIn(str(self.source), command)
        with patch.object(sys, "argv", ["install.py", "--iso", "disc.iso", "--directory", str(self.source)]):
            with self.assertRaises(SystemExit) as result, patch("sys.stderr"):
                install.main()
        self.assertEqual(result.exception.code, 2)

    def test_pipeline_install_and_update_keep_source_and_user_files(self):
        tools = self.root / "fake tools"
        tools.mkdir()
        for name in ("cmake", "ctest"):
            tool = tools / name
            tool.write_text("#!" + sys.executable + "\n" + SCENARIO.read_text())
            tool.chmod(0o755)
        env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ.get("PATH", ""),
                   PYTHONPATH=str(SCENARIO.parent), DTVR_TEST_MODE="ok")
        before = inventory(self.program)
        for update in (False, True):
            if update:
                (self.output / "saves").mkdir()
                (self.output / "saves/slot0.sav").write_bytes(b"saved progress")
            script = (
                "import sys; from pathlib import Path; import install, installer_checks; "
                f"install.KNOWN_EXE = installer_checks.KNOWN_EXE = {EXE_HASH!r}; "
                f"install.inspect_game = lambda data: {{'executable': {{'sha256': {EXE_HASH!r}}}}}; "
                "installer_checks.dependency_report = lambda *a, **k: {'ready': True, 'requirements': []}; "
                "install.main()")
            command = [sys.executable, "-c", script, "--directory", str(self.source),
                       "--output", str(self.output), "--jobs", "1", "--no-music"]
            if update:
                command.append("--update")
            result = subprocess.run(command, env={**env, "PYTHONPATH": os.pathsep.join(
                                        (str(Path(install.__file__).parent), str(SCENARIO.parent)))},
                                    capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            verify_installation(self.output)
            manifest = json.loads((self.output / "manifest.json").read_text())
            self.assertEqual(manifest["source_files"], before)
            self.assertEqual(manifest["source_kind"], "directory")
            self.assertEqual(manifest["cd_audio"]["status"], "unavailable-from-directory")
            self.assertNotIn(str(self.source), json.dumps(manifest))
            self.assertFalse((self.output / "data/program/rally.exe").exists())
            self.assertEqual(inventory(self.program), before)
        self.assertEqual((self.output / "saves/slot0.sav").read_bytes(), b"saved progress")
        self.assertEqual(len(list(self.root.glob("*.backup-*/installation"))), 1)


if __name__ == "__main__":
    unittest.main()
