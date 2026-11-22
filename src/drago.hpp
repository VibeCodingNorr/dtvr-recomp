// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "rules.hpp"

#include <vector>

namespace dtvr {

// Dr. Drago rides along with one player (DS:9CFC) and acts against that player
// after field actions (seg2:B58C). Evidence: docs/formats.md, round 10.
enum class DragoAction { Spend, Buy, GiveProperty, TearCard, GiveMoney, Block };

struct DragoState {
    int holder = -1;    // DS:9CFC, -1 (0xFF) = not in play; a new game starts without him
    int form = 0;       // DS:9CFD, initial value 0 from the data segment; F1/F2/F3
    int countdown = 0;  // DS:3DB4, turns until the next form change
};

// seg2:9C97 / B477: Dr. Drago joins `player`; the countdown restarts at
// random(3) + 4 (seg2:B48D).
void placeDrago(DragoState& drago, int player, MsRandom& rng);

// seg2:B3A4 at the start of the holder's turn: count down (seg2:B4A4); at zero
// restart the countdown and change form: 0 or 2 -> 1; 1 -> 2 after year 5 when
// random(2) != 0, otherwise 0. Returns true when the form changed.
bool dragoTurnStart(DragoState& drago, int current, int year, MsRandom& rng);

// seg2:B58C: form 0 always spends money; forms 1 and 2 pick from two 16-entry
// tables with random(15). Only meaningful when the holder is the current player.
DragoAction chooseDragoAction(int form, MsRandom& rng);

// seg2:B290 when a vehicle stops on a field: the holder hands Dr. Drago to the
// first other active player on the same field; otherwise a player stopping on
// the holder's field takes him. Returns the new holder.
int passDragoOnMeeting(DragoState& drago, const std::vector<PlayerSlot>& players, int current);

// seg2:5F03: a random other active player (random(n - 1) over the others in
// slot order); the player itself when it is the only active one.
int randomOtherPlayer(const std::vector<PlayerSlot>& players, int player, MsRandom& rng);

constexpr uint16_t kFlagDragoBlock = 0x01; // seg2:BFD1, sits out while holding Dr. Drago

} // namespace dtvr
