// SPDX-License-Identifier: GPL-3.0-only
// The overview OUTLINE of rally.exe (seg1:A240, paint from seg1:A330; round 37):
// butt/<n>/map.bmp in the map window, the route of the current player to the
// destination in red, the stage start (green) and the destination (blue), and the
// flags <letter>_FL of the drivers and Q_FL of Dr. Drago. Evidence in
// docs/formats.md "Overview".
#pragma once

#include "game.hpp"
#include "layout.hpp"
#include "mouse_control_view.hpp"
#include "sdl_assets.hpp"

#include <SDL.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace dtvr {

// SDL-free: the overview points of the route (seg1:A4E4 – A5B2): from the player's
// field always the first neighbour (up, right, down, left) closer to the destination,
// distances by seg2:43A8 with the route mask; pixel = field cell * 16 / (4096 / size) + 1.
std::vector<SDL_Point> overviewRoute(const Board& board, int from, int destination, uint16_t mask, int bitmapSize);
SDL_Point overviewPoint(const Board& board, int field, int bitmapSize);

class OverviewView {
public:
    using SpriteLookup = std::function<SDL_Texture*(const std::string&)>;
    OverviewView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
                 const Board& board, SpriteLookup sprite);
    void open() { open_ = true; zoomed_ = false; cell_ = 0; mouse_.close(); }
    void close() { open_ = false; mouse_.close(); }
    bool isOpen() const { return open_; }
    // Escape/Backspace/right button close. Enter toggles zoom; arrows pan the
    // enlarged overview on a 3 x 3 grid (seg1:A95E / AB7B). True only when closed.
    bool handle(const SDL_Event& event);
    void draw(const Game& game);

private:
    void compose(const Game& game);
    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    ScreenLayout layout_;
    const Board& board_;
    SpriteLookup sprite_;
    MouseControlView mouse_;
    Texture map_{nullptr, SDL_DestroyTexture};
    Texture layer_{nullptr, SDL_DestroyTexture};
    int mapSize_ = 0;
    bool open_ = false;
    bool zoomed_ = false; // DS:8C5E
    int cell_ = 0;       // DS:8C60: row * 3 + column
};

} // namespace dtvr
