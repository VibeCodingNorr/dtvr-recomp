// SPDX-License-Identifier: GPL-3.0-only
// Computer players (round 16, docs/formats.md): the driving decision of the
// seg5 state machine at seg5:6D20. SDL-free; the game passes its state in AiState.
#pragma once

#include "board.hpp"
#include "cards.hpp"
#include "economy.hpp"
#include "obstacles.hpp"
#include "rules.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace dtvr {

// Driving strategy (0x76 bytes, five records at DS:4E60 + n * 0x76).
struct AiStrategy {
    std::array<int16_t, 12> blue;   // +0x00 per month, money field (kind 1)
    double blueRich;                // +0x18 cash threshold (x inflation)
    int16_t blueDivisor;            // +0x20 divides the bonus when cash >= threshold
    std::array<int16_t, 12> red;    // +0x22 per month, money field (kind 2)
    double redRich;                 // +0x3A
    int16_t redDivisor;             // +0x42
    int16_t yellow, yellowHandFull; // +0x44, +0x46
    int16_t town, townNoMoney, townBuy, townNoBuy; // +0x48 .. +0x4E
    double townRich;                // +0x50
    int16_t townRichBonus;          // +0x58
    int16_t cardField, cardFieldPoor; // +0x5A, +0x5C
    int16_t destination;            // +0x5E
    int16_t perField, perFieldLast; // +0x60 (stages left), +0x62 (last stage)
    int16_t unused64;               // +0x64 (not read by seg5:87A8)
    int16_t stepCard;               // +0x66 holding the matching "n Felder" card
    std::array<int16_t, 3> dragoLose, dragoGain; // +0x68 / +0x6E by Dr. Drago's form
    int16_t randomRange;            // +0x74
};

// Town weights (DS:50AE / 5164): word +2 + 2 * category, double +0xA4.
struct AiTownWeights {
    std::array<int16_t, 82> words;  // +0x00 .. +0xA3
    double takeThreshold;           // +0xA4 (card Nimm only above it)
};

struct AiProfile {
    const AiStrategy* strategy;      // DS:534A
    const AiTownWeights* town;       // DS:534E
    std::array<double, 4> reserve;   // DS:5358 -> +0x12: cash kept back, by Dr. Drago state
};
// Per player slot (DS:534A + slot * 0x16).
const AiProfile& aiProfile(int slot);

// seg5:8B1B with flag 0: links of masked fields and links holding an obstacle
// are removed (graph entries per field: up, right, down, left).
using AiGraph = std::vector<std::array<int16_t, 4>>;
AiGraph buildAiGraph(const Board& board, uint16_t routeMask, const ObstacleTable& obstacles);
// seg5:8DBF: breadth-first distances from `destination`, 0xFFFF = unreachable.
std::vector<uint16_t> aiDistances(const AiGraph& graph, int destination);
// seg5:A212: direction index k with board.next[k] == to, or 4.
int aiDirection(const Board& board, int from, int to);

// The game state the computer reads.
struct AiState {
    const Board* board = nullptr;
    const std::vector<Town>* towns = nullptr;
    const Economy* economy = nullptr;
    const std::vector<PlayerSlot>* players = nullptr;
    std::array<std::array<int, 8>, 8> handTypes{}; // card type per hand slot, -1 = empty
    int dragoHolder = -1, dragoForm = 0;
    int month = 0;
    double inflation = 1.0;
    int remainingStages = 0;        // DS:A104
    int destination = -1;           // DS:A10D
    uint16_t routeMask = 0;         // DS:A10F
    bool steerless = false;         // DS:8D2A: card Steuerlos, random(1000) scores
    int cardCount(int player, int type) const;   // seg3:3FFC (type 0-based here)
    int cardSlot(int player, int type) const;    // seg3:0D26, -1 if none
    int handCount(int player) const;
    double cash(int player) const { return economy->money(player).cash; }
};

// seg5:9A34: cash above the reserve (index 0, or Dr. Drago's form + 1 while the
// player carries him) times inflation, at least 0.
double aiSpareMoney(const AiState& s, int player);

// seg5:8F01: the town dialog plan for `field`. Actions in order: kind 2 = play the
// hand slot `value`, kind 0 = buy entry `value`. `purchases` is DS:8D28.
struct AiAction { int kind; int value; };
struct AiTownPlan { std::vector<AiAction> actions; int purchases = 0; };
AiTownPlan aiTownPlan(const AiState& s, int player, int field);

// seg5:829D with seg5:87A8: the route of exactly `steps` fields with the highest
// score; empty if no route scores above 0. The fields exclude the start.
std::vector<int> aiChooseRoute(const AiState& s, int player, int steps, const AiGraph& graph,
                               const std::vector<uint16_t>& dist, MsRandom& rng);

} // namespace dtvr
