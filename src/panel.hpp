// SPDX-License-Identifier: GPL-3.0-only
// Contents of the player info panel PLINFO (seg5:61C5), SDL-free (round 22).
#pragma once

#include "game.hpp"
#include "layout.hpp"

#include <array>
#include <map>
#include <string>

namespace dtvr {

// seg6:137F: sprintf("$%li"), a minus sign moves in front of '$', then a '.'
// after every three digits: 30000 -> "$30.000", -1234567 -> "-$1.234.567".
std::string formatMoney(double value);

// DS:3936: eight rectangles (left, top, right, bottom) per resolution; 255 means
// the panel's client edge (seg5:5F88).
ScreenRect panelSlot(const ScreenLayout& layout, int slot);

struct PlayerInfo {
    std::string monthLine;   // slot 0: text 301 + month % 12, "%Fs (%d)" with year + 1
    std::string money;       // slot 1: cash of the current player
    std::string destination; // slot 2: destination town (seg2:271C(DS:A10D))
    std::array<int, 2> distance{}; // slot 3: digits of DS:22 at the player's field (99 unreachable)
    std::string stepsLabel;  // slot 4: text 201
    std::array<int, 2> steps{};    // slot 5: digits of DS:A778 - DS:9D40
    std::string fieldKind;   // slot 6: town name or text 202 + kind (DS:B632, 19 characters)
    std::string fieldIcon;   // slot 7: SBS frame BLAU / ROT / GELB / LILA for kinds 1, 2, 3, 5
};

// `texts` are the language strings (id -> UTF-8).
PlayerInfo playerInfo(const Game& game, const Board& board, const std::vector<Town>& towns,
                      const std::map<int, std::string>& texts);

} // namespace dtvr
