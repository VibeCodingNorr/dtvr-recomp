// SPDX-License-Identifier: GPL-3.0-only
// Contents of the town dialog ESTATES (seg1:4BDD / 4E39, rows seg1:65A7; round 30):
// one row per property of a town with owner, price, monthly income and yield.
// SDL-free; evidence in docs/formats.md "Town dialog".
#pragma once

#include "game.hpp"

#include <string>
#include <vector>

namespace dtvr {

// DS:3FC2: the active tab (0 none, 1 Kaufen, 2 Verkaufen).
enum class EstatesTab { None = 0, Buy = 1, Sell = 2 };

struct EstateRow {
    int town = 0;
    int entry = 0;
    std::string townName;   // UTF-8 (statistics rows)
    int owner = -1;         // player, -1 free
    bool wholeTown = false; // DS:948C: one player owns every entry (icon %c_m, income x2)
    std::string name;       // UTF-8
    double price = 0;       // column 2 (card factor in the matching tab)
    double income = 0;      // column 3: value * 0.001 * rate (x2 for a whole town)
    double yieldPercent = 0; // column 4: rate * 1.2
    bool disabled = false;  // drawn in colour 0x43
};

std::vector<EstateRow> estateRows(const Game& game, int town, EstatesTab tab);

// Statistics variant (seg1:4DE5 -> seg1:4EA8, list seg1:4504): the entries `player`
// owns in all towns, rented ones included; category DS:8C48 (0 all, otherwise entry
// +0x1E: 1 Industrie ... 6 Sonstige, texts 361-367); sort DS:8C4A (0 Wert, 1 Rendite,
// 2 Stadt, texts 368-370). Price column value * 0.7 (DS:868E); no row is disabled.
std::vector<EstateRow> statisticsRows(const Game& game, int player, int category, int sort);

} // namespace dtvr
