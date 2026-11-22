// SPDX-License-Identifier: GPL-3.0-only
// The card window of rally.exe ("Karte einsetzen", seg3:0887; round 29): one
// SBBUTTON per hand card with the picture bmp/<n>/d/<n>card_<type + 1>.bmp,
// arranged by cardWindowLayout. Evidence in docs/formats.md "Card window".
#pragma once

#include "game.hpp"
#include "layout.hpp"
#include "sdl_assets.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <vector>

namespace dtvr {

class CardView {
public:
    CardView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout);
    // The current player's hand; false (nothing opens) when it is empty (seg3:0922).
    bool open(const Game& game);
    void close() { buttons_.clear(); }
    bool isOpen() const { return !buttons_.empty(); }
    // The hand slot of a clicked card, -1 otherwise.
    int slotAt(int x, int y) const;
    void draw();

private:
    SDL_Texture* picture(int type);

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    ScreenLayout layout_;
    std::vector<std::pair<CardButton, int>> buttons_; // button, card type
    std::map<int, Texture> pictures_;
};

} // namespace dtvr
