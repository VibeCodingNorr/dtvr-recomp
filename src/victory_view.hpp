// SPDX-License-Identifier: GPL-3.0-only
// The victory sequence SIEG of rally.exe (seg7:71C4 -> seg7:7250(first, second,
// third); round 46): butt/Sieg.bmp with the banner s_bann_d.bmp, the crowd, band
// and speaker of butt/sbs/sieg and intro, the winner's car, the winner animated
// and the second and third places standing. Evidence in docs/formats.md
// "Victory sequence".
#pragma once

#include "window_border.hpp"

#include "sdl_assets.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace dtvr {

class VictoryView {
public:
    // B1-B8 of assorted.sbs for the 8-pixel border of seg6:1A68 (round 112).
    void setSprites(SpriteLookup sprites) { borderSprites_ = std::move(sprites); }
    VictoryView(SDL_Renderer* renderer, const std::filesystem::path& root, char language, const int& canvasWidth, const int& canvasHeight);
    // `ranking` lists slots best first (seg1:8EE1); false when the pictures are missing.
    bool start(const std::vector<int>& ranking, Uint32 now);
    bool active() const { return active_; }
    void stop() { active_ = false; }
    // seg7:73C5: with subtitles on, seg6:160B(hwnd, 0, -22) lifts the centred window.
    void setLift(int lift) { lift_ = lift; }
    int lift() const { return lift_; }
    void update(Uint32 now); // ends after 5000 passes (seg7:7D4E sends Enter)
    // seg7:7DEA / 7E16: WM_KEYDOWN with Escape, Space or Enter, or WM_LBUTTONDOWN,
    // once the first pass set DS:9CDE (seg7:7CE9).
    bool closes(const SDL_Event& event, Uint32 now) const;
    void draw(Uint32 now);

private:
    SpriteLookup borderSprites_;
    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    char language_; // the lan.inf letter DS:A0F0 (seg5:028C)
    const int& canvasWidth_; // the shared layout's size (live widescreen switch)
    const int& canvasHeight_;
    int lift_ = 0;
    Texture background_{nullptr, SDL_DestroyTexture};
    std::map<std::string, Texture> frames_;
    struct Place { std::string name; int x, y; };
    std::vector<Place> places_; // second and third place (standing frames)
    int winner_ = -1;  // slot
    int winnerY_ = 0;  // 0xCA - height of its first frame
    Uint32 started_ = 0;
    bool active_ = false;
};

} // namespace dtvr
