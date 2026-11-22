// SPDX-License-Identifier: GPL-3.0-only
// The FLIC window of rally.exe (seg7:62AA, procedure seg7:63E1; round 83): a card
// film butt/flc/karte<n>.flc at (10, 10) of the canvas, modal (seg6:0FB4), played
// `loops` times (DS:8D46) at the film's speed, its sound on channel 7 before
// (seg1:2D40) and stopped after (seg1:317D). Evidence in docs/formats.md "Films".
#pragma once

#include "flc.hpp"
#include "sdl_assets.hpp"

#include <SDL.h>

#include <filesystem>
#include <memory>

namespace dtvr {

class FilmView {
public:
    FilmView(SDL_Renderer* renderer, const std::filesystem::path& root) : renderer_(renderer), root_(root) {}
    // Film 1-4 (karte<n>.flc); false when the file is missing or broken.
    bool start(int film, int loops, int canvasX, int canvasY, Uint32 now);
    bool active() const { return film_ != nullptr; }
    void stop() { film_.reset(); }
    // Advances by the film's speed; ends after `loops` passes (seg7:6619).
    void update(Uint32 now);
    void draw();

private:
    void upload();

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    std::unique_ptr<FlcFilm> film_;
    Texture texture_{nullptr, SDL_DestroyTexture};
    SDL_Rect box_{};
    int loops_ = 1, shown_ = 0; // frames shown so far
    Uint32 nextFrame_ = 0;
};

} // namespace dtvr
