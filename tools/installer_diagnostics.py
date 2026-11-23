# SPDX-License-Identifier: GPL-3.0-only
"""Bounded local diagnostic export with conservative path removal."""

from collections import deque
from datetime import datetime, timezone
from pathlib import Path
import platform
import re

from project_version import read_version

ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
ABSOLUTE = re.compile(r"(?<![\w])/(?!/)[^\s]|[A-Za-z]:[\\/]|\\\\")


def redact(text, private_paths=()):
    text = ANSI.sub("", text)
    # Whole-line removal also covers unquoted paths containing spaces or escapes.
    paths = {str(path) for path in private_paths if str(path) and str(path) != "."}
    paths.add(str(Path.home()))
    for path in sorted(paths, key=len, reverse=True):
        text = text.replace(path, "<LOKALER_PFAD>")
        text = text.replace(path.replace("/", "\\/"), "<LOKALER_PFAD>")
    clean = []
    for line in text.splitlines():
        if ABSOLUTE.search(line) or re.search(r"\\x2[fF]|%2[fF]|\\u002[fF]", line):
            clean.append("[Zeile mit lokalem Pfad ausgeblendet]")
        else:
            clean.append("".join(c for c in line if c == "\t" or ord(c) >= 32))
    return "\n".join(clean)


class DiagnosticLog:
    def __init__(self):
        self.lines = deque(maxlen=1500)

    def append(self, text):
        self.lines.extend(line[:8192] for line in text.splitlines())

    def export(self, destination, private_paths=()):
        header = ("DTVR Recomp Installer-Diagnose\n"
                  f"Version: {read_version()}\nPython: {platform.python_version()}\n"
                  f"Architektur: {platform.machine()}\n"
                  f"Zeit (UTC): {datetime.now(timezone.utc).isoformat()}\n"
                  "Lokale Pfade werden ersetzt; verbleibende Pfadzeilen werden ausgeblendet.\n\n")
        Path(destination).write_text(header + redact("\n".join(self.lines), private_paths) + "\n",
                                     encoding="utf-8")
