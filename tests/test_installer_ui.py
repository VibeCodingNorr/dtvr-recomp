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


if __name__ == "__main__":
    unittest.main()
