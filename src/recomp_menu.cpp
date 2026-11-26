// SPDX-License-Identifier: GPL-3.0-only
#include "recomp_menu.hpp"

#include <algorithm>

namespace dtvr {
namespace {

using Kind = RecompMenu::Kind;
using Field = RecompMenu::Field;
using Action = RecompMenu::Action;
using Item = RecompMenu::Item;

Item command(std::string label, Action action, std::string hint) {
    Item i;
    i.kind = Kind::Command;
    i.label = std::move(label);
    i.command = action;
    i.hint = std::move(hint);
    return i;
}

Item toggle(std::string label, Field field, std::string hint, bool original = false) {
    Item i;
    i.kind = Kind::Toggle;
    i.label = std::move(label);
    i.field = field;
    i.hint = std::move(hint);
    i.choices = {"Aus", "An"};
    i.original = original;
    return i;
}

Item choice(std::string label, Field field, std::vector<std::string> choices, std::string hint) {
    Item i;
    i.kind = Kind::Choice;
    i.label = std::move(label);
    i.field = field;
    i.choices = std::move(choices);
    i.hint = std::move(hint);
    return i;
}

Item info(std::string label, std::string value) {
    Item i;
    i.label = std::move(label);
    i.value = std::move(value);
    return i;
}

// One row of the controls table: action, keyboard, game controller.
Item control(std::string label, std::string keyboard, std::string pad) {
    Item i = info(std::move(label), std::move(keyboard));
    i.pad = std::move(pad);
    return i;
}

} // namespace

// The keys of the game window and the pad mapping of src/pad_input.cpp (docs/formats.md
// "Joystick" / "Recomp pad layer"); the pad column holds while [Setup] Joystick is on.
RecompMenu::Page controlsPage() {
    RecompMenu::Page p;
    p.title = "Steuerung";
    p.columns = {"Aktion", "Tastatur", "Gamepad"};
    p.hint = "Das Gamepad wirkt wie der Joystick des Originals, solange die Option Joystick an ist; "
             "alle angeschlossenen Pads zählen gemeinsam.";
    p.items = {
        control("Fahren, Auswahl bewegen", "Pfeiltasten", "Kreuz, linker Stick"),
        control("Bestätigen, Schalter umlegen", "Enter, Leertaste", "A"),
        control("Abbrechen, zurück", "Esc", "B"),
        control("Recomp-Menü (Pause)", "Esc, F8 (Audio)", "Start"),
        control("Originales Spielmenü", "Rücktaste, rechte Maustaste", "Ansicht"),
        control("Schließen, Zeichen löschen", "Rücktaste", "X"),
        control("Karte einsetzen, Extras", "Spielmenü, F9", "Y"),
        control("Vorige / nächste Seite", "Umschalt+Tab / Tab", "LB / RB"),
        control("Umfeld verschieben", "Pfeiltasten", "rechter Stick"),
        control("Spielernamen eingeben", "Feld wählen, tippen", "A: Bildschirmtastatur"),
        control("Etappen ±1 / ±10 / ±50", "←→ / ↑↓ / Tab", "Kreuz / LB, RB"),
        control("Vollbild / Schriftglättung", "F11, Alt+Enter / F12", "Seite Grafik"),
    };
    return p;
}

RecompMenu::RecompMenu(bool inGame) { build(inGame); }

void RecompMenu::build(bool inGame) {
    pages_.clear();
    Page pause{"Pause", {}};
    pause.items.push_back(command("Weiterspielen", Action::Resume, "Zurück zum Spiel (Esc)."));
    if (inGame) {
        pause.items.push_back(command("Spiel speichern", Action::SaveGame, "Öffnet den Speicherdialog des Originals."));
        pause.items.push_back(command("Spiel laden", Action::LoadGame, "Lädt einen Spielstand; das laufende Spiel endet."));
        pause.items.push_back(command("Originales Spielmenü", Action::OriginalMenu,
                                      "Das Menü von rally.exe (auch mit der Rücktaste)."));
    }
    pause.items.push_back(command("Rallye beenden", Action::Quit, "Fragt wie das Original nach, bevor das Spiel endet."));
    pages_.push_back(std::move(pause));

    pages_.push_back({"Grafik", {
        toggle("Vollbild", Field::Fullscreen, "Fenster oder Vollbild auf dem Desktop (F11, Alt+Enter)."),
        toggle("Ganzzahlige Skalierung", Field::IntegerScale,
               "Scharfe Pixel mit schwarzen Rändern; aus: so groß wie möglich. Im Breitbild immer bildfüllend."),
        toggle("Breitbild", Field::Widescreen,
               "Breiteres Kartenfenster im Seitenverhältnis des Bildschirms (bis 21:9); wirkt sofort."),
        choice("Schriftglättung", Field::TextSmoothing, {"Original", "Bildgröße", "Bildschirmauflösung"},
               "Original: ohne Kantenglättung wie GDI mit 256 Farben (F12)."),
        toggle("Informationen", Field::Views, "Sehenswürdigkeiten auf der Karte (Option des Originals).", true),
        toggle("Animationen", Field::Animations, "Bewegte Objekte der Karte (Option des Originals).", true),
        toggle("Wellen und Jahreszeiten", Field::Colorcycle,
               "Farbzyklus des Wassers und Jahreszeiten der Karte (Option des Originals).", true),
    }});
    Item volume;
    volume.kind = Kind::Slider;
    volume.label = "Gesamtlautstärke";
    volume.field = Field::MasterVolume;
    volume.hint = "Musik, Effekte und Sprache gemeinsam; das Original kennt nur Ein / Aus, ihr Verhältnis bleibt.";
    Page audio{"Audio", {
        volume,
        toggle("Musik", Field::Midi, "Die MIDI-Musik des Originals, gespielt aus dem gewählten Klangset.", true),
    }};
    if (music_.ids.empty()) {
        Item none = info("Klangset", "nicht installiert");
        none.field = Field::MusicPack;
        audio.items.push_back(std::move(none));
    } else {
        audio.items.push_back(choice("Klangset", Field::MusicPack, music_.names,
                                     "Vollständige Ogg-Klangsets der installierten Musik. Wechsel beim nächsten Stück."));
    }
    Item playing = info("Es läuft", "");
    playing.field = Field::MusicPlaying;
    audio.items.push_back(std::move(playing));
    audio.items.push_back(toggle("Soundeffekte", Field::Wave, "Geräusche des Spiels (Option des Originals).", true));
    audio.items.push_back(toggle("Sprachausgabe", Field::Voice, "Stimme des Showmasters (Option des Originals).", true));
    audio.items.push_back(toggle("Untertitel", Field::Subtitle, "Untertitel zur Sprachausgabe ([Setup] Subtitle)."));
    pages_.push_back(std::move(audio));
    pages_.push_back({"Spiel", {
        toggle("Zeitlimit", Field::TimeOut, "Begrenzte Zeit pro Zug (Option des Originals).", true),
        choice("Computerz\u00fcge", Field::ComputerTempo, {"Original (1\u00d7)", "Schnell (2\u00d7)", "Sehr schnell (4\u00d7)"},
               "Solange ein Computer am Zug ist: Fahrt, Spielautomat, Ereignisse und Lesezeit seiner Meldungen."),
        choice("Animationstempo", Field::AnimationTempo, {"Original (1\u00d7)", "Schnell (2\u00d7)"},
               "Fahrten, Spielautomat und Ereignis-Animationen f\u00fcr alle. Zeitlimit, Musik und Sprache bleiben gleich."),
        choice("Wartezeit der Meldungen", Field::Pause, {"Normal", "1,5-fach", "2-fach", "3-fach"},
               "Wie lange Meldungen der Computer stehen bleiben (Tasten 1–4 des Originals)."),
        toggle("Intro beim Start", Field::ShowIntro, "Spielt das Intro vor dem Titelbild."),
        toggle("Computer spielen alle Karten", Field::ExtraCards,
               "Erweiterung: auch Karten, die rally.exe nie wählt. Gilt für neue Spiele."),
    }});
    pages_.push_back(controlsPage());
}

void RecompMenu::open(const Settings& settings, bool inGame) {
    inGame_ = inGame;
    build(inGame);
    settings_ = settings;
    page_ = 0;
    row_ = 0;
    inItems_ = true; // the first entry, Weiterspielen, has the focus
    open_ = true;
}

void RecompMenu::openAt(const Settings& settings, bool inGame, const std::string& page) {
    open(settings, inGame);
    for (size_t i = 0; i < pages_.size(); ++i)
        if (pages_[i].title == page) {
            focusPage(int(i));
            activate();
        }
}

void RecompMenu::setMusic(Music music) {
    const bool rebuild = music.ids != music_.ids || music.names != music_.names;
    music_ = std::move(music);
    if (!rebuild) return;
    build(inGame_);
    const auto& items = pages_[size_t(std::min(page_, int(pages_.size()) - 1))].items;
    if (row_ >= int(items.size()) || !selectable(items[size_t(row_)])) { row_ = 0; if (inItems_) moveRow(1); }
}

int RecompMenu::fieldValue(Field f) const {
    switch (f) {
    case Field::MusicPack: return music_.pack;
    case Field::MasterVolume: return settings_.masterVolume;
    case Field::Fullscreen: return settings_.fullscreen;
    case Field::IntegerScale: return settings_.integerScale;
    case Field::Widescreen: return settings_.widescreen;
    case Field::TextSmoothing: return settings_.textSmoothing;
    case Field::Views: return settings_.value[Settings::Views];
    case Field::Animations: return settings_.value[Settings::Animations];
    case Field::Colorcycle: return settings_.value[Settings::Colorcycle];
    case Field::Wave: return settings_.value[Settings::Wave];
    case Field::Midi: return settings_.value[Settings::Midi];
    case Field::Voice: return settings_.value[Settings::Voice];
    case Field::Subtitle: return settings_.subtitle;
    case Field::TimeOut: return settings_.value[Settings::TimeOut];
    case Field::Pause: return settings_.pause;
    case Field::ShowIntro: return settings_.showIntro;
    case Field::ExtraCards: return settings_.computerExtraCards;
    case Field::AnimationTempo: return settings_.animationTempo == 2 ? 1 : 0;
    case Field::ComputerTempo: return settings_.computerTempo == 4 ? 2 : settings_.computerTempo == 2 ? 1 : 0;
    case Field::MusicPlaying: case Field::MusicNote: case Field::None: break;
    }
    return 0;
}

void RecompMenu::setFieldValue(Field f, int v) {
    switch (f) {
    case Field::Fullscreen: settings_.fullscreen = v != 0; break;
    case Field::IntegerScale: settings_.integerScale = v != 0; break;
    case Field::Widescreen: settings_.widescreen = v != 0; break;
    case Field::TextSmoothing: settings_.textSmoothing = v; break;
    case Field::Views: settings_.value[Settings::Views] = v; break;
    case Field::Animations: settings_.value[Settings::Animations] = v; break;
    case Field::Colorcycle: settings_.value[Settings::Colorcycle] = v; break;
    case Field::Wave: settings_.value[Settings::Wave] = v; break;
    case Field::Midi: settings_.value[Settings::Midi] = v; break;
    case Field::Voice: settings_.value[Settings::Voice] = v; break;
    case Field::Subtitle: settings_.subtitle = v; break;
    case Field::TimeOut: settings_.value[Settings::TimeOut] = v; break;
    case Field::Pause: settings_.pause = v; break;
    case Field::ShowIntro: settings_.showIntro = v; break;
    case Field::ExtraCards: settings_.computerExtraCards = v != 0; break;
    case Field::AnimationTempo: settings_.animationTempo = v == 1 ? 2 : 1; break;
    case Field::ComputerTempo: settings_.computerTempo = v == 2 ? 4 : v == 1 ? 2 : 1; break;
    case Field::MusicPack: music_.pack = v; break;
    case Field::MasterVolume: settings_.masterVolume = std::clamp(v, 0, 100); break;
    case Field::MusicPlaying: case Field::MusicNote: case Field::None: break;
    }
}

int RecompMenu::fieldStates(const Item& item) const { return std::max<int>(1, int(item.choices.size())); }

std::string RecompMenu::valueText(const Item& item) const {
    if (item.field == Field::MusicPlaying) return music_.playing.empty() ? "nichts" : "Stück " + music_.playing;
    if (item.kind == Kind::Info) return item.value;
    if (item.kind == Kind::Command) return {};
    if (item.kind == Kind::Slider) return std::to_string(fieldValue(item.field)) + " %";
    const int v = fieldValue(item.field);
    return v >= 0 && v < int(item.choices.size()) ? item.choices[size_t(v)] : std::to_string(v);
}

void RecompMenu::moveRow(int delta) {
    const auto& items = pages_[size_t(page_)].items;
    if (items.empty()) return;
    // Info rows are skipped; a page of only info rows keeps the first row.
    int r = row_;
    for (size_t n = 0; n < items.size(); ++n) {
        r = (r + delta + int(items.size())) % int(items.size());
        if (selectable(items[size_t(r)])) { row_ = r; return; }
    }
}

void RecompMenu::focusPage(int page) {
    if (page < 0 || page >= int(pages_.size())) return;
    if (page != page_) row_ = 0;
    page_ = page;
    inItems_ = false;
}

void RecompMenu::focusItem(int row) {
    const auto& items = pages_[size_t(page_)].items;
    if (row < 0 || row >= int(items.size()) || !selectable(items[size_t(row)])) return;
    row_ = row;
    inItems_ = true;
}

RecompMenu::Action RecompMenu::step(int delta) {
    if (!inItems_) return Action::None;
    const Item& item = pages_[size_t(page_)].items[size_t(row_)];
    if (item.kind == Kind::Slider) { // no wrap-around
        const int old = fieldValue(item.field);
        setFieldValue(item.field, old + delta * kVolumeStep);
        if (fieldValue(item.field) == old) return Action::None;
        return musicField(item.field) ? Action::MusicChanged : Action::Changed;
    }
    if (item.kind != Kind::Toggle && item.kind != Kind::Choice) return Action::None;
    const int n = fieldStates(item);
    setFieldValue(item.field, ((std::max(0, fieldValue(item.field)) + delta) % n + n) % n);
    return musicField(item.field) ? Action::MusicChanged : Action::Changed;
}

RecompMenu::Action RecompMenu::setSlider(int percent) {
    if (!inItems_) return Action::None;
    const Item& item = pages_[size_t(page_)].items[size_t(row_)];
    if (item.kind != Kind::Slider) return Action::None;
    const int v = (std::clamp(percent, 0, 100) + kVolumeStep / 2) / kVolumeStep * kVolumeStep;
    if (v == fieldValue(item.field)) return Action::None;
    setFieldValue(item.field, v);
    return musicField(item.field) ? Action::MusicChanged : Action::Changed;
}

RecompMenu::Action RecompMenu::activate() {
    if (!inItems_) { // a page name: into its entries
        const auto& items = pages_[size_t(page_)].items;
        if (std::none_of(items.begin(), items.end(), [this](const Item& i) { return selectable(i); }))
            return Action::None; // only info rows (Steuerung)
        inItems_ = true;
        row_ = 0;
        if (!selectable(items[0])) moveRow(1);
        return Action::None;
    }
    const Item& item = pages_[size_t(page_)].items[size_t(row_)];
    if (item.kind == Kind::Command) {
        if (item.command == Action::Resume) open_ = false;
        return item.command;
    }
    return step(1);
}

RecompMenu::Action RecompMenu::input(Input in) {
    if (!open_) return Action::None;
    switch (in) {
    case Input::Up:
        if (inItems_) moveRow(-1);
        else focusPage((page_ + int(pages_.size()) - 1) % int(pages_.size()));
        return Action::None;
    case Input::Down:
        if (inItems_) moveRow(1);
        else focusPage((page_ + 1) % int(pages_.size()));
        return Action::None;
    case Input::Left: {
        if (!inItems_) return Action::None;
        const Kind k = pages_[size_t(page_)].items[size_t(row_)].kind;
        if (k == Kind::Toggle || k == Kind::Choice || k == Kind::Slider) return step(-1);
        inItems_ = false; // back to the page list
        return Action::None;
    }
    case Input::Right:
        if (!inItems_) return activate();
        return step(1);
    case Input::Accept:
        return activate();
    case Input::Back:
        if (inItems_ && page_ != 0) { inItems_ = false; return Action::None; }
        open_ = false;
        return Action::Resume;
    case Input::PrevPage: case Input::NextPage: { // LB / RB, Shift+Tab / Tab: the neighbour page
        const int n = int(pages_.size());
        const bool entries = inItems_;
        focusPage((page_ + (in == Input::NextPage ? 1 : n - 1)) % n);
        if (entries) activate(); // stays in the entries where the page has any
        return Action::None;
    }
    }
    return Action::None;
}

const KeyboardTexts& keyboardTexts() {
    static const KeyboardTexts texts{"Gro\u00df", "L\u00f6schen", "Leerzeichen", "Abbrechen", "Fertig",
                                     "A tippen  X l\u00f6schen  Y gro\u00df/klein  Start fertig  B abbrechen"};
    return texts;
}

} // namespace dtvr
