// SPDX-License-Identifier: GPL-3.0-only
// The board of the year balance BILANZ (no SDL): what the window's WM_PAINT
// (seg8:9AB8) draws on bil_leer.bmp from DS:8D5E: a player's sheet (slot 0..7,
// seg8:9B93..A1E2) or the worth graph of all players (0xA, seg8:A1E5..A64C).
// Evidence in docs/formats.md "Year balance" (round 149).
#pragma once

#include "game.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace dtvr {

// Coordinates are those of the 480 x 360 WinG surface (the bitmap).
struct BoardPoint { int x = 0, y = 0; };

struct BalanceSheet {
    // Fonts: 0 CreateFont(-18, weight 400, FF_SWISS, "MS Sans Serif") (seg8:9BC1),
    // 1 seg6:04F9(16, weight 400) (seg8:9C6E), 2 seg6:04F9(16, weight 600) (seg8:A0CC).
    struct Line {
        std::string label, value; // value empty: none
        int font = 1, top = 0;
        bool negative = false;    // value drawn in palette 0xED instead of 0x45
    };
    int left = 0xBB, right = 0xBB + 0xF6; // the DrawText rectangle (seg8:9BA5)
    std::vector<Line> lines;              // the label left (0x100), the value right (0x102)
};

// `texts` are the language strings (id -> UTF-8), `slot` the player (DS:8D5E).
BalanceSheet balanceSheet(const Game& game, int slot, const std::map<int, std::string>& texts);

struct BalanceGraph {
    // PALETTERGB colours of the pens (COLORREF 0x02BBGGRR without the flag).
    struct Rgb { uint8_t r = 0, g = 0, b = 0; };
    struct Polyline { int slot = 0; Rgb colour; std::vector<BoardPoint> points; }; // width 2
    struct Segment { BoardPoint from, to; };  // LineTo: `to` itself is not drawn
    std::vector<Polyline> players;             // seg8:A336..A407, in slot order
    struct Legend { char frame = 0; BoardPoint at; }; // 26 x 26 bilanz.sbs frame, opaque
    std::vector<Legend> legend;                // seg8:A42A..A49C
    Rgb tickColour{0xA7, 0xA3, 0x6B};          // width 1 (seg8:A4A4)
    std::vector<Segment> ticks;                // seg8:A4D6..A57D
    Rgb axisColour{0x8B, 0x87, 0x57};          // width 2 (seg8:A5A0)
    std::vector<Segment> axes;                 // seg8:A5D2..A62C
};

BalanceGraph balanceGraph(const Game& game);

// seg6:137F applied to the sheet's 32-bit values; seg8:9D6E: palette 0xED below 0.
// The year balance's "Vermögenszuwachs" (seg8:9C91..9D0D): with DS:A102 >= 2 the
// worth of the last year minus the one before, else the entry +0x66 + 4 * DS:A102
// (the year's rent for 0, the first worth for 1) minus the start capital 30000 (DS:85BE).
int32_t balanceGrowth(const PlayerMoney& money, int year);

} // namespace dtvr
