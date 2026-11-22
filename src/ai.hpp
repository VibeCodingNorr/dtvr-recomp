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

// Card weights (DS:50AE / 5164, read by the card choice seg5:7386), double +0xA4
// is the value above which the town plan uses Nimm.
struct AiCardWeights {
    std::array<int16_t, 91> words;  // +0x00 .. +0xB5 (raw, the double at +0xA4 included)
    double takeThreshold;           // +0xA4
    int16_t at(int offset) const { return words[size_t(offset / 2)]; }
    double doubleAt(int offset) const; // the doubles at +0x38, +0x44, +0x50, +0xA4
};

// Money record (DS:52A2 / 52D6 / 530A): property weights by category (word
// 1 + category, seg5:990A) and the cash kept back (+0x12, by Dr. Drago state).
struct AiMoney {
    std::array<int16_t, 9> words;
    std::array<double, 4> reserve;
};

struct AiProfile {
    const AiStrategy* strategy;      // DS:534A
    const AiCardWeights* cards;      // DS:534E
    const AiMoney* money;            // DS:5358
    const std::array<int16_t, 3>* targetOdds; // DS:535C
    int16_t shopRandom;              // DS:5356: random range added to card worth in the shop
};
// Per player slot (DS:534A + slot * 0x16).
const AiProfile& aiProfile(int slot);

// seg5:8B1B: links of masked fields are removed, and links holding an obstacle
// (flag 0, route planning); with `clearable` (flag 1, card choice) only rockfall
// removes a link (graph entries per field: up, right, down, left).
using AiGraph = std::vector<std::array<int16_t, 4>>;
AiGraph buildAiGraph(const Board& board, uint16_t routeMask, const ObstacleTable& obstacles,
                     bool clearable = false);
// seg5:99B3 -> seg5:9DE8: how many fields the player at `start` can drive at most
// (capped at 30; a reachable cycle also gives 30): depth-first without turning back,
// over the graph of seg5:8B1B(flag 0) where only rockfall removes a link and snow /
// rain need a Schneepflug (`ploughCards`), nails a Nägel weg card (`nailsCards`),
// used up along the path.
int reachableSteps(const Board& board, uint16_t routeMask, const ObstacleTable& obstacles, int start,
                   int nailsCards, int ploughCards);
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
    int year = 0;                   // DS:A102
    double inflation = 1.0;
    int remainingStages = 0;        // DS:A104
    int destination = -1;           // DS:A10D
    uint16_t routeMask = 0;         // DS:A10F
    bool steerless = false;         // DS:8D2A: card Steuerlos, random(1000) scores
    std::vector<uint16_t> fieldDist; // DS:22: last field distances (from the destination)
    bool extraCards = false;              // recomb extension: also the never-winning cards
    std::array<bool, 4> nailsDirections{}; // the current player's (Game::nailsDirections)
    std::array<int, 8> airports{-1, -1, -1, -1, -1, -1, -1, -1}; // Flugzeug rows 5602 + i as towns
    int cardCount(int player, int type) const;   // seg3:3FFC (type 0-based here)
    int cardSlot(int player, int type) const;    // seg3:0D26, -1 if none
    int handCount(int player) const;
    double cash(int player) const { return economy->money(player).cash; }
};

// seg5:9A34: cash above the reserve (index 0, or Dr. Drago's form + 1 while the
// player carries him) times inflation, at least 0.
double aiSpareMoney(const AiState& s, int player);

// seg5:8F01: the town dialog plan for `field`. Actions in order: kind 2 = play the
// hand slot `value`, kind 0 = buy entry `value`; kind 3 = sell entry `value` (only
// from the ComputerExtraCards extension). `purchases` is DS:8D28.
struct AiAction { int kind; int value; };
struct AiTownPlan { std::vector<AiAction> actions; int purchases = 0; };
AiTownPlan aiTownPlan(const AiState& s, int player, int field);

// seg5:9B0B: target of Zucker / Schlafen / Blockieren (mode 0, nearest to the
// destination) or Fiskus / Schulden (mode 1, richest); -1 if nobody qualifies.
int aiChooseTarget(const AiState& s, int player, int mode, MsRandom& rng);

// seg5:7386: card to play before spinning. slot -1 = spin (DS:8D20 = 0xFF).
struct AiCardChoice { int slot = -1; int type = -1; int target = -1; };
AiCardChoice aiChooseCard(const AiState& s, int player, const ObstacleTable& obstacles, MsRandom& rng);

// seg5:81AF: worth of card number `type + 1` from the table at DS:521A by game
// year (< 20, 20-39, >= 40); 0xFF = not listed.
int aiCardRank(int type, int year);
// seg5:7CD2: hand slot with the least worth (an unlisted card at once), -1 if none.
int aiLeastValuableCard(const AiState& s, int player);

// Card field (state 12): seg5:7D85 sells every hand card without a worth
// (kind 1, card type), seg5:7E4F buys offers by worth (kind 0, card type).
std::vector<AiAction> aiCardShopPlan(const AiState& s, int player, const std::vector<CardOffer>& offers,
                                     MsRandom& rng);

// seg5:829D with seg5:87A8: the route of exactly `steps` fields with the highest
// score; empty if no route scores above 0. The fields exclude the start.
std::vector<int> aiChooseRoute(const AiState& s, int player, int steps, const AiGraph& graph,
                               const std::vector<uint16_t>& dist, MsRandom& rng);

} // namespace dtvr
