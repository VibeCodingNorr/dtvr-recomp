// SPDX-License-Identifier: GPL-3.0-only
#include "estates.hpp"

#include <cctype>

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
        // seg1:6854..6BA6: grey (0x43) unless a rule for the tab DS:3FC2 and the card
        // mode DS:3FC6 applies. Only the drawing: the action (seg1:5545 / 55DB) does
        // not look at it, so with Verkaufen every row is grey but sells for x 1.5.
        const bool mine = player >= 0 && p.owner == uint8_t(player);
        const bool free = p.owner == kNoOwner;
        bool enabled = true;
        if (tab == EstatesTab::Sell)
            enabled = card == 0xFF && mine && p.landlord == kNoOwner;                   // seg1:68C6
        else if (tab == EstatesTab::Buy) {
            if (card == 0xFF || card == 3 || card == 4 || card == 5)
                enabled = free && cash >= row.price;                                    // seg1:686A / 6A5F..6B70
            else if (card == 6 || card == 7) enabled = !free && !mine;                  // Mieten, Nimm: seg1:6924 / 6982
            else if (card == 8) enabled = !free && !mine && cash >= value;              // Abkaufen: seg1:69E0
            else if (card == 9) enabled = free;                                         // Kredit: seg1:6B73
            else enabled = false;                                                       // mode 2 (Verkaufen): no rule
        }
        row.disabled = !enabled;
        rows.push_back(row);
    }
    return rows;
}

namespace {

// seg1:4886 passes the town names (record +6) to lstrcmp as they are stored, in
// the OEM code page. Windows 3.x lstrcmp (Microsoft KB Q100366 / Q96748): primary
// weights first — non-alphanumeric characters in ANSI order before digits before
// letters, letters without case — then the case only on a tie. In ANSI the OEM
// umlauts are no letters (0x81 undefined, 0x94 a quotation mark), so they sort
// before every letter; 0xE1 (ß, ANSI a-acute) gives the same order for this data
// whether a language driver treats it as a letter or not.
std::vector<int> collationKey(const std::string& oem) {
    std::vector<int> key;
    for (const char ch : oem) {
        const int c = static_cast<unsigned char>(ch);
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) key.push_back(0x200 + std::toupper(c));
        else if (c >= '0' && c <= '9') key.push_back(0x100 + c);
        else key.push_back(c);
    }
    key.push_back(-1); // then the case and the length (seg1: delayed comparison)
    for (const char ch : oem) key.push_back(static_cast<unsigned char>(ch));
    return key;
}

} // namespace

std::vector<EstateRow> statisticsRows(const Game& game, int player, int category, int sort, bool withRented) {
    std::vector<EstateRow> rows;
    const Economy& economy = game.economy();
    for (int town = 0; town < int(game.towns().size()); ++town) {
        const Town& t = game.towns()[size_t(town)];
        const TownState& state = economy.town(town);
        for (int e = 0; e < 10; ++e) {
            if (!economy.entryExists(town, e)) continue;
            const TownEntry& entry = t.entries[size_t(e)];
            if (state.entries[size_t(e)].owner != uint8_t(player)) continue;
            if (!withRented && state.entries[size_t(e)].landlord != kNoOwner) continue; // seg1:4504 argument 0
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
    // seg1:4735: an insertion sort that swaps neighbours only when they are out of
    // order (stable): 0 value ascending (entry +0x18), 1 rate descending (+0x1C),
    // 2 town names by lstrcmp (seg1:4886).
    if (sort == 0)
        std::stable_sort(rows.begin(), rows.end(), [](const EstateRow& a, const EstateRow& b) { return a.price < b.price; });
    else if (sort == 1)
        std::stable_sort(rows.begin(), rows.end(),
                         [](const EstateRow& a, const EstateRow& b) { return a.yieldPercent > b.yieldPercent; });
    else if (sort == 2)
        std::stable_sort(rows.begin(), rows.end(), [&game](const EstateRow& a, const EstateRow& b) {
            return collationKey(game.towns()[size_t(a.town)].name) < collationKey(game.towns()[size_t(b.town)].name);
        });
    return rows;
}

} // namespace dtvr
