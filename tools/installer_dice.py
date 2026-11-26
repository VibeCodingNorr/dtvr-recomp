# SPDX-License-Identifier: GPL-3.0-only
"""Original, code-drawn rolling die for installer activity (no game assets)."""

import math
import tkinter as tk

from installer_palette import WHITE, INK, MINT

PIPS = {
    1: ((0, 0),),
    2: ((-1, -1), (1, 1)),
    3: ((-1, -1), (0, 0), (1, 1)),
    4: ((-1, -1), (1, -1), (-1, 1), (1, 1)),
    5: ((-1, -1), (1, -1), (0, 0), (-1, 1), (1, 1)),
    6: ((-1, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (1, 1)),
}


class RollingDie(tk.Canvas):
    def __init__(self, parent):
        super().__init__(parent, width=72, height=64, bg=WHITE, highlightthickness=0, bd=0)
        self.running = False
        self.timer = None
        self.frame = 0
        self.face = 6
        self._draw()
        self.bind("<Destroy>", self._destroyed, add="+")

    def start(self):
        if not self.running:
            self.running = True
            self.frame = 0
            self._tick()

    def stop(self):
        self.running = False
        if self.timer is not None:
            self.after_cancel(self.timer)
            self.timer = None
        self._draw()

    def _destroyed(self, event):
        if event.widget == self:
            self.running = False
            if self.timer is not None:
                self.after_cancel(self.timer)
                self.timer = None

    def _tick(self):
        self.timer = None
        if not self.running:
            return
        self.frame += 1
        self.face = (1, 3, 5, 2, 6, 4)[(self.frame // 6) % 6]
        self._draw()
        self.timer = self.after(80, self._tick)

    def _draw(self):
        self.delete("all")
        angle = math.sin(self.frame * .28) * .32 if self.running else 0
        lift = abs(math.sin(self.frame * .28)) * 5 if self.running else 0
        cosine, sine = math.cos(angle), math.sin(angle)

        def point(x, y):
            return (36 + x * cosine - y * sine, 31 - lift + x * sine + y * cosine)

        self.create_oval(12, 51, 60, 59, fill="#e4dfd2", outline="")
        corners = [coordinate for x, y in ((-21, -21), (21, -21), (21, 21), (-21, 21))
                   for coordinate in point(x, y)]
        self.create_polygon(corners, fill=WHITE, outline=MINT, width=3, joinstyle="round")
        for x, y in PIPS[self.face]:
            px, py = point(x * 11, y * 11)
            self.create_oval(px - 3.2, py - 3.2, px + 3.2, py + 3.2, fill=INK, outline="")
