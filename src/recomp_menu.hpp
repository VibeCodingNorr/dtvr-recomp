// SPDX-License-Identifier: GPL-3.0-only
// The recomp menu (Escape): pause, settings and help of the native port, not part
// of rally.exe (docs/pause-menu.md). Pages on the left, their entries on the right;
// keyboard, controller (through its keys) and mouse share these commands. No SDL.
#pragma once

#include "settings.hpp"

#include <string>
#include <vector>

namespace dtvr {

class RecompMenu {
public:
    // Changed: settings(); MusicChanged: music() (sound set or volume).
    enum class Action { None, Resume, Changed, MusicChanged, SaveGame, LoadGame, OriginalMenu, Quit };
    enum class Input { Up, Down, Left, Right, Accept, Back };
    enum class Kind { Command, Toggle, Choice, Slider, Info };
    // Settings the entries edit (Settings fields; Original = a switch of the OPTION window)
    // and the local music preferences (music.ini).
    enum class Field { None, Fullscreen, IntegerScale, Widescreen, TextSmoothing, Views, Animations, Colorcycle,
                       Wave, Midi, Voice, Subtitle, TimeOut, Pause, ShowIntro, ExtraCards, AnimationTempo, ComputerTempo,
                       MusicPack, MusicVolume, MusicPlaying, MusicNote };

    // The installed Ogg sound sets (renderings of the original MIDI music or CD audio)
    // and what plays; the caller keeps it current while the menu is open.
    struct Music {
        std::vector<std::string> ids, names; // complete sets only
        int pack = -1;                       // index of the chosen set, -1 none
        int volume = 70;                     // percent, 0..100 in steps of 5
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
        std::string value;                // Kind::Info: right column
    };
    struct Page { std::string title; std::vector<Item> items; };

    explicit RecompMenu(bool inGame = true);
    void open(const Settings& settings, bool inGame);
    // Opens on a page with the focus on its first entry (F8: Audio).
    void openAt(const Settings& settings, bool inGame, const std::string& page);
    void setMusic(Music music); // rebuilds the Audio page when the sets change
    const Music& music() const { return music_; }
    void close() { open_ = false; }
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

private:
    int fieldValue(Field f) const;
    void setFieldValue(Field f, int v);
    int fieldStates(const Item& item) const;
    bool selectable(const Item& item) const { return item.kind != Kind::Info; }
    static bool musicField(Field f) { return f == Field::MusicPack || f == Field::MusicVolume; }
    void build(bool inGame);
    void moveRow(int delta);

    std::vector<Page> pages_;
    Settings settings_;
    Music music_;
    int page_ = 0, row_ = 0;
    bool inItems_ = false, open_ = false, inGame_ = true;
};

} // namespace dtvr
