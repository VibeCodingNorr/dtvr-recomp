// SPDX-License-Identifier: GPL-3.0-only
// The recomp menu (Escape): pause, settings and help of the native port, not part
// of rally.exe (docs/pause-menu.md). Pages on the left, their entries on the right;
// keyboard, controller (through its keys) and mouse share these commands. No SDL.
#pragma once

#include "settings.hpp"

#include <cstdint>
#include <utility>
#include <string>
#include <vector>

namespace dtvr {

class RecompMenu {
public:
    // Changed: settings(); MusicChanged: music() (sound set or volume).
    enum class Action { None, Resume, Changed, MusicChanged, SaveGame, LoadGame, OriginalMenu, Quit };
    enum class Input { Up, Down, Left, Right, Accept, Back, PrevPage, NextPage };
    // Binding: a pad action of the Steuerung page; Accept waits for its new button.
    enum class Kind { Command, Toggle, Choice, Slider, Info, Binding };
    // Settings the entries edit (Settings fields; Original = a switch of the OPTION window)
    // and the local music preferences (music.ini).
    enum class Field { None, Fullscreen, IntegerScale, Widescreen, TextSmoothing, Views, Animations, Colorcycle,
                       Wave, Midi, Voice, Subtitle, TimeOut, Pause, ShowIntro, ExtraCards, AnimationTempo, ComputerTempo,
                       MusicPack, MasterVolume, MusicPlaying, MusicNote, PadSticks, PadReset };

    // The installed Ogg sound sets (renderings of the original MIDI music or CD audio)
    // and what plays; the caller keeps it current while the menu is open.
    struct Music {
        std::vector<std::string> ids, names; // complete sets only
        int pack = -1;                       // index of the chosen set, -1 none
        std::string playing;                 // cue of the current piece
        std::string note;                    // pending switch, missing sets, errors
    };
    static constexpr int kVolumeStep = 5;

    struct Item {
        Kind kind = Kind::Info;
        std::string label, hint;
        Field field = Field::None;
        Action command = Action::None; // Kind::Command
        std::vector<std::string> choices; // Kind::Choice (Toggle: Aus / An)
        bool original = false;            // a switch of the original OPTION window
        std::string value;                // Kind::Info: right column (a table page: the keyboard)
        std::string pad;                  // a table page: the game controller column (Info rows)
        PadAction padAction = PadAction::kCount; // Kind::Binding
    };
    // A table page (Steuerung) shows its info rows compactly under the headings
    // `columns` (action, keyboard, pad) with `hint` in the footer, since no row takes
    // the focus.
    struct Page {
        std::string title;
        std::vector<Item> items;
        std::vector<std::string> columns{};
        std::string hint{};
        bool table() const { return !columns.empty(); }
    };
    static constexpr int kTableRows = 12; // rows a table page shows at once (it scrolls with the focus)
    static constexpr std::uint32_t kCaptureMs = 8000; // waiting for a pad button ends by itself

    explicit RecompMenu(bool inGame = true);
    void open(const Settings& settings, bool inGame);
    // Opens on a page with the focus on its first entry (F8: Audio).
    void openAt(const Settings& settings, bool inGame, const std::string& page);
    void setMusic(Music music); // rebuilds the Audio page when the sets change
    const Music& music() const { return music_; }
    void close() { open_ = false; capture_ = PadAction::kCount; }
    // Opens again where it was hidden (page, entry) with the current settings: after a
    // dialog of a Pause command (CommandPlan::returnAfter) has closed.
    void resume(const Settings& settings);
    // A line for the footer of the Pause page (a refused command); cleared by moving.
    void setNote(std::string note) { note_ = std::move(note); }
    const std::string& note() const { return note_; }
    bool isOpen() const { return open_; }

    Action input(Input in);
    // Mouse: focus an entry (page -1: the page list row `row`), activate with a click.
    void focusPage(int page);
    void focusItem(int row);
    Action activate();          // Accept on the focused entry
    Action step(int delta);     // Left / Right on the focused entry
    Action setSlider(int percent); // mouse on the focused slider's bar

