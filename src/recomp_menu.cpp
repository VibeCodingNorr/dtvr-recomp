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

Item toggle(std::string label, Field field, std::string hint, bool original = false, bool restart = false) {
    Item i;
    i.kind = Kind::Toggle;
    i.label = std::move(label);
    i.field = field;
    i.hint = std::move(hint);
    i.choices = {"Aus", "An"};
    i.original = original;
    i.restart = restart;
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

} // namespace

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
               "Breiteres Kartenfenster im Seitenverhältnis des Bildschirms (bis 21:9).", false, true),
        choice("Schriftglättung", Field::TextSmoothing, {"Original", "Bildgröße", "Bildschirmauflösung"},
               "Original: ohne Kantenglättung wie GDI mit 256 Farben (F12)."),
        toggle("Informationen", Field::Views, "Sehenswürdigkeiten auf der Karte (Option des Originals).", true),
        toggle("Animationen", Field::Animations, "Bewegte Objekte der Karte (Option des Originals).", true),
        toggle("Wellen und Jahreszeiten", Field::Colorcycle,
               "Farbzyklus des Wassers und Jahreszeiten der Karte (Option des Originals).", true),
    }});
    Page audio{"Audio", {
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
    Item volume;
    volume.kind = Kind::Slider;
    volume.label = "Musiklautstärke";
    volume.field = Field::MusicVolume;
    volume.hint = "Lautstärke der Musik; Geräusche und Sprache bleiben unverändert.";
    audio.items.push_back(std::move(volume));
    Item playing = info("Es läuft", "");
    playing.field = Field::MusicPlaying;
    audio.items.push_back(std::move(playing));
    audio.items.push_back(toggle("Soundeffekte", Field::Wave, "Geräusche des Spiels (Option des Originals).", true));
    audio.items.push_back(toggle("Sprachausgabe", Field::Voice, "Stimme des Showmasters (Option des Originals).", true));
    audio.items.push_back(toggle("Untertitel", Field::Subtitle, "Untertitel zur Sprachausgabe ([Setup] Subtitle)."));
    pages_.push_back(std::move(audio));
    pages_.push_back({"Spiel", {
        toggle("Zeitlimit", Field::TimeOut, "Begrenzte Zeit pro Zug (Option des Originals).", true),
        choice("Wartezeit der Meldungen", Field::Pause, {"Normal", "1,5-fach", "2-fach", "3-fach"},
               "Wie lange Meldungen der Computer stehen bleiben (Tasten 1–4 des Originals)."),
        toggle("Intro beim Start", Field::ShowIntro, "Spielt das Intro vor dem Titelbild."),
        toggle("Computer spielen alle Karten", Field::ExtraCards,
               "Erweiterung: auch Karten, die rally.exe nie wählt. Gilt für neue Spiele."),
    }});
    pages_.push_back({"Steuerung", {
        info("Menü / Pause", "Esc"),
        info("Originales Spielmenü", "Rücktaste"),
        info("Würfeln, bestätigen", "Enter / Leertaste"),
        info("Fahren", "Pfeiltasten"),
        info("Audio und Klangset", "F8"),
        info("Vollbild", "F11 / Alt+Enter"),
        info("Schriftglättung", "F12"),
        info("Gamepad", "Steuerkreuz, A = Enter, B = Esc"),
    }});
}

void RecompMenu::open(const Settings& settings, bool inGame) {
    inGame_ = inGame;
    build(inGame);
    settings_ = opened_ = settings;
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
    music.volume = std::clamp(music.volume, 0, 100);
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
    case Field::MusicVolume: return music_.volume;
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
    case Field::MusicPlaying: case Field::MusicNote: case Field::None: break;
    }
    return 0;
}

void RecompMenu::setFieldValue(Field f, int v) {
    switch (f) {
    case Field::Fullscreen: settings_.fullscreen = v != 0; break;
    case Field::IntegerScale: settings_.integerScale = v != 0; break;
    case Field::Widescreen:
        settings_.widescreen = v != 0;
        restartPending_ = settings_.widescreen != opened_.widescreen;
        break;
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
    case Field::MusicPack: music_.pack = v; break;
    case Field::MusicVolume: music_.volume = std::clamp(v, 0, 100); break;
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
    }
    return Action::None;
}

} // namespace dtvr
