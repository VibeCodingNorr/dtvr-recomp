// SPDX-License-Identifier: GPL-3.0-only
// The direction choice of the Nägel card (seg3:1C96, procedure seg3:1E5E; round
// 55, input round 96): a 0x226 x 0x104 window on butt/n.bmp with the map around
// the player in the compass, text 510, "Ok" and the triangles of the available
// directions; an arrow key or a click in a triangle (PtInRegion, seg3:1F6E) selects,
// Enter or a click on Ok confirms; it cannot be cancelled. Evidence in
// docs/formats.md "Nägel".
#pragma once

#include "layout.hpp"
#include "sdl_assets.hpp"
#include "text.hpp"

#include <SDL.h>

#include <array>
#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class NailsView {
public:
    NailsView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
              const std::map<int, std::string>& texts, TextRenderer& text);
    // `available`: up, right, down, left (Game::nailsDirections); the last available
    // one is selected (seg3:2366 ff.).
    void open(const std::array<bool, 4>& available);
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    // The confirmed direction 1 up, 2 right, 3 down, 4 left (seg3:2096); 0 nothing.
    int handle(const SDL_Event& event);
    // `compass`: the map around the player (96 x 96 world pixels, seg3:2557), or null.
    void draw(SDL_Texture* compass);

private:
    SDL_Rect box() const;

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    ScreenLayout layout_;
    const std::map<int, std::string>& texts_;
    TextRenderer& text_;
    Texture paper_{nullptr, SDL_DestroyTexture};
    std::array<bool, 4> available_{};
    int selected_ = 0; // DS:8C10
    bool open_ = false;
};

} // namespace dtvr
