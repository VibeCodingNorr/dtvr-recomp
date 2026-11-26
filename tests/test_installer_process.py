# SPDX-License-Identifier: GPL-3.0-only
import json
import queue
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from installer_process import InstallerProcess, installer_command, dependency_command


class ProcessTests(unittest.TestCase):
    def test_no_bank_is_an_explicit_music_opt_out(self):
        self.assertIn("--no-music", installer_command("/path/disc.iso", "/path/install"))
        self.assertIn("--no-music", dependency_command())
        self.assertNotIn("--no-music", installer_command("/path/disc.iso", "/path/install", "bank.sf2"))
        self.assertNotIn("--no-music", dependency_command("bank.sf2"))

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
        self.assertIn("--update", installer_command(iso, "/path/to/install", update=True))
        with self.assertRaises(ValueError):
            installer_command("", "target")

    def test_stage_events_and_failure(self):
        worker = InstallerProcess()
        worker.start([sys.executable, "-c", "print('DTVR_STAGE:4:build', flush=True); raise SystemExit(3)"])
        events = self.wait_done(worker)
        self.assertIn(("stage", 4), events)
        self.assertFalse(events[-1][1]["ok"])
        self.assertIn("build", events[-1][1]["error"])

    def test_audio_progress_and_activity_reach_the_ui_queue(self):
        worker = InstallerProcess()
        lines = ["DTVR_STAGE:3:audio", "Converting CD audio: segment-01.ogg",
                 "Soundtrack rendered-gm: rendering a1", "Soundtrack rendered-gm: rendering e2",
                 "DTVR_ACTIVITY:CD-Klangset prüfen und zusammenstellen"]
        worker.start([sys.executable, "-c", "print(" + repr("\n".join(lines)) + ")"])
        events = self.wait_done(worker)
        progress = [value for kind, value in events if kind == "progress"]
        self.assertEqual([entry["completed"] for entry in progress], [0, 0, 2])
        self.assertIn(("activity", "CD-Klangset prüfen und zusammenstellen"), events)
        self.assertTrue(events[-1][1]["ok"])

    def test_preflight_json(self):
        worker = InstallerProcess()
        worker.start([sys.executable, "-c", "print('{\"program_files\": 4}')"], check=True)
        result = self.wait_done(worker)[-1][1]
        self.assertTrue(result["ok"])
        self.assertEqual(result["report"]["program_files"], 4)

    def test_dependency_failure_and_installation_result_are_structured(self):
        worker = InstallerProcess()
        worker.start([sys.executable, "-c",
                      "print('DTVR_DEPENDENCIES:{\"ready\": false}'); raise SystemExit(1)"], check=True)
        result = self.wait_done(worker)[-1][1]
        self.assertFalse(result["ok"])
        self.assertFalse(result["dependencies"]["ready"])
        worker.start([sys.executable, "-c",
                      "print('DTVR_RESULT:{\"project_version\":\"0.0.1\",\"backup\":\"backup path\"}')"])
        result = self.wait_done(worker)[-1][1]
        self.assertTrue(result["ok"])
        self.assertEqual(result["installation"]["backup"], "backup path")

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
