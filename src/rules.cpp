// SPDX-License-Identifier: GPL-3.0-only
#include "rules.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace dtvr {

int MsRandom::rand() {
    state_ = state_ * 0x343FDu + 0x269EC3u;
    return int((state_ >> 16) & 0x7FFF);
}

int MsRandom::random(int n) { return n <= 0 ? 0 : rand() % (n + 1); }

std::vector<int> spinReels(MsRandom& rng, int reels, int maximum) {
    std::vector<int> values;
    for (int i = 0; i < reels; ++i) values.push_back(rng.random(maximum) + 1);
    return values;
}

Direction opposite(Direction d) {
    static constexpr Direction table[5] = {Direction::None, Direction::Down, Direction::Left,
                                           Direction::Up, Direction::Right};
    return table[size_t(d) < 5 ? size_t(d) : 0];
}

TileProperties::TileProperties(const std::vector<uint8_t>& pit) {
    if (pit.size() != 0x4000) throw std::runtime_error("europe.pit: erwartet 16384 Bytes");
    for (size_t i = 0; i < flags_.size(); ++i) {
        const size_t at = (0x1000 + i) * 2;
        flags_[i] = uint16_t(pit[at] | pit[at + 1] << 8);
    }
}

uint16_t TileProperties::cell(const WorldMap& map, int x, int y) const {
    return tile(map.cell(0, x, y)) | tile(map.cell(1, x, y));
}

bool MoveCounter::step(Direction d, bool commit) {
    backtrack_ = false;
    if (allowance_ == 0) return false;
    if (allowance_ < 0) return true;
    if (!history_.empty() && opposite(d) == history_.back()) {
        if (commit) history_.pop_back();
        backtrack_ = true;
        return true;
    }
    if (!history_.empty() && int(history_.size()) == allowance_) return false;
    if (commit) history_.push_back(d);
    return true;
}

std::vector<int> fieldDistances(const Board& board, int start, uint16_t blockMask) {
    const int n = int(board.fields.size());
    std::vector<int> dist(size_t(n), -1);
    if (start < 0 || start >= n) return dist;
    dist[size_t(start)] = 0;
    std::vector<int> frontier{start};
    for (int level = 1; level <= 0x50 && !frontier.empty(); ++level) {
        std::vector<int> next;
        for (int f : frontier) {
            const Field& field = board.fields[size_t(f)];
            if (field.flags & blockMask) continue;
            for (int16_t to : field.next)
                if (to != Field::kNone && dist[size_t(to)] == -1) {
                    dist[size_t(to)] = level;
                    next.push_back(to);
                }
        }
        frontier = std::move(next);
    }
    return dist;
}

std::vector<int> fieldRoute(const Board& board, int from, int to) {
    const auto dist = fieldDistances(board, to);
    if (from < 0 || from >= int(dist.size()) || dist[size_t(from)] < 0) return {};
    std::vector<int> route{from};
    int at = from;
    while (dist[size_t(at)] > 0) {
        int chosen = -1;
        for (int16_t n : board.fields[size_t(at)].next)
            if (n != Field::kNone && dist[size_t(n)] >= 0 && dist[size_t(n)] < dist[size_t(at)]) { chosen = n; break; }
        if (chosen < 0) return {};
        route.push_back(at = chosen);
    }
    return route;
}

std::vector<int> fieldsAfterSteps(const Board& board, int start, int steps) {
    const int n = int(board.fields.size());
    if (start < 0 || start >= n || steps < 0) return {};
    // State: (field, direction of the last step); a reversal would undo, so skip it.
    std::set<std::pair<int, int>> current{{start, 0}};
    for (int s = 0; s < steps; ++s) {
        std::set<std::pair<int, int>> next;
        for (auto [f, last] : current)
            for (int k = 0; k < 4; ++k) {
                const int16_t to = board.fields[size_t(f)].next[size_t(k)];
                const Direction d = Direction(k + 1);
                if (to == Field::kNone || (last && opposite(d) == Direction(last))) continue;
                next.insert({to, k + 1});
            }
        current = std::move(next);
    }
    std::set<int> fields;
    for (auto [f, last] : current) fields.insert(f);
    return {fields.begin(), fields.end()};
}

} // namespace dtvr
