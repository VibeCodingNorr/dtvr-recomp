# SPDX-License-Identifier: GPL-3.0-only
"""Project-owned file/directory chooser with an optional system-dialog fallback."""

import fnmatch
import os
from pathlib import Path


def child_directories(directory, show_hidden=False):
    """List only immediate directories; no recursive traversal of symlinks."""
    return child_entries(directory, show_hidden)


def child_entries(directory, show_hidden=False, patterns=None):
    """Directories first, then files matching case-insensitive glob patterns."""
    children = []
    with os.scandir(directory) as entries:
        for entry in entries:
            if not show_hidden and entry.name.startswith("."):
                continue
            try:
                if entry.is_dir():
                    children.append(Path(entry.path))
                elif patterns is not None and entry.is_file() and any(
                        fnmatch.fnmatchcase(entry.name.casefold(), pattern.casefold()) for pattern in patterns):
                    children.append(Path(entry.path))
            except OSError:
                continue
    return sorted(children, key=lambda path: (not path.is_dir(), path.name.casefold(), path.name))


def nearest_directory(path):
    try:
        path = Path(path).expanduser()
        while not path.is_dir() and path != path.parent:
            path = path.parent
        return path.resolve() if path.is_dir() else Path.home()
    except (OSError, ValueError, RuntimeError):
        return Path.home()


