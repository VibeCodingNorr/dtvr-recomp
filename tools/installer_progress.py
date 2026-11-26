# SPDX-License-Identifier: GPL-3.0-only
"""Measured subtask progress and conservative, run-local remaining-time estimates."""

import re
import time

from soundtrack import CUES


def progress_event(line, stage):
    """Use existing tool output; only count work finished before the next item."""
    if stage == 3:
        match = re.match(r"Soundtrack ([a-z0-9-]+): (?:rendering )?([a-z0-9]+)(?:\s|$)", line)
        if match and match[2] in CUES:
            return {"group": "midi-" + match[1], "completed": CUES.index(match[2]),
                    "total": len(CUES),
                    "label": f"Musikstück {CUES.index(match[2]) + 1}/{len(CUES)}: {match[2]}"}
        match = re.match(r"Converting CD audio: (.+)", line)
        if match:
            return {"group": "cd", "completed": 0, "total": 0,
                    "label": f"CD-Aufnahme konvertieren: {match[1].strip()}"}
    if stage == 4:
        match = re.match(r"\[\s*(\d+)%\]\s*(.*)", line)
        if match and 0 <= int(match[1]) <= 100:
            return {"group": "build", "completed": int(match[1]), "total": 100,
                    "label": "Kompilieren: " + match[1] + " %"}
    if stage == 5:
        match = re.match(r"\s*(\d+)/(\d+)\s+Test\s+#", line)
        if match:
            return {"group": "tests", "completed": int(match[1]), "total": int(match[2]),
                    "label": f"Tests: {match[1]}/{match[2]}"}
    return None


def duration(seconds):
    seconds = max(0, int(seconds))
    minutes, seconds = divmod(seconds, 60)
    hours, minutes = divmod(minutes, 60)
    return f"{hours}:{minutes:02}:{seconds:02}" if hours else f"{minutes}:{seconds:02}"


class ProgressEstimate:
    def __init__(self, clock=time.monotonic):
        self.clock = clock
        self.reset()

    def reset(self):
        self.started = self.clock()
        self.group = None
        self.completed = self.total = 0
        self.baseline = 0
        self.group_started = self.started
        self.label = "Vorbereitung läuft …"

    def stage(self, label):
        self.group = None
        self.completed = self.total = 0
        self.label = label + " …"

    def update(self, event):
        now = self.clock()
        if event["group"] != self.group:
            self.group = event["group"]
            self.group_started = now
            self.baseline = event["completed"]
            self.completed = event["completed"]
        # Parallel compiler output can arrive out of order.
        if event["completed"] < self.completed:
            return
        self.completed = max(self.completed, event["completed"])
        self.total = event["total"]
        self.label = event["label"]

    def remaining(self):
        now = self.clock()
        units = self.completed - self.baseline
        elapsed = now - self.group_started
        if not self.total or units < 2 or elapsed < 2:
            return "Restzeit der Teilaufgabe: wird geschätzt …"
        if self.completed >= self.total:
            return "Teilaufgabe wird abgeschlossen …"
        estimate = elapsed / units * (self.total - self.completed)
        return "Restzeit der Teilaufgabe: ca. " + duration(max(1, estimate)) + " (grobe Schätzung)"

    def elapsed(self):
        return "Verstrichen: " + duration(self.clock() - self.started)
