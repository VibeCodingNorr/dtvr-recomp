// SPDX-License-Identifier: GPL-3.0-only
// The Flugzeug booking of rally.exe (card 27, seg3:31D5, procedure seg3:3384;
// round 54): a 0x114 x 0x100 window on butt/f.bmp with "Flugbuchung nach:" (text
// 5601) and the towns 5602-5609 (Moskau only with the east open); Brussels is
// closed before the last stage (text 8300). Evidence in docs/formats.md "Flugzeug".
#pragma once

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
    FlightView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
               const std::map<int, std::string>& texts, TextRenderer& text);
    void open(bool eastOpen, bool lastStage);
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    // The chosen row's town name (UTF-8), "" for none; `closed` when Brussels was
    // refused (the caller shows text 8300); `cancel` on Escape / right button.
    struct Result { std::string town; bool closed = false, cancel = false; };
    Result handle(const SDL_Event& event);
    void draw();

private:
    SDL_Rect box() const;
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
};

} // namespace dtvr
