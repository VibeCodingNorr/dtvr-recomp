// SPDX-License-Identifier: GPL-3.0-only
#include "estates.hpp"

#include "board.hpp"

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

} // namespace dtvr
