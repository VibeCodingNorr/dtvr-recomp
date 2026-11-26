// SPDX-License-Identifier: GPL-3.0-only
// The [Setup] options of rally.exe (seg5:1FA0 reads them from the profile with
// default 1, Subtitle 0; the options window seg1:ACE7 edits eight of them; round
// 39). Kept in the user's configuration directory, not in the original profile.
// No SDL.
#pragma once

#include "pad_map.hpp"

#include <array>
#include <filesystem>
#include <string>

namespace dtvr {

struct Settings {
    // Order of the options window's switches (names DS:7E0A, values DS:B5D0..B5DE).
    enum Index { Wave, Midi, Voice, TimeOut, Views, Animations, Colorcycle, Joystick, kCount };
    std::array<int, kCount> value{1, 1, 1, 1, 1, 1, 1, 1};
    int subtitle = 0; // DS:B5DF
    int showIntro = 1; // seg5:0174: [Setup] ShowIntro (default 1) plays the intro seg7:0000
    // [Extensions] (recomp only): ComputerExtraCards lets computers also play the
    // cards rally.exe's scores never pick (applied to new games, saved with them).
    bool computerExtraCards = false;
    // [Extensions] AnimationTempo (recomp only): 1 or 2 times the speed of event
    // animations, the vehicles' drive and the slot machine (src/presentation_clock.hpp);
    // a former 4 reads as 2, others as 1.
    int animationTempo = 1;
    // [Extensions] ComputerTempo (recomp only): 1, 2 or 4 while a computer is the current
    // player; it also shortens the reading time of its messages. Others read as 1.
    int computerTempo = 1;
    // [Audio] MasterVolume (recomp only, 0..100): one level for music, effects and voices.
    // rally.exe only switches them on and off and runs both devices at full volume
    // (seg5:2164 / 2194), so their relation stays the original's.
    int masterVolume = 100;
    // [Player] P0-P7 (seg1:02CE ff., GetPrivateProfileInt with the defaults 1, 2, 0 ...):
    // the slots the player setup PLAYER opens with (0 empty, 1 human, 2 computer). It
    // changes the player records directly (seg1:0A8A), and seg1:041E writes all eight
    // back after the window closes, also when it was cancelled. Rebuild decision: values
    // other than 0-2 read as the default (rally.exe would keep any byte).
    std::array<int, 8> players{1, 2, 0, 0, 0, 0, 0, 0};
    // [Default] Rounds (seg8:099B / 0A6A): the stage count the window TEMP offers and keeps.
    int rounds = 99;
    // [Setup] Pause (seg6:0D60, DS:1642, 0-3; keys 1-4 via seg6:0DA0): waits of
    // seg6:0EE0 are multiplied by 1.0 / 1.5 / 2.0 / 3.0 (DS:858E).
    int pause = 0;
    double pauseFactor() const { static const double f[4] = {1.0, 1.5, 2.0, 3.0}; return f[pause & 3]; }
    // [Display] (recomp only, not in rally.exe): fullscreen on the desktop resolution
    // and whole-number scaling of the canvas with black borders.
    bool fullscreen = false;
    bool integerScale = true;
    // Widescreen (recomp only): the canvas takes the display's aspect (up to 21:9) and
    // the map window shows more of the world. Off by default; read at start.
    bool widescreen = false;
    // Text smoothing (recomp only): 0 Original (no anti-aliasing, as GDI on 256 colours),
    // 1 smoothed at canvas size (scaled like the art), 2 smoothed at screen resolution.
    int textSmoothing = 1;
    // [Gamepad] (recomp only): which pad button triggers which pad action, chosen on the
    // recomp menu's Steuerung page (src/pad_map.hpp). Not in rally.exe's [Setup].
    PadMap pad;

    static const char* key(int index);       // profile key ("Wave", "Midi", ...)
    static const char* switchName(int index); // picture name ("fx", "midi", ...)
    static int states(int index) { return index == Joystick ? 3 : 2; } // op_menu/2joy.bmp
    void toggle(int index) { value[size_t(index)] = (value[size_t(index)] + 1) % states(index); }

    std::string encode() const;
    static Settings decode(const std::string& text); // unknown lines and bad values are ignored
};

// The project was called DTVR Recomb: when `path` (a file or folder below a
// "dtvr-recomp" folder) does not exist yet, copy it once from the same place under
// "dtvr-recomb". The old files are never moved or changed. True when nothing had to
// be done or the copy succeeded.
bool adoptFormerName(const std::filesystem::path& path, std::string& error);

// $XDG_CONFIG_HOME/dtvr-recomp/settings.ini, else ~/.config/dtvr-recomp/settings.ini.
std::filesystem::path defaultSettingsFile();
Settings loadSettings(const std::filesystem::path& file);       // defaults when missing
bool saveSettings(const std::filesystem::path& file, const Settings& settings, std::string& error);

} // namespace dtvr
