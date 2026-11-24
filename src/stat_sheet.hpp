// SPDX-License-Identifier: GPL-3.0-only
// What the monthly statement STAT of rally.exe (seg1:972F, round R1) shows: the month
// and, in the order of the ranking seg1:8EE1, each player's month cash flow, rent,
// cash and wealth. No SDL. Evidence in docs/formats.md "Monthly statement".
#pragma once

#include "game.hpp"

#include <array>

namespace dtvr {

struct StatRow {
    int player = -1;   // DS:957A + rank; -1 for 0xFF (no player at this rank)
    double income = 0; // player +0x4A, the month's cash flow (seg1:9F86)
    double rent = 0;   // +0x52, the month's rent income (seg1:9FE7)
    double cash = 0;   // +0x42 (seg1:A04D)
    double wealth = 0; // +0x42 + +0x5A (seg1:A0B7 / A0CF)
};

struct StatSheet {
    int monthText = 301;           // 301 + DS:A100 % 12 (seg1:9BD8)
    std::array<StatRow, 8> rows{}; // the eight ranks (seg1:9E3B), always drawn
};

StatSheet statSheet(const Game& game);

} // namespace dtvr
