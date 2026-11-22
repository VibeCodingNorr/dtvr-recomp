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
    // `mask` is DS:18 of seg3:0887(mask): a card is enabled when (category & mask) ==
    // category (seg3:0701, otherwise message 0x7ED(1) to its SBBUTTON); a negative
    // mask enables every card (the discard choice of seg3:1057).
    bool open(const Game& game, int mask);
    void close() { buttons_.clear(); pressed_ = -1; }
    bool isOpen() const { return !buttons_.empty(); }
    // SBBUTTON input (seg1:0E78). `sound` is the effect for channel 2 (-1 none).
    struct Input { int slot = -1; bool cancel = false; int sound = -1; };
    // Left button down (seg1:1311): focus; a disabled card sounds 7, else it is pressed.
    Input mouseDown(int x, int y);
    // Left button up (seg1:1398): a card released over itself is activated (Plopp02).
    Input mouseUp(int x, int y);
    // Keys (seg1:10B4): arrows move the focus (sound 8), Tab / Shift+Tab right / left,
    // space or Return activate the focused card, Escape / Backspace cancel.
    Input key(SDL_Keycode key, Uint16 mod);
    // `disabled` is the frame dis_<language> of <n>_assort.sbs laid over a disabled
    // card (SBBUTTON paint seg1:1902, state bit 0x10).
    void draw(SDL_Texture* disabled);

private:
    SDL_Texture* picture(int type);
    bool enabled(int type) const;
    int indexOfSlot(int slot) const;
    Input activate(int index);
    bool moveFocus(int dx, int dy);

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    ScreenLayout layout_;
    std::vector<std::pair<CardButton, int>> buttons_; // button, card type
    int mask_ = -1;
    int focus_ = -1;   // hand slot with state bit 4 (red frame)
    int pressed_ = -1; // hand slot with state bit 1 (picture at (1, 1))
    std::map<int, Texture> pictures_;
};

} // namespace dtvr
