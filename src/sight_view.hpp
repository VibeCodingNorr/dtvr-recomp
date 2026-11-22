// SPDX-License-Identifier: GPL-3.0-only
// The sights of rally.exe (round 87): with [Setup] Views a human's vehicle stops at
// the 40 view point cells (europe.pit bit 0x10, seg2:1DE2); Enter there (seg2:0BC3 ->
// seg2:16AA -> seg6:4DCB(x, y)) opens the window Tourism (0x200 x 0xE8) with
// butt/unipaper.bmp, the picture butt/views/<x>_<y>.bmp at (10, 10) and the text
// butt/<lang>/views/<x>_<y>.txt right of it. Evidence in docs/formats.md "Sights".
#pragma once

#include "sdl_assets.hpp"
#include "text.hpp"

#include <SDL.h>

#include <filesystem>
#include <string>

namespace dtvr {

class SightView {
public:
    SightView(SDL_Renderer* renderer, const std::filesystem::path& root, TextRenderer& text)
        : renderer_(renderer), root_(root), text_(text) {}
    // False when the cell has no picture.
    bool open(int cellX, int cellY);
    bool isOpen() const { return open_; }
    // Any key or a left click closes it (seg6:518B); true for consumed input.
    bool handle(const SDL_Event& event);
    void draw();

private:
    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    TextRenderer& text_;
    Texture paper_{nullptr, SDL_DestroyTexture};
    Texture picture_{nullptr, SDL_DestroyTexture};
    std::string body_;
    bool open_ = false;
};

} // namespace dtvr
