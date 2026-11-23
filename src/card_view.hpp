// SPDX-License-Identifier: GPL-3.0-only
// The card window of rally.exe ("Karte einsetzen", seg3:0887; round 29): one
// SBBUTTON per hand card with the picture bmp/<n>/d/<n>card_<type + 1>.bmp,
// arranged by cardWindowLayout. Evidence in docs/formats.md "Card window".
#pragma once

#include "game.hpp"
#include "layout.hpp"
#include "sb_button.hpp"
#include "sdl_assets.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>

namespace dtvr {

class CardView {
public:
    CardView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout);
    // The current player's hand; false (nothing opens) when it is empty (seg3:0922).
    // `mask` is DS:18 of seg3:0887(mask): a card is enabled when (category & mask) ==
    // category (seg3:0701, otherwise message 0x7ED(1) to its SBBUTTON); a negative
    // mask enables every card (the discard choice of seg3:1057).
    bool open(const Game& game, int mask);
    void close() { buttons_.clear(); types_.clear(); }
    bool isOpen() const { return !buttons_.buttons().empty(); }
    // SBBUTTON input (seg1:0E78); the id of an activated button is the hand slot.
    using Input = SbButtonGroup::Input;
    Input mouseDown(int x, int y) { return buttons_.mouseDown(x, y); }
    Input mouseUp(int x, int y) { return buttons_.mouseUp(x, y); }
    Input key(SDL_Keycode key, Uint16 mod) { return buttons_.key(key, mod, true); }
    // `disabled` is the frame dis_<language> of <n>_assort.sbs laid over a disabled
    // card (SBBUTTON paint seg1:1902, state bit 0x10).
    void draw(SDL_Texture* disabled);

private:
    SDL_Texture* picture(int type);

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    SbButtonGroup buttons_;      // id = hand slot
    std::map<int, int> types_;   // hand slot -> card type
    std::map<int, Texture> pictures_;
};

} // namespace dtvr
