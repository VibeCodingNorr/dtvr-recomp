// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "board.hpp"
#include "rules.hpp"

#include <array>
#include <string>
#include <vector>

namespace dtvr {

// Random events and the opening of Eastern Europe (round 11, docs/formats.md).
enum class EventKind {
    None, OpenEast, Rain, Storm, Snow, Rockfall, Robbers, Flood, Earthquake, Drought, Nessie,
    Volcano, ProfessorGood, ProfessorBad,
};
std::string eventName(EventKind kind);

struct EventClock {
    int turns = 0;      // DS:A0FE, +1 per player change
    int countdown = 0;  // DS:A108, player changes until the next event
    int hold = 0;       // DS:A111, counts down (use open)
};

// seg8:1B91: countdown = active * (10 - year / 32) + random(active).
int nextEventCountdown(int activePlayers, int year, MsRandom& rng);

// seg8:180B: table by progress (active players n): turns < 120 / n -> table 1,
// month < 500 / n -> table 2, month < 900 / n -> table 3, else table 4; entry random(15).
EventKind chooseEvent(int activePlayers, int turns, int month, MsRandom& rng);

// seg8:1700-17E9: the East opens instead of an event when the sum of all players'
// property values exceeds (8 + 2n) % of `westValue`, or after year 40.
bool eastShouldOpen(double westValue, double ownedValue, int activePlayers, int year);

// seg1:A15B: total value of all entries of towns on kind-4/6 fields whose
// flagset does not contain the mask (0x4000 = Eastern Europe).
double westPropertyValue(const Board& board, const std::vector<Town>& towns, uint16_t mask);

// Links that cross a cell with tile flag 0x20 (border barriers until the East
// opens; seg2:2404 stops a vehicle there unless an obstacle entry allows passing).
// Index: field * 4 + (direction - 1).
std::vector<bool> barrierLinks(const Board& board, const WorldMap& map, const TileProperties& props);

// seg8:1FDB: the first active player (slot order) with cash > 500,000 x inflation
// who stands on a field with a right-hand link (the original also checks that the
// cell two to the right is no sea route; pass that check result per player).
int robberyVictim(const std::vector<PlayerSlot>& players, const Board& board,
                  const std::vector<double>& cash, double inflation,
                  const std::vector<bool>& seaTwoRight);

} // namespace dtvr