class FolderDialog:
    def __init__(self, parent, title, initial, filetypes=None):
        import tkinter as tk
        from tkinter import filedialog, ttk
        self.filetypes = filetypes
        self.system_chooser = filedialog.askopenfilename if filetypes else filedialog.askdirectory
        self.result = None
        self.current = nearest_directory(initial)
        self.window = tk.Toplevel(parent)
        self.window.title(title)
        self.window.transient(parent)
        self.window.geometry("760x520")
        self.window.minsize(620, 420)
        self.path = tk.StringVar(value=str(self.current))
        self.hidden = tk.BooleanVar(value=False)
        self.filter = tk.StringVar(value=filetypes[0][0] if filetypes else "")
        self.notice = tk.StringVar()
        body = ttk.Frame(self.window, padding=18)
        body.pack(fill="both", expand=True)
        toolbar = ttk.Frame(body)
        toolbar.pack(fill="x")
        ttk.Button(toolbar, text="Persönlicher Ordner", command=lambda: self.navigate(Path.home())).pack(side="left")
        ttk.Button(toolbar, text="Dateisystem", command=lambda: self.navigate(Path("/"))).pack(side="left", padx=6)
        self.up = ttk.Button(toolbar, text="Eine Ebene höher", command=lambda: self.navigate(self.current.parent))
        self.up.pack(side="left")
        ttk.Checkbutton(toolbar, text="Versteckte Einträge", variable=self.hidden, command=self.refresh).pack(side="right")
        path_row = ttk.Frame(body)
        path_row.pack(fill="x", pady=(14, 10))
        ttk.Label(path_row, text="Pfad").pack(side="left", padx=(0, 8))
        entry = ttk.Entry(path_row, textvariable=self.path)
        entry.pack(side="left", fill="x", expand=True)
        entry.bind("<Return>", lambda event: self.navigate(self.path.get()))
        ttk.Button(path_row, text="Öffnen", command=lambda: self.navigate(self.path.get())).pack(side="left", padx=(8, 0))
        if filetypes:
            filters = ttk.Combobox(body, textvariable=self.filter, values=[label for label, _ in filetypes], state="readonly")
            filters.pack(fill="x", pady=(0, 10))
            filters.bind("<<ComboboxSelected>>", lambda event: self.refresh())
        listing = ttk.Frame(body)
        listing.pack(fill="both", expand=True)
        self.tree = ttk.Treeview(listing, columns=("name",), show="headings", selectmode="browse")
        self.tree.heading("name", text="Ordner und Dateien" if filetypes else "Ordner")
        self.tree.column("name", width=500)
        scroll = ttk.Scrollbar(listing, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=scroll.set)
        self.tree.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")
        self.tree.bind("<Double-1>", self.open_selected)
        self.tree.bind("<Return>", self.open_selected)
        self.tree.bind("<BackSpace>", lambda event: self.navigate(self.current.parent))
        ttk.Label(body, textvariable=self.notice, wraplength=690).pack(fill="x", pady=(10, 8))
        buttons = ttk.Frame(body)
        buttons.pack(fill="x")
        ttk.Button(buttons, text="Systemdialog", command=self.system_dialog).pack(side="left")
        if not filetypes:
            ttk.Button(buttons, text="Aktuellen Ordner verwenden", command=lambda: self.accept(self.current)).pack(side="left", padx=8)
        ttk.Button(buttons, text="Abbrechen", command=self.window.destroy).pack(side="right")
        ttk.Button(buttons, text="Auswählen", command=self.choose).pack(side="right", padx=8)
        self.window.bind("<Escape>", lambda event: self.window.destroy())
        self.window.protocol("WM_DELETE_WINDOW", self.window.destroy)
        self.refresh()
        if filetypes:
            initial_file = Path(initial).expanduser()
            for index, child in enumerate(self.children):
                if child == initial_file:
                    self.tree.selection_set(str(index))
                    self.tree.see(str(index))

    def refresh(self):
        self.tree.delete(*self.tree.get_children())
        try:
            patterns = None
            if self.filetypes:
                patterns = next(value for label, value in self.filetypes if label == self.filter.get())
                patterns = patterns.split() if isinstance(patterns, str) else patterns
            self.children = child_entries(self.current, self.hidden.get(), patterns)
            for index, child in enumerate(self.children):
                self.tree.insert("", "end", iid=str(index), values=(child.name + ("/" if self.filetypes and child.is_dir() else ""),))
            self.notice.set("Ordner öffnen oder eine Datei auswählen." if self.filetypes
                            else "Ordner per Doppelklick öffnen oder hier auswählen.")
        except OSError as error:
            self.children = []
            self.notice.set(f"Ordner kann nicht gelesen werden: {error.strerror or error}")
        self.path.set(str(self.current))
        self.up.configure(state="disabled" if self.current == self.current.parent else "normal")

    def navigate(self, path):
        candidate = Path(path).expanduser()
        if not candidate.is_absolute():
            candidate = self.current / candidate
        try:
            if self.filetypes and candidate.is_file():
                self.accept(candidate)
                return
            if not candidate.is_dir():
                self.notice.set("Bitte einen vorhandenen Ordner angeben.")
                return
            self.current = candidate.resolve(strict=True)
            self.refresh()
        except (OSError, RuntimeError) as error:
            self.notice.set(f"Ordner kann nicht geöffnet werden: {error}")

    def selected(self):
        selection = self.tree.selection()
        return self.children[int(selection[0])] if selection else None

    def open_selected(self, event=None):
        if self.selected() is not None:
            self.navigate(self.selected())

    def choose(self):
        selection = self.selected()
        if self.filetypes:
            if selection is None:
                self.notice.set("Bitte eine vorhandene Datei auswählen oder ihren Pfad eingeben.")
            elif selection.is_dir():
                self.navigate(selection)
            else:
                self.accept(selection)
        else:
            self.accept(selection or self.current)

    def accept(self, path):
        try:
            path = Path(path).resolve(strict=True)
            if not (path.is_file() if self.filetypes else path.is_dir()):
                raise ValueError("Auswahl ist keine Datei." if self.filetypes else "Auswahl ist kein Ordner.")
            self.result = str(path)
            self.window.destroy()
        except (OSError, ValueError) as error:
            self.notice.set(str(error))

    def system_dialog(self):
        options = {"filetypes": self.filetypes} if self.filetypes else {"mustexist": True}
        path = self.system_chooser(parent=self.window, title=self.window.title(),
                                   initialdir=str(self.current), **options)
        if path:
            self.accept(path)

    def show(self):
        self.window.wait_visibility()
        self.window.grab_set()
        self.window.wait_window()
        return self.result


def choose_directory(parent, title, initial):
    return FolderDialog(parent, title, initial).show()


def choose_file(parent, title, initial, filetypes):
    return FolderDialog(parent, title, initial, filetypes=filetypes).show()
