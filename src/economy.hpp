// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "board.hpp"
#include "rules.hpp"

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

// seg1:8A3B: the settlement writes worth entries for years 0..300 (+0x6A + 4 * year).
constexpr int kWorthYears = 301;

struct PlayerMoney {
    double cash = kStartingCash;   // player +0x42 (seg2:59E6 / 5A47 also update +0x4A)
    double creditDebt = 0;         // player +0x51A, card Kredit
    double monthlyIncome = 0;      // player +0x52, recomputed each month
    double propertyValue = 0;      // player +0x5A, recomputed each month
    // +0x66: the year's rent, seg1:8A07 adds the truncated +0x52 each month, seg1:8BC6
    // clears it after the year balance ("Mieteinnahmen" of the balance board).
    int32_t yearRent = 0;
    // +0x6A: cash + property value per year, seg1:8A93 writes the truncated
    // ftol(+0x5A) + +0x42 into the entry of the running year (DS:A102) every month.
    std::array<int32_t, kWorthYears> worth{};
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
    // seg1:832A: sell an own, not rented entry; payout = value, x1.5 with the card
    // Verkaufen (DS:869E). The entry becomes free.
    bool sell(int player, int town, int entry, bool withCard);
    // seg1:85F9: monthly income, burdens, rent expiry and property values of the
    // players in play (slot order), their year's rent and the worth of `year` (DS:A102;
    // not written for years above 300 or below 0). Without arguments: every slot, no worth.
    void settleMonth(const std::vector<int>& activePlayers, int year);
    void settleMonth();
    // seg1:8BC6 after the year balance: the year's rent (+0x66) is cleared for all slots.
    void clearYearRent();
    // Year end (seg2:0784): seg1:8AA7 charges every positive credit debt before the
    // settlement; seg1:8B41 clears the debts afterwards.
    void chargeCredits(const std::vector<int>& activePlayers);
    void clearCredits();

    // Money cards (seg3 handlers; each ends the turn afterwards).
    double fiskus(int target);                       // 14AD: (cash + property) * 0.1
    double balanceAccounts(const std::vector<int>& activePlayers); // 15B0 Konto: average
    void zero(int player);                           // 18EA: cash = 0
    bool passDebt(int player, int target);           // 1790 Schulden: only if cash < 0

    // Used by Dr. Drago (round 10).
    struct PropertyRef { int town = -1; int entry = -1; };
    // seg2:B943: entries of `owner` (kNoOwner = free entries) with 0 < value <= limit;
    // the k-th of them in town/entry order with k = random(count - 1).
    PropertyRef pickProperty(uint8_t owner, double limit, MsRandom& rng) const;
    // seg1:832A / 7DC5 with a factor pointer: no ownership or cash checks.
    void sellWithFactor(int player, PropertyRef p, double factor);
    void buyWithFactor(int player, PropertyRef p, double factor);
    // seg2:5FF7 + seg1:8EE1: property values from owned entries, then the active
    // player with the lowest cash + property value (last of a descending sort).
    int poorest(const std::vector<int>& activePlayers);

    // seg8:4F1B: for every player slot and town entries 0..7 (8 and 9 are not
    // checked) owned by the player: an insured player keeps them (the card is
    // used once), otherwise the entry becomes free with burden and rent cleared.
    // Returns the players whose protection card was used.
    std::array<bool, 8> damageTown(int town, const std::array<bool, 8>& insured);
    bool townHasOwner(int town) const;

    // Save support: replaces all town and money state; false (unchanged) on a size mismatch.
    bool restore(const std::vector<TownState>& towns, const std::vector<PlayerMoney>& money);
    size_t townCount() const { return states_.size(); }
    size_t playerCount() const { return money_.size(); }

private:
    void updateWholeOwner(int town); // seg1:7CF6
    uint32_t value(int town, int entry) const;
    uint16_t incomeFactor(int town, int entry) const;

    const std::vector<Town>& towns_;
    std::vector<TownState> states_;
    std::vector<PlayerMoney> money_;
};

} // namespace dtvr
