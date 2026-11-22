// SPDX-License-Identifier: GPL-3.0-only
#include "economy.hpp"

#include <stdexcept>

namespace dtvr {
namespace {

constexpr double kIncomeRate = 0.001;   // DS:86BE
constexpr double kWholeTownFactor = 2.0; // DS:86C6, otherwise 1.0 (DS:86D6)
constexpr uint8_t kRentMonths = 12;      // seg1:7F51

} // namespace

Economy::Economy(const std::vector<Town>& towns, int players)
    : towns_(towns), states_(towns.size()), money_(size_t(players)) {
    if (players <= 0) throw std::invalid_argument("Economy: keine Spieler");
}

bool Economy::entryExists(int town, int entry) const {
    if (town < 0 || size_t(town) >= towns_.size() || entry < 0 || entry >= 10) return false;
    return !towns_[size_t(town)].entries[size_t(entry)].name.empty(); // name byte != 0 (seg1:7D2D)
}

uint32_t Economy::value(int town, int entry) const {
    return towns_[size_t(town)].entries[size_t(entry)].value;
}

uint16_t Economy::incomeFactor(int town, int entry) const {
    return towns_[size_t(town)].entries[size_t(entry)].words[0];
}

double Economy::price(int town, int entry, BuyMode mode) const {
    if (!entryExists(town, entry)) return 0;
    const double v = value(town, entry);
    switch (mode) {
    case BuyMode::Discount75: return v * 0.75; // DS:86A6
    case BuyMode::Discount50: return v * 0.5;  // DS:86AE
    case BuyMode::Discount10: return v * 0.1;  // DS:86B6
    case BuyMode::Normal:
    case BuyMode::Credit:
    case BuyMode::BuyFromPlayer: return v;
    default: return 0;
    }
}

bool Economy::buy(int player, int town, int entry, BuyMode mode) {
    if (player < 0 || size_t(player) >= money_.size() || !entryExists(town, entry)) return false;
    PropertyState& p = states_[size_t(town)].entries[size_t(entry)];
    PlayerMoney& buyer = money_[size_t(player)];
    const uint8_t me = uint8_t(player);
    switch (mode) {
    case BuyMode::Rent: // seg1:7E79: another player's entry, 12 months
        if (p.owner == kNoOwner || p.owner == me) return false;
        p.landlord = p.owner;
        p.monthsLeft = kRentMonths;
        p.owner = me;
        break;
    case BuyMode::Take: // seg1:7FA6
        if (p.owner == kNoOwner || p.owner == me) return false;
        p.owner = me;
        break;
    case BuyMode::BuyFromPlayer: { // seg1:8097: buyer pays the owner the value
        if (p.owner == kNoOwner || p.owner == me) return false;
        const double cost = value(town, entry);
        if (buyer.cash < cost) return false;
        money_[p.owner].cash += cost;
        buyer.cash -= cost;
        p.owner = me;
        break;
    }
    case BuyMode::Normal:
    case BuyMode::Discount75:
    case BuyMode::Discount50:
    case BuyMode::Discount10:
    case BuyMode::Credit: { // seg1:8192: free entries only
        if (p.owner != kNoOwner) return false;
        const double cost = price(town, entry, mode);
        if (mode == BuyMode::Credit) {
            buyer.creditDebt += cost; // paid later; no cash check (seg1:828B)
        } else {
            if (buyer.cash < cost) return false;
            buyer.cash -= cost;
        }
        p.owner = me;
        break;
    }
    }
    updateWholeOwner(town);
    return true;
}

void Economy::updateWholeOwner(int town) {
    TownState& t = states_[size_t(town)];
    t.wholeOwner = kNoOwner;
    uint8_t owner = kNoOwner;
    bool any = false;
    for (int e = 0; e < 10; ++e) {
        if (!entryExists(town, e)) continue;
        const uint8_t o = t.entries[size_t(e)].owner;
        if (o == kNoOwner) return;
        if (any && o != owner) return;
        owner = o;
        any = true;
    }
    if (any) t.wholeOwner = owner;
}

void Economy::settleMonth() {
    for (auto& m : money_) { m.monthlyIncome = 0; m.propertyValue = 0; }
    // Loop order of seg1:85F9: players outside, towns inside. A rent that expires
    // hands the entry back immediately, so a landlord with a higher index already
    // collects it in the same month.
    for (size_t player = 0; player < money_.size(); ++player) {
        for (size_t t = 0; t < states_.size(); ++t) {
            TownState& town = states_[t];
            const double factor = town.wholeOwner == player ? kWholeTownFactor : 1.0;
            for (int e = 0; e < 10; ++e) {
                PropertyState& p = town.entries[size_t(e)];
                if (!entryExists(int(t), e) || p.owner != player) continue;
                const double income = value(int(t), e) * kIncomeRate * incomeFactor(int(t), e) * factor;
                const uint8_t holder = p.landlord == kNoOwner ? p.owner : p.landlord;
                if (holder < money_.size()) money_[holder].propertyValue += value(int(t), e);
                PlayerMoney& m = money_[player];
                if (p.burden != 0) {
                    p.burden -= income;
                    if (p.burden < 0) { m.cash -= p.burden; p.burden = 0; } // surplus paid out
                } else {
                    m.cash += income;
                    m.monthlyIncome += income;
                }
                if (p.landlord != kNoOwner && --p.monthsLeft == 0) {
                    p.owner = p.landlord;
                    p.landlord = kNoOwner;
                }
            }
        }
    }
}

} // namespace dtvr
