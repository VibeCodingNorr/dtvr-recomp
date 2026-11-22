// SPDX-License-Identifier: GPL-3.0-only
#include "obstacles.hpp"

#include <cstdlib>

namespace dtvr {
namespace {

constexpr int kDx[4] = {0, 1, 0, -1}, kDy[4] = {-1, 0, 1, 0};

// seg2:5F03 with 0xFF: any active player, random(n - 1); the current one if alone.
int randomActive(const std::vector<PlayerSlot>& players, int current, MsRandom& rng) {
    std::vector<int> active;
    for (int p = 0; p < kPlayerSlots && size_t(p) < players.size(); ++p)
        if (players[size_t(p)].status) active.push_back(p);
    if (active.size() <= 1) return current;
    return active[size_t(rng.random(int(active.size()) - 1))];
}

} // namespace

int clearingCard(uint16_t type) {
    if (type == kRockfall || type == kSnow || type == kRain) return 36; // card number 0x25
    if (type == kNails) return 38;                                    // card number 0x27
    return -1;
}

bool ObstacleTable::add(const Obstacle& o) {
    for (auto& s : slots_)
        if (s.type && s.x == o.x && s.y == o.y) s = Obstacle{}; // seg8:680C -> 697C
    for (auto& s : slots_)
        if (!s.type) { s = o; return true; }
    return false;
}

std::vector<Obstacle> ObstacleTable::tick() {
    std::vector<Obstacle> removed;
    for (auto& s : slots_) {
        if (!s.type) continue;
        if (s.duration == 0) { removed.push_back(s); s = Obstacle{}; }
        else --s.duration;
    }
    return removed;
}

int ObstacleTable::onLink(const Board& board, int field, Direction dir) const {
    if (field < 0 || dir == Direction::None) return -1;
    const int16_t other = board.fields[size_t(field)].next[size_t(dir) - 1];
    for (int i = 0; i < kSlots; ++i) {
        const Obstacle& s = slots_[size_t(i)];
        if (!s.type) continue;
        if (s.field == field && s.dir == int(dir)) return i;
        if (s.field == other && s.dir == int(opposite(dir))) return i;
    }
    return -1;
}

int ObstacleTable::count() const {
    int n = 0;
    for (const auto& s : slots_) n += s.type != 0;
    return n;
}

LinkRef linkAtCell(const Board& board, int x, int y) {
    for (size_t i = 0; i < board.fields.size(); ++i) {
        const Field& f = board.fields[i];
        for (int k = 1; k <= 2; ++k) { // right and down cover every link once
            const int16_t n = f.next[size_t(k)];
            if (n == Field::kNone) continue;
            const Field& g = board.fields[size_t(n)];
            const int steps = std::abs(g.x - f.x) + std::abs(g.y - f.y);
            for (int s = 1; s < steps; ++s)
                if (f.x + kDx[k] * s == x && f.y + kDy[k] * s == y) return {int(i), k + 1};
        }
    }
    return {};
}

ObstacleSpot pickObstacleSpot(const Board& board, const std::vector<PlayerSlot>& players, int current,
                              int destination, uint16_t routeMask, MsRandom& rng,
                              const WorldMap* map, const TileProperties* props) {
    const int player = randomActive(players, current, rng);
    int at = players[size_t(player)].field;
    if (at < 0) return {};
    const auto dist = fieldDistances(board, destination, routeMask);
    auto d = [&](int f) { return unsigned(dist[size_t(f)]); }; // -1 compares as 0xFFFF
    int k = rng.random(int(dist[size_t(at)]) - 1);
    for (int guard = 0; d(at) != 0 && guard < 1000; ++guard) {
        int next = -1;
        for (int16_t n : board.fields[size_t(at)].next)
            if (n != Field::kNone && d(n) < d(at)) { next = n; break; }
        if (next < 0) break;
        at = next;
        if (k == 0) break;
        --k;
    }
    const Field& f = board.fields[size_t(at)];
    for (int dir = 0; dir < 4; ++dir) {
        if (f.next[size_t(dir)] == Field::kNone) continue;
        const int x = f.x + kDx[dir] * 2, y = f.y + kDy[dir] * 2;
        if (map && props) {
            if (props->cell(*map, x, y) & TileProperties::kSeaRoute) continue;
            if (map->cell(1, x, y) & 0x8000) continue; // seg6:2CF7
        }
        return {at, dir + 1, x, y};
    }
    return {};
}

} // namespace dtvr
