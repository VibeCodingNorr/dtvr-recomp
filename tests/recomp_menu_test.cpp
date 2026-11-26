// SPDX-License-Identifier: GPL-3.0-only
// The recomp menu (Escape): navigation, settings edits and commands.
#include "recomp_menu.hpp"

#include <cstdio>
#include <string>

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

    // The controls page: a table (action, keyboard, pad) whose pad actions are Binding
    // rows; it scrolls (more rows than RecompMenu::kTableRows).
    const int controls = pageIndex(m, "Steuerung");
    m.focusPage(controls);
    CHECK(m.activate() == Action::None && m.inItems() && m.row() == 0);
    const RecompMenu::Page& table = m.pages()[size_t(controls)];
    CHECK(table.table() && table.columns.size() == 3 && !table.hint.empty());
    CHECK(m.valueText(table.items[0]) == "Pfeil hoch" && m.padText(table.items[0]) == "Kreuz hoch");
    const auto padRow = [&](const std::string& pad) {
        for (const RecompMenu::Item& i : table.items)
            if (m.padText(i) == pad) return &i;
        return static_cast<const RecompMenu::Item*>(nullptr);
    };
    // Every pad action has exactly one Binding row, and every row names both columns.
    int bindings = 0;
    bool complete = true;
    for (const RecompMenu::Item& i : table.items) {
        bindings += i.kind == RecompMenu::Kind::Binding;
        if (i.kind != RecompMenu::Kind::Command)
            complete = complete && !i.label.empty() && !i.value.empty() && !m.padText(i).empty();
    }
    CHECK(complete && bindings == kPadActionCount);
    for (const char* button : {"A", "B", "X", "Y", "Start", "Ansicht", "LB", "RB", "Links / rechts"})
        CHECK(padRow(button) != nullptr);
    CHECK(padRow("Links / rechts") && padRow("Links / rechts")->value == "Pfeiltasten, 2/4/6/8");
    CHECK(padRow("B") && padRow("B")->value == "Esc");
    CHECK(padRow("X") && padRow("X")->value == "Rücktaste");
    CHECK(padRow("LB") && padRow("LB")->value == "Umschalt+Tab" && padRow("RB")->value == "Tab");
    CHECK(padRow("Ansicht") && padRow("Ansicht")->value.find("Rücktaste") == 0);
    CHECK(padRow("A: Bildschirmtastatur") != nullptr);
    const auto rowOf = [&](PadAction a) {
        for (size_t i = 0; i < table.items.size(); ++i)
            if (table.items[i].kind == RecompMenu::Kind::Binding && table.items[i].padAction == a) return int(i);
        return -1;
    };

    // Remapping: Accept waits for a button; the pad's button is bound.
    m.focusItem(rowOf(PadAction::Erase));
    CHECK(m.activate() == Action::None && m.capturing() && m.captureAction() == PadAction::Erase);
    CHECK(m.padText(table.items[size_t(rowOf(PadAction::Erase))]) == "Taste drücken …");
    // Meanwhile the keys do nothing but Escape / Backspace (Back), which cancel.
    CHECK(m.input(Input::Down) == Action::None && m.row() == rowOf(PadAction::Erase) && m.capturing());
    CHECK(m.input(Input::Back) == Action::None && !m.capturing() && m.isOpen() && m.settings().pad.isDefault());
    CHECK(!m.padNote().empty());
    // A free button (LT): bound, Changed.
    CHECK(m.activate() == Action::None && m.capture(int(PadPhysical::LT)) == Action::Changed && !m.capturing());
    CHECK(m.settings().pad.of(PadAction::Erase) == PadPhysical::LT && m.padText(m.pages()[size_t(controls)].items[size_t(rowOf(PadAction::Erase))]) == "LT");
    // A taken button: the two actions swap, and the note names the other one.
    m.focusItem(rowOf(PadAction::Confirm));
    m.activate();
    CHECK(m.capture(int(PadPhysical::B)) == Action::Changed);
    CHECK(m.settings().pad.of(PadAction::Confirm) == PadPhysical::B && m.settings().pad.of(PadAction::Cancel) == PadPhysical::A);
    CHECK(m.padNote().find("Getauscht") != std::string::npos && m.padNote().find("Abbrechen") != std::string::npos);
    CHECK(m.settings().pad.valid());
    // The info rows and the key line follow the map.
    CHECK(padRow("B: Bildschirmtastatur") != nullptr);
    CHECK(m.keyLine().find("Enter/B") != std::string::npos && m.keyLine().find("Esc/A") != std::string::npos);
    CHECK(keyboardHelp(m.settings().pad).find("B tippen") == 0);
    // The same button again: unchanged.
    m.activate();
    CHECK(m.capture(int(PadPhysical::B)) == Action::None && m.settings().pad.of(PadAction::Confirm) == PadPhysical::B);
    // Invalid physical: ignored, still waiting; capture without waiting does nothing.
    m.activate();
    CHECK(m.capture(99) == Action::None && m.capturing());
    // Timeout: the clock starts on the first call.
    CHECK(!m.expireCapture(1000) && m.captureSecondsLeft(1000) == 8);
    CHECK(!m.expireCapture(1000 + RecompMenu::kCaptureMs - 1) && m.capturing());
    CHECK(m.expireCapture(1000 + RecompMenu::kCaptureMs) && !m.capturing());
    CHECK(m.capture(int(PadPhysical::X)) == Action::None);
    // Closing ends a wait.
    m.activate();
    m.close();
    CHECK(!m.capturing());
    m.open(m.settings(), true);
    m.focusPage(controls);
    m.activate();
    // The sticks choice and the reset.
    const RecompMenu::Page& table2 = m.pages()[size_t(controls)];
    int sticksRow = -1, resetRow = -1;
    for (size_t i = 0; i < table2.items.size(); ++i) {
        if (table2.items[i].field == RecompMenu::Field::PadSticks) sticksRow = int(i);
        if (table2.items[i].field == RecompMenu::Field::PadReset) resetRow = int(i);
    }
    CHECK(sticksRow >= 0 && resetRow > sticksRow && resetRow == int(table2.items.size()) - 1);
    CHECK(int(table2.items.size()) > RecompMenu::kTableRows); // the view scrolls
    m.focusItem(sticksRow);
    CHECK(m.step(1) == Action::Changed && m.settings().pad.swapSticks && m.padText(table2.items[size_t(sticksRow)]) == "Rechts / links");
    m.focusItem(resetRow);
    CHECK(m.activate() == Action::Changed && m.settings().pad.isDefault() && !m.padNote().empty());
    CHECK(m.activate() == Action::None); // already the defaults
    // Keyboard navigation reaches every selectable row (the reset too) and wraps.
    m.focusItem(0);
    int selectable = 0;
    for (const RecompMenu::Item& i : table2.items) selectable += i.kind != RecompMenu::Kind::Info;
    for (int i = 0; i + 1 < selectable; ++i) m.input(Input::Down);
    CHECK(m.row() == resetRow);
    CHECK(m.input(Input::Down) == Action::None && m.row() == 0);
    CHECK(controlsPage().title == "Steuerung");
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
    CHECK(m.input(Input::PrevPage) == Action::None && m.page() == controls && m.inItems() && m.row() == 0);
    m.input(Input::Back); // to the page list
    CHECK(m.input(Input::NextPage) == Action::None && m.page() == 0 && !m.inItems());       // from the list: list
    CHECK(m.input(Input::NextPage) == Action::None && m.page() == 1 && !m.inItems());
    CHECK(m.settings().fullscreen == base.fullscreen); // switching pages changes nothing

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("recomp_menu_test: OK");
    return failures != 0;
}
