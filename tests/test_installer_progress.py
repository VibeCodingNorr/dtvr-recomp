# SPDX-License-Identifier: GPL-3.0-only
"""Progress follows real work; waiting must not manufacture completion."""

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from installer_progress import ProgressEstimate, duration, progress_event
from soundtrack import CUES


class ProgressTests(unittest.TestCase):
    def setUp(self):
        self.now = 0
        self.estimate = ProgressEstimate(lambda: self.now)

    def test_audio_counts_preceding_titles_not_title_being_rendered(self):
        for index, cue in enumerate(CUES):
            for suffix in (f"rendering {cue}", f"{cue} (identical source reused)"):
                event = progress_event(f"Soundtrack rendered-gm: {suffix}\n", 3)
                self.assertEqual(event["completed"], index)
                self.assertEqual(event["total"], len(CUES))
        self.assertIsNone(progress_event("Soundtrack rendered-gm: rendering unknown", 3))
        self.assertIsNone(progress_event("Soundtrack rendered-gm: rendering a1", 4))
        event = progress_event("Converting CD audio: segment-01.ogg\n", 3)
        self.assertEqual(event["total"], 0)
        self.assertIn("segment-01.ogg", event["label"])

    def test_estimate_warms_up_and_counts_elapsed_without_advancing_work(self):
        self.estimate.update(progress_event("Soundtrack rendered-gm: rendering a1", 3))
        self.assertIn("wird geschätzt", self.estimate.remaining())
        self.now = 20
        self.estimate.update(progress_event("Soundtrack rendered-gm: rendering e2", 3))
        self.assertIn(f"ca. {duration(10 * (len(CUES) - 2))}", self.estimate.remaining())
        self.now = 120
        self.assertEqual(self.estimate.completed, 2)
        self.assertIn("2:00", self.estimate.elapsed())
        self.assertIn("grobe Schätzung", self.estimate.remaining())

    def test_stage_and_new_run_reset_estimates(self):
        self.estimate.update(progress_event("[ 10%] Building CXX object", 4))
        self.now = 10
        self.estimate.update(progress_event("[ 20%] Building CXX object", 4))
        self.assertIn("ca.", self.estimate.remaining())
        self.estimate.stage("Audio prüfen")
        self.assertEqual(self.estimate.completed, 0)
        self.assertIn("wird geschätzt", self.estimate.remaining())
        self.now = 30
        self.estimate.reset()
        self.assertEqual(self.estimate.elapsed(), "Verstrichen: 0:00")

    def test_parallel_build_does_not_move_backwards_or_finish_installation(self):
        for percentage in (0, 50, 49):
            self.estimate.update(progress_event(f"[{percentage:3}%] Building", 4))
        self.assertEqual(self.estimate.completed, 50)
        self.assertEqual(self.estimate.label, "Kompilieren: 50 %")
        self.now = 20
        self.estimate.update(progress_event("[100%] Built target", 4))
        self.assertIn("abgeschlossen", self.estimate.remaining())
        self.assertIsNone(progress_event("[101%] invalid", 4))
        self.assertEqual(progress_event("  3/12 Test #3: rules ... Passed", 5)["completed"], 3)
        self.assertEqual(duration(3661), "1:01:01")


if __name__ == "__main__":
    unittest.main()
