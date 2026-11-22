# SPDX-License-Identifier: GPL-3.0-only
import json
import queue
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from installer_process import InstallerProcess, installer_command


class ProcessTests(unittest.TestCase):
    def wait_done(self, worker):
        collected = []
        while True:
            event = worker.events.get(timeout=10)
            collected.append(event)
            if event[0] == "done":
                worker.thread.join(timeout=3)
                self.assertFalse(worker.running)
                return collected

    def test_arguments_are_passed_without_shell(self):
        iso = "/path/to/disc with $(literal).iso"
        command = installer_command(iso, "/path/to/new installation", "bank.sf2", "dry", True)
        self.assertIn(iso, command)
        self.assertIn("/path/to/new installation", command)
        self.assertIn("--check", command)
        self.assertIn("dry", command)
        with self.assertRaises(ValueError):
            installer_command("", "target")

    def test_stage_events_and_failure(self):
        worker = InstallerProcess()
        worker.start([sys.executable, "-c", "print('DTVR_STAGE:4:build', flush=True); raise SystemExit(3)"])
        events = self.wait_done(worker)
        self.assertIn(("stage", 4), events)
        self.assertFalse(events[-1][1]["ok"])
        self.assertIn("build", events[-1][1]["error"])

    def test_preflight_json(self):
        worker = InstallerProcess()
        worker.start([sys.executable, "-c", "print('{\"program_files\": 4}')"], check=True)
        result = self.wait_done(worker)[-1][1]
        self.assertTrue(result["ok"])
        self.assertEqual(result["report"]["program_files"], 4)

    def test_cancellation(self):
        worker = InstallerProcess()
        worker.start([sys.executable, "-u", "-c",
                      "import time\nprint('ready', flush=True)\ntry:\n while True: time.sleep(.1)\nexcept KeyboardInterrupt: raise SystemExit(130)"])
        self.assertEqual(worker.events.get(timeout=10), ("log", "ready\n"))
        worker.cancel()
        result = self.wait_done(worker)[-1][1]
        self.assertTrue(result["cancelled"])
        self.assertFalse(result["ok"])


if __name__ == "__main__":
    unittest.main()
