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


if __name__ == "__main__":
    unittest.main()
