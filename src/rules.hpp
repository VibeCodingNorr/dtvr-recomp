// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "board.hpp"
#include "worldmap.hpp"

#include <array>
#include <functional>
#include <string>
#include <cstdint>
#include <vector>

namespace dtvr {

// Microsoft C runtime rand()/srand() as linked into rally.exe (seg10:1DDE / 1DC6).
class MsRandom {
public:
    explicit MsRandom(uint16_t seed = 1) { srand(seed); }
    void srand(uint16_t seed) { state_ = seed; } // high word cleared, as at seg10:1DD1
    int rand();
    // seg6:1343: n <= 0 -> 0, otherwise rand() % (n + 1), i.e. 0..n.
    int random(int n);
    uint32_t state() const { return state_; }
    void setState(uint32_t state) { state_ = state; } // save support

private:
    uint32_t state_ = 1;
};

// Slot machine (seg2:6426 setup, seg2:776A reels, seg2:7EC2 sum): each reel is
// random(maximum) + 1; the move allowance is the sum. WM_USER+6 uses 1 reel with
// maximum 5 (a value of 1..6).
std::vector<int> spinReels(MsRandom& rng, int reels, int maximum);

enum class Direction : uint8_t { None = 0, Up = 1, Right = 2, Down = 3, Left = 4 };
Direction opposite(Direction d); // table DS:19E4 = {0, 3, 4, 1, 2}

// europe.pit: 8192 uint16; entries 0x1000 + tile hold per-tile flags (seg2:3260).
class TileProperties {
public:
    static constexpr uint16_t kField = 0x01;     // field marker cell
    static constexpr uint16_t kRoad = 0x02;      // movement requires it in the step direction
    static constexpr uint16_t kSeaRoute = 0x04;  // with kRoad: seaway (seg2:1EB5 checks both)
    static constexpr uint16_t kWater = 0x08;
    static constexpr uint16_t kViewPoint = 0x10; // stop only with [Setup] Views (seg2:1DE2)
    static constexpr uint16_t kUnknown20 = 0x20;

