// SPDX-License-Identifier: GPL-3.0-only
#include "drago.hpp"

#include <array>

namespace dtvr {
namespace {

using A = DragoAction;
// Far-pointer tables built on the stack at seg2:B5A0 (form 1) and seg2:B640 (form 2);
// seg2:BDA8 in table B forwards to "give property".
constexpr std::array<DragoAction, 16> kFormOne = {
    A::Spend, A::Spend, A::Spend, A::Spend, A::GiveMoney, A::GiveMoney, A::GiveMoney, A::GiveMoney,
    A::GiveMoney, A::GiveMoney, A::TearCard, A::TearCard, A::GiveProperty, A::GiveProperty, A::Buy, A::Buy};
constexpr std::array<DragoAction, 16> kFormTwo = {
    A::Spend, A::Block, A::Block, A::Block, A::GiveMoney, A::GiveMoney, A::GiveMoney, A::TearCard,
    A::TearCard, A::GiveProperty, A::Buy, A::Buy, A::Buy, A::GiveProperty, A::GiveProperty, A::GiveProperty};

} // namespace

void placeDrago(DragoState& drago, int player, MsRandom& rng) {
    drago.holder = player;
    drago.countdown = rng.random(3) + 4;
}

bool dragoTurnStart(DragoState& drago, int current, int year, MsRandom& rng) {
    if (drago.holder < 0 || drago.holder != current) return false;
    if (drago.countdown > 0) --drago.countdown;
    if (drago.countdown != 0) return false;
    drago.countdown = rng.random(3) + 4;
    if (drago.form == 0 || drago.form == 2) drago.form = 1;
    else drago.form = (year > 5 && rng.random(2) != 0) ? 2 : 0;
    return true;
}

DragoAction chooseDragoAction(int form, MsRandom& rng) {
    if (form == 0) return A::Spend;
    const auto& table = form == 1 ? kFormOne : kFormTwo;
    return table[size_t(rng.random(15))];
}

int passDragoOnMeeting(DragoState& drago, const std::vector<PlayerSlot>& players, int current) {
    if (drago.holder < 0 || activePlayerCount(players) <= 1) return drago.holder;
    const int here = players[size_t(current)].field;
    if (drago.holder == current) {
        for (int p = 0; p < kPlayerSlots && size_t(p) < players.size(); ++p)
            if (p != current && players[size_t(p)].status && players[size_t(p)].field == here) {
                drago.holder = p;
                break;
            }
    } else if (players[size_t(drago.holder)].field == here) {
        drago.holder = current;
    }
    return drago.holder;
}

int randomOtherPlayer(const std::vector<PlayerSlot>& players, int player, MsRandom& rng) {
    if (activePlayerCount(players) == 1) return player;
    std::vector<int> others;
    for (int p = 0; p < kPlayerSlots && size_t(p) < players.size(); ++p)
        if (players[size_t(p)].status && p != player) others.push_back(p);
    if (others.empty()) return player;
    return others[size_t(rng.random(int(others.size()) - 1))];
}

} // namespace dtvr
