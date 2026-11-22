// SPDX-License-Identifier: GPL-3.0-only
// The town dialog ESTATES of rally.exe (round 30): 558 x 420 pixels, a header strip
// with the tabs Kaufen / Verkaufen / Karte and one 42-pixel strip per property,
// all SBS frames of sbs/buy.sbs. Contents from estates.hpp; evidence in
// docs/formats.md "Town dialog".
#pragma once

#include "estates.hpp"
#include "layout.hpp"
#include "text.hpp"

#include <SDL.h>

#include <array>
#include <functional>
#include <map>
#include <string>

namespace dtvr {

class EstatesView {
public:
    using SpriteLookup = std::function<SDL_Texture*(const std::string&)>;
    EstatesView(SDL_Renderer* renderer, const ScreenLayout& layout, const std::map<int, std::string>& texts,
                TextRenderer& text, SpriteLookup sprite);

    // seg1:4CF1: no tab, no selected row, scrolled to the top.
    void open(int town);
    // Statistik (seg1:4DE5): the variant with bit 0x10 in DS:3FC2 for `player`.
    void openStatistics(int player);
    // seg1:4E39 -> seg1:4EA8(0): the statistics as the forced sale (DS:3FC2 = 0x10):
    // header item 0 is Verkaufen (text 360) and enters the sale mode 0x12.
    void openForcedSale(int player);
    bool forced() const { return forced_; }
    // After a forced sale with `count` rows left (seg1:57FF ff.).
    void afterForcedSale(int count);
    void close() { town_ = -1; statistics_ = false; forced_ = false; saleMode_ = false; }
    bool isOpen() const { return town_ >= 0 || statistics_; }
    bool statistics() const { return statistics_; }
    int player() const { return player_; }
    int category() const { return category_; }
    int sort() const { return sort_; }
    int town() const { return town_; }
    EstatesTab tab() const { return tab_; }

    struct Action {
        enum class Kind { None, Tab, Row, Cards, Close, ForcedSell } kind = Kind::None;
        int entry = -1;     // Row / ForcedSell: the property entry
        int town = -1;      // ForcedSell: its town
        bool sound = false; // Close: Plopp02 first (the header's close item, seg1:54F8)
    };
    // `rows` are the current contents (estateRows of this town and tab).
    Action handle(const SDL_Event& event, const std::vector<EstateRow>& rows);
    // After a successful purchase or sale with `count` rows (seg1:55A5 / 5646: VK_DOWN).
    void afterAction(int count);
    void draw(const std::vector<EstateRow>& rows, const std::array<char, 8>& ownerLetters);
    SDL_Rect box() const;

private:
    int rowAt(int x, int y, int count) const;
    void keepVisible(int count);

    SDL_Renderer* renderer_;
    ScreenLayout layout_;
    const std::map<int, std::string>& texts_; // language file, UTF-8
    TextRenderer& text_;
    SpriteLookup sprite_;
    int town_ = -1;
    EstatesTab tab_ = EstatesTab::None; // DS:3FC2 / DS:3FBC
    int focus_ = 0; // DS:3FB6: header focus 0 Kaufen, 1 Verkaufen, 2 Karte, 3 close; -1 = 0xFF
    int selected_ = -1;                 // DS:3FBE, index into the rows
    int scroll_ = 0;                    // DS:8C4C, first visible row
    bool statistics_ = false;
    bool forced_ = false;   // DS:3FC2 = 0x10 (the menu's statistics are 0x30)
    bool saleMode_ = false; // DS:3FC2 = 0x12
    int player_ = -1;
    int category_ = 0;                  // DS:8C48 (texts 361-367)
    int sort_ = 0;                      // DS:8C4A (texts 368-370)

    void drawStatisticsHeader();
};

} // namespace dtvr
