// SPDX-License-Identifier: GPL-3.0-only
// The options window OPTION of rally.exe (seg1:ACE7; round 39): butt/<n>/<lang>/
// menu.bmp, eight switches op_menu/<value><name>.bmp at the places of the player
// setup's drivers (switch i at slot i, table DS:1BD4) and star / stop of pl_menu.
#pragma once

#include "layout.hpp"
#include "sb_button.hpp"
#include "sdl_assets.hpp"
#include "settings.hpp"
#include "text.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class OptionsView {
public:
    OptionsView(SDL_Renderer* renderer, const std::filesystem::path& root, char language, const ScreenLayout& layout,
                TextRenderer& text);
    // seg1:ADEE copies the values to DS:9478, but nothing reads the copy back.
    void open(const Settings& settings);
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    const Settings& settings() const { return settings_; }
    // F12 changes the text smoothing everywhere (game window) and updates the copy.
    void setTextSmoothing(int mode) { settings_.textSmoothing = mode; }

    // Toggled: a switch changed (seg1:B2E2 ff. apply it at once); Close: star, stop,
    // F10 and the cancel of an SBBUTTON all run seg1:B539, which keeps the values.
    // RecompMenu: the recomp addition between stop and star (not in rally.exe) was
    // clicked: further settings (graphics, sound sets, music volume) in the recomp menu.
    enum class Result { None, Toggled, Close, RecompMenu };
    Result handle(const SDL_Event& event);
    // The SBBUTTON sound of the last handled event for channel 2, -1 none.
    int takeSound() { const int s = sound_; sound_ = -1; return s; }
    void draw();

private:
    SDL_Texture* bitmap(const std::filesystem::path& relative);
    SDL_Rect extraRect() const; // the recomp label between stop and star

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    char language_; // the lan.inf letter DS:A0F0 (seg5:028C)
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    TextRenderer& text_;
    SetupGeometry geometry_;
    std::map<std::string, Texture> cache_;
    Settings settings_;
    bool open_ = false;
    SbButtonGroup buttons_; // ids: switch index, kStar, kStop
    int sound_ = -1;
};

} // namespace dtvr
