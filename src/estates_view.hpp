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
    void close() { town_ = -1; }
    bool isOpen() const { return town_ >= 0; }
    int town() const { return town_; }
    EstatesTab tab() const { return tab_; }

    struct Action {
        enum class Kind { None, Tab, Row, Cards, Close } kind = Kind::None;
        int entry = -1; // Row: the property entry
    };
    // `rows` are the current contents (estateRows of this town and tab).
    Action handle(const SDL_Event& event, const std::vector<EstateRow>& rows);
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
    int selected_ = -1;                 // DS:3FBE, index into the rows
    int scroll_ = 0;                    // DS:8C4C, first visible row
};

} // namespace dtvr
