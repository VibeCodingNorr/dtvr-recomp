# SPDX-License-Identifier: GPL-3.0-only
"""Supporting installer windows, kept separate from the installation assistant."""

import tkinter as tk
from tkinter import ttk


class DependencyDialog:
    def __init__(self, parent, report):
        self.window = tk.Toplevel(parent)
        self.window.title("Voraussetzungen und Pakete")
        self.window.geometry("800x620")
        self.window.minsize(680, 540)
        self.window.transient(parent)
        self.command = report.get("command", "")
        body = ttk.Frame(self.window, padding=20)
        body.pack(fill="both", expand=True)
        system = report.get("distribution", {}).get("name", "Unbekannt")
        ttk.Label(body, text=f"Erkanntes System: {system}", font=("TkDefaultFont", 12, "bold")).pack(anchor="w")
        ttk.Label(body, text="Alle Voraussetzungen sind vorhanden." if report["ready"] else
                  "Diese Übersicht zeigt vorhandene und fehlende Voraussetzungen.").pack(anchor="w", pady=(6, 14))
        listing = ttk.Frame(body)
        listing.pack(fill="both", expand=True)
        table = ttk.Treeview(listing, columns=("label", "state"), show="headings", height=11)
        table.heading("label", text="Voraussetzung")
        table.heading("state", text="Status")
        table.column("label", width=500, anchor="w")
        table.column("state", width=130, anchor="w")
        for item in report["items"]:
            state = "Vorhanden" if item["ok"] else "Nicht prüfbar" if item.get("state") == "unknown" else "Fehlt"
            table.insert("", "end", values=(item["label"], state))
        scroll = ttk.Scrollbar(listing, orient="vertical", command=table.yview)
        table.configure(yscrollcommand=scroll.set)
        table.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")
        hint = ("Befehl kopieren und selbst im Terminal ausführen. Danach erneut prüfen."
                if self.command else "Kein Paketbefehl nötig." if report["ready"] else
                "Für dieses System ist kein Paketbefehl hinterlegt. Benötigte Komponenten stehen oben.")
        ttk.Label(body, text=hint, wraplength=730).pack(anchor="w", pady=(16, 8))
        command = tk.Text(body, height=3, wrap="word", font=("TkFixedFont", 10))
        command.insert("1.0", self.command or "—")
        command.configure(state="disabled")
        command.pack(fill="x")
        actions = ttk.Frame(body)
        actions.pack(fill="x", pady=(12, 0))
        ttk.Button(actions, text="Befehl kopieren", command=self.copy,
                   state="normal" if self.command else "disabled").pack(side="left")
        ttk.Button(actions, text="Schließen", command=self.window.destroy).pack(side="right")
        self.window.bind("<Escape>", lambda event: self.window.destroy())

    def copy(self):
        self.window.clipboard_clear()
        self.window.clipboard_append(self.command)
