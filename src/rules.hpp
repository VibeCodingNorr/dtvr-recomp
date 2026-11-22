// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "board.hpp"
#include "worldmap.hpp"

#include <array>
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

// Fields where a walk of exactly `steps` field steps can end when every step is
// counted by MoveCounter (reversals undo, so only non-reversing walks count).
std::vector<int> fieldsAfterSteps(const Board& board, int start, int steps);

} // namespace dtvr
