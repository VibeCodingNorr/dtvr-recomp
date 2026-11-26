# SPDX-License-Identifier: GPL-3.0-only
"""Whole-pipeline faults with real temporary files, processes and synthetic ISO."""

import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import unittest

from unittest.mock import patch

from installer_scenario import disc_fixture, synthetic_profiles
from test_installer_update import installation

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from installer_checks import digest
from installer_process import InstallerProcess
from installation_manifest import verify_installation

SCENARIO = Path(__file__).with_name("installer_scenario.py")


def digest_bytes(data):
    return hashlib.sha256(data).hexdigest()


class PipelineFailureTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="dtvr-installer-faults-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.iso = self.root / "original disc ü.iso"
        self.iso.write_bytes(disc_fixture())
        self.iso_hash = digest(self.iso)
        profiles = patch("editions.PROFILES", synthetic_profiles(digest_bytes(b"synthetic executable")))
        profiles.start()
        self.addCleanup(profiles.stop)
        self.output = self.root / "game with spaces $(literal) ü"
        self.config = self.root / "config/settings.ini"
        self.config.parent.mkdir()
        self.config.write_text("[Setup]\nWave=0\n")
        tools = self.root / "fake tools"
        tools.mkdir()
        for name in ("cmake", "ctest"):
            tool = tools / name
            tool.write_text("#!" + sys.executable + "\n" + SCENARIO.read_text())
            tool.chmod(0o755)
        self.env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ.get("PATH", ""),
                        PYTHONPATH=str(SCENARIO.parent), XDG_CONFIG_HOME=str(self.config.parent),
                        DTVR_TEST_CONFIG=str(self.config.parent))

    def old_installation(self):
        installation(self.output)
        (self.output / "saves").mkdir()
        (self.output / "saves/slot0.sav").write_bytes(b"saved progress")
        (self.output / "notes.txt").write_text("user notes")
        return self.snapshot()

    def snapshot(self):
        return {p.relative_to(self.output).as_posix(): digest(p)
                for p in self.output.rglob("*") if p.is_file()}

    def command(self, mode, update=False):
        return [sys.executable, "-u", str(SCENARIO), str(self.iso), str(self.output),
                mode, "update" if update else "new"]

    def assert_inputs_intact(self):
        self.assertEqual(digest(self.iso), self.iso_hash)
        self.assertEqual(self.config.read_text(), "[Setup]\nWave=0\n")
        self.assertEqual(list(self.root.glob(".dtvr-install-*")), [])

    def test_failures_never_publish_or_damage_previous_installation(self):
        for update in (False, True):
            previous = self.old_installation() if update else None
            for mode in ("import-space", "build-fail", "install-fail", "smoke-fail", "publish-fail"):
                with self.subTest(update=update, fault=mode):
                    result = subprocess.run(self.command(mode, update),
                                            env=dict(self.env, DTVR_TEST_MODE=mode),
                                            capture_output=True, text=True, timeout=15)
                    self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                    self.assertIn("Installation failed:", result.stderr)
                    self.assertNotIn("Traceback", result.stderr)
                    self.assertEqual(self.snapshot(), previous or {})
                    self.assertEqual(self.output.exists(), update)
                    self.assertEqual(list(self.root.glob("*.backup-*")), [])
                    self.assert_inputs_intact()

    def test_cancel_import_and_compile_also_stops_child_processes(self):
        previous = self.old_installation()
        for mode in ("import-cancel", "build-cancel"):
            with self.subTest(stage=mode):
                worker = InstallerProcess()
                old_mode = os.environ.copy()
                os.environ.update(self.env, DTVR_TEST_MODE=mode)
                child = None
                try:
                    worker.start(self.command(mode, update=True))
                    while True:
                        kind, line = worker.events.get(timeout=15)
                        self.assertNotEqual(kind, "done", line)
                        if kind == "log" and line.startswith("TEST_CHILD:"):
                            child = int(line.split(":")[1])
                        if kind == "log" and line.startswith("TEST_READY:"):
                            break
                    worker.cancel()
                    while True:
                        kind, result = worker.events.get(timeout=15)
                        if kind == "done":
                            break
                    worker.thread.join(timeout=3)
                    self.assertTrue(result["cancelled"], result)
                    self.assertFalse(result["ok"])
                    self.assertFalse(worker.running)
                finally:
                    if worker.running:
                        worker.cancel()
                        worker.thread.join(timeout=3)
                    os.environ.clear()
                    os.environ.update(old_mode)
                if child:
                    status = Path(f"/proc/{child}/stat")
                    if status.exists():
                        self.assertEqual(status.read_text().split()[2], "Z", "compiler descendant still running")
                self.assertEqual(self.snapshot(), previous)
                self.assert_inputs_intact()

    def test_success_and_launcher_preserve_literal_paths(self):
        result = subprocess.run(self.command("success"), env=dict(self.env, DTVR_TEST_MODE="success"),
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(verify_installation(self.output)["files_verified"], 6)
        launched = subprocess.run([str(self.output / "play")], env=dict(self.env, DTVR_TEST_MODE="success"),
                                  capture_output=True, text=True, check=True)
        args = json.loads(launched.stdout.split("TEST_RUNTIME:")[1])
        self.assertEqual(args, ["--data", str(self.output / "data"), "--game"])
        self.assert_inputs_intact()

    def test_real_cli_rejects_invalid_media_without_writes(self):
        installer = SCENARIO.parents[1] / "tools/install.py"
        for data in (b"not a disc", disc_fixture()[:-2048]):
            with self.subTest(size=len(data)):
                self.iso.write_bytes(data)
                before = digest(self.iso)
                result = subprocess.run([sys.executable, str(installer), "--iso", str(self.iso),
                                         "--output", str(self.output), "--check", "--no-music"],
                                        capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertIn("Installation failed:", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(digest(self.iso), before)
                self.assertFalse(self.output.exists())
                self.assertEqual(list(self.root.glob(".dtvr-install-*")), [])


if __name__ == "__main__":
    unittest.main()