    explicit TileProperties(const std::vector<uint8_t>& pit); // throws on wrong size
    uint16_t tile(unsigned index) const { return flags_[index & WorldMap::kTileMask]; }
    // OR of both layers' tile flags; cells outside the map read tile 0 like seg6:26B6.
    uint16_t cell(const WorldMap& map, int x, int y) const;

private:
    std::array<uint16_t, 0x1000> flags_{};
};

// Field step counter of seg2:3620. `allowance` = -1 means unlimited, 0 blocks movement.
// A step opposite to the last recorded one takes that step back.
class MoveCounter {
public:
    explicit MoveCounter(int allowance = 0) : allowance_(allowance) {}
    // Save support: a counter with the steps already taken.
    MoveCounter(int allowance, std::vector<Direction> history) : allowance_(allowance), history_(std::move(history)) {}
    const std::vector<Direction>& history() const { return history_; }
    bool step(Direction d, bool commit = true);
    int allowance() const { return allowance_; }
    int taken() const { return int(history_.size()); }
    bool lastWasBacktrack() const { return backtrack_; }

private:
    int allowance_;
    std::vector<Direction> history_;
    bool backtrack_ = false;
};

// Breadth-first distances over the field graph from `start` (seg2:43A8): at most
// 80 levels; a field whose flagset & blockMask != 0 is not expanded. -1 = unreached.
std::vector<int> fieldDistances(const Board& board, int start, uint16_t blockMask = 0);

// Route of seg2:9996: distances from `to` with no mask, then from `from` always take
// the first neighbour (up, right, down, left) with a smaller distance. Includes both
// ends; empty if unreachable.
std::vector<int> fieldRoute(const Board& board, int from, int to);

// Arrival (seg2:0D38): the field action runs when the vehicle is on a field,
// grid-aligned ((x | y) & 15 == 0, seg2:2C0F), the steps taken equal the
// allowance or the allowance is unlimited, and no dialog is open (DS:9EDE == 0).
bool arrivalTriggers(int field, int x, int y, int taken, int allowance, bool dialogOpen);

// Money outcome of a blue (kind 1, gain) or red (kind 2, loss) field, seg2:0D85 /
// seg2:0F15. Month tables from seg2:0BD7..0C4A, factors DS:85DE (0.5) and DS:85E6
// (0.75). Random calls in the original order: amount, then message.
struct MoneyEvent {
    double amount = 0;   // positive for kind 1, negative for kind 2; inflation applied
    int base = 0;        // amount before inflation
    int minimum = 0, maximum = 0; // range for this month before inflation
    int textId = 0;      // string resource 4001+0..14 (blue) or 4101+0..13 (red)
};
MoneyEvent moneyField(int kind, int month, double inflation, MsRandom& rng);
// The range only, without consuming random numbers.
MoneyEvent moneyRange(int kind, int month);

// Dr. Drago's money amount (seg2:B793): blue-field month table x form factor
// (0.5, 1.0, 2.0 for forms 0, 1, 2), drawn like a money field, times inflation.
double dragoAmount(int month, int form, double inflation, MsRandom& rng);

// Yearly inflation step (seg2:079E): factor *= 1.01 + 0.01 * random(5), i.e. +1..6 %.
// The game starts with 1.0 (seg4:0DFD copies DS:8586).
double inflateYear(double factor, MsRandom& rng);

// Destination town reward (seg2:11A6): 300,000 (DS:85EE) or, when DS:A104 != 0,
// 100,000 (DS:85F6), times inflation.
double destinationReward(bool alternative, double inflation);

// ---- Players, months and stages (round 6) ----

// The parts of a 0x522-byte player record that the turn logic reads.
struct PlayerSlot {
    uint8_t status = 0;   // +0x00: 0 = inactive; 1 is required for "humans left" (seg2:5C5A)
    uint16_t flags = 0;   // +0x40: bit 1 / bit 2 = sit out one turn, bit 3 cleared on turn change
    int field = -1;       // +0x3C: field the player stands on
};
constexpr int kPlayerSlots = 8;
int activePlayerCount(const std::vector<PlayerSlot>& players); // seg2:0368
// First active slot (seg2:1A41); 0 when none is active.
int firstActivePlayer(const std::vector<PlayerSlot>& players);

struct Calendar {
    int month = 0;          // DS:A100, counts up without wrapping; month % 12 is used
    int year = 0;           // DS:A102
    double inflation = 1.0; // DS:A11B
};

struct TurnChange {
    int player = -1;               // -1 when no player is active
    int monthsAdvanced = 0;
    int yearsEnded = 0;
    std::vector<int> satOut;       // players that consumed a sit-out flag
};
// Callbacks so the economy runs at the original points of seg2:0784-07EB.
struct TurnHooks {
    std::function<void()> chargeCredits;    // year end, before the settlement (seg1:8AA7)
    std::function<void()> settleMonth;      // every month advance (seg1:85F9)
    std::function<void()> clearCredits;     // year end, after inflation (seg1:8B41)
    std::function<void(int)> afterSitOut;   // Dr. Drago acts on a sit-out turn (seg2:0977)
    std::function<void(int)> afterPlayerChange; // event trigger seg8:1624 (seg2:07EF/07FC)
    int dragoHolder = -1;                   // DS:9CFC for flag bit 0
};

// nextPlayer of seg2:06FC: cyclic order over 8 slots skipping inactive ones. When
// the order reaches the first active slot the month advances; at month % 12 == 11
// the year ends first (credits, settlement, year + 1, inflation, clear credits).
// seg2:089D then checks the new player's flags: bit 0 (Dr. Drago's block) is
// cleared and the player sits out only while holding Dr. Drago (seg2:C00C, text
// random(4)); otherwise bit 1 or bit 2 make the player sit out once (text random(2),
// then the sit-out hook). The search continues after a sit-out.
TurnChange nextPlayer(std::vector<PlayerSlot>& players, int current, Calendar& calendar, MsRandom& rng,
                      const TurnHooks& hooks = {});

// Target player of a card (seg2:51EA): with one active player the first active
// slot, with two the other active player, otherwise the requested active
// player other than `current` (chosen in a dialog); -1 if that is not valid.
int resolveTarget(const std::vector<PlayerSlot>& players, int current, int requested);
// seg2:51EA(0xFF): like resolveTarget, but every active player (the current one
// included) may be chosen; with two players the dialog is shown as well.
int resolveAnyTarget(const std::vector<PlayerSlot>& players, int requested);

// seg2:47D2: random(fields - 1) until the field is not masked (DS:A10F) and its
// cell, inflated by 3 (seg2:46C8), does not intersect the visible map area.
struct CellRect { int left = 0, top = 0, right = 0, bottom = 0; };
int pickHiddenField(const Board& board, uint16_t routeMask, const CellRect& view, MsRandom& rng);
constexpr uint16_t kFlagCardsBlocked = 0x08; // checked at seg2:0B29, texts 1020-1022
constexpr uint16_t kFlagSteerless = 0x20;    // card Steuerlos (seg3:1994), used at seg2:0A6F

// Start field: first kind-6 field whose town name equals `name` ignoring ASCII case
// (seg2:0000 with DS:1A00 = "LONDON", compared by _fstricmp seg10:22CA).
int startField(const Board& board, const std::vector<Town>& towns, const std::string& name = "LONDON");

// New destination (seg5:1E2C): candidates are all kind-6 fields in field order
// (seg5:1D70). Repeat idx = random(count) until the slot is filled, differs from
// `origin` and `current`, its field has no flagset & mask bits, and no active
// player's +0x3C equals idx. The last test compares a field number with the list
// index; this mirrors the original code. Returns -1 if no candidate exists.
int chooseDestination(const Board& board, const std::vector<PlayerSlot>& players, int origin,
                      int current, uint16_t mask, MsRandom& rng);

// Stages: DS:A104 = remaining stages from [MAIN] Rounds (new-game default 99).
struct StageResult {
    double reward = 0;  // 100,000 or 300,000 times inflation
    bool gameOver = false;
    bool lastStageNext = false; // remaining dropped to 0: the next destination ends the game
    int remaining = 0;
};
// Reaching the destination (seg2:11A6 / 12D8 / 1438).
StageResult reachDestination(int remaining, double inflation);

// Fields where a walk of exactly `steps` field steps can end when every step is
// counted by MoveCounter (reversals undo, so only non-reversing walks count).
std::vector<int> fieldsAfterSteps(const Board& board, int start, int steps);

} // namespace dtvr
