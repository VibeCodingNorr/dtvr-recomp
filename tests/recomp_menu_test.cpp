// SPDX-License-Identifier: GPL-3.0-only
// The recomp menu (Escape): navigation, settings edits and commands.
#include "recomp_menu.hpp"

#include <cstdio>

using namespace dtvr;
using Action = RecompMenu::Action;
using Input = RecompMenu::Input;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static int pageIndex(const RecompMenu& m, const char* title) {
    for (size_t i = 0; i < m.pages().size(); ++i)
        if (m.pages()[i].title == title) return int(i);
    return -1;
}

int main() {
    RecompMenu m;
    CHECK(!m.isOpen() && m.input(Input::Accept) == Action::None);
    Settings base;
    m.open(base, true);
    // Opens on Weiterspielen; Accept resumes and closes.
    CHECK(m.isOpen() && m.page() == 0 && m.row() == 0 && m.inItems());
    CHECK(m.input(Input::Accept) == Action::Resume && !m.isOpen());

    // Commands of the pause page in a game; without a game no save, load or original menu.
    m.open(base, true);
    CHECK(m.input(Input::Down) == Action::None && m.input(Input::Accept) == Action::SaveGame && m.isOpen());
    CHECK(m.input(Input::Down) == Action::None && m.input(Input::Accept) == Action::LoadGame);
    CHECK(m.input(Input::Down) == Action::None && m.input(Input::Accept) == Action::OriginalMenu);
    CHECK(m.input(Input::Up) == Action::None && m.input(Input::Up) == Action::None && m.input(Input::Up) == Action::None
          && m.input(Input::Up) == Action::None && m.input(Input::Accept) == Action::Quit); // wraps to the last entry
    m.open(base, false);
    CHECK(m.pages()[0].items.size() == 2 && m.input(Input::Down) == Action::None && m.input(Input::Accept) == Action::Quit);

    // Back from the pause page resumes; from another page it returns to the page list first.
    m.open(base, true);
    CHECK(m.input(Input::Back) == Action::Resume && !m.isOpen());
    m.open(base, true);
    const int graphics = pageIndex(m, "Grafik");
    CHECK(graphics > 0);
    m.focusPage(graphics);
    CHECK(!m.inItems() && m.input(Input::Right) == Action::None && m.inItems() && m.row() == 0);
    // Vollbild: Accept toggles, Left / Right step; the menu keeps its own copy.
    CHECK(m.input(Input::Accept) == Action::Changed && m.settings().fullscreen && !base.fullscreen);
    CHECK(m.input(Input::Right) == Action::Changed && !m.settings().fullscreen);
    CHECK(m.input(Input::Back) == Action::None && m.isOpen() && !m.inItems());
    CHECK(m.input(Input::Back) == Action::Resume && !m.isOpen());

    // Breitbild switches at once (the game window relayouts).
    m.open(base, true);
    m.focusPage(graphics);
    m.focusItem(2);
    CHECK(m.pages()[size_t(graphics)].items[2].field == RecompMenu::Field::Widescreen);
    CHECK(m.activate() == Action::Changed && m.settings().widescreen);
    CHECK(m.step(-1) == Action::Changed && !m.settings().widescreen);
    // Schriftglättung cycles 0..2 in both directions.
    m.focusItem(3);
    CHECK(m.settings().textSmoothing == 1 && m.step(1) == Action::Changed && m.settings().textSmoothing == 2);
    CHECK(m.step(1) == Action::Changed && m.settings().textSmoothing == 0 && m.valueText(m.pages()[size_t(graphics)].items[3]) == "Original");
    CHECK(m.step(-1) == Action::Changed && m.settings().textSmoothing == 2);
    // Audio: the master volume first (steps of 5, no wrap-around), then the original
    // MIDI switch; rally.exe only switches music, effects and voices on and off.
    const int audio = pageIndex(m, "Audio");
    m.focusPage(audio);
    const auto& items = m.pages()[size_t(audio)].items;
    CHECK(items[0].kind == RecompMenu::Kind::Slider && items[0].field == RecompMenu::Field::MasterVolume);
    m.focusItem(0);
    CHECK(m.settings().masterVolume == 100 && m.step(1) == Action::None && m.valueText(items[0]) == "100 %");
    CHECK(m.input(Input::Left) == Action::Changed && m.settings().masterVolume == 95);
    CHECK(m.setSlider(42) == Action::Changed && m.settings().masterVolume == 40 && m.setSlider(-3) == Action::Changed
          && m.settings().masterVolume == 0 && m.setSlider(1) == Action::None);
    m.focusItem(1);
    CHECK(items[1].original && m.activate() == Action::Changed && m.settings().value[Settings::Midi] == 0);
    // Without sound sets the Klangset row is information only.
    m.focusItem(2);
    CHECK(m.row() == 1 && m.valueText(items[2]) == "nicht installiert");
    // With sets: the choice of the set.
    RecompMenu::Music music;
    music.ids = {"gm", "cd"};
    music.names = {"General MIDI", "CD-Audio"};
    music.pack = 0;
    music.playing = "a1";
    m.setMusic(music);
    const auto& rebuilt = m.pages()[size_t(audio)].items;
    CHECK(rebuilt[2].kind == RecompMenu::Kind::Choice);
    m.focusItem(2);
    CHECK(m.valueText(rebuilt[2]) == "General MIDI" && m.step(1) == Action::MusicChanged && m.music().pack == 1);
    CHECK(m.step(1) == Action::MusicChanged && m.music().pack == 0 && m.settings().masterVolume == 0);
    CHECK(m.valueText(rebuilt[3]) == "Stück a1");
    // Updating what plays keeps the focus; F8 opens the Audio page.
    music.playing = "b2";
    m.setMusic(music);
    CHECK(m.row() == 2 && m.valueText(m.pages()[size_t(audio)].items[3]) == "Stück b2");
    m.openAt(base, false, "Audio");
    CHECK(m.isOpen() && m.page() == audio && m.inItems() && m.row() == 0);
    const int game = pageIndex(m, "Spiel");
    m.focusPage(game);
    m.focusItem(1); // Computerzüge: 1x, 2x, 4x
    CHECK(m.settings().computerTempo == 1 && m.step(1) == Action::Changed && m.settings().computerTempo == 2);
    CHECK(m.step(1) == Action::Changed && m.settings().computerTempo == 4 && m.step(1) == Action::Changed
          && m.settings().computerTempo == 1 && m.step(-1) == Action::Changed && m.settings().computerTempo == 4);
    m.focusItem(2); // Animationstempo: only 1x and 2x
    CHECK(m.settings().animationTempo == 1 && m.step(1) == Action::Changed && m.settings().animationTempo == 2);
    CHECK(m.step(1) == Action::Changed && m.settings().animationTempo == 1);
    m.focusItem(3); // Wartezeit: four steps, separate from both tempos
    CHECK(m.step(-1) == Action::Changed && m.settings().pause == 3 && m.step(1) == Action::Changed && m.settings().pause == 0);
    CHECK(m.settings().computerTempo == 4 && m.settings().animationTempo == 1);

    // The controls page has only info rows: no entry takes the focus.
    const int controls = pageIndex(m, "Steuerung");
    m.focusPage(controls);
    m.focusItem(0);
    CHECK(!m.inItems() && m.step(1) == Action::None && m.activate() == Action::None && !m.inItems());
    // It is a table (action, keyboard, pad) that fits the panel; every row names both
    // the keys and the pad, and the pad column covers every button of the pad layer.
    const RecompMenu::Page& table = m.pages()[size_t(controls)];
    CHECK(table.table() && table.columns.size() == 3 && !table.hint.empty());
    CHECK(!table.items.empty() && int(table.items.size()) <= RecompMenu::kTableRows);
    CHECK(m.valueText(table.items[0]) == "Pfeiltasten");
    const auto padRow = [&](const std::string& pad) {
        for (const RecompMenu::Item& i : table.items)
            if (i.pad == pad) return &i;
        return static_cast<const RecompMenu::Item*>(nullptr);
    };
    bool complete = true;
    for (const RecompMenu::Item& i : table.items)
        complete = complete && i.kind == RecompMenu::Kind::Info && !i.label.empty() && !i.value.empty() && !i.pad.empty();
    CHECK(complete);
    for (const char* button : {"A", "B", "X", "Y", "Start", "Ansicht", "LB / RB", "rechter Stick"})
        CHECK(padRow(button) != nullptr);
    CHECK(padRow("B") && padRow("B")->value == "Esc");
    CHECK(padRow("X") && padRow("X")->value == "Rücktaste");
    CHECK(padRow("LB / RB") && padRow("LB / RB")->value == "Umschalt+Tab / Tab");
    CHECK(padRow("Ansicht") && padRow("Ansicht")->value.find("Rücktaste") == 0);
    // The other pages are no tables.
    CHECK(!m.pages()[0].table() && controlsPage().title == "Steuerung");
    // Page list wraps.
    m.focusPage(0);
    CHECK(m.input(Input::Up) == Action::None && m.page() == int(m.pages().size()) - 1);
    m.focusPage(99); // ignored
    CHECK(m.page() == int(m.pages().size()) - 1);

    // LB / RB (Shift+Tab / Tab): the neighbour page, wrapping; in the entries they stay in
    // the entries (first selectable row), on the page list on the list.
    m.open(base, true);
    CHECK(m.inItems() && m.page() == 0);
    CHECK(m.input(Input::NextPage) == Action::None && m.page() == pageIndex(m, "Grafik") && m.inItems() && m.row() == 0);
    CHECK(m.input(Input::PrevPage) == Action::None && m.page() == 0 && m.inItems());
    CHECK(m.input(Input::PrevPage) == Action::None && m.page() == controls && !m.inItems()); // info rows only
    CHECK(m.input(Input::NextPage) == Action::None && m.page() == 0 && !m.inItems());       // from the list: list
    CHECK(m.input(Input::NextPage) == Action::None && m.page() == 1 && !m.inItems());
    CHECK(m.settings().fullscreen == base.fullscreen); // switching pages changes nothing

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("recomp_menu_test: OK");
    return failures != 0;
}
