# SPDX-License-Identifier: GPL-3.0-only
"""Background installer process and event queue; independent of Tk widgets."""

from collections import deque
import json
import os
from pathlib import Path
import queue
import re
import signal
import subprocess
import sys
import threading

REPO = Path(__file__).resolve().parents[1]


def installer_command(iso, output, soundfont="", style="classic", check=False, update=False):
    if not str(iso).strip() or not str(output).strip():
        raise ValueError("Bitte Original-ISO und Zielordner auswählen.")
    command = [sys.executable, "-u", str(REPO / "tools/install.py"),
               "--iso", str(iso), "--output", str(output)]
    if soundfont:
        command += ["--soundfont", str(soundfont), "--music-style", style,
                    "--music-pack-id", "rendered-gm", "--music-pack-name", "SoundFont Ogg"]
    if check:
        command.append("--check")
    if update:
        command.append("--update")
    return command


class InstallerProcess:
    def __init__(self):
        self.events = queue.Queue()
        self.thread = None
        self.process = None
        self.cancel_requested = threading.Event()
        self.lock = threading.Lock()

    @property
    def running(self):
        return self.thread is not None and self.thread.is_alive()

    def start(self, command, check=False):
        if self.running:
            raise ValueError("Ein Installationsprozess läuft bereits.")
        self.cancel_requested.clear()
        self.thread = threading.Thread(target=self._work, args=(command, check), daemon=True)
        self.thread.start()

    def cancel(self):
        self.cancel_requested.set()
        with self.lock:
            process = self.process
        if process is not None:
            try:
                os.killpg(process.pid, signal.SIGINT)
            except ProcessLookupError:
                pass

    def _work(self, command, check):
        tail = deque(maxlen=30)
        checked_output = []
        try:
            with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  text=True, encoding="utf-8", errors="replace", bufsize=1,
                                  start_new_session=True) as process:
                with self.lock:
                    self.process = process
                if self.cancel_requested.is_set():
                    self.cancel()
                for line in process.stdout:
                    tail.append(line)
                    if check:
                        checked_output.append(line)
                    else:
                        match = re.match(r"DTVR_STAGE:([1-7]):", line)
                        if match:
                            self.events.put(("stage", int(match.group(1))))
                        self.events.put(("log", line))
                code = process.wait()
            result = {"ok": code == 0, "cancelled": self.cancel_requested.is_set() and code != 0,
                      "check": check, "error": "".join(tail)[-6000:]}
            if check and code == 0:
                result["report"] = json.loads("".join(checked_output))
        except (OSError, ValueError) as error:
            result = {"ok": False, "cancelled": self.cancel_requested.is_set(),
                      "check": check, "error": str(error)}
        finally:
            with self.lock:
                self.process = None
        self.events.put(("done", result))
