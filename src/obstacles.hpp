// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "board.hpp"
#include "rules.hpp"

#include <array>
#include <vector>

namespace dtvr {

// Road obstacles (round 12, docs/formats.md): table DS:5E with 30 entries of
// 0x17 bytes (+0 type, +6/+8 cell, +0xA duration, ...). The obstacle tile gets
// tile flag 0x20 (seg2:3305), so a vehicle entering the cell runs seg2:2404.
enum ObstacleType : uint16_t {
    kRockfall = 0x4, kSnow = 0x8, kRain = 0x10, kNails = 0x900,
    // Not road obstacles: delayed investment results that use the same table
    // (seg8:7311 / 7343, paid out on expiry by seg8:652C / 6394).
    kProfessorInvestment = 0x400, kFootballInvestment = 0x800,
};

struct Obstacle {
    uint16_t type = 0;  // 0 = free slot
    int duration = 0;   // player changes left (+0xA)
    int x = 0, y = 0;   // map cell
    int field = -1;     // link the cell lies on: field and direction (1..4)
    int dir = 0;
    int player = -1;    // investments: investor (stored in the layer byte +0xE)
    double amount = 0;  // investments: invested sum (+0xF)
};

// Card type that clears an obstacle (seg2:2455 / 252A via seg3:0D26): Schneepflug
// (type 36) for rockfall, snow and rain, Nägel weg (type 38) for nails; -1 otherwise.
int clearingCard(uint16_t type);

class ObstacleTable {
public:
    static constexpr int kSlots = 30;
    // seg8:67F8: removes an obstacle on the same cell, then uses the first free slot.
    bool add(const Obstacle& o);
    // seg8:61FE once per turn change: expire entries at 0, otherwise count down.
    // Returns the removed obstacles.
    std::vector<Obstacle> tick();
    // Index of the obstacle on the link field/dir (either direction), or -1.
    int onLink(const Board& board, int field, Direction dir) const;
    void remove(int index) { slots_[size_t(index)] = Obstacle{}; } // seg8:697C
    const std::array<Obstacle, kSlots>& slots() const { return slots_; }
    int count() const;

private:
    std::array<Obstacle, kSlots> slots_{};
};

// Link (field, direction) whose inner cells contain map cell x/y; field -1 if none.
struct LinkRef { int field = -1; int dir = 0; };
LinkRef linkAtCell(const Board& board, int x, int y);

// seg8:5F43 for land events (mode 0): a random active player (seg2:5F03(0xFF)),
// field distances from the destination (with the route mask), k = random(d - 1)
// and k + 1 steps along the shortest way; then the first link (up, right, down,
// left) whose cell two steps away is not a sea route and not a foreground cell
// (map/props optional). Returns field -1 when no cell qualifies.
struct ObstacleSpot { int field = -1; int dir = 0; int x = 0; int y = 0; };
ObstacleSpot pickObstacleSpot(const Board& board, const std::vector<PlayerSlot>& players, int current,
                              int destination, uint16_t routeMask, MsRandom& rng,
                              const WorldMap* map, const TileProperties* props);

} // namespace dtvr
