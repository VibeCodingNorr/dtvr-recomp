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
        const double cost = value(town, entry);
        if (p.owner == kNoOwner) { // seg1:806C -> 8176: a free entry is a normal purchase (round 147)
            if (buyer.cash < cost) return false; // seg1:8287
            buyer.cash -= cost;
            p.owner = me;
            break;
        }
        // The own entry (seg1:8093) or too little cash (seg1:80E4) reach seg1:8176 too,
        // where seg1:8192 refuses an entry that is not free.
        if (p.owner == me || buyer.cash < cost) return false;
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

bool Economy::sell(int player, int town, int entry, bool withCard) {
    if (player < 0 || size_t(player) >= money_.size() || !entryExists(town, entry)) return false;
    PropertyState& p = states_[size_t(town)].entries[size_t(entry)];
    if (p.owner != uint8_t(player) || p.landlord != kNoOwner) return false;
    p.owner = kNoOwner;
    money_[size_t(player)].cash += value(town, entry) * (withCard ? 1.5 : 1.0);
    updateWholeOwner(town);
    return true;
}

void Economy::chargeCredits(const std::vector<int>& activePlayers) {
    for (int p : activePlayers) {
        PlayerMoney& m = money_[size_t(p)];
        if (m.creditDebt > 0) m.cash -= m.creditDebt;
    }
}

void Economy::clearCredits() {
    for (auto& m : money_)
        if (m.creditDebt > 0) m.creditDebt = 0;
}

double Economy::fiskus(int target) {
    PlayerMoney& m = money_[size_t(target)];
    const double tax = (m.cash + m.propertyValue) * 0.1; // DS:8636
    m.cash -= tax;
    return tax;
}

double Economy::balanceAccounts(const std::vector<int>& activePlayers) {
    if (activePlayers.size() <= 1) return activePlayers.empty() ? 0 : money_[size_t(activePlayers[0])].cash;
    double sum = 0;
    for (int p : activePlayers) sum += money_[size_t(p)].cash;
    const double average = sum / double(activePlayers.size());
    for (int p : activePlayers) money_[size_t(p)].cash = average;
    return average;
}

void Economy::zero(int player) { money_[size_t(player)].cash = 0; }

bool Economy::passDebt(int player, int target) {
    PlayerMoney& m = money_[size_t(player)];
    if (m.cash >= 0) return false;
    money_[size_t(target)].cash += m.cash; // target takes the negative balance
    m.cash = 0;
    return true;
}

Economy::PropertyRef Economy::pickProperty(uint8_t owner, double limit, MsRandom& rng) const {
    auto eligible = [&](size_t t, int e) {
        const double v = towns_[t].entries[size_t(e)].value;
        return v != 0 && v <= limit && states_[t].entries[size_t(e)].owner == owner;
    };
    int count = 0;
    for (size_t t = 0; t < states_.size(); ++t)
        for (int e = 0; e < 10; ++e) count += eligible(t, e);
    if (count == 0) return {};
    int k = rng.random(count - 1);
    for (size_t t = 0; t < states_.size(); ++t)
        for (int e = 0; e < 10; ++e)
            if (eligible(t, e) && k-- == 0) return {int(t), e};
    return {};
}

void Economy::sellWithFactor(int player, PropertyRef p, double factor) {
    states_[size_t(p.town)].entries[size_t(p.entry)].owner = kNoOwner;
    money_[size_t(player)].cash += value(p.town, p.entry) * factor;
    updateWholeOwner(p.town);
}

void Economy::buyWithFactor(int player, PropertyRef p, double factor) {
    states_[size_t(p.town)].entries[size_t(p.entry)].owner = uint8_t(player);
    money_[size_t(player)].cash -= value(p.town, p.entry) * factor;
    updateWholeOwner(p.town);
}

std::array<bool, 8> Economy::damageTown(int town, const std::array<bool, 8>& insured) {
    std::array<bool, 8> used{};
    TownState& t = states_[size_t(town)];
    for (size_t p = 0; p < 8 && p < money_.size(); ++p)
        for (size_t e = 0; e < 8; ++e) {
            PropertyState& s = t.entries[e];
            if (s.owner != p) continue;
            if (insured[p]) { used[p] = true; continue; }
            s.owner = kNoOwner;
            s.burden = 0;
            s.monthsLeft = 0;
            if (s.landlord != kNoOwner) s.landlord = kNoOwner;
        }
    updateWholeOwner(town);
    return used;
}

bool Economy::townHasOwner(int town) const {
    for (const auto& e : states_[size_t(town)].entries)
        if (e.owner != kNoOwner) return true;
    return false;
}

int Economy::poorest(const std::vector<int>& activePlayers) {
    for (size_t p = 0; p < money_.size(); ++p) {
        money_[p].propertyValue = 0;
        for (size_t t = 0; t < states_.size(); ++t)
            for (int e = 0; e < 10; ++e)
                if (states_[t].entries[size_t(e)].owner == p) money_[p].propertyValue += value(int(t), e);
    }
    int result = -1;
    double lowest = 0;
    for (int p : activePlayers) { // last of a stable descending order = lowest, later index on ties
        const double worth = money_[size_t(p)].cash + money_[size_t(p)].propertyValue;
        if (result < 0 || worth <= lowest) { result = p; lowest = worth; }
    }
    return result;
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

bool Economy::restore(const std::vector<TownState>& towns, const std::vector<PlayerMoney>& money) {
    if (towns.size() != states_.size() || money.size() != money_.size()) return false;
    states_ = towns;
    money_ = money;
    return true;
}

} // namespace dtvr
