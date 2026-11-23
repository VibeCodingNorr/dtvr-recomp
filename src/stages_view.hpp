// SPDX-License-Identifier: GPL-3.0-only
// The stage count window TEMP of rally.exe (seg8:0747, procedure seg8:08D6): after
// the player setup of a new game (seg4:0E54) a child of the canvas at (0, 0), 390
// pixels wide and sm.bmp + 36 high, with butt/<n>/p1.bmp as paper, bmp/<n>/sm.bmp
// at (10, 10), text 810 beside it and an SBINPUT field (seg1:1C71) at the element
// seg6:1140(8) holding [Default] Rounds. Enter in the field or a left click on the
// window accepts 1-300 (written back to [Default] Rounds, DS:A104 / A106); other
// values play Zaehler2. Evidence in docs/formats.md "Stage count window".
//
// Recomp addition (not in rally.exe, always shown): a slider 1-300 below the text,
// on a 40 pixel taller window cut from the same paper; it and the field follow
// each other.
#pragma once

#include "layout.hpp"
#include "sdl_assets.hpp"
#include "text.hpp"
#include "window_border.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class StagesView {
public:
    static constexpr int kMaxStages = 300; // seg8:0A45: 0x12C
    StagesView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
               const std::map<int, std::string>& texts, TextRenderer& text);
    // `profile` is [Default] Rounds; outside 0..300 the field starts with 99 (seg8:09A6).
    void open(int profile);
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    enum class Result { None, Accept };
    Result handle(const SDL_Event& event);
    int stages() const { return stages_; }              // the accepted count
    int takeSound() { const int s = sound_; sound_ = -1; return s; }
    void setSprites(SpriteLookup sprites) { borderSprites_ = std::move(sprites); } // B1-B8
    void draw();

    // seg8:0A0D: the first four characters (GetWindowText(…, 5)) read by atoi.
    static int parse(const std::string& text);

private:
    SDL_Rect window() const;
    SDL_Rect client() const;
    SDL_Rect field() const;
    SDL_Rect track() const;
    void setFromSlider(int x);
    SDL_Texture* bitmap(const std::filesystem::path& file);
    bool accept();

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    const std::map<int, std::string>& texts_;
    TextRenderer& text_;
    std::map<std::string, Texture> cache_;
    bool open_ = false;
    bool dragging_ = false;
    std::string input_;     // the SBINPUT text, at most 19 characters (seg1:1D2A)
    int stages_ = 99;
    int sound_ = -1;
    SpriteLookup borderSprites_;
};

} // namespace dtvr
