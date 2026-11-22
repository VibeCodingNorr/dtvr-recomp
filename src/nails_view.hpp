// SPDX-License-Identifier: GPL-3.0-only
// The direction choice of the Nägel card (seg3:1C96, procedure seg3:1E5E; round
// 55): a 0x226 x 0x104 window on butt/n.bmp with text 510 and four arrow
// triangles; a click in a triangle (PtInRegion, seg3:1F6E) or an arrow key chooses
// the direction. Evidence in docs/formats.md "Nägel".
#pragma once

#include "layout.hpp"
#include "sdl_assets.hpp"
#include "text.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class NailsView {
public:
    NailsView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
              const std::map<int, std::string>& texts, TextRenderer& text);
    void open();
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    // 1 up, 2 right, 3 down, 4 left; -2 cancelled (Escape / right button); 0 nothing.
    int handle(const SDL_Event& event);
    void draw();

private:
    SDL_Rect box() const;

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    ScreenLayout layout_;
    const std::map<int, std::string>& texts_;
    TextRenderer& text_;
    Texture paper_{nullptr, SDL_DestroyTexture};
    bool open_ = false;
};

} // namespace dtvr
