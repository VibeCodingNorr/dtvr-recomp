#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Native Linux installer assistant: select original data and compile automatically."""

import json
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
from installer_process import InstallerProcess, installer_command, dependency_command
from installer_diagnostics import DiagnosticLog
from installer_dialogs import DependencyDialog
from installer_folders import choose_directory, choose_file
from project_version import read_version, version_tuple
from installer_layout import InstallerLayout
from soundfont_defaults import default_soundfont

STAGES = {1: "Originaldaten und Voraussetzungen prüfen", 2: "Originaldaten importieren",
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
        self.backup = None
        self.diagnostic = DiagnosticLog()
        self.detail = tk.StringVar()
        self.active_output = None
        self.iso = tk.StringVar()
        self.source_kind = tk.StringVar(value="iso")
        self.output = tk.StringVar(value=str(default_output()))
        self.update = tk.BooleanVar(value=(default_output() / "manifest.json").is_file())
        self.audio = tk.StringVar(value="Vollständiges Ogg-Set aus Soundfont")
        bank = default_soundfont()
        self.bank = tk.StringVar(value=str(bank) if bank else "")
        self.music_hint = tk.StringVar()
        self.style = tk.StringVar(value="Classic — Hall und Chorus")
        self.status = tk.StringVar(value="Original-ISO oder Spielverzeichnis auswählen, dann prüfen oder installieren.")
        self.controls = []
        self.operation = None
        self.layout = InstallerLayout(self)
        self._soundfont_state()
        self.bank.trace_add("write", lambda *args: self._soundfont_state())
        self._mode_changed()
        self.iso.trace_add("write", lambda *args: self.iso_hint.set("Quelle ausgewählt. Mit „Nur prüfen“ die Ausgabe verifizieren."))
        root.protocol("WM_DELETE_WINDOW", self._close)
        root.after(100, self._poll)

    def _choose_iso(self):
        if self.source_kind.get() == "directory":
            path = choose_directory(self.root, "Original-Spielverzeichnis auswählen", Path.home())
        else:
            path = choose_file(self.root, "Eigene Original-ISO auswählen",
                               Path(self.iso.get()).expanduser() if self.iso.get() else Path.home(),
                               (("Disc-Abbild", "*.iso *.bin"), ("Alle Dateien", "*")))
        if path:
            self.iso.set(path)

    def _source_changed(self):
        self.iso.set("")
        self.iso_hint.set("Spielordner mit rally.exe oder übergeordneten Ordner mit program/ wählen. Dateien werden mit SHA-256 geprüft."
                          if self.source_kind.get() == "directory" else "Wähle deine eigene Original-ISO.")

    def _choose_output(self):
        path = Path(self.output.get()).expanduser()
        parent = choose_directory(self.root,
                                  "Vorhandene Installation auswählen" if self.update.get() else "Übergeordneten Zielordner auswählen",
                                  path if self.update.get() else path.parent)
        if parent:
            self.output.set(str(Path(parent) if self.update.get() else Path(parent) / "dtvr-recomb"))
            self._mode_changed()

    def _mode_changed(self):
        if self.update.get():
            version = "unbekannt"
            try:
                manifest = Path(self.output.get()).expanduser() / "manifest.json"
                if manifest.stat().st_size <= 8 * 1024 * 1024:
                    value = json.loads(manifest.read_text()).get("project_version")
                    version_tuple(value)
                    version = value
            except (OSError, ValueError, AttributeError):
                pass
            self.detail.set(f"Update: {version} → {read_version()}. Spiel vorher schließen. Die bisherige Installation bleibt als Sicherung erhalten.")
            self.install_button.configure(text="Update installieren  →")
            self.output_hint.set("Vorhandene Installation aktualisieren; eine vollständige Sicherung bleibt erhalten.")
        else:
            self.detail.set("Eigene ISO oder Original-Spielverzeichnis erforderlich. Ein neuer Zielordner wird angelegt.")
            self.install_button.configure(text="Installieren  →")
            self.output_hint.set("Eine neue Installation wird hier angelegt.")

    def _dependencies(self):
        if self.busy:
            return
        bank = self.bank.get().strip() if self.audio.get().startswith("Vollständiges") else ""
        self.operation = "dependencies"
        self.status.set("Alle Pakete und Bibliotheken werden geprüft …")
        self.system_state.set("Voraussetzungen werden geprüft …")
        self._append("Voraussetzungen prüfen (einschließlich CD-Audio).\n")
        self.worker.start(dependency_command(bank or ("requested" if self.audio.get().startswith("Vollständiges") else "")), True)
        self._set_busy(True)

    def _export_diagnostic(self):
        filename = filedialog.asksaveasfilename(parent=self.root, title="Bereinigtes Diagnoseprotokoll speichern",
                    defaultextension=".txt", initialfile=f"dtvr-recomb-{read_version()}-diagnose.txt",
                    filetypes=(("Textdatei", "*.txt"),))
        if filename:
            try:
                self.diagnostic.export(filename, [self.iso.get(), self.output.get(), self.bank.get(), str(REPO), str(self.backup or "")])
                self.status.set("Bereinigtes Diagnoseprotokoll gespeichert.")
            except OSError as error:
                messagebox.showerror("Speichern fehlgeschlagen", str(error), parent=self.root)

    def _choose_bank(self):
        path = choose_file(self.root, "Eigenen Soundfont auswählen",
                           Path(self.bank.get()).expanduser() if self.bank.get() else Path.home(),
                           (("SoundFont", "*.sf2 *.sf3"), ("Alle Dateien", "*")))
        if path:
            self.bank.set(path)

    def _soundfont_state(self):
        original = self.audio.get().startswith("Vollständiges")
        enabled = not self.busy and original
        if original:
            self.music_hint.set("Originalmusik wird aus deinen originalen MIDI-Dateien vorbereitet." if self.bank.get().strip()
                                else "Originalmusik ist vorgesehen. Unter „Anpassen“ bitte eine GM-SoundFont auswählen.")
        else:
            self.music_hint.set("Kein neues Klangset. Bei Updates bleiben vorhandene Sets erhalten; CD-Aufnahmen werden archiviert.")
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
        self.backup_button.configure(state="normal" if self.backup and not busy and shutil.which("xdg-open") else "disabled")
        self._soundfont_state()

    def _begin(self, check):
        if self.busy:
            return
        self._mode_changed()
        try:
            bank = self.bank.get().strip() if self.audio.get().startswith("Vollständiges") else ""
            if self.audio.get().startswith("Vollständiges") and not bank:
                if not self.layout.music_open:
                    self.layout.toggle_music()
                raise ValueError("Für die Originalmusik fehlt eine GM-SoundFont. Bitte eine lokale Bank auswählen "
                                 "oder ausdrücklich „Musikvorbereitung überspringen“ einstellen.")
            command = installer_command(self.iso.get().strip(), self.output.get().strip(), bank,
                                        "dry" if self.style.get().startswith("Dry") else "classic", check, self.update.get(),
                                        self.source_kind.get())
            self.active_output = Path(self.output.get().strip()).expanduser().resolve()
            self.operation = "check" if check else "install"
            self.progress.configure(value=0)
            self.system_state.set("Originaldaten und Voraussetzungen werden geprüft …")
            self.status.set("Voraussetzungen werden geprüft …" if check else "Installation wird vorbereitet …")
            self._append("\n--- Vorabprüfung ---\n" if check else "\n--- Installation ---\n")
            self.worker.start(command, check)
            self._set_busy(True)
            if not check:
                self.layout.show("working")
        except (OSError, ValueError) as error:
            messagebox.showerror("Eingabe prüfen", str(error), parent=self.root)

    def _append(self, text):
        self.diagnostic.append(text)
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
                    if self.operation == "install":
                        self.layout.step(0 if data == 1 else 1)
                elif kind == "done":
                    self._set_busy(False)
                    if data["ok"] and data["check"] and data["report"].get("kind") == "dependencies":
                        report = data["report"]
                        self.system_state.set("✓  Voraussetzungen erfüllt" if report["ready"] else "Voraussetzungen fehlen – Pakete prüfen")
                        self.diagnostic.append(json.dumps(report, ensure_ascii=False))
                        self.status.set("Alle Voraussetzungen vorhanden." if report["ready"] else "Voraussetzungen fehlen. Die Paketübersicht zeigt die Details.")
                        DependencyDialog(self.root, report)
                    elif data["ok"] and data["check"]:
                        report = data["report"]
                        self.status.set("Prüfung erfolgreich. Bereit zum Installieren und Kompilieren.")
                        self.iso_hint.set("✓  Unterstützte Ausgabe erkannt")
                        self.system_state.set("✓  Voraussetzungen erfüllt")
                        self._append(f"Originaldaten erkannt: {report['program_files']} Dateien. Build-Werkzeuge und Zielordner geprüft.\n")
                        if report.get("update"):
                            self._append(f"Update von {report['previous_version']}; bisherige Installation wird als Sicherung behalten.\n")
                        if report.get("source_kind") == "directory":
                            self._append("Quelldateien mit SHA-256 erfasst. Verzeichnisse enthalten keine CD-Audiospuren; die MIDI-Musik bleibt nutzbar.\n")
                        else:
                            self._append("CD-Audioprofil erkannt.\n" if report["cd_audio_supported"] else "Kein bekanntes CD-Audioprofil; ein Soundfont kann das vollständige Ogg-Set liefern.\n")
                    elif data["ok"]:
                        self.installed = self.active_output
                        result = data.get("installation", {})
                        self.backup = Path(result["backup"]) if result.get("backup") else None
                        version = result.get("project_version", read_version())
                        self.status.set(f"Fertig. DTVR Recomb {version} ist installiert und geprüft.")
                        if self.backup:
                            self.detail.set(f"Update von {result.get('previous_version', 'unbekannt')} auf {version}. Sicherung: {self.backup}")
                        else:
                            self.detail.set(f"Neuinstallation von Version {version} abgeschlossen.")
                        self.backup_button.configure(state="normal" if self.backup and shutil.which("xdg-open") else "disabled")
                        self.progress.configure(value=7)
                        self.launch_button.configure(state="normal")
                        self.folder_button.configure(state="normal" if shutil.which("xdg-open") else "disabled")
                        self.layout.show("complete")
                    elif data["cancelled"]:
                        self.status.set("Abgebrochen. Du kannst einen neuen Installationslauf starten.")
                        self.system_state.set("Vorgang abgebrochen")
                        if self.operation == "install":
                            self.layout.show("working")
                    else:
                        self.status.set("Installation nicht abgeschlossen. Details stehen im Protokoll.")
                        self.system_state.set("Prüfung fehlgeschlagen – Protokoll verfügbar")
                        if self.operation == "install":
                            self.layout.show("working")
                        self._append(data["error"] + "\n")
                        if data.get("dependencies"):
                            self.diagnostic.append(json.dumps(data["dependencies"], ensure_ascii=False))
                            DependencyDialog(self.root, data["dependencies"])
                        else:
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

    def _open_folder(self, path=None):
        path = path or self.installed
        if path:
            try:
                subprocess.Popen(["xdg-open", str(path)], start_new_session=True)
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
