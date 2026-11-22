// SPDX-License-Identifier: GPL-3.0-only
// The [Setup] options of rally.exe (seg5:1FA0 reads them from the profile with
// default 1, Subtitle 0; the options window seg1:ACE7 edits eight of them; round
// 39). Kept in the user's configuration directory, not in the original profile.
// No SDL.
#pragma once

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
    // [Setup] Pause (seg6:0D60, DS:1642, 0-3; keys 1-4 via seg6:0DA0): waits of
    // seg6:0EE0 are multiplied by 1.0 / 1.5 / 2.0 / 3.0 (DS:858E).
    int pause = 0;
    double pauseFactor() const { static const double f[4] = {1.0, 1.5, 2.0, 3.0}; return f[pause & 3]; }
    // [Display] (recomb only, not in rally.exe): fullscreen on the desktop resolution
    // and whole-number scaling of the canvas with black borders.
    bool fullscreen = false;
    bool integerScale = true;

    static const char* key(int index);       // profile key ("Wave", "Midi", ...)
    static const char* switchName(int index); // picture name ("fx", "midi", ...)
    static int states(int index) { return index == Joystick ? 3 : 2; } // op_menu/2joy.bmp
    void toggle(int index) { value[size_t(index)] = (value[size_t(index)] + 1) % states(index); }

    std::string encode() const;
    static Settings decode(const std::string& text); // unknown lines and bad values are ignored
};

// $XDG_CONFIG_HOME/dtvr-recomb/settings.ini, else ~/.config/dtvr-recomb/settings.ini.
std::filesystem::path defaultSettingsFile();
Settings loadSettings(const std::filesystem::path& file);       // defaults when missing
bool saveSettings(const std::filesystem::path& file, const Settings& settings, std::string& error);

} // namespace dtvr
