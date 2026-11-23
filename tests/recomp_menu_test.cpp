// SPDX-License-Identifier: GPL-3.0-only
// The recomb menu (Escape): navigation, settings edits and commands.
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

    // Breitbild needs a restart; turning it back clears the note.
    m.open(base, true);
    m.focusPage(graphics);
    m.focusItem(2);
    CHECK(m.pages()[size_t(graphics)].items[2].field == RecompMenu::Field::Widescreen);
    CHECK(m.activate() == Action::Changed && m.settings().widescreen && m.restartPending());
    CHECK(m.step(-1) == Action::Changed && !m.settings().widescreen && !m.restartPending());
    // Schriftglättung cycles 0..2 in both directions.
    m.focusItem(3);
    CHECK(m.settings().textSmoothing == 1 && m.step(1) == Action::Changed && m.settings().textSmoothing == 2);
    CHECK(m.step(1) == Action::Changed && m.settings().textSmoothing == 0 && m.valueText(m.pages()[size_t(graphics)].items[3]) == "Original");
    CHECK(m.step(-1) == Action::Changed && m.settings().textSmoothing == 2);
    // Original switches edit the [Setup] values; Musik is the original MIDI switch.
    const int audio = pageIndex(m, "Audio");
    m.focusPage(audio);
    m.focusItem(0);
    CHECK(m.pages()[size_t(audio)].items[0].original && m.activate() == Action::Changed && m.settings().value[Settings::Midi] == 0);
    // Without sound sets the Klangset row is information only.
    m.focusItem(1);
    CHECK(m.row() == 0 && m.valueText(m.pages()[size_t(audio)].items[1]) == "nicht installiert");
    // With sets: choice of the set and the volume slider (steps of 5, no wrap-around).
    RecompMenu::Music music;
    music.ids = {"gm", "cd"};
    music.names = {"General MIDI", "CD-Audio"};
    music.pack = 0;
    music.volume = 95;
    music.playing = "a1";
    m.setMusic(music);
    const auto& items = m.pages()[size_t(audio)].items;
    CHECK(items[1].kind == RecompMenu::Kind::Choice && items[2].kind == RecompMenu::Kind::Slider);
    m.focusItem(1);
    CHECK(m.valueText(items[1]) == "General MIDI" && m.step(1) == Action::MusicChanged && m.music().pack == 1);
    CHECK(m.step(1) == Action::MusicChanged && m.music().pack == 0 && m.settings().value[Settings::Midi] == 0);
    m.focusItem(2);
    CHECK(m.step(1) == Action::MusicChanged && m.music().volume == 100 && m.step(1) == Action::None);
    CHECK(m.input(Input::Left) == Action::MusicChanged && m.music().volume == 95 && m.valueText(items[2]) == "95 %");
    CHECK(m.setSlider(42) == Action::MusicChanged && m.music().volume == 40 && m.setSlider(-3) == Action::MusicChanged
          && m.music().volume == 0 && m.setSlider(1) == Action::None);
    CHECK(m.valueText(items[3]) == "Stück a1");
    // Updating what plays keeps the focus; F8 opens the Audio page.
    music.playing = "b2";
    m.setMusic(music);
    CHECK(m.row() == 2 && m.valueText(m.pages()[size_t(audio)].items[3]) == "Stück b2");
    m.openAt(base, false, "Audio");
    CHECK(m.isOpen() && m.page() == audio && m.inItems() && m.row() == 0);
    const int game = pageIndex(m, "Spiel");
    m.focusPage(game);
    m.focusItem(1); // Wartezeit: four steps
    CHECK(m.step(-1) == Action::Changed && m.settings().pause == 3 && m.step(1) == Action::Changed && m.settings().pause == 0);

    // The controls page has only info rows: no entry takes the focus.
    const int controls = pageIndex(m, "Steuerung");
    m.focusPage(controls);
    m.focusItem(0);
    CHECK(!m.inItems() && m.step(1) == Action::None && m.activate() == Action::None && !m.inItems());
    CHECK(m.valueText(m.pages()[size_t(controls)].items[0]) == "Esc");
    // Page list wraps.
    m.focusPage(0);
    CHECK(m.input(Input::Up) == Action::None && m.page() == int(m.pages().size()) - 1);
    m.focusPage(99); // ignored
    CHECK(m.page() == int(m.pages().size()) - 1);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("recomp_menu_test: OK");
    return failures != 0;
}
