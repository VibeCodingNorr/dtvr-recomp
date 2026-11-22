// SPDX-License-Identifier: GPL-3.0-only
// Tests for Dr. Drago (round 10) and the related economy and turn rules.
#include "drago.hpp"
#include "economy.hpp"

#include <cstdio>
#include <stdexcept>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    // Placement and countdown 4..7.
    DragoState d;
    MsRandom rng(1), ref(1);
    placeDrago(d, 2, rng);
    CHECK(d.holder == 2 && d.countdown == ref.random(3) + 4 && d.countdown >= 4 && d.countdown <= 7);

    // Form changes only on the holder's turn when the countdown reaches zero.
    d.countdown = 2;
    CHECK(!dragoTurnStart(d, 1, 0, rng) && d.countdown == 2);       // not the holder
    CHECK(!dragoTurnStart(d, 2, 0, rng) && d.countdown == 1);
    CHECK(dragoTurnStart(d, 2, 0, rng) && d.form == 1 && d.countdown >= 4);
    d.countdown = 1;
    CHECK(dragoTurnStart(d, 2, 3, rng) && d.form == 0);             // year <= 5: back to 0
    d.form = 2; d.countdown = 1;
    CHECK(dragoTurnStart(d, 2, 9, rng) && d.form == 1);
    int twos = 0;
    for (int i = 0; i < 50; ++i) {
        d.form = 1; d.countdown = 1;
        dragoTurnStart(d, 2, 9, rng);
        CHECK(d.form == 0 || d.form == 2);
        twos += d.form == 2;
    }
    CHECK(twos > 0 && twos < 50);
    DragoState none;
    CHECK(!dragoTurnStart(none, 0, 0, rng));

    // Action tables.
    CHECK(chooseDragoAction(0, rng) == DragoAction::Spend);
    bool sawBlock = false;
    for (int i = 0; i < 200; ++i) {
        CHECK(chooseDragoAction(1, rng) != DragoAction::Block);
        sawBlock |= chooseDragoAction(2, rng) == DragoAction::Block;
    }
    CHECK(sawBlock);

    // Passing on meetings and random other players.
    std::vector<PlayerSlot> players(8);
    players[0].status = players[1].status = players[3].status = 1;
    players[0].field = 5; players[1].field = 7; players[3].field = 5;
    DragoState meet;
    meet.holder = 0;
    CHECK(passDragoOnMeeting(meet, players, 0) == 3);                // holder meets player 3
    CHECK(passDragoOnMeeting(meet, players, 1) == 3);                // nobody on field 7
    players[1].field = 5;
    CHECK(passDragoOnMeeting(meet, players, 1) == 1);                // stops on the holder's field
    for (int i = 0; i < 30; ++i) {
        const int o = randomOtherPlayer(players, 0, rng);
        CHECK(o == 1 || o == 3);
    }
    std::vector<PlayerSlot> alone(8);
    alone[4].status = 1;
    CHECK(randomOtherPlayer(alone, 4, rng) == 4);

    // Drago amounts: blue table x form factor, times inflation.
    for (int i = 0; i < 20; ++i) {
        const double a = dragoAmount(0, 2, 1.0, rng);               // January, form 2: 3600..4800
        CHECK(a >= 3600 && a <= 4800 && int(a) % 100 == 0);
        CHECK(dragoAmount(6, 0, 2.0, rng) >= 18000);                 // July, form 0 x2: 9000..12000 x2
    }
    bool threw = false;
    try { dragoAmount(0, 3, 1.0, rng); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);

    // Property picks, gifts and the poorest player.
    std::vector<Town> towns(1);
    towns[0].entries[0].name = "A"; towns[0].entries[0].value = 20000;
    towns[0].entries[1].name = "B"; towns[0].entries[1].value = 50000;
    Economy eco(towns, 3);
    auto freeOne = eco.pickProperty(kNoOwner, 30000, rng);
    CHECK(freeOne.town == 0 && freeOne.entry == 0);                  // B is above the limit
    CHECK(eco.pickProperty(0, 30000, rng).town == -1);
    eco.buyWithFactor(1, freeOne, 1.5);
    CHECK(eco.money(1).cash == 30000 - 30000 && eco.town(0).entries[0].owner == 1);
    CHECK(eco.pickProperty(1, 30000, rng).entry == 0);
    CHECK(eco.poorest({0, 1, 2}) == 1);                              // 0 + 20000 property
    Economy tie(towns, 3);
    CHECK(tie.poorest({0, 1, 2}) == 2);                              // ties: last of the sort
    eco.sellWithFactor(1, freeOne, 0.0);
    CHECK(eco.town(0).entries[0].owner == kNoOwner && eco.money(1).cash == 0);
    CHECK(eco.poorest({0, 1, 2}) == 1);

    // nextPlayer: Dr. Drago's block makes the holder sit out once; hooks run.
    std::vector<PlayerSlot> turn(8);
    turn[0].status = turn[1].status = 1;
    turn[1].flags = kFlagDragoBlock;
    Calendar cal;
    TurnHooks hooks;
    int settled = 0, sitOutHook = 0;
    hooks.settleMonth = [&] { ++settled; };
    hooks.afterSitOut = [&](int) { ++sitOutHook; };
    hooks.dragoHolder = 1;
    auto tc = nextPlayer(turn, 0, cal, rng, hooks);
    CHECK(tc.player == 0 && tc.satOut == std::vector<int>{1} && turn[1].flags == 0 && settled == 1);
    CHECK(sitOutHook == 0);                                          // no Drago action on a block
    turn[1].flags = kFlagDragoBlock;
    hooks.dragoHolder = 0;                                           // no longer holding Dr. Drago
    tc = nextPlayer(turn, 0, cal, rng, hooks);
    CHECK(tc.player == 1 && turn[1].flags == 0);
    turn[0].flags = 0x2;
    tc = nextPlayer(turn, 1, cal, rng, hooks);
    CHECK(tc.player == 1 && sitOutHook == 1);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("drago_test: OK");
    return failures != 0;
}
