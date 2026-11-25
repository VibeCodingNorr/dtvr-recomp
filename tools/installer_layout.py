# SPDX-License-Identifier: GPL-3.0-only
"""Approved sidebar/card design; installer workflow stays in installer_gui."""

import tkinter as tk
from tkinter import ttk

from installer_checks import REPO
from installer_theme import BODY, NAVY, INK, MUTED, MINT, AMBER, WHITE, Card, branding_image, route_image, configure
from installer_route import LEFT as ROUTE_LEFT, TOP as ROUTE_TOP
from project_version import read_version


class InstallerLayout:
    def __init__(self, app):
        self.app = app
        root = app.root
        self.family = configure(root)
        root.title(f"DTVR Recomp Setup · {read_version()}")
        root.geometry("1200x900")
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
        self.eyebrow = tk.StringVar(value="DEIN START IN DIE RALLYE")
        ttk.Label(header, textvariable=self.eyebrow, style="Eyebrow.TLabel").pack(anchor="w")
        self.heading = tk.StringVar(value="Mach dich startklar.")
        self.subtitle = tk.StringVar(value="Deine Originaldaten, dein Zielordner – den Rest übernehmen wir.")
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
        side = tk.Frame(root, bg=NAVY, width=280)
        side.pack(side="left", fill="y")
        side.pack_propagate(False)
        self.nav = tk.Canvas(side, bg=NAVY, highlightthickness=0, height=490)
        self.nav.pack(fill="x")
        self.route_images = [route_image(root, REPO, active) for active in range(3)]
        logo = branding_image(root, REPO, "logo", 264, 88)
        if logo:
            self.images.append(logo)
            self.nav.create_image(8, 30, image=logo, anchor="nw")
        else:
            self.nav.create_text(26, 52, text="DTVR", fill="#f5edda", font=(self.family, 23, "bold"), anchor="nw")
            self.nav.create_text(28, 91, text="RECOMP", fill=MINT, font=(self.family, 12), anchor="nw")
        icon = branding_image(root, REPO, "icon", 64, 64)
        if icon:
            self.images.append(icon)
            root.iconphoto(True, icon)
        self.nav.create_text(28, 158, text="DEINE ROUTE ZUM SPIEL", fill="#a4b9ad",
                             font=(self.family, 9, "bold"), anchor="nw")
        bottom = tk.Frame(side, bg=NAVY)
        bottom.pack(side="bottom", fill="x", padx=28, pady=30)
        for label in ("Originalspiel.", "Neuer Start."):
            tk.Label(bottom, text=label, bg=NAVY, fg="#f5edda", anchor="w",
                     font=(self.family, 13, "bold")).pack(fill="x")
        for label in ("Eigene Originaldaten erforderlich.", "Deine Daten bleiben lokal.", f"Setup {read_version()}"):
            tk.Label(bottom, text=label, bg=NAVY, fg="#91a3b8", anchor="w",
                     font=(self.family, 9), wraplength=224, justify="left").pack(
                         fill="x", pady=(12 if label.startswith("Eigene") else 3, 0))

    def step(self, active):
        self.nav.delete("step")
        # The dashed route joins three real workflow phases; it is not a second
        # progress estimate alongside the backend's seven-stage progress bar.
        artwork = self.route_images[active]
        if artwork:
            self.nav.create_image(ROUTE_LEFT, ROUTE_TOP, image=artwork, anchor="nw", tags="step")
        else:
            # Minimal source-only fallback if neither packaged PNGs nor librsvg
            # are available. Packaged installers always contain the smooth art.
            self.nav.create_line(48, 218, 48, 394, fill="#7d938c", width=2, tags="step")
        for index, (title, subtitle) in enumerate((("Vorbereiten", "Quelle, Ziel und Musik"),
                                                 ("Installieren", "Importieren und prüfen"), ("Losspielen", "Bereit für die Rallye"))):
            y = 218 + index * 88
            current = index == active
            completed = index < active
            color = MINT if current or completed else AMBER
            if not artwork:
                self.nav.create_oval(29, y-19, 67, y+19, fill=color if current else NAVY,
                                     outline=color, width=2, tags="step")
            if completed and not artwork:
                self.nav.create_line(40, y, 46, y+6, 57, y-7, fill=color,
                                     width=3, capstyle="round", joinstyle="round", tags="step")
            elif not completed:
                self.nav.create_text(48, y, text=str(index+1), fill=NAVY if current else color,
                                     font=(self.family, 11, "bold"), tags="step")
            self.nav.create_text(86, y-13, text=title, anchor="nw", fill="#fff8eb" if current else "#a9bbb1",
                                 font=(self.family, 12, "bold"), tags="step")
            self.nav.create_text(86, y+12, text=subtitle, anchor="nw", fill="#a4b9ad" if current else "#82998b",
                                 font=(self.family, 9), width=170, tags="step")

    def _footer(self, main):
        app = self.app
        bottom = ttk.Frame(main, padding=(32, 12, 32, 18))
        bottom.pack(side="bottom", fill="x")
        ttk.Separator(bottom).pack(fill="x", pady=(0, 12))
        actions = ttk.Frame(bottom)
        actions.pack(fill="x")
        utilities = ttk.Frame(bottom)
        utilities.pack(fill="x", pady=(10, 0))
        ttk.Label(actions, text="Deine Daten bleiben lokal.", foreground=MUTED).pack(side="left")
        ttk.Button(utilities, text="Diagnose speichern", style="Quiet.TButton",
                   command=app._export_diagnostic).pack(side="left")
        ttk.Button(utilities, text="Protokoll", style="Quiet.TButton", command=self.show_log).pack(side="left", padx=6)
        self.close_button = ttk.Button(utilities, text="Schließen", style="Quiet.TButton", command=app._close)
        self.close_button.pack(side="right")
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
        choices = ttk.Frame(self.prepare)
        choices.pack(fill="x", pady=(0, 12))
        mode_group = ttk.Frame(choices)
        mode_group.pack(side="left")
        ttk.Label(mode_group, text="Installationsart", foreground=MUTED,
                  font=(self.family, 9)).pack(anchor="w", pady=(0, 6))
        modes = ttk.Frame(mode_group)
        modes.pack(anchor="w")
        for label, value in (("Neu installieren", False), ("Aktualisieren", True)):
            choice = ttk.Radiobutton(modes, text=label, variable=app.update, value=value,
                                      command=app._mode_changed, style="Segment.TRadiobutton")
            choice.pack(side="left")
            app.controls.append(choice)
        app.iso_hint = tk.StringVar(value="Wähle deine eigene Original-ISO.")
        app.output_hint = tk.StringVar(value="Eine neue Installation wird hier angelegt.")
        source_group = ttk.Frame(choices)
        source_group.pack(side="right")
        ttk.Label(source_group, text="Datenquelle", foreground=MUTED,
                  font=(self.family, 9)).pack(anchor="w", pady=(0, 6))
        sources = ttk.Frame(source_group)
        sources.pack(anchor="w")
        for label, value in (("Original-ISO", "iso"), ("Spielverzeichnis", "directory")):
            choice = ttk.Radiobutton(sources, text=label, variable=app.source_kind, value=value,
                                     command=app._source_changed, style="Segment.TRadiobutton")
            choice.pack(side="left")
            app.controls.append(choice)
        self.path_row(self.card(self.prepare), "01  /  DEINE ORIGINALDATEN", app.iso, app._choose_iso, app.iso_hint, "Auswählen")
        entry, _ = self.path_row(self.card(self.prepare), "02  /  DEIN ZIELORDNER", app.output, app._choose_output, app.output_hint, "Ordner wählen")
        entry.bind("<FocusOut>", lambda e: app._mode_changed())
        music = self.card(self.prepare)
        music.columnconfigure(0, weight=1)
        ttk.Label(music, text="03  /  DEINE ORIGINALMUSIK", style="CardTitle.TLabel").grid(row=0, column=0, sticky="w")
        self.music_button = ttk.Button(music, text="Anpassen  ▾", command=self.toggle_music)
        self.music_button.grid(row=0, column=1, sticky="e")
        app.controls.append(self.music_button)
        ttk.Label(music, textvariable=app.music_hint,
                  style="Card.TLabel", wraplength=630).grid(row=1, column=0, columnspan=2, sticky="w", pady=(5, 0))
        self.music_details = ttk.Frame(music, style="Card.TFrame")
        self.music_details.columnconfigure(1, weight=1)
        ttk.Label(self.music_details, text="Musik", style="Card.TLabel").grid(row=0, column=0, sticky="w", padx=(0, 10), pady=(12, 8))
        app.audio_choice = ttk.Combobox(self.music_details, textvariable=app.audio, state="readonly",
                                        values=("Vollständiges Ogg-Set aus Soundfont", "Musikvorbereitung überspringen"))
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
        ttk.Label(card, text="DEIN SPIEL NIMMT FAHRT AUF", style="CardTitle.TLabel").pack(anchor="w", pady=(0, 12))
        ttk.Label(card, textvariable=app.status, style="CardHeading.TLabel", wraplength=760).pack(fill="x", pady=(0, 18))
        app.progress = ttk.Progressbar(card, maximum=7)
        app.progress.pack(fill="x", pady=(0, 18))
        activity_row = ttk.Frame(card, style="Card.TFrame")
        activity_row.pack(fill="x")
        app.activity_pulse = ttk.Progressbar(activity_row, mode="indeterminate", maximum=100, length=80)
        app.activity_pulse.pack(side="right", padx=(12, 0))
        ttk.Label(activity_row, textvariable=app.activity_text, style="Card.TLabel", wraplength=600).pack(side="left", fill="x", expand=True)
        app.subprogress = ttk.Progressbar(card, maximum=100)
        app.subprogress.pack(fill="x", pady=(6, 8))
        ttk.Label(card, textvariable=app.timing_text, style="Card.TLabel", wraplength=730).pack(fill="x", pady=(0, 12))
        ttk.Label(card, text="Originaldaten übernehmen, das Spiel bauen und alles prüfen.\n"
                  "Du kannst den Fortschritt hier verfolgen.", style="Card.TLabel", wraplength=730).pack(fill="x")
        self.log_button = ttk.Button(card, text="Protokoll anzeigen  ▾", command=self.toggle_log)
        self.log_button.pack(anchor="w", pady=(18, 0))
        self.log_panel = ttk.Frame(card, style="Card.TFrame")
        self.log_open = False
        app.log = tk.Text(self.log_panel, height=13, wrap="word", bg=NAVY, fg="#e1eee3", relief="flat",
                           padx=16, pady=14, font="TkFixedFont", state="disabled")
        scrollbar = ttk.Scrollbar(self.log_panel, orient="vertical", command=app.log.yview)
        app.log.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side="right", fill="y")
        app.log.pack(side="left", fill="both", expand=True)
        ttk.Label(self.activity, textvariable=app.detail, wraplength=760, foreground=MUTED).pack(fill="x", pady=(4, 12))

    def toggle_log(self):
        self.set_log_open(not self.log_open)

    def set_log_open(self, visible):
        self.log_open = visible
        if visible:
            self.log_panel.pack(fill="both", expand=True, pady=(14, 0))
        else:
            self.log_panel.pack_forget()
        self.log_button.configure(text="Protokoll einklappen  ▴" if visible else "Protokoll anzeigen  ▾")

    def _completion(self):
        app = self.app
        self.complete = ttk.Frame(self.content)
        card = self.card(self.complete)
        flag = tk.Canvas(card, width=68, height=68, bg=WHITE, highlightthickness=0)
        flag.pack(anchor="w", pady=(0, 14))
        flag.create_oval(0, 0, 67, 67, fill="#edf3df", outline="")
        flag.create_line(23, 51, 23, 17, fill=INK, width=3)
        for row in range(3):
            for column in range(4):
                x, y = 25 + column * 6, 17 + row * 6
                flag.create_rectangle(x, y, x+6, y+6, fill=INK if (row+column) % 2 == 0 else AMBER, outline="")
        ttk.Label(card, text="ZIEL ERREICHT", style="CardTitle.TLabel").pack(anchor="w", pady=(0, 10))
        ttk.Label(card, text="Deine Rallye kann beginnen.", style="CardHeading.TLabel").pack(anchor="w", pady=(0, 16))
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
        if self.phase in ("prepare", "complete"):
            self.return_phase = self.phase
        self.set_log_open(True)
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
            self.eyebrow.set("DEIN START IN DIE RALLYE")
            self.heading.set("Mach dich startklar.")
            self.subtitle.set("Deine Originaldaten, dein Zielordner – den Rest übernehmen wir.")
            self.app.check_button.pack(side="left", padx=(0, 10))
            self.app.install_button.pack(side="left")
            self.step(0)
        elif phase == "working":
            self.activity.pack(fill="x")
            self.eyebrow.set("ETAPPE 2  /  INSTALLIEREN" if self.app.busy else "DEINE INSTALLATION IM BLICK")
            self.heading.set("Deine Rallye wird vorbereitet." if self.app.busy else "Dein Installationsprotokoll.")
            self.subtitle.set("Wir bereiten dein Spiel für Linux vor." if self.app.busy
                              else "Hier findest du die Details des letzten Laufs.")
            if self.app.busy:
                self.return_phase = "prepare"
                self.app.cancel_button.pack(side="right")
            else:
                self.set_log_open(True)
                self.return_button.pack(side="left", padx=(0, 10))
                if self.app.installed:
                    self.app.launch_button.pack(side="left")
                else:
                    self.app.install_button.pack(side="left")
            self.step(2 if self.return_phase == "complete" else 1)
        else:
            self.complete.pack(fill="x")
            self.eyebrow.set("ETAPPE 3  /  LOSSPIELEN")
            self.heading.set("Bereit zur Rallye.")
            self.subtitle.set("Dein Spiel ist nativ unter Linux installiert.")
            self.more_button.pack(side="left", padx=(0, 10))
            self.app.launch_button.pack(side="left")
            self.step(2)
        self.viewport.yview_moveto(0)

    def prepare_again(self):
        if not self.app.busy:
            self.app._mode_changed()
            self.show("prepare")
