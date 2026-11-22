# SPDX-License-Identifier: GPL-3.0-only
"""Visual language shared by the installer and its supporting dialogs."""

import base64
from pathlib import Path
import shutil
import subprocess
import tkinter as tk
from tkinter import font, ttk

NAVY = "#101c2e"
BODY = "#f8fafc"
INK = "#17283d"
MUTED = "#64748b"
MINT = "#6ce5c2"
BORDER = "#e0e7ed"
WHITE = "#ffffff"


def configure(root):
    families = set(font.families(root))
    family = next((name for name in ("DejaVu Sans", "Liberation Sans", "Noto Sans", "Helvetica")
                   if name in families), font.nametofont("TkDefaultFont").actual("family"))
    for name in ("TkDefaultFont", "TkTextFont", "TkMenuFont", "TkHeadingFont"):
        font.nametofont(name).configure(family=family, size=10)
    style = ttk.Style(root)
    style.theme_use("clam")
    style.configure(".", font=(family, 10), background=BODY, foreground=INK)
    style.configure("TFrame", background=BODY)
    style.configure("TLabel", background=BODY)
    style.configure("TButton", background=WHITE, foreground=INK, bordercolor=BORDER,
                    lightcolor=WHITE, darkcolor=WHITE, padding=(16, 10), focuscolor=WHITE)
    style.map("TButton", background=[("active", "#eff5f6")], foreground=[("disabled", "#94a3b8")])
    style.configure("Primary.TButton", background=MINT, bordercolor=MINT, focuscolor=MINT,
                    font=(family, 11, "bold"), padding=(24, 12))
    style.map("Primary.TButton", background=[("disabled", "#e1e8e8"), ("active", "#8cedd0")],
              bordercolor=[("disabled", "#e1e8e8")])
    style.configure("Quiet.TButton", background=BODY, bordercolor=BODY, foreground=MUTED,
                    lightcolor=BODY, darkcolor=BODY, padding=(8, 8))
    style.configure("Card.TFrame", background=WHITE)
    style.configure("Card.TLabel", background=WHITE, foreground=MUTED)
    style.configure("CardTitle.TLabel", background=WHITE, foreground="#718096",
                    font=(family, 9, "bold"))
    style.configure("Heading.TLabel", font=(family, 25, "bold"), foreground=INK)
    style.configure("Eyebrow.TLabel", font=(family, 9, "bold"), foreground="#698176")
    style.configure("Subtitle.TLabel", foreground=MUTED, font=(family, 11))
    style.configure("TEntry", padding=(10, 9), fieldbackground="#f6f9fa",
                    foreground=INK, bordercolor=BORDER, lightcolor=BORDER, darkcolor=BORDER)
    style.map("TEntry", bordercolor=[("focus", "#79cdb3")])
    style.configure("TCombobox", padding=(9, 7), fieldbackground="#f6f9fa", bordercolor=BORDER)
    style.map("TCombobox", fieldbackground=[("readonly", "#f6f9fa")])
    style.layout("Segment.TRadiobutton", style.layout("Toolbutton"))
    style.configure("Segment.TRadiobutton", background="#e9eef2", foreground=MUTED,
                    padding=(20, 9), borderwidth=0, font=(family, 10))
    style.map("Segment.TRadiobutton", background=[("selected", WHITE), ("active", "#f0f4f6")],
              foreground=[("selected", INK)])
    style.configure("Horizontal.TProgressbar", background=MINT, troughcolor="#e9eef2",
                    bordercolor="#e9eef2", lightcolor=MINT, darkcolor=MINT)
    style.configure("Vertical.TScrollbar", background="#dde5eb", troughcolor=BODY, borderwidth=0)
    return family


def branding_image(root, repository, name, width, height):
    # Packaging exports our SVGs; no converter is required on the installed host.
    png = Path(repository) / "assets/branding" / f"installer-{name}.png"
    if png.is_file():
        return tk.PhotoImage(master=root, file=str(png))
    renderer = shutil.which("rsvg-convert")
    if renderer:
        try:
            result = subprocess.run([renderer, "-w", str(width), "-h", str(height),
                                     str(Path(repository) / "assets/branding" / f"{name}.svg")],
                                    capture_output=True, check=True, timeout=5)
            return tk.PhotoImage(master=root, data=base64.b64encode(result.stdout))
        except (OSError, subprocess.SubprocessError, tk.TclError):
            pass
    return None


class Card(tk.Canvas):
    """Rounded white card whose native controls resize with the window."""
    def __init__(self, parent, padding=18):
        super().__init__(parent, bg=BODY, highlightthickness=0, height=100, bd=0)
        self.padding = padding
        self.content = tk.Frame(self, bg=WHITE)
        self.item = self.create_window(padding, padding, window=self.content, anchor="nw")
        self.content.bind("<Configure>", self._fit)
        self.bind("<Configure>", self._resize)

    def _fit(self, event=None):
        self.configure(height=self.content.winfo_reqheight() + 2 * self.padding)

    def _resize(self, event):
        self.itemconfigure(self.item, width=max(1, event.width - 2 * self.padding))
        def wrap(parent):
            for child in parent.winfo_children():
                if isinstance(child, ttk.Label) and int(child.cget("wraplength") or 0) > 0:
                    child.configure(wraplength=max(180, event.width - 2*self.padding - 8))
                wrap(child)
        wrap(self.content)
        w, h, r = event.width - 1, event.height - 1, 13
        self.delete("surface")
        self.create_polygon(r, 0, w-r, 0, w, 0, w, r, w, h-r, w, h, w-r, h,
                            r, h, 0, h, 0, h-r, 0, r, 0, 0,
                            smooth=True, fill=WHITE, outline=BORDER, tags="surface")
        self.tag_lower("surface")
