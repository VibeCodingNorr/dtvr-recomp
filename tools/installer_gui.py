#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Native Linux installer assistant: select an ISO and compile automatically."""

import base64
import os
from pathlib import Path
import queue
import shutil
import subprocess
import sys

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
except ImportError:
    raise SystemExit("Der grafische Installer benötigt Python mit Tk-Unterstützung (python-tk / python3-tk).")

from installer_checks import REPO, default_output
from installer_process import InstallerProcess, installer_command
from project_version import read_version

STAGES = {1: "ISO und Voraussetzungen prüfen", 2: "Originaldaten importieren",
          3: "Ogg-Audio vorbereiten", 4: "Linux-Build kompilieren",
          5: "Build testen und installieren", 6: "Installation starten und prüfen",
          7: "Dateien prüfen und Installation abschließen"}


class InstallerWindow:
    def __init__(self, root):
        self.root = root
        self.worker = InstallerProcess()
        self.busy = False
        self.close_when_done = False
        self.installed = None
        self.active_output = None
        self.iso = tk.StringVar()
        self.output = tk.StringVar(value=str(default_output()))
        self.update = tk.BooleanVar(value=(default_output() / "manifest.json").is_file())
        self.audio = tk.StringVar(value="Nur CD-Aufnahmen vorbereiten")
        self.bank = tk.StringVar()
        self.style = tk.StringVar(value="Classic — Hall und Chorus")
        self.status = tk.StringVar(value="Original-ISO auswählen, dann prüfen oder installieren.")
        root.title(f"DTVR Recomb {read_version()} — Installer")
        root.geometry("900x780")
        root.minsize(780, 700)
        root.configure(bg="#f3f5f7")
        style = ttk.Style(root)
        style.theme_use("clam")
        style.configure("TFrame", background="#f3f5f7")
        style.configure("TLabel", background="#f3f5f7", font=("TkDefaultFont", 10))
        style.configure("TButton", padding=(12, 7), font=("TkDefaultFont", 10))
        style.configure("Primary.TButton", background="#6ce5c2", foreground="#142338")
        style.map("Primary.TButton", background=[("active", "#92efd4"), ("disabled", "#dce2e8")])
        style.configure("Horizontal.TProgressbar", background="#6ce5c2", troughcolor="#dce2e8")
        style.configure("Title.TLabel", font=("TkDefaultFont", 12, "bold"))
        self.controls = []
        self._header()
        body = ttk.Frame(root, padding=(28, 18))
        body.pack(fill="both", expand=True)
        body.columnconfigure(1, weight=1)
        self._path_row(body, 0, "Original-ISO", self.iso, self._choose_iso)
        self._path_row(body, 1, "Installieren nach", self.output, self._choose_output)
        ttk.Label(body, text="Musik vorbereiten").grid(row=2, column=0, sticky="w", pady=8)
        self.audio_choice = ttk.Combobox(body, textvariable=self.audio, state="readonly",
                                        values=("Nur CD-Aufnahmen vorbereiten", "Vollständiges Ogg-Set aus Soundfont"))
        self.audio_choice.grid(row=2, column=1, columnspan=2, sticky="ew", pady=8)
        self.audio_choice.bind("<<ComboboxSelected>>", lambda _: self._soundfont_state())
        self.bank_entry, self.bank_button = self._path_row(body, 3, "Soundfont (optional)", self.bank, self._choose_bank)
        ttk.Label(body, text="Klangvariante").grid(row=4, column=0, sticky="w", pady=8)
        self.style_choice = ttk.Combobox(body, textvariable=self.style, state="readonly",
                                        values=("Classic — Hall und Chorus", "Dry — ohne Effekte"))
        self.style_choice.grid(row=4, column=1, columnspan=2, sticky="ew", pady=8)
        self.update_choice = ttk.Checkbutton(body, text="Vorhandene Installation aktualisieren (mit Sicherung)", variable=self.update)
        self.update_choice.grid(row=5, column=0, columnspan=3, sticky="w", pady=(8, 4))
        self.controls.append(self.update_choice)
        ttk.Label(body, text="Eigene ISO erforderlich. Spiel vor Updates schließen; Spielstände und Einstellungen bleiben erhalten.",
                  wraplength=820).grid(row=6, column=0, columnspan=3, sticky="w", pady=(0, 14))
        actions = ttk.Frame(body)
        actions.grid(row=7, column=0, columnspan=3, sticky="ew")
        self.check_button = ttk.Button(actions, text="Voraussetzungen prüfen", command=lambda: self._begin(True))
        self.check_button.pack(side="left")
        self.install_button = ttk.Button(actions, text="Installieren und kompilieren", style="Primary.TButton", command=lambda: self._begin(False))
        self.install_button.pack(side="left", padx=10)
        self.cancel_button = ttk.Button(actions, text="Abbrechen", command=self._cancel, state="disabled")
        self.cancel_button.pack(side="right")
        ttk.Label(body, textvariable=self.status, style="Title.TLabel", wraplength=810).grid(
            row=8, column=0, columnspan=3, sticky="w", pady=(18, 8))
        self.progress = ttk.Progressbar(body, maximum=7)
        self.progress.grid(row=9, column=0, columnspan=3, sticky="ew", pady=(0, 10))
        self.log = tk.Text(body, height=9, wrap="word", bg="#101c2e", fg="#dce6ef",
                           insertbackground="white", font=("TkFixedFont", 9), relief="flat", padx=12, pady=10)
        self.log.grid(row=10, column=0, columnspan=3, sticky="nsew")
        self.log.configure(state="disabled")
        body.rowconfigure(10, weight=1)
        footer = ttk.Frame(body)
        footer.grid(row=11, column=0, columnspan=3, sticky="ew", pady=(12, 0))
        self.launch_button = ttk.Button(footer, text="Spiel starten", command=self._launch, state="disabled")
        self.launch_button.pack(side="left")
        self.folder_button = ttk.Button(footer, text="Installationsordner öffnen", command=self._open_folder, state="disabled")
        self.folder_button.pack(side="left", padx=10)
        ttk.Button(footer, text="Schließen", command=self._close).pack(side="right")
        self._soundfont_state()
        root.protocol("WM_DELETE_WINDOW", self._close)
        root.after(100, self._poll)

    def _header(self):
        canvas = tk.Canvas(self.root, height=105, bg="#101c2e", highlightthickness=0)
        canvas.pack(fill="x")
        # Our own road/die mark, drawn with native geometry; no original game assets.
        canvas.create_line(36, 104, 36, 83, 83, 75, 83, 27, fill="#f5edda", width=13, smooth=True)
        canvas.create_line(36, 104, 36, 83, 83, 75, 83, 27, fill="#142338", width=2, dash=(4, 4), smooth=True)
        canvas.create_rectangle(23, 19, 63, 59, fill="#6ce5c2", outline="")
        for x, y in ((33, 29), (53, 29), (43, 39), (33, 49), (53, 49)):
            canvas.create_oval(x-3, y-3, x+3, y+3, fill="#142338", outline="")
        canvas.create_oval(75, 19, 91, 35, fill="#f3b64c", outline="#142338", width=3)
        canvas.create_text(122, 38, anchor="w", text="DTVR RECOMB", fill="#f5edda", font=("TkDefaultFont", 24, "bold"))
        canvas.create_text(124, 73, anchor="w", text="Aus deiner Original-ISO zum nativen Linux-Build", fill="#6ce5c2", font=("TkDefaultFont", 12))
        renderer = shutil.which("rsvg-convert")
        if renderer:
            try:
                banner = subprocess.run([renderer, "-w", "350", "-h", "105", str(REPO / "assets/branding/logo.svg")],
                                        check=True, capture_output=True, timeout=5).stdout
                self.banner = tk.PhotoImage(data=base64.b64encode(banner))
                canvas.delete("all")
                canvas.create_image(0, 0, anchor="nw", image=self.banner)
                canvas.create_text(375, 35, anchor="w", text=f"INSTALLER {read_version()}", fill="#f5edda", font=("TkDefaultFont", 18, "bold"))
                canvas.create_text(375, 70, anchor="w", text="ISO auswählen · automatisch kompilieren", fill="#6ce5c2", font=("TkDefaultFont", 12))
                png = subprocess.run([renderer, "-w", "64", "-h", "64", str(REPO / "assets/branding/icon.svg")],
                                     check=True, capture_output=True, timeout=5).stdout
                self.icon = tk.PhotoImage(data=base64.b64encode(png))
                self.root.iconphoto(True, self.icon)
            except (OSError, subprocess.SubprocessError, tk.TclError):
                pass

    def _path_row(self, parent, row, label, variable, browse):
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", padx=(0, 16), pady=8)
        entry = ttk.Entry(parent, textvariable=variable)
        entry.grid(row=row, column=1, sticky="ew", pady=8)
        button = ttk.Button(parent, text="Auswählen …", command=browse)
        button.grid(row=row, column=2, padx=(10, 0), pady=8)
        self.controls += [entry, button]
        return entry, button

    def _choose_iso(self):
        path = filedialog.askopenfilename(title="Eigene Original-ISO auswählen", filetypes=(("Disc-Abbild", "*.iso *.ISO *.bin"), ("Alle Dateien", "*")))
        if path:
            self.iso.set(path)

    def _choose_output(self):
        parent = filedialog.askdirectory(title="Vorhandene Installation auswählen" if self.update.get() else "Übergeordneten Zielordner auswählen")
        if parent:
            self.output.set(str(Path(parent) if self.update.get() else Path(parent) / "dtvr-recomb"))

    def _choose_bank(self):
        path = filedialog.askopenfilename(title="Eigenen Soundfont auswählen", filetypes=(("SoundFont", "*.sf2 *.sf3"), ("Alle Dateien", "*")))
        if path:
            self.bank.set(path)

    def _soundfont_state(self):
        enabled = not self.busy and self.audio.get().startswith("Vollständiges")
        self.bank_entry.configure(state="normal" if enabled else "disabled")
        self.bank_button.configure(state="normal" if enabled else "disabled")
        self.style_choice.configure(state="readonly" if enabled else "disabled")

    def _set_busy(self, busy):
        self.busy = busy
        for widget in self.controls + [self.check_button, self.install_button]:
            widget.configure(state="disabled" if busy else "normal")
        self.audio_choice.configure(state="disabled" if busy else "readonly")
        self.cancel_button.configure(state="normal" if busy else "disabled")
        self.launch_button.configure(state="normal" if self.installed and not busy else "disabled")
        self.folder_button.configure(state="normal" if self.installed and not busy and shutil.which("xdg-open") else "disabled")
        self._soundfont_state()

    def _begin(self, check):
        if self.busy:
            return
        try:
            bank = self.bank.get().strip() if self.audio.get().startswith("Vollständiges") else ""
            if self.audio.get().startswith("Vollständiges") and not bank:
                raise ValueError("Für ein vollständiges Ogg-Set bitte einen Soundfont auswählen.")
            command = installer_command(self.iso.get().strip(), self.output.get().strip(), bank,
                                        "dry" if self.style.get().startswith("Dry") else "classic", check, self.update.get())
            self.active_output = Path(self.output.get().strip()).expanduser().resolve()
            self.progress.configure(value=0)
            self.status.set("Voraussetzungen werden geprüft …" if check else "Installation wird vorbereitet …")
            self._append("\n--- Vorabprüfung ---\n" if check else "\n--- Installation ---\n")
            self.worker.start(command, check)
            self._set_busy(True)
        except (OSError, ValueError) as error:
            messagebox.showerror("Eingabe prüfen", str(error), parent=self.root)

    def _append(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", text)
        if int(self.log.index("end-1c").split(".")[0]) > 1500:
            self.log.delete("1.0", "500.0")
        self.log.see("end")
        self.log.configure(state="disabled")

    def _poll(self):
        try:
            while True:
                kind, data = self.worker.events.get_nowait()
                if kind == "log":
                    self._append(data)
                elif kind == "stage":
                    self.status.set(f"Schritt {data}/7: {STAGES[data]}")
                    self.progress.configure(value=data-1)
                elif kind == "done":
                    self._set_busy(False)
                    if data["ok"] and data["check"]:
                        report = data["report"]
                        self.status.set("Prüfung erfolgreich. Bereit zum Installieren und Kompilieren.")
                        self._append(f"ISO erkannt: {report['program_files']} Dateien. Build-Werkzeuge und Zielordner geprüft.\n")
                        if report.get("update"):
                            self._append(f"Update von {report['previous_version']}; bisherige Installation wird als Sicherung behalten.\n")
                        self._append("CD-Audioprofil erkannt.\n" if report["cd_audio_supported"] else "Kein bekanntes CD-Audioprofil; ein Soundfont kann das vollständige Ogg-Set liefern.\n")
                    elif data["ok"]:
                        self.installed = self.active_output
                        self.status.set("Fertig. Der native Build ist installiert und geprüft.")
                        self.progress.configure(value=7)
                        self.launch_button.configure(state="normal")
                        self.folder_button.configure(state="normal" if shutil.which("xdg-open") else "disabled")
                    elif data["cancelled"]:
                        self.status.set("Abgebrochen. Du kannst einen neuen Installationslauf starten.")
                    else:
                        self.status.set("Installation nicht abgeschlossen. Details stehen im Protokoll.")
                        self._append(data["error"] + "\n")
                        messagebox.showerror("Vorgang fehlgeschlagen", data["error"][-3000:], parent=self.root)
                    if self.close_when_done:
                        self.root.destroy()
                        return
        except queue.Empty:
            pass
        self.root.after(100, self._poll)

    def _cancel(self):
        if self.busy:
            self.status.set("Abbruch angefordert. Temporäre Dateien werden aufgeräumt …")
            self.cancel_button.configure(state="disabled")
            self.worker.cancel()

    def _launch(self):
        if self.installed:
            try:
                subprocess.Popen([str(self.installed / "play"), "--game"], start_new_session=True)
            except OSError as error:
                messagebox.showerror("Start fehlgeschlagen", str(error), parent=self.root)

    def _open_folder(self):
        if self.installed:
            try:
                subprocess.Popen(["xdg-open", str(self.installed)], start_new_session=True)
            except OSError as error:
                messagebox.showerror("Ordner konnte nicht geöffnet werden", str(error), parent=self.root)

    def _close(self):
        if not self.busy:
            self.root.destroy()
        elif messagebox.askyesno("Installation läuft", "Installation abbrechen und anschließend schließen?", parent=self.root):
            self.close_when_done = True
            self._cancel()


def main():
    try:
        root = tk.Tk()
    except tk.TclError as error:
        raise SystemExit(f"Keine grafische Sitzung verfügbar: {error}\nCLI verwenden: python3 tools/install.py --help")
    InstallerWindow(root)
    root.mainloop()


if __name__ == "__main__":
    main()
