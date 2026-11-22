// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "board.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace dtvr {

// Property economy recovered from rally.exe (evidence: docs/formats.md, round 8).
// Town records (europe_<lang>.tow) are read-only; the runtime state lives in a
// separate 0x6F-byte record per town at DS:5A.

constexpr uint8_t kNoOwner = 0xFF;
constexpr double kStartingCash = 30000.0; // DS:85BE copied by seg2:033A

struct PropertyState {            // 11 bytes at town state +1 + entry * 11
    uint8_t owner = kNoOwner;     // +0
    double burden = 0;            // +1..+8, paid off by income first (origin open)
    uint8_t landlord = kNoOwner;  // +9: owner while rented out (card Mieten)
    uint8_t monthsLeft = 0;       // +10: rent months left (12 when rented)
};

struct TownState {
    uint8_t wholeOwner = kNoOwner;            // +0: set when one player owns every entry
    std::array<PropertyState, 10> entries{};
};

// Buy modes: DS:3FC6 holds card type + 1 while a property card is active.
enum class BuyMode : uint8_t {
    Normal = 0xFF, Discount75 = 3, Discount50 = 4, Discount10 = 5,
    Rent = 6, Take = 7, BuyFromPlayer = 8, Credit = 9,
};

struct PlayerMoney {
    double cash = kStartingCash;   // player +0x42 (seg2:59E6 / 5A47 also update +0x4A)
    double creditDebt = 0;         // player +0x51A, card Kredit
    double monthlyIncome = 0;      // player +0x52, recomputed each month
    double propertyValue = 0;      // player +0x5A, recomputed each month
};

class Economy {
public:
    Economy(const std::vector<Town>& towns, int players);

    bool entryExists(int town, int entry) const;
    const TownState& town(int index) const { return states_[size_t(index)]; }
    const PlayerMoney& money(int player) const { return money_[size_t(player)]; }
    void addMoney(int player, double amount) { money_[size_t(player)].cash += amount; }

    // Price before the purchase for `mode` (value of the entry, times 0.75/0.5/0.1 for
    // the discount modes); 0 for modes that do not charge the buyer this way.
    double price(int town, int entry, BuyMode mode) const;
    // seg1:7DC5 without a factor pointer. Returns false when the preconditions fail.
    bool buy(int player, int town, int entry, BuyMode mode);
    // seg1:85F9: monthly income, burdens, rent expiry and property values.
    void settleMonth();

private:
    void updateWholeOwner(int town); // seg1:7CF6
    uint32_t value(int town, int entry) const;
    uint16_t incomeFactor(int town, int entry) const;

    const std::vector<Town>& towns_;
    std::vector<TownState> states_;
    std::vector<PlayerMoney> money_;
};

} // namespace dtvr
