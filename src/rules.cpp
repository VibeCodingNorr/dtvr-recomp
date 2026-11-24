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

namespace {
MoneyEvent scaledRange(const int (&row)[2], double factor) {
    MoneyEvent e;
    e.minimum = int(row[0] * factor); // _ftol truncates
    e.maximum = int(row[1] * factor);
    return e;
}

int drawAmount(const MoneyEvent& range, MsRandom& rng) {
    const int steps = int(unsigned(range.maximum - range.minimum) / 100u);
    return range.minimum + rng.random(steps) * 100;
}
} // namespace

MoneyEvent moneyRange(int kind, int month) {
    if (kind != 1 && kind != 2) throw std::invalid_argument("moneyRange: Feldtyp 1 oder 2 erwartet");
    const unsigned m = unsigned(month) % 12; // unsigned division as at seg2:0D8D
    return scaledRange(kind == 1 ? kBlue[m] : kRed[m], kind == 1 ? 0.5 : 0.75);
}

double dragoAmount(int month, int form, double inflation, MsRandom& rng) {
    // seg2:B793: the blue-field table scaled by 0.5 / 1.0 / 2.0 (DS:865E..866E).
    static constexpr double factors[3] = {0.5, 1.0, 2.0};
    if (form < 0 || form > 2) throw std::invalid_argument("dragoAmount: Form 0 bis 2");
    return drawAmount(scaledRange(kBlue[unsigned(month) % 12], factors[form]), rng) * inflation;
}

MoneyEvent moneyField(int kind, int month, double inflation, MsRandom& rng) {
    MoneyEvent e = moneyRange(kind, month);
    e.base = drawAmount(e, rng);
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

int activePlayerCount(const std::vector<PlayerSlot>& players) {
    int n = 0;
    for (size_t i = 0; i < players.size() && i < size_t(kPlayerSlots); ++i) n += players[i].status != 0;
    return n;
}

int firstActivePlayer(const std::vector<PlayerSlot>& players) {
    if (activePlayerCount(players) == 0) return 0;
    for (size_t i = 0; i < players.size() && i < size_t(kPlayerSlots); ++i)
        if (players[i].status) return int(i);
    return 0;
}

int resolveTarget(const std::vector<PlayerSlot>& players, int current, int requested) {
    const int active = activePlayerCount(players);
    if (active == 1) return firstActivePlayer(players);
    if (active == 2) {
        for (int p = 0; p < kPlayerSlots && size_t(p) < players.size(); ++p)
            if (players[size_t(p)].status && p != current) return p;
        return -1;
    }
    if (requested < 0 || requested >= kPlayerSlots || size_t(requested) >= players.size()) return -1;
    if (requested == current || !players[size_t(requested)].status) return -1;
    return requested;
}

int resolveAnyTarget(const std::vector<PlayerSlot>& players, int requested) {
    if (activePlayerCount(players) == 1) return firstActivePlayer(players);
    if (requested < 0 || requested >= kPlayerSlots || size_t(requested) >= players.size()) return -1;
    return players[size_t(requested)].status ? requested : -1;
}

int pickHiddenField(const Board& board, uint16_t routeMask, const CellRect& view, MsRandom& rng) {
    const int count = int(board.fields.size());
    for (int tries = 0; tries < 100000; ++tries) { // the original loops without a limit
        const int f = rng.random(count - 1);
        const Field& field = board.fields[size_t(f)];
        const CellRect r{field.x - 3, field.y - 3, field.x + 4, field.y + 4};
        const bool visible = r.left < view.right && view.left < r.right && r.top < view.bottom && view.top < r.bottom;
        if (!visible && !(field.flags & routeMask)) return f;
    }
    return -1;
}

TurnChange nextPlayer(std::vector<PlayerSlot>& players, int current, Calendar& calendar, MsRandom& rng,
                      const TurnHooks& hooks, int resumePlayer) {
    auto call = [](const std::function<void()>& f) { if (f) f(); };
    if (players.size() < size_t(kPlayerSlots)) players.resize(kPlayerSlots);
    TurnChange change;
    int player = resumePlayer >= 0 ? resumePlayer : current;
    bool resuming = resumePlayer >= 0;
    for (;;) {
        if (resuming) { // back from the year balance: seg1:8BC6, 8B41 and the month
            resuming = false;
            call(hooks.clearYearRent);
            call(hooks.clearCredits);
            ++change.yearsEnded;
            ++calendar.month;
            ++change.monthsAdvanced;
        } else {
            if (player >= 0 && player < kPlayerSlots) players[size_t(player)].flags &= 0xFFF7;
            if (activePlayerCount(players) == 0) return change;
            do player = (player + 1) % kPlayerSlots; // seg2:19DD
            while (!players[size_t(player)].status);
            if (player == firstActivePlayer(players)) {
                if (unsigned(calendar.month) % 12 == 11) {
                    call(hooks.chargeCredits);
                    call(hooks.settleMonth);
                    ++calendar.year;
                    calendar.inflation = inflateYear(calendar.inflation, rng);
                    if (hooks.yearBalance && hooks.yearBalance()) { // the show runs; wait for it
                        change.suspended = true;
                        change.resumePlayer = player;
                        return change;
                    }
                    call(hooks.clearYearRent); // seg2:07CA
                    call(hooks.clearCredits);  // seg2:07CF
                    ++change.yearsEnded;
                } else {
                    call(hooks.settleMonth);
                }
                ++calendar.month;
                ++change.monthsAdvanced;
            }
        }
        if (hooks.afterPlayerChange) hooks.afterPlayerChange(player);
        PlayerSlot& p = players[size_t(player)];
        if (p.flags & 0x1) { // seg2:08C2: checked first; ends the flag checks either way
            p.flags &= 0xFFFE;
            if (hooks.dragoHolder == player) {
                change.texts.push_back({3251 + rng.random(4), player}); // seg2:B74D
                change.satOut.push_back(player);
                continue;
            }
            change.player = player;
            return change;
        }
        const uint16_t sitOut = (p.flags & 0x2) ? 0x2 : (p.flags & 0x4) ? 0x4 : 0;
        if (sitOut) {
            p.flags &= uint16_t(~sitOut);
            change.texts.push_back({(sitOut == 0x2 ? 1000 : 1010) + rng.random(2), player}); // seg2:0915 / 09B3
            change.satOut.push_back(player);
            if (hooks.afterSitOut) hooks.afterSitOut(player);
            continue;
        }
        change.player = player;
        return change;
    }
}

int startField(const Board& board, const std::vector<Town>& towns, const std::string& name) {
    auto equal = [](const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            auto fold = [](char c) { return c >= 'a' && c <= 'z' ? char(c - 32) : c; };
            if (fold(a[i]) != fold(b[i])) return false;
        }
        return true;
    };
    for (size_t i = 0; i < board.fields.size(); ++i) {
        const Field& f = board.fields[i];
        if (f.kind != 6) continue;
        for (const Town& t : towns)
            if (t.x == f.x && t.y == f.y && equal(t.name, name)) return int(i);
    }
    return -1;
}

