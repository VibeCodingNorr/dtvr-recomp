# SPDX-License-Identifier: GPL-3.0-only
"""Approved sidebar/card design; installer workflow stays in installer_gui."""

import tkinter as tk
from tkinter import ttk

from installer_checks import REPO
from installer_theme import BODY, NAVY, INK, MUTED, MINT, WHITE, Card, branding_image, configure
from project_version import read_version


class InstallerLayout:
    def __init__(self, app):
        self.app = app
        root = app.root
        self.family = configure(root)
        root.title(f"DTVR Recomb Setup · {read_version()}")
        root.geometry("1180x900")
        root.minsize(960, 760)
        root.configure(bg=BODY)
        self.phase = "prepare"
        self.return_phase = "prepare"
        self.images = []
        self._sidebar(root)
        main = tk.Frame(root, bg=BODY)
        main.pack(side="left", fill="both", expand=True)
        header = ttk.Frame(main, padding=(32, 25, 32, 0))
        header.pack(fill="x")
        ttk.Label(header, text="WILLKOMMEN ZURÜCK AM START", style="Eyebrow.TLabel").pack(anchor="w")
        self.heading = tk.StringVar(value="Installation vorbereiten")
        self.subtitle = tk.StringVar(value="Wähle deine Original-ISO. Der Installer erledigt Import und Build.")
        ttk.Label(header, textvariable=self.heading, style="Heading.TLabel").pack(anchor="w", pady=(8, 6))
        subtitle = ttk.Label(header, textvariable=self.subtitle, style="Subtitle.TLabel", wraplength=810)
        subtitle.pack(anchor="w")
        self._footer(main)
        center = ttk.Frame(main, padding=(32, 14, 24, 0))
        center.pack(fill="both", expand=True)
        self.viewport = tk.Canvas(center, bg=BODY, highlightthickness=0, bd=0)
        self.scrollbar = ttk.Scrollbar(center, orient="vertical", command=self.viewport.yview)
        self.scrollbar.pack(side="right", fill="y")
        self.viewport.pack(side="left", fill="both", expand=True, padx=(0, 6))
        self.viewport.configure(yscrollcommand=self.scroll)
        self.content = ttk.Frame(self.viewport)
        self.window = self.viewport.create_window(0, 0, window=self.content, anchor="nw")
        self.content.bind("<Configure>", lambda event: self.viewport.configure(scrollregion=self.viewport.bbox("all")))
        self.viewport.bind("<Configure>", self.resize)
        for event, delta in (("<Button-4>", -1), ("<Button-5>", 1)):
            root.bind(event, lambda e, d=delta: self.viewport.yview_scroll(d, "units"), add="+")
        root.bind("<MouseWheel>", lambda e: self.viewport.yview_scroll(-int(e.delta / 120), "units"), add="+")
        main.bind("<Configure>", lambda e: subtitle.configure(wraplength=max(300, main.winfo_width()-70)), add="+")
        self.prepare = ttk.Frame(self.content)
        self.prepare.pack(fill="x")
        self._form()
        self._activity()
        self._completion()
        self.step(0)

    def resize(self, event):
        self.viewport.itemconfigure(self.window, width=event.width)
        if hasattr(self, "detail_label"):
            self.detail_label.configure(wraplength=max(260, event.width-8))

    def scroll(self, first, last):
        self.scrollbar.set(first, last)
        if float(first) <= 0 and float(last) >= 1:
            self.scrollbar.pack_forget()
        elif not self.scrollbar.winfo_manager():
            self.scrollbar.pack(side="right", fill="y", before=self.viewport)

    def _sidebar(self, root):
        side = tk.Frame(root, bg=NAVY, width=244)
        side.pack(side="left", fill="y")
        side.pack_propagate(False)
        self.nav = tk.Canvas(side, bg=NAVY, highlightthickness=0, height=500)
        self.nav.pack(fill="x")
        logo = branding_image(root, REPO, "logo", 224, 67)
        if logo:
            self.images.append(logo)
            self.nav.create_image(10, 38, image=logo, anchor="nw")
        else:
            self.nav.create_text(26, 52, text="DTVR", fill="#f5edda", font=(self.family, 23, "bold"), anchor="nw")
            self.nav.create_text(28, 91, text="RECOMB", fill=MINT, font=(self.family, 12), anchor="nw")
        icon = branding_image(root, REPO, "icon", 64, 64)
        if icon:
            self.images.append(icon)
            root.iconphoto(True, icon)
        self.nav.create_text(28, 151, text="DEIN WEG ZUM SPIEL", fill="#8ca1b8",
                             font=(self.family, 9, "bold"), anchor="nw")
        self.nav.create_line(43, 200, 43, 367, fill="#2c3b50", width=2)
        self.nav.create_line(33, 476, 33, 451, 192, 451, 192, 420, fill="#253b51", width=3, smooth=True)
        self.nav.create_oval(29, 472, 37, 480, fill=MINT, outline="")
        self.nav.create_oval(188, 416, 196, 424, fill="#f3b64c", outline="")
        bottom = tk.Frame(side, bg=NAVY)
        bottom.pack(side="bottom", fill="x", padx=28, pady=30)
        for label in ("Deine Kopie.", "Dein Linux-Spiel."):
            tk.Label(bottom, text=label, bg=NAVY, fg="#f5edda", anchor="w",
                     font=(self.family, 13, "bold")).pack(fill="x")
        for label in ("Original-ISO erforderlich.", "Deine Daten bleiben lokal.", f"Setup {read_version()}"):
            tk.Label(bottom, text=label, bg=NAVY, fg="#91a3b8", anchor="w",
                     font=(self.family, 9)).pack(fill="x", pady=(12 if label.startswith("Original") else 3, 0))

    def step(self, active):
        self.nav.delete("step")
        for index, (title, subtitle) in enumerate((("Vorbereiten", "ISO, Ziel und Musik"),
                                                 ("Installieren", "Import und Build"), ("Losspielen", "Alles bereit"))):
            y = 204 + index * 80
            current = index == active
            if current:
                self.nav.create_rectangle(14, y-25, 230, y+34, fill="#1a2d42", outline="", tags="step")
            self.nav.create_oval(27, y-16, 59, y+16, fill=MINT if current else NAVY,
                                 outline=MINT if current else "#415269", tags="step")
            self.nav.create_text(43, y, text=str(index+1), fill=NAVY if current else "#91a2b7",
                                 font=(self.family, 11, "bold"), tags="step")
            self.nav.create_text(76, y-11, text=title, anchor="nw", fill="#f5edda" if current else "#9aabc0",
                                 font=(self.family, 12, "bold"), tags="step")
            self.nav.create_text(76, y+11, text=subtitle, anchor="nw", fill="#a3b7ca" if current else "#687d95",
                                 font=(self.family, 9), tags="step")

    def _footer(self, main):
        app = self.app
        bottom = ttk.Frame(main, padding=(32, 12, 32, 18))
        bottom.pack(side="bottom", fill="x")
        ttk.Separator(bottom).pack(fill="x", pady=(0, 12))
        actions = ttk.Frame(bottom)
        actions.pack(fill="x")
        ttk.Button(actions, text="Diagnose speichern", style="Quiet.TButton",
                   command=app._export_diagnostic).pack(side="left")
        ttk.Button(actions, text="Protokoll", style="Quiet.TButton", command=self.show_log).pack(side="left", padx=6)
        self.close_button = ttk.Button(actions, text="Schließen", style="Quiet.TButton", command=app._close)
        self.close_button.pack(side="left", padx=8)
        self.primary = ttk.Frame(actions)
        self.primary.pack(side="right")
        app.check_button = ttk.Button(self.primary, text="Nur prüfen", command=lambda: app._begin(True))
        app.check_button.pack(side="left", padx=(0, 10))
        app.install_button = ttk.Button(self.primary, text="Installieren  →", style="Primary.TButton", command=lambda: app._begin(False))
        app.install_button.pack(side="left")
        app.cancel_button = ttk.Button(self.primary, text="Abbrechen", command=app._cancel, state="disabled")
        self.return_button = ttk.Button(self.primary, text="Zurück", command=lambda: self.show(self.return_phase))
        self.more_button = ttk.Button(self.primary, text="Weitere Installation", command=self.prepare_again)
        app.launch_button = ttk.Button(self.primary, text="Spiel starten  →", style="Primary.TButton", command=app._launch, state="disabled")
        # Folder/backup actions belong to the completion panel, not the main footer.

    def card(self, parent):
        card = Card(parent)
        card.pack(fill="x", pady=(0, 12))
        return card.content

    def path_row(self, parent, title, variable, command, hint, button_text):
        parent.columnconfigure(0, weight=1)
        ttk.Label(parent, text=title, style="CardTitle.TLabel").grid(row=0, column=0, columnspan=2, sticky="w", pady=(0, 10))
        entry = ttk.Entry(parent, textvariable=variable)
        entry.grid(row=1, column=0, sticky="ew", padx=(0, 12))
        button = ttk.Button(parent, text=button_text, command=command)
        button.grid(row=1, column=1, sticky="e")
        ttk.Label(parent, textvariable=hint, style="Card.TLabel", wraplength=650).grid(
            row=2, column=0, columnspan=2, sticky="w", pady=(8, 0))
        self.app.controls += [entry, button]
        return entry, button

    def _form(self):
        app = self.app
        modes = ttk.Frame(self.prepare, padding=(4, 4), style="Card.TFrame")
        modes.pack(anchor="w", pady=(0, 16))
        for label, value in (("Neu installieren", False), ("Aktualisieren", True)):
            choice = ttk.Radiobutton(modes, text=label, variable=app.update, value=value,
                                      command=app._mode_changed, style="Segment.TRadiobutton")
            choice.pack(side="left")
            app.controls.append(choice)
        app.iso_hint = tk.StringVar(value="Wähle deine eigene Original-ISO.")
        app.output_hint = tk.StringVar(value="Eine neue Installation wird hier angelegt.")
        self.path_row(self.card(self.prepare), "ORIGINAL-ISO", app.iso, app._choose_iso, app.iso_hint, "ISO auswählen")
        entry, _ = self.path_row(self.card(self.prepare), "ZIELORDNER", app.output, app._choose_output, app.output_hint, "Ordner wählen")
        entry.bind("<FocusOut>", lambda e: app._mode_changed())
        music = self.card(self.prepare)
        music.columnconfigure(0, weight=1)
        ttk.Label(music, text="Musik & Klangset", style="CardTitle.TLabel").grid(row=0, column=0, sticky="w")
        self.music_button = ttk.Button(music, text="Anpassen  ▾", command=self.toggle_music)
        self.music_button.grid(row=0, column=1, sticky="e")
        app.controls.append(self.music_button)
        ttk.Label(music, text="Optional: Mit eigener SoundFont ein vollständiges Ogg-Set erstellen.",
                  style="Card.TLabel", wraplength=630).grid(row=1, column=0, columnspan=2, sticky="w", pady=(5, 0))
        self.music_details = ttk.Frame(music, style="Card.TFrame")
        self.music_details.columnconfigure(1, weight=1)
        ttk.Label(self.music_details, text="Musik", style="Card.TLabel").grid(row=0, column=0, sticky="w", padx=(0, 10), pady=(12, 8))
        app.audio_choice = ttk.Combobox(self.music_details, textvariable=app.audio, state="readonly",
                                        values=("Nur CD-Aufnahmen vorbereiten", "Vollständiges Ogg-Set aus Soundfont"))
        app.audio_choice.grid(row=0, column=1, columnspan=2, sticky="ew", pady=(12, 8))
        app.audio_choice.bind("<<ComboboxSelected>>", lambda e: app._soundfont_state())
        ttk.Label(self.music_details, text="SoundFont", style="Card.TLabel").grid(row=1, column=0, sticky="w")
        app.bank_entry = ttk.Entry(self.music_details, textvariable=app.bank)
        app.bank_entry.grid(row=1, column=1, sticky="ew", padx=(0, 10), pady=6)
        app.bank_button = ttk.Button(self.music_details, text="Auswählen", command=app._choose_bank)
        app.bank_button.grid(row=1, column=2)
        app.controls += [app.bank_entry, app.bank_button]
        ttk.Label(self.music_details, text="Klang", style="Card.TLabel").grid(row=2, column=0, sticky="w")
        app.style_choice = ttk.Combobox(self.music_details, textvariable=app.style, state="readonly",
                                        values=("Classic — Hall und Chorus", "Dry — ohne Effekte"))
        app.style_choice.grid(row=2, column=1, columnspan=2, sticky="ew", pady=6)
        self.music_open = False
        system = ttk.Frame(self.prepare, padding=(14, 10))
        system.pack(fill="x")
        app.system_state = tk.StringVar(value="Voraussetzungen noch nicht geprüft")
        state_label = ttk.Label(system, textvariable=app.system_state, foreground=MUTED)
        state_label.pack(side="left")
        app.system_state.trace_add("write", lambda *args: state_label.configure(
            foreground="#39745e" if app.system_state.get().startswith("✓") else
            "#a36722" if "fehlen" in app.system_state.get() or "fehlgeschlagen" in app.system_state.get() else MUTED))
        app.packages_button = ttk.Button(system, text="Pakete prüfen", style="Quiet.TButton", command=app._dependencies)
        app.packages_button.pack(side="right")
        app.controls.append(app.packages_button)
        self.detail_label = ttk.Label(self.prepare, textvariable=app.detail, foreground=MUTED, wraplength=760)
        self.detail_label.pack(fill="x", pady=(4, 8))

    def toggle_music(self):
        self.music_open = not self.music_open
        if self.music_open:
            self.music_details.grid(row=2, column=0, columnspan=2, sticky="ew")
        else:
            self.music_details.grid_remove()
        self.music_button.configure(text="Schließen  ▴" if self.music_open else "Anpassen  ▾")

    def _activity(self):
        app = self.app
        self.activity = ttk.Frame(self.content)
        card = self.card(self.activity)
        ttk.Label(card, textvariable=app.status, style="CardTitle.TLabel", wraplength=760).pack(fill="x", pady=(0, 14))
        app.progress = ttk.Progressbar(card, maximum=7)
        app.progress.pack(fill="x", pady=(0, 16))
        app.log = tk.Text(card, height=15, wrap="word", bg=NAVY, fg="#dce6ef", relief="flat",
                           padx=16, pady=14, font="TkFixedFont", state="disabled")
        app.log.pack(fill="both", expand=True)
        ttk.Label(self.activity, textvariable=app.detail, wraplength=760, foreground=MUTED).pack(fill="x", pady=(4, 12))

    def _completion(self):
        app = self.app
        self.complete = ttk.Frame(self.content)
        card = self.card(self.complete)
        ttk.Label(card, text="✓  Alles bereit", style="CardTitle.TLabel").pack(anchor="w", pady=(8, 18))
        ttk.Label(card, textvariable=app.status, style="Card.TLabel", wraplength=740).pack(anchor="w")
        ttk.Label(card, textvariable=app.detail, style="Card.TLabel", wraplength=740).pack(anchor="w", pady=(14, 20))
        buttons = ttk.Frame(card, style="Card.TFrame")
        buttons.pack(fill="x", pady=(0, 6))
        app.folder_button = ttk.Button(buttons, text="Installationsordner öffnen", command=app._open_folder, state="disabled")
        app.folder_button.pack(side="left", padx=(0, 10))
        app.backup_button = ttk.Button(buttons, text="Sicherung öffnen", command=lambda: app._open_folder(app.backup), state="disabled")
        app.backup_button.pack(side="left")
        ttk.Button(self.complete, text="Protokoll ansehen", command=self.show_log).pack(anchor="w")

    def show_log(self):
        self.return_phase = self.phase if self.phase in ("prepare", "complete") else "prepare"
        self.show("working")

    def show(self, phase):
        self.phase = phase
        for frame in (self.prepare, self.activity, self.complete):
            frame.pack_forget()
        for button in (self.app.check_button, self.app.install_button, self.app.cancel_button,
                       self.return_button, self.more_button, self.app.launch_button):
            button.pack_forget()
        if phase == "prepare":
            self.prepare.pack(fill="x")
            self.heading.set("Installation vorbereiten")
            self.subtitle.set("Wähle deine Original-ISO. Der Installer erledigt Import und Build.")
            self.app.check_button.pack(side="left", padx=(0, 10))
            self.app.install_button.pack(side="left")
            self.step(0)
        elif phase == "working":
            self.activity.pack(fill="x")
            self.heading.set("Installation im Blick" if self.app.busy else "Installationsprotokoll")
            self.subtitle.set("Fortschritt und Protokoll zeigen, was gerade passiert.")
            if self.app.busy:
                self.return_phase = "prepare"
                self.app.cancel_button.pack(side="right")
            else:
                self.return_button.pack(side="left", padx=(0, 10))
                if self.app.installed:
                    self.app.launch_button.pack(side="left")
                else:
                    self.app.install_button.pack(side="left")
            self.step(2 if self.return_phase == "complete" else 1)
        else:
            self.complete.pack(fill="x")
            self.heading.set("Bereit für die nächste Runde.")
            self.subtitle.set("Dein Spiel ist nativ unter Linux installiert.")
            self.more_button.pack(side="left", padx=(0, 10))
            self.app.launch_button.pack(side="left")
            self.step(2)
        self.viewport.yview_moveto(0)

    def prepare_again(self):
        if not self.app.busy:
            self.app._mode_changed()
            self.show("prepare")
