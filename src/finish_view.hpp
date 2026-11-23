// SPDX-License-Identifier: GPL-3.0-only
// The finish sequence ZIEL of rally.exe (seg7:2628; round 45): a 0x1F0 x 0x178
// window with butt/ziel.bmp, the banner butt/<lang>/banner.bmp at (6, 117) (colour
// 0xFF transparent), the town photo butt/<lang>/ziel/<town>.bmp filling the white
// (0xFF) sky of ziel.bmp, and
// sixteen animation records of butt/sbs/ziel; the driver's car <letter>ZielCar.sbs
// drives left by 2 pixels per 20 ms step until x < -160. Evidence in
// docs/formats.md "Finish sequence".
#pragma once

#include "window_border.hpp"

#include "board.hpp"
#include "sdl_assets.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class FinishView {
public:
    // B1-B8 of assorted.sbs for the 8-pixel border of seg6:1A68 (round 112).
    void setSprites(SpriteLookup sprites) { borderSprites_ = std::move(sprites); }
    FinishView(SDL_Renderer* renderer, const std::filesystem::path& root, const int& canvasWidth, const int& canvasHeight);
    // Builds the scene for the driver letter and the town (OEM name); false when the
    // pictures are missing (the original then skips the sequence, seg7:26E8).
    bool start(char driverLetter, const std::string& oemTownName, Uint32 now);
    bool active() const { return active_; }
    void stop() { active_ = false; }
    // Ends by itself when the car has left the scene (seg7:3546).
    void update(Uint32 now);
    void draw(Uint32 now);

private:
    SpriteLookup borderSprites_;
    int steps(Uint32 now) const;

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    const int& canvasWidth_; // the shared layout's size (live widescreen switch)
    const int& canvasHeight_;
    Texture background_{nullptr, SDL_DestroyTexture};
    std::map<std::string, Texture> frames_;
    Uint32 started_ = 0;
    bool active_ = false;
};

} // namespace dtvr
