// SPDX-License-Identifier: GPL-3.0-only
#include "estates.hpp"

#include "board.hpp"

#include <algorithm>

namespace dtvr {

std::vector<EstateRow> estateRows(const Game& game, int town, EstatesTab tab) {
    std::vector<EstateRow> rows;
    const Economy& economy = game.economy();
    if (town < 0 || size_t(town) >= game.towns().size()) return rows;
    const Town& t = game.towns()[size_t(town)];
    const TownState& state = economy.town(town);
    const int player = game.currentPlayer();
    const int card = game.propertyMode(); // DS:3FC6
    const double cash = player >= 0 ? economy.money(player).cash : 0;
    for (int e = 0; e < 10; ++e) {
        if (!economy.entryExists(town, e)) continue; // seg1:672B: empty name
        const TownEntry& entry = t.entries[size_t(e)];
        const PropertyState& p = state.entries[size_t(e)];
        EstateRow row;
        row.town = town;
        row.entry = e;
        row.owner = p.owner == kNoOwner ? -1 : p.owner;
        row.wholeTown = state.wholeOwner != kNoOwner;
        row.name = oemToUtf8(entry.name);
        // seg1:673D: the price column follows the card of the active tab.
        const double value = entry.value;
        row.price = value;
        if (card == 2 && tab == EstatesTab::Sell) row.price = value * 1.5;             // DS:869E
        else if (tab == EstatesTab::Buy && (card == 3 || card == 4 || card == 5))
            row.price = economy.price(town, e, BuyMode(card));                          // DS:86A6/AE/B6
        row.income = value * 0.001 * entry.words[0] * (row.wholeTown ? 2.0 : 1.0);      // DS:86BE, DS:86C6
        row.yieldPercent = entry.words[0] * 1.2;                                        // DS:86CE, "%.1f%%"
        // seg1:6854: without a tab every row is normal; with a tab only the rows the
        // action applies to (the checks of Economy::buy / sell for the card mode).
        const bool mine = player >= 0 && p.owner == uint8_t(player);
        const bool free = p.owner == kNoOwner;
        bool enabled = true;
        if (tab == EstatesTab::Sell) enabled = mine && p.landlord == kNoOwner;
        else if (tab == EstatesTab::Buy) {
            if (card == 6 || card == 7) enabled = !free && !mine;                       // Mieten, Nimm
            else if (card == 8) enabled = !free && !mine && cash >= value;              // Abkaufen
            else if (card == 9) enabled = free;                                         // Kredit
            else enabled = free && cash >= row.price;
        }
        row.disabled = !enabled;
        rows.push_back(row);
    }
    return rows;
}

std::vector<EstateRow> statisticsRows(const Game& game, int player, int category, int sort) {
    std::vector<EstateRow> rows;
    const Economy& economy = game.economy();
    for (int town = 0; town < int(game.towns().size()); ++town) {
        const Town& t = game.towns()[size_t(town)];
        const TownState& state = economy.town(town);
        for (int e = 0; e < 10; ++e) {
            if (!economy.entryExists(town, e)) continue;
            const TownEntry& entry = t.entries[size_t(e)];
            if (state.entries[size_t(e)].owner != uint8_t(player)) continue;
            if (category != 0 && entry.words[1] != category) continue; // entry +0x1E (seg1:4607)
            EstateRow row;
            row.town = town;
            row.entry = e;
            row.owner = player;
            row.wholeTown = state.wholeOwner != kNoOwner;
            row.name = oemToUtf8(entry.name);
            row.townName = oemToUtf8(t.name);
            row.price = entry.value * 0.7;                                                 // DS:868E
            row.income = entry.value * 0.001 * entry.words[0] * (row.wholeTown ? 2.0 : 1.0); // DS:86BE, 86C6
            row.yieldPercent = entry.words[0] * 1.2;                                       // DS:86CE
            rows.push_back(row);
        }
    }
    // Sort order: assumption (the sort code after seg1:4735 is not traced): value and
    // yield descending, towns in file order.
    if (sort == 0)
        std::stable_sort(rows.begin(), rows.end(), [](const EstateRow& a, const EstateRow& b) { return a.price > b.price; });
    else if (sort == 1)
        std::stable_sort(rows.begin(), rows.end(),
                         [](const EstateRow& a, const EstateRow& b) { return a.yieldPercent > b.yieldPercent; });
    return rows;
}

} // namespace dtvr
