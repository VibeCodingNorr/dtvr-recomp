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
    // `language`: the lan.inf letter DS:A0F0 (the Statistik header spacing depends on it).
    EstatesView(SDL_Renderer* renderer, const ScreenLayout& layout, const std::map<int, std::string>& texts,
                TextRenderer& text, SpriteLookup sprite, char language);

    // seg1:4CF1: no tab, no selected row, scrolled to the top.
    // `handEmpty`: seg1:4D18 locks Karte (DS:3FBA = 2) when seg2:3C00 counts no hand card.
    void open(int town, bool handEmpty = false);
    // seg1:8D1A after the card window of the town dialog (seg4:0AB0 / 0B27): back to
    // the header; with a property card mode (DS:3FC6) Karte is locked, the focus goes to
    // Verkaufen (mode 2, Kaufen locked) or Kaufen (Verkaufen locked) and the posted
    // Enter opens that tab. Returns true when a tab opened (Plopp02).
    bool afterCardWindow(int propertyMode);
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
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    const std::map<int, std::string>& texts_; // language file, UTF-8
    TextRenderer& text_;
    SpriteLookup sprite_;
    char language_; // the lan.inf letter DS:A0F0 (seg5:028C)
    int town_ = -1;
    EstatesTab tab_ = EstatesTab::None; // DS:3FC2 / DS:3FBC
    int focus_ = 0; // DS:3FB6: header focus 0 Kaufen, 1 Verkaufen, 2 Karte, 3 close; -1 = 0xFF
    int locked_ = -1;        // DS:3FB8: the locked header item (0xFF none; 3 the forced sale)
    bool cardsLocked_ = false; // DS:3FBA == 2: Karte is locked
    bool isLocked(int item) const { return item >= 0 && (item == locked_ || (item == 2 && cardsLocked_)); }
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
