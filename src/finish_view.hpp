// SPDX-License-Identifier: GPL-3.0-only
// The finish sequence ZIEL of rally.exe (seg7:2628; round 45): a 0x1F0 x 0x178
// window with butt/ziel.bmp, the banner butt/<lang>/banner.bmp at (6, 117) (colour
// 0xFF transparent), the town photo butt/<lang>/ziel/<town>.bmp filling the white
// (0xFF) sky of ziel.bmp, and sixteen animation records of butt/sbs/ziel drawn with
// rally.exe's dirty rectangles (FinishScene, RecordCanvas); the driver's car
// <letter>ZielCar.sbs drives left by 2 pixels per 20 ms pass until x < -160.
// Evidence in docs/formats.md "Finish sequence".
#pragma once

#include "window_border.hpp"

#include "board.hpp"
#include "finale_scenes.hpp"
#include "record_canvas.hpp"
#include "sdl_assets.hpp"

#include <SDL.h>

#include <filesystem>
#include <string>

namespace dtvr {

class FinishView {
public:
    // B1-B8 of assorted.sbs for the 8-pixel border of seg6:1A68 (round 112).
    void setSprites(SpriteLookup sprites) { borderSprites_ = std::move(sprites); }
    FinishView(SDL_Renderer* renderer, const std::filesystem::path& root, char language, const int& canvasWidth, const int& canvasHeight);
    // Builds the scene for the driver letter and the town (OEM name); false when the
    // pictures are missing (the original then skips the sequence, seg7:26E8).
    bool start(char driverLetter, const std::string& oemTownName, Uint32 now);
    bool active() const { return active_; }
    void stop() { active_ = false; }
    // Runs the 20 ms passes; ends by itself when the car has left the scene (seg7:3546)
    // and returns true in the call that ends it (the window is destroyed: ZIEL's
    // WM_DESTROY stops the voice, seg7:4E28).
    bool update(Uint32 now);
    void draw(Uint32 now);

private:
    SpriteLookup borderSprites_;
    void runPass();

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    char language_; // the lan.inf letter DS:A0F0 (seg5:028C)
    const int& canvasWidth_; // the shared layout's size (live widescreen switch)
    const int& canvasHeight_;
    RecordCanvas canvas_{0x1E0, 0x168, FinishScene::kRecords};
    FinishScene scene_;
    Uint32 lastPass_ = 0;
    bool active_ = false;
    bool shown_ = false; // ShowWindow on the first pass (seg7:35FC); hidden before
};

} // namespace dtvr
