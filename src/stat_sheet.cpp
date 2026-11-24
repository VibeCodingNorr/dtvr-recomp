// SPDX-License-Identifier: GPL-3.0-only
#include "stat_sheet.hpp"

namespace dtvr {

StatSheet statSheet(const Game& game) {
    StatSheet sheet;
    sheet.monthText = 301 + int(unsigned(game.calendar().month) % 12);
    // seg1:972F calls seg1:8EE1 first: DS:957A..9581 = 0xFF, then the slots in play by
    // +0x42 + +0x5A, highest first (Game::ranking).
    const std::vector<int> rank = game.ranking();
    for (size_t i = 0; i < rank.size() && i < sheet.rows.size(); ++i) {
        const PlayerMoney& m = game.economy().money(rank[i]);
        StatRow& row = sheet.rows[i];
        row.player = rank[i];
        row.income = m.monthFlow;
        row.rent = m.monthlyIncome;
        row.cash = m.cash;
        row.wealth = m.cash + m.propertyValue; // fld +0x42, fadd +0x5A
    }
    return sheet;
}

} // namespace dtvr
