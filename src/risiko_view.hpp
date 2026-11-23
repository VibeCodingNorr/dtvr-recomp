// SPDX-License-Identifier: GPL-3.0-only
// The Risiko window of rally.exe (seg7:674D, procedure seg7:6BF0; round 58): a
// 0xF6 x 0x9E child window showing a 230 x 142 frame of sbs/risiko.sbs chosen by
// the light DS:9C92 (r-light0/2/3/4, otherwise r-normal); the pressed CHANCE /
// STOP buttons are copied from r-button. Evidence in docs/formats.md "Risiko".
#pragma once

#include "window_border.hpp"

#include "layout.hpp"
#include "sdl_assets.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class RisikoView {
public:
    void setSprites(SpriteLookup sprites) { borderSprites_ = std::move(sprites); } // B1-B8
    RisikoView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout);
    // The button area at a screen point (seg7:6C83): 1 CHANCE (press), 2 STOP, 0 none.
    int buttonAt(int x, int y) const;
    // Shows a button pressed for a moment (DS:A29E / DS:A0EE while the input is handled).
    void press(int button, Uint32 now) { pressed_ = button; pressedUntil_ = now + 150; }
    void draw(int light, Uint32 now);

private:
    SpriteLookup borderSprites_;
    SDL_Rect frame() const;
    void load();

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    ScreenLayout layout_;
    std::map<std::string, Texture> frames_;
    bool loaded_ = false;
    int pressed_ = 0;
    Uint32 pressedUntil_ = 0;
};

} // namespace dtvr
