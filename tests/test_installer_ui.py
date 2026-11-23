# SPDX-License-Identifier: GPL-3.0-only
"""Opt-in Tk checks: run with DTVR_TEST_GUI=1 in a graphical session or Xvfb."""

import json
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
try:
    import tkinter as tk
except ImportError:
    tk = None


@unittest.skipUnless(tk and os.environ.get("DTVR_TEST_GUI") == "1", "explicit Tk/Xvfb check")
class InstallerUiTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="dtvr-installer-ui-check-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.root = tk.Tk()
        def close():
            for callback in self.root.tk.call("after", "info"):
                self.root.after_cancel(callback)
            self.root.destroy()
        self.addCleanup(close)

    def test_folder_navigation_and_current_directory_selection(self):
        from installer_folders import FolderDialog
        child = self.directory / "child with spaces"
        child.mkdir()
        dialog = FolderDialog(self.root, "Test", self.directory)
        dialog.navigate(child)
        self.assertEqual(dialog.current, child)
        dialog.navigate("missing")
        self.assertIn("vorhandenen", dialog.notice.get())
        dialog.window.after(50, lambda: dialog.accept(dialog.current))
        self.assertEqual(dialog.show(), str(child))

    def test_file_picker_filters_navigates_and_accepts_typed_path(self):
        from installer_folders import FolderDialog
        child = self.directory / "images"
        child.mkdir()
        iso = child / "Disc ü.ISO"
        iso.write_bytes(b"synthetic image")
        (child / "notes.txt").write_text("test")
        kinds = (("Disc-Abbild", "*.iso *.bin"), ("Alle Dateien", "*"))
        dialog = FolderDialog(self.root, "ISO", self.directory, filetypes=kinds)
        dialog.tree.selection_set("0")
        dialog.open_selected()
        self.assertEqual(dialog.current, child)
        self.assertEqual(dialog.children, [iso])
        dialog.filter.set("Alle Dateien")
        dialog.refresh()
        self.assertEqual(set(dialog.children), {iso, child / "notes.txt"})
        dialog.accept(child)
        self.assertIsNone(dialog.result)
        dialog.window.after(50, lambda: dialog.navigate(str(iso)))
        self.assertEqual(dialog.show(), str(iso))

    def test_file_picker_system_fallback_and_cancel(self):
        from installer_folders import FolderDialog
        iso = self.directory / "disc.iso"
        iso.write_bytes(b"synthetic image")
        kinds = (("Disc-Abbild", "*.iso *.bin"), ("Alle Dateien", "*"))
        dialog = FolderDialog(self.root, "ISO", iso, filetypes=kinds)
        self.assertEqual(dialog.selected(), iso)
        dialog.system_chooser = lambda **kwargs: str(iso)
        dialog.window.after(50, dialog.system_dialog)
        self.assertEqual(dialog.show(), str(iso))
        dialog = FolderDialog(self.root, "Cancel", self.directory, filetypes=kinds)
        dialog.choose()
        self.assertIsNone(dialog.result)
        dialog.window.after(50, dialog.window.destroy)
        self.assertIsNone(dialog.show())

    def test_iso_selection_uses_project_file_picker(self):
        from installer_gui import InstallerWindow
        app = InstallerWindow(self.root)
        iso = self.directory / "disc.iso"
        with patch("installer_gui.choose_file", return_value=str(iso)) as chooser, \
                patch("installer_gui.filedialog.askopenfilename") as system:
            app._choose_iso()
        self.assertEqual(app.iso.get(), str(iso))
        chooser.assert_called_once()
        system.assert_not_called()

    def test_original_music_is_selected_and_system_bank_is_forwarded(self):
        from installer_gui import InstallerWindow
        bank = Path("/path/to/system-gm.sf2")
        with patch("installer_gui.default_soundfont", return_value=bank):
            app = InstallerWindow(self.root)
        self.assertTrue(app.audio.get().startswith("Vollständiges"))
        self.assertEqual(app.bank.get(), str(bank))
        self.assertIn("Originalmusik", app.music_hint.get())
        app.iso.set("/path/to/disc.iso")
        app.output.set(str(self.directory / "new installation"))
        with patch.object(app.worker, "start") as start:
            app._begin(True)
        command = start.call_args.args[0]
        self.assertIn("--soundfont", command)
        self.assertIn(str(bank), command)
        self.assertNotIn("--no-music", command)

    def test_missing_bank_requires_selection_or_explicit_no_music(self):
        from installer_gui import InstallerWindow
        with patch("installer_gui.default_soundfont", return_value=None):
            app = InstallerWindow(self.root)
        app.iso.set("/path/to/disc.iso")
        app.output.set(str(self.directory / "new installation"))
        with patch.object(app.worker, "start") as start, patch("installer_gui.messagebox.showerror") as error:
            app._begin(True)
        start.assert_not_called()
        self.assertTrue(app.layout.music_open)
        self.assertIn("Originalmusik", error.call_args.args[1])
        app.audio.set("Musikvorbereitung überspringen")
        app._soundfont_state()
        with patch.object(app.worker, "start") as start:
            app._begin(True)
        self.assertIn("--no-music", start.call_args.args[0])

    def test_directory_selection_and_command(self):
        from installer_gui import InstallerWindow
        with patch("installer_gui.default_soundfont", return_value=None):
            app = InstallerWindow(self.root)
        app.iso.set("/path/to/disc.iso")
        app.source_kind.set("directory")
        app._source_changed()
        self.assertEqual(app.iso.get(), "")
        self.assertIn("SHA-256", app.iso_hint.get())
        with patch("installer_gui.choose_directory", return_value=str(self.directory)):
            app._choose_iso()
        self.assertEqual(app.iso.get(), str(self.directory))
        app.output.set(str(self.directory / "new installation"))
        app.audio.set("Musikvorbereitung überspringen")
        with patch.object(app.worker, "start") as start:
            app._begin(True)
        command = start.call_args.args[0]
        self.assertIn("--directory", command)
        self.assertNotIn("--iso", command)
        self.assertIn(str(self.directory), command)

    def test_optional_system_dialog_and_cancellation(self):
        from installer_folders import FolderDialog
        dialog = FolderDialog(self.root, "Test", self.directory)
        dialog.system_chooser = lambda **kwargs: str(self.directory)
        dialog.window.after(50, dialog.system_dialog)
        self.assertEqual(dialog.show(), str(self.directory))
        dialog = FolderDialog(self.root, "Cancel", self.directory)
        dialog.window.after(50, dialog.window.destroy)
        self.assertIsNone(dialog.show())

    def test_dependency_dialog_copies_literal_command(self):
        from installer_dialogs import DependencyDialog
        report = {"ready": False, "distribution": {"name": "Test"},
                  "items": [{"label": "CMake", "ok": False}], "command": "sudo apt-get install cmake"}
        dialog = DependencyDialog(self.root, report)
        self.addCleanup(dialog.window.destroy)
        dialog.copy()
        self.assertEqual(self.root.clipboard_get(), report["command"])

    def test_modes_and_update_result_show_version_and_backup(self):
        from installer_gui import InstallerWindow
        output = self.directory / "installation"
        output.mkdir()
        (output / "manifest.json").write_text(json.dumps({"project_version": "0.0.1"}))
        app = InstallerWindow(self.root)
        app.output.set(str(output))
        app.update.set(True)
        app._mode_changed()
        self.assertIn("0.0.1", app.detail.get())
        self.assertIn("Update", app.install_button.cget("text"))
        app.active_output = output
        app.worker.events.put(("done", {"ok": True, "check": False, "error": "",
            "installation": {"project_version": "0.0.1", "previous_version": "0.0.1",
                             "backup": str(self.directory / "backup/installation")}}))
        app._poll()
        self.assertEqual(app.backup, self.directory / "backup/installation")
        self.assertIn("Sicherung:", app.detail.get())
        self.assertEqual(app.layout.phase, "complete")
        app.layout.show_log()
        self.assertEqual(app.layout.return_phase, "complete")
        app.update.set(False)
        app._mode_changed()
        self.assertIn("neuer Zielordner", app.detail.get())

    def test_package_probe_needs_no_iso(self):
        from installer_gui import InstallerWindow
        app = InstallerWindow(self.root)
        with patch("installer_gui.DependencyDialog") as dialog:
            app._dependencies()
            deadline = time.monotonic() + 15
            while app.busy and time.monotonic() < deadline:
                self.root.update()
                time.sleep(.01)
            self.assertFalse(app.busy)
            dialog.assert_called_once()
            self.assertEqual(dialog.call_args.args[1]["kind"], "dependencies")

    def test_diagnostic_save_redacts_selected_paths(self):
        from installer_gui import InstallerWindow
        app = InstallerWindow(self.root)
        source = self.directory / "my original disc.iso"
        target = self.directory / "diagnose.txt"
        app.iso.set(str(source))
        app._append(f"Failure reading {source}\nCompiler missing: cmake\n")
        with patch("installer_gui.filedialog.asksaveasfilename", return_value=str(target)):
            app._export_diagnostic()
        self.assertNotIn(str(self.directory), target.read_text())
        self.assertIn("Compiler missing: cmake", target.read_text())

    def test_design_preserves_music_controls_and_cancellation_view(self):
        from installer_gui import InstallerWindow
        app = InstallerWindow(self.root)
        self.assertEqual(app.layout.phase, "prepare")
        app.layout.toggle_music()
        self.root.update_idletasks()
        self.assertTrue(app.layout.music_open)
        app.audio.set("Vollständiges Ogg-Set aus Soundfont")
        app._soundfont_state()
        self.assertEqual(str(app.bank_entry.cget("state")), "normal")
        app.operation = "install"
        app._set_busy(True)
        app.layout.show("working")
        self.assertEqual(str(app.bank_entry.cget("state")), "disabled")
        app.worker.events.put(("stage", 4))
        app.worker.events.put(("done", {"ok": False, "check": False, "cancelled": True, "error": ""}))
        app._poll()
        self.assertFalse(app.busy)
        self.assertEqual(app.layout.phase, "working")
        self.assertEqual(app.progress.cget("value"), 3)
        app.layout.prepare_again()
        self.assertEqual(app.layout.phase, "prepare")
        self.assertEqual(str(app.bank_entry.cget("state")), "normal")

    def test_design_fits_minimum_size_and_expands_cards(self):
        from installer_gui import InstallerWindow
        app = InstallerWindow(self.root)
        self.root.geometry("960x760")
        app.layout.toggle_music()
        self.root.update()
        for widget in (app.check_button, app.install_button):
            self.assertGreater(widget.winfo_width(), 1)
            self.assertLessEqual(widget.winfo_rootx() + widget.winfo_width(),
                                 self.root.winfo_rootx() + self.root.winfo_width())
        self.assertGreater(app.layout.viewport.winfo_height(), 100)
        self.assertTrue(app.layout.music_details.winfo_ismapped())

    def test_collapsed_log_keeps_output_and_completion_return(self):
        from installer_gui import InstallerWindow
        app = InstallerWindow(self.root)
        app.operation = "install"
        app._set_busy(True)
        app.layout.show("working")
        self.assertFalse(app.layout.log_open)
        app._append("First stage finished.\n")
        app.layout.toggle_log()
        self.root.update_idletasks()
        self.assertTrue(app.log.winfo_ismapped())
        self.assertIn("First stage finished.", app.log.get("1.0", "end"))
        app.layout.toggle_log()
        app._append("Second stage finished.\n")
        app.layout.toggle_log()
        self.assertIn("Second stage finished.", app.log.get("1.0", "end"))

        app._set_busy(False)
        app.installed = self.directory
        app.layout.show("complete")
        app.layout.show_log()
        app.layout.show_log()  # revisiting the log must retain the completion page
        self.assertEqual(app.layout.return_phase, "complete")
        app.layout.return_button.invoke()
        self.assertEqual(app.layout.phase, "complete")

    def test_failure_opens_log_after_collapsed_installation(self):
        from installer_gui import InstallerWindow
        app = InstallerWindow(self.root)
        app.operation = "install"
        app._set_busy(True)
        app.layout.show("working")
        self.assertFalse(app.layout.log_open)
        app.worker.events.put(("done", {"ok": False, "check": False,
                               "cancelled": False, "error": "Synthetic build failure"}))
        with patch("installer_gui.messagebox.showerror"):
            app._poll()
        self.assertFalse(app.busy)
        self.assertTrue(app.layout.log_open)
        self.assertIn("Synthetic build failure", app.log.get("1.0", "end"))


if __name__ == "__main__":
    unittest.main()
