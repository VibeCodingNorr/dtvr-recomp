// SPDX-License-Identifier: GPL-3.0-only
// The Flugzeug booking of rally.exe (card 27, seg3:31D5, procedure seg3:3384;
// round 54): a 0x114 x 0x100 window on butt/f.bmp with "Flugbuchung nach:" (text
// 5601) and the towns 5602-5609 (Moskau only with the east open); Brussels is
// unavailable during the final stage (text 8300). Evidence in docs/formats.md "Flugzeug".
#pragma once

#include "window_border.hpp"

#include "layout.hpp"
#include "sdl_assets.hpp"
#include "text.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class FlightView {
public:
    void setSprites(SpriteLookup sprites) { borderSprites_ = std::move(sprites); } // B1-B8
    FlightView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
               const std::map<int, std::string>& texts, TextRenderer& text);
    void open(bool eastOpen, bool lastStage);
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    // The chosen row's town name (UTF-8), "" for none; `closed` when Brussels was
    // refused (the caller shows text 8300). The traced original handles no cancel
    // input; `cancel` is retained for caller compatibility and remains false.
    struct Result { std::string town; bool closed = false, cancel = false; };
    Result handle(const SDL_Event& event);
    // The sound of the last handled event for channel 2 (seg1:2C9B), -1 none.
    int takeSound() { const int s = sound_; sound_ = -1; return s; }
    void draw();

private:
    SpriteLookup borderSprites_;
    SDL_Rect windowRect() const; // CreateWindow size, centred
    SDL_Rect box() const;        // the client area inside the border (seg3:3938)
    int rows() const { return eastOpen_ ? 8 : 7; } // DS:8C1E + 1 (seg3:3633)
    std::string text(int id) const;
    Result choose(int row);

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    ScreenLayout layout_;
    const std::map<int, std::string>& texts_;
    TextRenderer& text_;
    Texture paper_{nullptr, SDL_DestroyTexture};
    bool open_ = false, eastOpen_ = false, lastStage_ = false;
    int focus_ = 0;
    int sound_ = -1;
};

} // namespace dtvr
