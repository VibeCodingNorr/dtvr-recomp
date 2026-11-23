// SPDX-License-Identifier: GPL-3.0-only
// The card shop of the violet card fields (seg3:4437, procedure seg3:4640, paint
// seg3:4D0E / 5234 / 553C; round 52): header with Kaufen / Verkaufen / Beenden,
// the offers or the own hand as card pictures of butt/<lang>/bcards.sbs, and an
// info line with the chosen card's name, price and the cash. Evidence in
// docs/formats.md "Card shop".
#pragma once

#include "game.hpp"
#include "layout.hpp"
#include "sdl_assets.hpp"
#include "text.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace dtvr {

class CardShopView {
public:
    CardShopView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
                 const std::map<int, std::string>& texts, TextRenderer& text);
    void open(const Game& game);
    void close() { open_ = false; }
    bool isOpen() const { return open_; }

    struct Action {
        enum class Kind { None, Buy, Sell, Close } kind = Kind::None;
        int value = -1; // card type to buy or hand slot to sell
        int sound = -1; // seg1:2C9B sound number for this event (2 Plopp02, 7 Zaehler2)
    };
    Action handle(const SDL_Event& event, const Game& game);
    // After a successful sale (seg3:49E0 ff.): the list is rebuilt, a selection past
    // its end moves to the last card (0xFF when empty), and with an empty hand the
    // view posts Backspace to itself, i.e. returns silently to the header.
    void afterSale(const Game& game);
    void draw(const Game& game);

private:
    struct Item { int type; int value; }; // offer: deck position; hand: slot
    std::vector<Item> items(const Game& game) const;
    SDL_Rect box() const;
    std::string text(int id) const;
    Action enter(const Game& game);

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    const std::map<int, std::string>& texts_;
    TextRenderer& text_;
    std::map<std::string, Texture> frames_;
    bool open_ = false;
    int mode_ = 0;     // DS:9C82: 0 choose, 1 buy, 2 sell
    int choice_ = 0;   // DS:9C9A: 0 Kaufen, 1 Verkaufen, 2 Beenden
    int selected_ = -1; // DS:A0EA
};

} // namespace dtvr
