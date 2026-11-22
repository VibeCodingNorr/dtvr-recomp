// SPDX-License-Identifier: GPL-3.0-only
// Computer players: driving decision and town plan (round 16; evidence in
// docs/formats.md, "Computer players"). Values from the rally.exe data segment.
#include "ai.hpp"

#include <algorithm>
#include <functional>

namespace dtvr {
namespace {

const AiStrategy kStrategies[5] = {
    {{-2, -2, 0, 2, 5, 8, 8, 8, 6, 2, -1, -2}, 100000.0, 3,
     {-9, -10, -5, -4, -4, -3, -3, -3, -4, -4, -6, -8}, 200000.0, 2,
     3, -3, 3, -1, 2, -1, 250000.0, 10,
     2, -2, 1000, 1, 2, -3, 12,
     {5, 10, 15}, {-5, -10, -15}, 1}, // DS:4E60
    {{-1, -1, 0, 2, 3, 5, 5, 5, 3, 1, 0, -1}, 100000.0, 2,
     {-18, -20, -8, -4, -4, -4, -4, -4, -4, -6, -8, -16}, 200000.0, 2,
     3, -3, 3, -1, 2, -1, 250000.0, 5,
     2, -2, 1000, 2, 4, -2, 20,
     {6, 12, 20}, {-6, -12, -20}, 0}, // DS:4ED6
    {{-1, -1, 0, 2, 3, 5, 5, 5, 3, 1, 0, -1}, 200000.0, 2,
     {-18, -20, -8, -4, -4, -4, -4, -4, -4, -6, -8, -16}, 300000.0, 2,
     4, -4, 3, -1, 2, -1, 250000.0, 6,
     8, -8, 1000, 2, 4, -2, 20,
     {8, 16, 24}, {-8, -16, -24}, 0}, // DS:4F4C
    {{0, 0, 0, 2, 5, 8, 8, 8, 6, 2, 0, 0}, 200000.0, 2,
     {-9, -10, -5, -3, -3, -2, -2, -2, -3, -3, -6, -8}, 200000.0, 3,
     1, -1, 2, 0, 1, 0, 250000.0, 2,
     1, -1, 1000, 1, 3, -2, 2,
     {2, 0, 0}, {0, 0, 0}, 0}, // DS:4FC2
    {{-2, -2, 0, 2, 5, 8, 8, 8, 6, 2, -1, -2}, 100000.0, 2,
     {-18, -20, -8, -4, -4, -4, -4, -4, -4, -6, -8, -16}, 250000.0, 2,
     2, -2, 3, 0, 3, 0, 150000.0, 12,
     3, -3, 1000, 2, 3, -3, 12,
     {6, 12, 20}, {-6, -12, -20}, 0}, // DS:5038
};
const AiCardWeights kCardWeights[2] = {
    {{10, 2, 6, 9, 12, 15, 30, 40, 50, 60, 16, 17, 18, 19, 1000, 13,
      18, 26, 6, 8, 6, 8, 12, 4, 6, 8, 6, 8, 0, 0, 27136, 16632,
      16, 8, 0, 0, 27136, -16152, 13, 12, 0, 0, 27136, -16152, 14, 12, 45, 15,
      8, 30, 6, 8, 25, 8, 8, 6, 25, 8, 8, 6, 6, 8, 6, 12,
      7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 27136, 16632, 20, 20, 14, 12, 1000}, 100000.0}, // DS:50AE
    {{10, 1, 6, 9, 12, 15, 30, 40, 50, 60, 16, 17, 18, 19, 1000, 0,
      12, 20, 8, 12, 8, 12, 12, 12, 8, 8, 8, 12, 0, 0, 27136, 16632,
      15, 10, 0, 0, 27136, -16152, 13, 12, 0, 0, 27136, -16152, 14, 12, 45, 12,
      8, 30, 6, 8, 25, 8, 8, 6, 25, 8, 8, 12, 6, 8, 12, 12,
      7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 27136, 16632, 20, 20, 14, 12, 1000}, 100000.0}, // DS:5164
};
const AiMoney kMoney[3] = {
    {{1, 0, 0, 0, 2, -1, 0, 0, 0}, {0.0, 10000.0, 20000.0, 50000.0}},  // DS:52A2
    {{2, 0, 0, 0, 10, -1, 0, 0, 0}, {0.0, 10000.0, 20000.0, 50000.0}}, // DS:52D6
    {{5, 0, 5, 0, 2, -1, 5, 0, 0}, {0.0, 10000.0, 20000.0, 50000.0}},  // DS:530A
};

// Target odds for seg5:9B0B (DS:533E / 5344): random(15) <= odds[i] picks place i.
const std::array<int16_t, 3> kTargetOdds[2] = {{11, 9, 13}, {13, 13, 15}};

// DS:534A + slot * 0x16: strategy, card weights, money record, target odds, shop random.
const AiProfile kProfiles[8] = {
    {&kStrategies[1], &kCardWeights[1], &kMoney[0], &kTargetOdds[1], 0},
    {&kStrategies[1], &kCardWeights[0], &kMoney[1], &kTargetOdds[1], 3},
    {&kStrategies[0], &kCardWeights[0], &kMoney[2], &kTargetOdds[0], 0},
    {&kStrategies[4], &kCardWeights[0], &kMoney[0], &kTargetOdds[0], 2},
    {&kStrategies[3], &kCardWeights[0], &kMoney[0], &kTargetOdds[0], 8},
    {&kStrategies[0], &kCardWeights[1], &kMoney[0], &kTargetOdds[0], 2},
    {&kStrategies[0], &kCardWeights[0], &kMoney[0], &kTargetOdds[0], 4},
    {&kStrategies[2], &kCardWeights[1], &kMoney[0], &kTargetOdds[1], 2},
};

constexpr uint16_t kUnreached = 0xFFFF;

} // namespace

const AiProfile& aiProfile(int slot) { return kProfiles[size_t(slot & 7)]; }

int AiState::cardCount(int player, int type) const {
    return int(std::count(handTypes[size_t(player)].begin(), handTypes[size_t(player)].end(), type));
}

int AiState::cardSlot(int player, int type) const {
    for (int s = 0; s < 8; ++s)
        if (handTypes[size_t(player)][size_t(s)] == type) return s;
    return -1;
}

int AiState::handCount(int player) const {
    return 8 - cardCount(player, -1);
}

int aiDirection(const Board& board, int from, int to) {
    for (int k = 0; k < 4; ++k)
        if (board.fields[size_t(from)].next[size_t(k)] == to) return k;
    return 4;
}

AiGraph buildAiGraph(const Board& board, uint16_t routeMask, const ObstacleTable& obstacles, bool clearable) {
    AiGraph graph(board.fields.size());
    for (size_t i = 0; i < board.fields.size(); ++i) {
        const Field& f = board.fields[i];
        for (size_t k = 0; k < 4; ++k) {
            int16_t n = f.flags & routeMask ? Field::kNone : f.next[k];
            if (n != Field::kNone && (board.fields[size_t(n)].flags & routeMask)) n = Field::kNone;
            graph[i][k] = n;
        }
    }
    // Obstacle types 4, 8, 0x10, 0x900 (seg5:8C66): with flag 0 every one removes
    // its link in both directions; investments (0x400 / 0x800) are skipped.
    for (const Obstacle& o : obstacles.slots()) {
        if (o.type != kRockfall && o.type != kSnow && o.type != kRain && o.type != kNails) continue;
        if (clearable && o.type != kRockfall) continue; // flag 1: counted as clearable, link stays
        if (o.field < 0 || o.dir < 1 || o.dir > 4) continue;
        const int a = o.field, b = board.fields[size_t(a)].next[size_t(o.dir - 1)];
        if (b == Field::kNone) continue;
        const int d = aiDirection(board, a, b);
        if (d > 3) continue;
        graph[size_t(a)][size_t(d)] = Field::kNone;
        graph[size_t(b)][size_t((d + 2) & 3)] = Field::kNone;
    }
    return graph;
}

int reachableSteps(const Board& board, uint16_t routeMask, const ObstacleTable& obstacles, int start,
                   int nailsCards, int ploughCards) {
    constexpr int kCap = 0x1E; // seg5:9FE6 / A00A
    if (start < 0 || size_t(start) >= board.fields.size()) return 0;
    AiGraph graph = buildAiGraph(board, routeMask, obstacles, true); // flag 0: rockfall only
    // Link counts of seg5:8B1B: +8 nails (0x900), +0xC snow / rain (8 / 0x10), both directions.
    std::vector<std::array<int, 4>> nails(board.fields.size()), snow(board.fields.size());
    for (const Obstacle& o : obstacles.slots()) {
        if (o.type != kNails && o.type != kSnow && o.type != kRain) continue;
        if (o.field < 0 || o.dir < 1 || o.dir > 4) continue;
        const int a = o.field, b = board.fields[size_t(a)].next[size_t(o.dir - 1)];
        if (b == Field::kNone) continue;
        const int d = aiDirection(board, a, b);
        if (d > 3) continue;
        auto& counts = o.type == kNails ? nails : snow;
        ++counts[size_t(a)][size_t(d)];
        ++counts[size_t(b)][size_t((d + 2) & 3)];
    }
    std::vector<int> path{start};
    int best = 0;
    bool cycle = false;
    // seg5:9ECE ff.: neighbours 0..3, the link back to the previous field removed.
    std::function<void(int, int, int, int)> visit = [&](int field, int back, int nailsLeft, int ploughLeft) {
        for (int k = 0; k < 4 && !cycle && best < kCap; ++k) {
            const int next = graph[size_t(field)][size_t(k)];
            if (next == Field::kNone || k == back) continue;
            const int needNails = nails[size_t(field)][size_t(k)], needPlough = snow[size_t(field)][size_t(k)];
            if (nailsLeft < needNails || ploughLeft < needPlough) continue; // seg5:9F3E
            for (int p : path)
                if (p == next) { cycle = true; return; }                     // seg5:9FD3: a cycle -> 30
            const int r = (k + 2) & 3;
            nails[size_t(field)][size_t(k)] = snow[size_t(field)][size_t(k)] = 0; // cleared on the way
            nails[size_t(next)][size_t(r)] = snow[size_t(next)][size_t(r)] = 0;
            path.push_back(next);
            best = std::max(best, int(path.size()) - 1);
            visit(next, r, nailsLeft - needNails, ploughLeft - needPlough);
            path.pop_back();
            nails[size_t(field)][size_t(k)] = nails[size_t(next)][size_t(r)] = needNails; // seg5:A0DF: restored
            snow[size_t(field)][size_t(k)] = snow[size_t(next)][size_t(r)] = needPlough;
        }
    };
    visit(start, -1, nailsCards, ploughCards);
    return cycle ? kCap : std::min(best, kCap);
}

std::vector<uint16_t> aiDistances(const AiGraph& graph, int destination) {
    std::vector<uint16_t> dist(graph.size(), kUnreached);
    if (destination < 0 || size_t(destination) >= graph.size()) return dist;
    dist[size_t(destination)] = 0;
    std::vector<int> frontier{destination};
    for (uint16_t level = 1; !frontier.empty(); ++level) {
        std::vector<int> next;
        for (int f : frontier)
            for (int16_t n : graph[size_t(f)])
                if (n != Field::kNone && dist[size_t(n)] == kUnreached) {
                    dist[size_t(n)] = level;
                    next.push_back(n);
                }
        frontier.swap(next);
    }
    return dist;
}

double aiSpareMoney(const AiState& s, int player) {
    const int k = s.dragoHolder >= 0 && s.dragoHolder == player ? s.dragoForm + 1 : 0;
    const double spare = s.cash(player) - aiProfile(player).money->reserve[size_t(k)] * s.inflation;
    return spare > 0 ? spare : 0.0;
}

namespace {

// seg5:990A: income factor (+0x1C) plus the money record's weight for the
// entry's category (word 1 + category; the original data uses categories 1-6).
int32_t entryScore(const TownEntry& e, const AiMoney& m) {
    const size_t index = size_t(1 + e.words[1]);
    const int32_t v = int32_t(e.words[0]) + (index < m.words.size() ? m.words[index] : 0);
    return v < 0 ? 0 : v;
}

} // namespace

AiTownPlan aiTownPlan(const AiState& s, int player, int field) {
    AiTownPlan plan;
    const Board& board = *s.board;
    if (field < 0 || size_t(field) >= board.fields.size()) return plan;
    const Field& f = board.fields[size_t(field)];
    if (f.kind != 4 && f.kind != 6) return plan;
    int town = -1;                                           // seg2:276B
    for (size_t i = 0; i < s.towns->size(); ++i)
        if ((*s.towns)[i].x == f.x && (*s.towns)[i].y == f.y) { town = int(i); break; }
    if (town < 0) return plan;
    const TownState& state = s.economy->town(town);
    if (state.wholeOwner == player) return plan;             // seg1:7CDE
    const Town& rec = (*s.towns)[size_t(town)];
    const AiProfile& profile = aiProfile(player);

    // Classes: 0 free, 1 own, 2 other player's, 3 rented, 4 burdened.
    struct Item { int cls; int32_t score; int entry; };
    std::vector<Item> items;
    for (int e = 0; e < 10; ++e) {
        if (rec.entries[size_t(e)].name.empty()) break;      // the list stops at the first gap
        const PropertyState& p = state.entries[size_t(e)];
        int cls = p.owner == player ? 1 : p.owner == kNoOwner ? 0 : 2;
        if (p.landlord != kNoOwner) cls = 3;
        else if (p.burden != 0) cls = 4;
        items.push_back({cls, entryScore(rec.entries[size_t(e)], *profile.money), e});
    }
    if (items.empty()) return plan;
    // Bubble sort, highest score first (seg5:911F); class and entry move along.
    for (size_t i = items.size() - 1; i > 0; --i)
        for (size_t j = 0; j < i; ++j)
            if (items[j + 1].score > items[j].score) std::swap(items[j], items[j + 1]);
    const bool blocked = (*s.players)[size_t(player)].flags & kFlagCardsBlocked;
    auto value = [&](const Item& it) { return double(rec.entries[size_t(it.entry)].value); };
    int mode = 0xFF;
    if (!blocked) { // Nimm (type 6) on the best other player's entry above the threshold
        const int slot = s.cardSlot(player, 6);
        if (slot >= 0)
            for (const Item& it : items)
                if (it.cls == 2 && profile.cards->takeThreshold < value(it)) {
                    mode = 7;
                    plan.actions.push_back({2, slot});
                    plan.actions.push_back({0, it.entry});
                    ++plan.purchases;
                    break;
                }
    }
    if (mode == 0xFF) { // best discount card: Kaufen 25, Gold 50, Platin 90 (seg5:92F5)
        int best = 0, bestSlot = -1, bestType = -1;
        for (int slot = 0; slot < 8; ++slot) {
            const int t = s.handTypes[size_t(player)][size_t(slot)];
            const int bonus = t == 2 ? 25 : t == 3 ? 50 : t == 4 ? 90 : 0;
            if (best < bonus) { best = bonus; bestSlot = slot; bestType = t; }
        }
        if (best > 0) {
            mode = bestType + 1;
            plan.actions.push_back({2, bestSlot});
        }
    }
    double spare = aiSpareMoney(s, player);
    if (spare > 0) { // free entries while the (discounted) value is below the spare money
        for (const Item& it : items) {
            if (it.cls != 0) continue;
            double price = value(it);
            if (mode == 3) price *= 0.75;      // DS:86F6
            else if (mode == 4) price *= 0.5;  // DS:86EE
            else if (mode == 5) price *= 0.1;  // DS:86FE
            if (price < spare) {
                plan.actions.push_back({0, it.entry});
                spare -= price;
                ++plan.purchases;
            }
        }
    }
    if (plan.purchases == 0) { plan.actions.clear(); mode = 0xFF; }
    if (!blocked && mode == 0xFF) { // Abkaufen (type 7): one other player's entry below the spare money
        const int slot = s.cardSlot(player, 7);
        if (slot >= 0)
            for (const Item& it : items)
                if (it.cls == 2 && value(it) < spare) {
                    mode = 8;
                    plan.actions.push_back({2, slot});
                    plan.actions.push_back({0, it.entry});
                    ++plan.purchases;
                    break;
                }
    }
    if (!blocked && mode == 0xFF) { // Mieten (type 5): the best other player's entry
        const int slot = s.cardSlot(player, 5);
        if (slot >= 0)
            for (const Item& it : items)
                if (it.cls == 2) {
                    mode = 6;
                    plan.actions.push_back({2, slot});
                    plan.actions.push_back({0, it.entry});
                    ++plan.purchases;
                    break;
                }
    }
    // Recomb extension (not in rally.exe, whose town plan never uses these cards):
    // only when no property card was chosen above.
    if (s.extraCards && !blocked && mode == 0xFF) {
        double cash = s.cash(player);
        const int sellSlot = s.cardSlot(player, 1); // Verkaufen: x1.5 (DS:869E)
        if (cash < 0 && sellSlot >= 0) {           // in debt: sell own entries, most valuable first
            std::vector<Item> own;
            for (const Item& it : items)
                if (it.cls == 1) own.push_back(it);
            std::sort(own.begin(), own.end(), [&](const Item& a, const Item& b) { return value(a) > value(b); });
            if (!own.empty()) {
                mode = 2;
                plan.actions.push_back({2, sellSlot});
                for (const Item& it : own) {
                    if (cash >= 0) break;
                    plan.actions.push_back({3, it.entry});
                    cash += value(it) * 1.5;
                }
            }
        }
        const int creditSlot = s.cardSlot(player, 8); // Kredit: a free entry without cash
        if (mode == 0xFF && plan.purchases == 0 && creditSlot >= 0)
            for (const Item& it : items)
                if (it.cls == 0) { // the best scored free entry
                    plan.actions.push_back({2, creditSlot});
                    plan.actions.push_back({0, it.entry});
                    ++plan.purchases;
                    break;
                }
    }
    return plan;
}

namespace {

// seg5:87A8: score of a route ending on `end`. Dr. Drago state: -1 not in play,
// 0 the player carries him, 1 somebody else does.
uint16_t scoreEnd(const AiState& s, int player, int end, int dragoStart, int dragoEnd,
                  const std::vector<uint16_t>& dist, int townPurchases, MsRandom& rng) {
    if (s.steerless) return uint16_t(rng.random(1000));
    const AiStrategy& st = *aiProfile(player).strategy;
    const double cash = s.cash(player);
    int score = 1000;
    switch (s.board->fields[size_t(end)].kind) {
    case 1: {
        int v = st.blue[size_t(s.month % 12)];
        if (!(st.blueRich * s.inflation > cash)) v /= st.blueDivisor;
        score += v;
        break;
    }
    case 2: {
        int v = st.red[size_t(s.month % 12)];
        if (!(st.redRich * s.inflation > cash)) v /= st.redDivisor;
        score += v;
        break;
    }
    case 3:
        score += st.yellow;
        if (s.handCount(player) == 8) score += st.yellowHandFull;
        break;
    case 4:
    case 6:
        score += st.town;
        if (!(aiSpareMoney(s, player) > 0)) score += st.townNoMoney;
        else if (townPurchases) {
            score += st.townBuy;
            if (!(st.townRich * s.inflation > cash)) score += st.townRichBonus;
        } else score += st.townNoBuy;
        break;
    case 5:
        score += st.cardField;
        if (!(aiSpareMoney(s, player) > 0)) score += st.cardFieldPoor;
        break;
    default: break;
    }
    const uint16_t d = dist[size_t(end)];
    if (d != kUnreached) {
        if (d == 0) score += st.destination;
        else {
            score -= (s.remainingStages ? st.perField : st.perFieldLast) * d;
            if (d > 6) score += st.perField;
        }
        // Card number d + 0x1D (seg3:3FFC, 1-based) = type d + 28: "d Felder".
        if (d >= 1 && d <= 6 && s.cardCount(player, d + 28) > 0) score += st.stepCard;
    }
    if (dragoStart != -1) {
        const size_t form = size_t(std::clamp(s.dragoForm, 0, 2));
        if (dragoStart == 0 && dragoEnd == 1) score += st.dragoLose[form];
        else if (dragoStart == 1 && dragoEnd == 0) score += st.dragoGain[form];
    }
    score += rng.random(st.randomRange);
    return uint16_t(score);
}

} // namespace

std::vector<int> aiChooseRoute(const AiState& s, int player, int steps, const AiGraph& graph,
                               const std::vector<uint16_t>& dist, MsRandom& rng) {
    std::vector<int> best;
    if (steps <= 0) return best;
    const auto& players = *s.players;
    const int start = players[size_t(player)].field;
    // seg5:8940 evaluates the town the player stands in, not the route's end.
    const int townPurchases = aiTownPlan(s, player, start).purchases;

    struct Frame { int field; std::array<int16_t, 4> next; int index; int drago; int dragoField; };
    std::vector<Frame> stack;
    Frame root{start, graph[size_t(start)], 0, -1, 0};
    if (s.dragoHolder >= 0) {
        root.drago = s.dragoHolder == player ? 0 : 1;
        root.dragoField = players[size_t(s.dragoHolder)].field;
    }
    stack.push_back(root);
    uint16_t bestScore = 0;
    while (!stack.empty()) {
        Frame& top = stack.back();
        if (top.index >= 4) { stack.pop_back(); continue; }
        const int k = top.index++;
        const int16_t n = top.next[size_t(k)];
        if (n == Field::kNone) continue;
        Frame child{n, graph[size_t(n)], 0, top.drago, top.dragoField};
        child.next[size_t((k + 2) & 3)] = Field::kNone; // no immediate U-turn
        if (top.drago == 1) {                            // meet the carrier: he passes to us
            if (top.dragoField == n) child.drago = 0;
        } else if (top.drago == 0) {                     // we carry him: meet somebody else
            for (int p = 0; p < kPlayerSlots && size_t(p) < players.size(); ++p)
                if (players[size_t(p)].status && p != player && players[size_t(p)].field == n) {
                    child.drago = 1;
                    break;
                }
            child.dragoField = n;
        }
        stack.push_back(child);
        if (int(stack.size()) - 1 == steps) {
            const uint16_t score = scoreEnd(s, player, n, stack.front().drago, child.drago, dist,
                                            townPurchases, rng);
            if (score > bestScore) {
                bestScore = score;
                best.clear();
                for (size_t i = 1; i < stack.size(); ++i) best.push_back(stack[i].field);
            }
            stack.back().index = 4; // do not go deeper
        }
    }
    return best;
}

} // namespace dtvr
