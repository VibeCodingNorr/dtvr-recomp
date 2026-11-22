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

bool arrivalTriggers(int field, int x, int y, int taken, int allowance, bool dialogOpen) {
    if (field < 0 || dialogOpen) return false;
    if (((x | y) & 0xF) != 0) return false;
    return taken == allowance || allowance == -1;
}

namespace {
// {minimum, maximum} per month 0..11 as stored on the stack at seg2:0BD7.
constexpr int kBlue[12][2] = {{1800, 2400}, {3600, 4800}, {7200, 9600}, {9000, 12000},
    {12600, 16800}, {14400, 19200}, {18000, 24000}, {16200, 21600}, {14400, 19200},
    {10800, 14400}, {7200, 9600}, {5400, 7200}};
constexpr int kRed[12][2] = {{19600, 27000}, {15200, 24300}, {12600, 18900}, {10500, 16200},
    {8800, 13500}, {7300, 10800}, {3300, 5400}, {5300, 8100}, {9200, 13500},
    {12400, 18900}, {14300, 21600}, {16500, 24300}};
} // namespace

MoneyEvent moneyRange(int kind, int month) {
    if (kind != 1 && kind != 2) throw std::invalid_argument("moneyRange: Feldtyp 1 oder 2 erwartet");
    const unsigned m = unsigned(month) % 12; // unsigned division as at seg2:0D8D
    const auto& row = kind == 1 ? kBlue[m] : kRed[m];
    const double factor = kind == 1 ? 0.5 : 0.75;
    MoneyEvent e;
    e.minimum = int(row[0] * factor); // _ftol truncates
    e.maximum = int(row[1] * factor);
    return e;
}

MoneyEvent moneyField(int kind, int month, double inflation, MsRandom& rng) {
    MoneyEvent e = moneyRange(kind, month);
    const int steps = int(unsigned(e.maximum - e.minimum) / 100u);
    e.base = e.minimum + rng.random(steps) * 100;
    e.amount = (kind == 1 ? 1.0 : -1.0) * e.base * inflation;
    e.textId = kind == 1 ? 4001 + rng.random(14) : 4101 + rng.random(13);
    return e;
}

double inflateYear(double factor, MsRandom& rng) {
    return (rng.random(5) * 0.01 + 1.01) * factor;
}

double destinationReward(bool alternative, double inflation) {
    return (alternative ? 100000.0 : 300000.0) * inflation;
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
