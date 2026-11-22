// SPDX-License-Identifier: GPL-3.0-only
// Computer players: card choice before spinning (seg5:7386) and card targets
// (seg5:9B0B). Round 17; evidence in docs/formats.md, "Computer card play".
#include "ai.hpp"

#include <algorithm>
#include <cstring>

namespace dtvr {

double AiCardWeights::doubleAt(int offset) const {
    double v;
    std::array<uint16_t, 4> raw;
    for (size_t i = 0; i < 4; ++i) raw[i] = uint16_t(words[size_t(offset / 2) + i]);
    std::memcpy(&v, raw.data(), sizeof v); // little-endian IEEE double as in the data segment
    return v;
}

namespace {

constexpr uint16_t kUnreached = 0xFFFF;

uint16_t fieldDistanceOf(const AiState& s, int field) {
    return field >= 0 && size_t(field) < s.fieldDist.size() ? s.fieldDist[size_t(field)] : kUnreached;
}

bool activeOther(const AiState& s, int p, int player) {
    return size_t(p) < s.players->size() && (*s.players)[size_t(p)].status && p != player;
}

} // namespace

int aiCardRank(int type, int year) {
    // DS:521A: {card number, worth before year 20, before year 40, later}, 0xFF ends.
    struct Rank { uint8_t number; std::array<uint8_t, 3> worth; };
    static constexpr Rank kRanks[] = {
        {23, {10, 10, 10}}, {24, {12, 12, 12}}, {25, {14, 14, 14}}, {26, {16, 16, 16}}, {31, {8, 8, 8}},
        {32, {8, 8, 8}}, {33, {8, 8, 8}}, {34, {8, 8, 8}}, {35, {8, 8, 8}}, {36, {13, 13, 13}},
        {37, {5, 5, 5}}, {39, {5, 5, 5}}, {38, {4, 4, 4}}, {40, {255, 4, 4}}, {15, {6, 6, 6}},
        {17, {6, 6, 6}}, {1, {3, 3, 3}}, {16, {3, 3, 3}}, {10, {6, 6, 6}}, {11, {7, 7, 7}},
        {12, {7, 7, 7}}, {13, {7, 7, 7}}, {20, {6, 6, 6}}, {21, {6, 6, 6}}, {22, {6, 6, 6}},
        {7, {9, 11, 13}}, {8, {6, 8, 12}}, {3, {7, 9, 11}}, {4, {8, 10, 12}}, {5, {9, 11, 13}},
        {6, {6, 6, 6}}, {18, {9, 12, 13}}, {29, {5, 5, 5}},
    };
    const size_t era = year >= 40 ? 2 : year >= 20 ? 1 : 0;
    for (const Rank& r : kRanks)
        if (r.number == type + 1) return r.worth[era];
    return 0xFF;
}

int aiLeastValuableCard(const AiState& s, int player) {
    int best = 100, slot = -1;
    for (int k = 0; k < 8; ++k) {
        const int type = s.handTypes[size_t(player)][size_t(k)];
        if (type < 0) continue;
        const int rank = aiCardRank(type, s.year);
        if (rank == 0xFF) { best = 0; slot = k; }       // unlisted: taken at once
        else if (rank < best) { best = rank; slot = k; }
    }
    return best == 100 ? -1 : slot;
}

std::vector<AiAction> aiCardShopPlan(const AiState& s, int player, const std::vector<CardOffer>& offers,
                                     MsRandom& rng) {
    std::vector<AiAction> plan;
    double money = 0;
    int sold = 0;
    for (int k = 0; k < 8; ++k) { // seg5:7D85: sell cards without a worth for half their value
        const int type = s.handTypes[size_t(player)][size_t(k)];
        if (type < 0 || aiCardRank(type, s.year) != 0xFF) continue;
        plan.push_back({1, type});
        ++sold;
        money += cardSellPrice(type);
    }
    money += aiSpareMoney(s, player);  // seg5:7E87
    if (!(money > 0)) return plan;
    int free = 8 - (s.handCount(player) - sold);
    if (free == 0) return plan;
    struct Candidate { int type; double value; int score; };
    std::vector<Candidate> list;
    const uint16_t d = fieldDistanceOf(s, (*s.players)[size_t(player)].field);
    for (const CardOffer& o : offers) {
        const double value = cardBuyPrice(o.type);
        if (value > money) continue;
        const int rank = aiCardRank(o.type, s.year);
        if (rank == 0xFF) continue;
        int score = uint8_t(rng.random(aiProfile(player).shopRandom) + rank);
        if (d != kUnreached && d >= 1 && d <= 6 && (o.type + 1) - d == 0x1D) score = uint8_t(score + 10);
        list.push_back({o.type, value, score});
    }
    for (size_t i = list.size(); i-- > 1;)                // bubble sort, highest first (seg5:7FF2)
        for (size_t j = 0; j < i; ++j)
            if (list[j + 1].score > list[j].score) std::swap(list[j], list[j + 1]);
    for (const Candidate& c : list) {
        if (c.value > money) break;
        plan.push_back({0, c.type});
        money -= c.value;
        if (--free == 0) break;
    }
    return plan;
}

int aiChooseTarget(const AiState& s, int player, int mode, MsRandom& rng) {
    const AiCardWeights& w = *aiProfile(player).cards;
    struct Candidate { int32_t value; int player; };
    std::vector<Candidate> list;
    for (int p = 0; p < kPlayerSlots; ++p) {
        if (!activeOther(s, p, player) || ((*s.players)[size_t(p)].flags & 0x6)) continue; // not sitting out
        int32_t v = 0;
        if (mode == 0) { // near the destination, plus the per-slot weight (+0x84 / +0x94)
            const uint16_t d = fieldDistanceOf(s, (*s.players)[size_t(p)].field);
            if (d != kUnreached) v += 100 - d;
            v += w.at(0x84 + 2 * p);
            v += rng.random(w.at(0x94 + 2 * p));
        } else {         // wealth: cash + property value, truncated (seg10:368A)
            const PlayerMoney& m = s.economy->money(p);
            v = int32_t(m.cash + m.propertyValue);
        }
        list.push_back({v, p});
    }
    if (list.empty()) return -1;
    if (list.size() == 1) return list[0].player;
    for (size_t i = list.size() - 1; i > 0; --i)         // bubble sort, highest first
        for (size_t j = 0; j < i; ++j)
            if (list[j + 1].value > list[j].value) std::swap(list[j], list[j + 1]);
    const int r = rng.random(15);
    const auto& odds = *aiProfile(player).targetOdds;
    if (list.size() == 2) return odds[0] >= r ? list[0].player : list[1].player;
    if (odds[1] >= r) return list[0].player;
    if (odds[2] >= r) return list[1].player;
    return list[2].player;
}

AiCardChoice aiChooseCard(const AiState& s, int player, const ObstacleTable& obstacles, MsRandom& rng) {
    AiCardChoice choice;
    const auto& players = *s.players;
    const PlayerSlot& me = players[size_t(player)];
    if (me.flags & kFlagCardsBlocked) return choice;
    const AiCardWeights& w = *aiProfile(player).cards;
    // Distances on the graph whose snow, rain and nails count as clearable (flag 1).
    const auto dist = aiDistances(buildAiGraph(*s.board, s.routeMask, obstacles, true), s.destination);
    const uint16_t d0 = dist[size_t(me.field)];
    const uint16_t myDist = fieldDistanceOf(s, me.field);
    const double cash = s.cash(player);
    uint16_t best = uint16_t(w.at(0));                   // the threshold doubles as the first best
    for (int slot = 0; slot < 8; ++slot) {
        const int type = s.handTypes[size_t(player)][size_t(slot)];
        if (type < 0 || !(cardTypes()[size_t(type)].category & 2)) continue;
        const int number = type + 1;                     // seg3:40A6 returns 1-based numbers
        uint16_t score = uint16_t(rng.random(w.at(0x2)));
        int target = -1;
        auto bonus = [&](int offset) { return uint16_t(w.at(offset) + rng.random(w.at(offset + 2))); };
        switch (number) {
        case 23: case 24: case 25: case 26: {            // 2-5 Walzen within a distance band
            const int i = number - 23;
            if (uint16_t(w.at(0x4 + 2 * i)) <= d0 && uint16_t(w.at(0xC + 2 * i)) >= d0
                && !(me.flags & kFlagSteerless))
                score = uint16_t(w.at(0x14 + 2 * i));
            break;
        }
        case 30: case 31: case 32: case 33: case 34: case 35: // "n Felder" exactly to the goal
            if (number - d0 == 0x1D) score = uint16_t(w.at(0x1C));
            break;
        case 36:                                         // Hau ab while carrying Dr. Drago
            if (s.dragoHolder >= 0 && s.dragoHolder == player)
                score = uint16_t(w.at(0x1E + 2 * std::clamp(s.dragoForm, 0, 2)));
            break;
        case 15: case 17: case 16: {                     // Zucker, Schlafen, Blockieren
            const int t = aiChooseTarget(s, player, 0, rng);
            if (t >= 0) { score = bonus(number == 15 ? 0x24 : number == 17 ? 0x28 : 0x34); target = t; }
            break;
        }
        case 10: {                                       // Fiskus on the richest
            const int t = aiChooseTarget(s, player, 1, rng);
            if (t >= 0) { score = bonus(0x30); target = t; }
            break;
        }
        case 1:                                          // Weihnachten in December
            if (s.month % 12 == 11) score = bonus(0x2C);
            break;
        case 11: {                                       // Konto when well below the average
            double sum = 0;
            int n = 0;
            for (int p = 0; p < kPlayerSlots && size_t(p) < players.size(); ++p)
                if (players[size_t(p)].status) { sum += s.cash(p); ++n; }
            if (n && !(cash + w.doubleAt(0x38) * s.inflation > sum / n)) score = bonus(0x40);
            break;
        }
        case 13:                                         // Zero when deep in debt
            if (!(w.doubleAt(0x44) * s.inflation < cash)) score = bonus(0x4C);
            break;
        case 12:                                         // Schulden when deep in debt
            if (!(w.doubleAt(0x50) * s.inflation < cash)) {
                const int t = aiChooseTarget(s, player, 1, rng);
                if (t >= 0) { score = bonus(0x58); target = t; }
            }
            break;
        case 20: {                                       // Tschüss
            if (uint16_t(w.at(0x68)) > myDist) break;
            uint16_t closest = 0;
            int victim = -1;
            for (int p = 0; p < kPlayerSlots; ++p) {
                if (!activeOther(s, p, player)) continue;
                const uint16_t d = fieldDistanceOf(s, players[size_t(p)].field);
                if (d == kUnreached || uint16_t(w.at(0x6A)) < d) continue;
                const uint16_t v = uint16_t(1000 - d);
                if (closest < v) { closest = v; victim = p; }
            }
            if (closest) { score = bonus(0x6C); target = victim; }
            if (myDist == kUnreached || uint16_t(w.at(0x5C)) <= myDist) { // send himself away
                const uint16_t self = bonus(0x5E);
                if (self > score) { score = self; target = player; }
            } else if (uint16_t(w.at(0x62)) <= myDist) {
                const uint16_t self = bonus(0x64);
                if (self > score) { score = self; target = player; }
            }
            break;
        }
        case 22: {                                       // Konferenz when others are close to the goal
            if (uint16_t(w.at(0x70)) > myDist) break;
            for (int p = 0; p < kPlayerSlots; ++p) {
                if (!activeOther(s, p, player)) continue;
                const uint16_t d = fieldDistanceOf(s, players[size_t(p)].field);
                if (d != kUnreached && uint16_t(w.at(0x72)) >= d) { score = bonus(0x74); break; }
            }
            break;
        }
        case 21: {                                       // Punch someone on the same field near the goal
            if (myDist == kUnreached || uint16_t(w.at(0x78)) < myDist) break;
            for (int p = 0; p < kPlayerSlots; ++p) {
                if (p == player || size_t(p) >= players.size() || !players[size_t(p)].status) continue;
                if (players[size_t(p)].field != me.field) continue;
                score = bonus(0x7A);
                break;
            }
            break;
        }
        case 18: {                                       // Erdbeben in another player's whole town
            const Field& f = s.board->fields[size_t(me.field)];
            if (f.kind != 4 && f.kind != 6) break;
            int town = -1;
            for (size_t i = 0; i < s.towns->size(); ++i)
                if ((*s.towns)[i].x == f.x && (*s.towns)[i].y == f.y) { town = int(i); break; }
            if (town < 0) break;
            const TownState& ts = s.economy->town(town);
            if (ts.wholeOwner == kNoOwner || ts.entries[0].owner == player) break;
            score = bonus(0xAC);
            break;
        }
        case 29:                                         // 1 oder 2 when at most two fields away
            if (myDist <= 2) score = bonus(0xB0);
            break;
        case 19: {                                       // Nägel: recomb extension only (see below)
            if (!s.extraCards) break;
            // Not in rally.exe (its random value never wins): when an opponent trails
            // at most 8 fields behind, the nails go into an available direction away
            // from the destination (the farthest neighbour); just above the threshold,
            // so any original card with a higher score still wins. No extra draws.
            bool trailing = false;
            for (int p = 0; p < int(s.players->size()); ++p) {
                if (!activeOther(s, p, player)) continue;
                const uint16_t dp = dist[size_t((*s.players)[size_t(p)].field)];
                if (dp != kUnreached && dp > d0 && dp - d0 <= 8) trailing = true;
            }
            int dir = -1;
            uint16_t far = d0;
            for (int k = 0; k < 4; ++k) {
                const int n = s.board->fields[size_t(me.field)].next[size_t(k)];
                if (!s.nailsDirections[size_t(k)] || n == Field::kNone) continue;
                if (dist[size_t(n)] != kUnreached && dist[size_t(n)] > far) { far = dist[size_t(n)]; dir = k; }
            }
            if (trailing && dir >= 0) {
                score = uint16_t(w.at(0) + 1);
                target = dir + 1;
            }
            break;
        }
        case 14: {                                       // Steuerlos: recomb extension only
            if (!s.extraCards) break;
            // The leader steers like a computer with random routes for one turn: the
            // other player nearest to the destination, at most 10 fields away and
            // nearer than this computer.
            int leader = -1;
            uint16_t near = d0;
            for (int p = 0; p < int(s.players->size()); ++p) {
                if (!activeOther(s, p, player) || ((*s.players)[size_t(p)].flags & (0x6 | kFlagSteerless))) continue;
                const uint16_t dp = dist[size_t((*s.players)[size_t(p)].field)];
                if (dp != kUnreached && dp <= 10 && dp < near) { near = dp; leader = p; }
            }
            if (leader >= 0) { score = uint16_t(w.at(0) + 1); target = leader; }
            break;
        }
        case 27:                                         // Risiko: recomb extension only
            // Far from the destination the gamble for up to four times the spin pays.
            if (s.extraCards && d0 != kUnreached && d0 >= 10) score = uint16_t(w.at(0) + 1);
            break;
        case 28: {                                       // Flugzeug: recomb extension only
            if (!s.extraCards) break;
            // The bookable airport nearest to the destination when it saves more than
            // six fields (rows: Moskau only with the east open, Brüssel not on the last
            // stage, seg3:33F2 / 3633).
            const int rows = s.routeMask == 0 ? 8 : 7;
            uint16_t bestDist = d0;
            for (int row = 0; row < rows; ++row) {
                const int town = s.airports[size_t(row)];
                if (town < 0 || (row == 1 && s.remainingStages == 0)) continue;
                const auto& t = (*s.towns)[size_t(town)];
                int field = -1;
                for (size_t f = 0; f < s.board->fields.size(); ++f)
                    if (s.board->fields[f].x == t.x && s.board->fields[f].y == t.y) { field = int(f); break; }
                if (field < 0) continue;
                const uint16_t df = dist[size_t(field)];
                if (df != kUnreached && df + 6 < bestDist) { bestDist = df; target = town; }
            }
            if (target >= 0) score = uint16_t(w.at(0) + 1);
            break;
        }
        default: break; // the property cards Verkaufen / Kredit: the random value only
        }
        if (score > best) {
            best = score;
            choice = {slot, type, target};
        }
    }
    if (uint16_t(w.at(0)) >= best) return {};
    return choice;
}

} // namespace dtvr