    const Settings& settings() const { return settings_; }
    const std::vector<Page>& pages() const { return pages_; }
    int page() const { return page_; }
    int row() const { return row_; }
    bool inItems() const { return inItems_; }
    // Value shown for an entry ("An", "Bildgröße", ...); empty for commands.
    std::string valueText(const Item& item) const;
    // A table page's pad column: the bound button, the stick choice or the info text.
    std::string padText(const Item& item) const;
    // The footer's key line with the pad buttons in use.
    std::string keyLine() const;

    // Remapping (Steuerung page): Accept on a Binding row waits for a pad button.
    bool capturing() const { return capture_ != PadAction::kCount; }
    PadAction captureAction() const { return capture_; }
    // The pressed physical button: bound to the waiting action (a taken button is
    // swapped, padNote() says with which action). Changed when the map changed.
    Action capture(int physical);
    void cancelCapture(std::string note);
    // Starts the wait's clock on the first call; ends the wait after kCaptureMs.
    bool expireCapture(std::uint32_t nowMs);
    int captureSecondsLeft(std::uint32_t nowMs) const;
    // The result of the last remapping (shown in the footer of the Steuerung page).
    const std::string& padNote() const { return padNote_; }

private:
    int fieldValue(Field f) const;
    void setFieldValue(Field f, int v);
    int fieldStates(const Item& item) const;
    bool selectable(const Item& item) const { return item.kind != Kind::Info; }
    static bool musicField(Field f) { return f == Field::MusicPack; }
    void build(bool inGame);
    void moveRow(int delta);
    void refreshControls(); // the Steuerung page shows the current map

    std::vector<Page> pages_;
    Settings settings_;
    Music music_;
    int page_ = 0, row_ = 0;
    bool inItems_ = false, open_ = false, inGame_ = true;
    PadAction capture_ = PadAction::kCount;
    bool captureStamped_ = false;
    std::uint32_t captureSince_ = 0;
    std::string padNote_;
    std::string note_;
};

// What the game window does with a command of the Pause page. Every press has one
// consumer and the dialog is the only window in front: like MAIN1's 1062 (seg4:07B8:
// seg5:52AC closes the menu DS:9EDE, then seg8:B157(1)) the original menu that was
// open under the recomp menu (ONFIELD, MAIN1, RIGHTBTN) closes before the dialog.
struct CommandPlan {
    int originalCommand = 0;  // WM_COMMAND id run instead (1062 Spiel speichern, 1021 Spiel laden), 0 none
    bool hide = false;        // the recomp menu closes for the dialog / menu
    bool returnAfter = false; // it opens again when that dialog closes (not after a load)
    bool refused = false;     // another window of the game is open: the menu stays, note()
};
// `gameWindowOpen`: a window of rally.exe other than its menus has the input (message,
// card window, town dialog, OPTION, ...); rally.exe saves only from MAIN1 (1062).
CommandPlan planCommand(RecompMenu::Action action, bool gameWindowOpen);

// The Steuerung page: keyboard and pad side by side with the pad map's buttons
// (recomp texts, not in the language file); its Binding rows remap the pad.
RecompMenu::Page controlsPage(const PadMap& map = PadMap{});
// Short German name of a pad action (remapping notes).
const char* padActionLabel(PadAction a);

// Recomp-only labels of the on-screen keyboard (src/osk_view.cpp) and its help line;
// like the menu's texts they are not in the language file.
struct KeyboardTexts { std::string shift, erase, space, cancel, done, help; };
const KeyboardTexts& keyboardTexts();
// The on-screen keyboard's help with the pad buttons in use: two lines ("\n"), the
// buttons of the original layer, then the keyboard's own shortcuts.
std::string keyboardHelp(const PadMap& map);
// Its pad shortcuts with the buttons in use: the caret (PrevPage / NextPage) and the
// triggers that no action has (RT space, LT focus to Fertig), joined by `separator`
// (the help's second line).
std::string keyboardShortcuts(const PadMap& map, const char* separator);

} // namespace dtvr