int chooseDestination(const Board& board, const std::vector<PlayerSlot>& players, int origin,
                      int current, uint16_t mask, MsRandom& rng) {
    std::vector<int> slots = destinationCandidates(board);
    const int count = int(slots.size());
    slots.resize(60, -1);
    bool possible = false;
    for (int i = 0; i < count; ++i)
        possible |= slots[size_t(i)] != origin && slots[size_t(i)] != current
                    && !(board.fields[size_t(slots[size_t(i)])].flags & mask);
    if (!possible) return -1;
    for (int attempt = 0; attempt < 100000; ++attempt) {
        const int idx = rng.random(count);
        bool occupied = false;
        for (size_t p = 0; p < players.size() && p < size_t(kPlayerSlots); ++p)
            if (players[p].status && players[p].field == idx) { occupied = true; break; }
        const int field = slots[size_t(idx)];
        if (field == -1 || field == origin || field == current || occupied) continue;
        if (board.fields[size_t(field)].flags & mask) continue;
        return field;
    }
    return -1;
}

std::vector<int> destinationCandidates(const Board& board) {
    std::vector<int> slots;
    for (size_t i = 0; i < board.fields.size() && slots.size() < 60; ++i)
        if (board.fields[i].kind == 6) slots.push_back(int(i));
    return slots;
}

int lastStageDestination(const Board& board, const std::vector<Town>& towns, int current) {
    auto fold = [](unsigned char c) { return c >= 'a' && c <= 'z' ? c - 32 : int(c); };
    auto same = [&](const std::string& a, const char* b) { // USER.LSTRCMPI == 0
        size_t i = 0;
        for (; i < a.size() && b[i]; ++i)
            if (fold(static_cast<unsigned char>(a[i])) != fold(static_cast<unsigned char>(b[i]))) return false;
        return i == a.size() && !b[i];
    };
    auto search = [&](std::initializer_list<const char*> names) {
        for (int field : destinationCandidates(board)) {
            const Field& f = board.fields[size_t(field)];
            for (const Town& t : towns) { // seg2:271C -> 276B: the first town at the cell
                if (t.x != f.x || t.y != f.y) continue;
                for (const char* n : names)
                    if (same(t.name, n)) return field;
                break;
            }
        }
        return -1;
    };
    if (const int f = search({"BR\x9A" "SSEL", "br\x81" "ssel"}); f >= 0) return f;          // seg7:4B89
    if (const int f = search({"BR\x9A" "SSEL", "br\x81" "ssel", "brussels"}); f >= 0) return f; // seg7:47AB
    return current;
}

int startFacing(const Field& field) {
    for (int k = 0; k < 4; ++k)
        if (field.next[size_t(k)] != Field::kNone) return k + 1;
    return 0;
}

std::array<int, kPlayerSlots> masterStartFields(const Board& board, const std::vector<PlayerSlot>& players,
                                                 int first) {
    std::array<int, kPlayerSlots> moved;
    moved.fill(-1);
    int index = first;
    for (int p = 0; p < kPlayerSlots && size_t(p) < players.size(); ++p) {
        if (!players[size_t(p)].status) continue;
        if (index >= 0 && size_t(index) < kMasterStartCells.size()) {
            const MapCell c = kMasterStartCells[size_t(index)];
            for (size_t i = 0; i < board.fields.size(); ++i)
                if (board.fields[i].x == c.x && board.fields[i].y == c.y) moved[size_t(p)] = int(i);
        }
        ++index; // seg2:5E84: also when no field matched
    }
    return moved;
}

StageResult reachDestination(int remaining, double inflation) {
    StageResult r;
    if (remaining == 0) {
        r.reward = destinationReward(false, inflation);
        r.gameOver = true;
        return r;
    }
    r.reward = destinationReward(true, inflation);
    r.remaining = remaining - 1;
    r.lastStageNext = r.remaining == 0;
    return r;
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
