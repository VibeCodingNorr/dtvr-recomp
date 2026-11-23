# SPDX-License-Identifier: GPL-3.0-only
"""Visual language shared by the installer and its supporting dialogs."""

import base64
from pathlib import Path
import shutil
import subprocess
import tkinter as tk
from tkinter import font, ttk

NAVY = "#101c2e"
BODY = "#f7f1e4"
INK = "#183a39"
MUTED = "#69756c"
MINT = "#72ce9e"
AMBER = "#f3b64c"
BORDER = "#e2dbcc"
WHITE = "#fffdf7"


def configure(root):
    families = {name.casefold(): name for name in font.families(root)}
    family = next((families[name.casefold()] for name in ("DejaVu Sans", "Liberation Sans", "Noto Sans", "Helvetica")
                   if name.casefold() in families), font.nametofont("TkDefaultFont").actual("family"))
    for name in ("TkDefaultFont", "TkTextFont", "TkMenuFont", "TkHeadingFont"):
        font.nametofont(name).configure(family=family, size=10)
    style = ttk.Style(root)
    style.theme_use("clam")
    style.configure(".", font=(family, 10), background=BODY, foreground=INK)
    style.configure("TFrame", background=BODY)
    style.configure("TLabel", background=BODY)
    style.configure("TButton", background=WHITE, foreground=INK, bordercolor=BORDER,
                    lightcolor=WHITE, darkcolor=WHITE, padding=(16, 10), focuscolor=WHITE)
    style.map("TButton", background=[("active", "#eee8da")], foreground=[("disabled", "#9a9e94")])
    style.configure("Primary.TButton", background=MINT, bordercolor=MINT, focuscolor=MINT,
                    font=(family, 11, "bold"), padding=(24, 12))
    style.map("Primary.TButton", background=[("disabled", "#dde4d9"), ("active", "#91dfb5")],
              bordercolor=[("disabled", "#dde4d9")])
    style.configure("Quiet.TButton", background=BODY, bordercolor=BODY, foreground=MUTED,
                    lightcolor=BODY, darkcolor=BODY, padding=(8, 8))
    style.configure("Card.TFrame", background=WHITE)
    style.configure("Card.TLabel", background=WHITE, foreground=MUTED)
    style.configure("CardTitle.TLabel", background=WHITE, foreground="#596f5d",
                    font=(family, 9, "bold"))
    style.configure("CardHeading.TLabel", background=WHITE, foreground=INK,
                    font=(family, 17, "bold"))
    style.configure("Heading.TLabel", font=(family, 25, "bold"), foreground=INK)
    style.configure("Eyebrow.TLabel", font=(family, 9, "bold"), foreground="#54755d")
    style.configure("Subtitle.TLabel", foreground=MUTED, font=(family, 11))
    style.configure("TEntry", padding=(10, 9), fieldbackground="#fbf8f0",
                    foreground=INK, bordercolor=BORDER, lightcolor=BORDER, darkcolor=BORDER)
    style.map("TEntry", bordercolor=[("focus", "#79cdb3")])
    style.configure("TCombobox", padding=(9, 7), fieldbackground="#fbf8f0", bordercolor=BORDER)
    style.map("TCombobox", fieldbackground=[("readonly", "#fbf8f0")])
    style.layout("Segment.TRadiobutton", style.layout("Toolbutton"))
    style.configure("Segment.TRadiobutton", background="#eee7d8", foreground=MUTED,
                    padding=(20, 9), borderwidth=0, font=(family, 10))
    style.map("Segment.TRadiobutton", background=[("selected", "#dcecdf"), ("active", "#e8e1d2")],
              foreground=[("selected", INK)])
    style.configure("Horizontal.TProgressbar", background="#59af7f", troughcolor="#eee7d8",
                    bordercolor="#eee7d8", lightcolor="#59af7f", darkcolor="#59af7f", thickness=10)
    style.configure("Vertical.TScrollbar", background="#dcd6c7", troughcolor=BODY, borderwidth=0)
    return family


def branding_image(root, repository, name, width, height):
    # Packaging exports our SVGs; no converter is required on the installed host.
    png = Path(repository) / "assets/branding" / f"installer-{name}.png"
    if png.is_file():
        return tk.PhotoImage(master=root, file=str(png))
    renderer = shutil.which("rsvg-convert")
    if renderer:
        try:
            source = Path(repository) / "assets/branding" / f"{name}-dark.svg"
            if not source.is_file():
                source = Path(repository) / "assets/branding" / f"{name}.svg"
            result = subprocess.run([renderer, "-w", str(width), "-h", str(height),
                                     str(source)],
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
        w, h, r = event.width - 1, event.height - 1, 18
        self.delete("surface")
        self.create_polygon(r, 0, w-r, 0, w, 0, w, r, w, h-r, w, h, w-r, h,
                            r, h, 0, h, 0, h-r, 0, r, 0, 0,
                            smooth=True, fill=WHITE, outline=BORDER, tags="surface")
        self.tag_lower("surface")
