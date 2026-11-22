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
    int entry = 0;
    int owner = -1;         // player, -1 free
    bool wholeTown = false; // DS:948C: one player owns every entry (icon %c_m, income x2)
    std::string name;       // UTF-8
    double price = 0;       // column 2 (card factor in the matching tab)
    double income = 0;      // column 3: value * 0.001 * rate (x2 for a whole town)
    double yieldPercent = 0; // column 4: rate * 1.2
    bool disabled = false;  // drawn in colour 0x43
};

std::vector<EstateRow> estateRows(const Game& game, int town, EstatesTab tab);

} // namespace dtvr
