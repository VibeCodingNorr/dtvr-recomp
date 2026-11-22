// SPDX-License-Identifier: GPL-3.0-only
// Computer turns (round 16): the seg5 state machine at seg5:6D20 reduced to its
// decisions; see docs/formats.md, "Computer players".
#include "game.hpp"

#include "ai.hpp"

namespace dtvr {

bool Game::setComputer(int player, bool computer) {
    // seg2:5E93 switches a slot between status 1 (human) and 2 (computer).
    if (player < 0 || player >= kPlayerSlots || !players_[size_t(player)].status) return false;
    players_[size_t(player)].status = computer ? 2 : 1;
    return true;
}

bool Game::isComputer(int player) const {
    return player >= 0 && player < kPlayerSlots && players_[size_t(player)].status == 2;
}

bool Game::computerControls() const {
    // seg2:5C02, plus card Steuerlos (bit 0x20, seg2:0A6F) for one turn.
    return isComputer(current_) || (players_[size_t(current_)].flags & kFlagSteerless);
}

AiState Game::aiState(bool steerless) const {
    AiState s;
    s.board = &board_;
    s.towns = &towns_;
    s.economy = &economy_;
    s.players = &players_;
    for (int p = 0; p < kPlayerSlots; ++p)
        for (int k = 0; k < 8; ++k) s.handTypes[size_t(p)][size_t(k)] = deck_.typeOfSlot(hands_[size_t(p)][size_t(k)]);
    s.dragoHolder = drago_.holder;
    s.dragoForm = drago_.form;
    s.month = calendar_.month;
    s.year = calendar_.year;
    s.inflation = calendar_.inflation;
    s.remainingStages = remaining_;
    s.destination = destination_;
    s.routeMask = routeMask();
    s.steerless = steerless;
    // DS:22 holds the last distances of seg2:43A8; during play they come from the
    // stage destination with the route mask (approximation, see docs).
    for (int d : fieldDistances(board_, destination_, routeMask())) s.fieldDist.push_back(d < 0 ? 0xFFFF : uint16_t(d));
    return s;
}

bool Game::runComputerTurn() {
    if (phase_ != Phase::Roll || !computerControls()) return false;
    std::vector<std::string> log;
    struct Acting { bool& flag; explicit Acting(bool& f) : flag(f) { flag = true; } ~Acting() { flag = false; } } acting(aiActing_);
    auto collect = [&] { log.insert(log.end(), messages_.begin(), messages_.end()); };
    // Steuerlos: seg2:0A6F clears the bit and seg5:A1CD sets DS:8D2A = 1.
    const bool steerless = players_[size_t(current_)].flags & kFlagSteerless;
    declineInvestment(); // the computer's answer to an offer is not traced yet
    const int player = current_;
    // State 1: card choice (seg5:7386); without a card the computer spins (state 2).
    const AiCardChoice card = aiChooseCard(aiState(steerless), player, obstacles_, rng_);
    if (card.slot < 0 || !playCard(card.slot, card.target)) roll();
    for (int guard = 0; phase_ == Phase::Risiko && guard < 1000; ++guard) risikoTick(); // presses inside
    players_[size_t(player)].flags &= uint16_t(~kFlagSteerless); // after roll: computerControls() still held
    collect();
    if (phase_ == Phase::Roll && current_ != player) { // the card ended the turn
        messages_ = log;
        return true;
    }
    if (phase_ == Phase::Move) {
        // States 5 and 6: plan the route over exactly the spun number of fields, then drive it.
        const AiState state = aiState(steerless);
        const AiGraph graph = buildAiGraph(board_, routeMask(), obstacles_);
        const auto dist = aiDistances(graph, destination_);
        const auto route = aiChooseRoute(state, player, counter_.allowance(), graph, dist, rng_);
        for (int next : route) {
            if (phase_ != Phase::Move) break;
            const int d = aiDirection(board_, players_[size_t(player)].field, next);
            if (d > 3 || !step(Direction(d + 1))) break;
            collect();
        }
        if (phase_ == Phase::Move) { // no route found: the original state machine would wait
            log.push_back("Computer: keine Route gefunden");
            messages_ = log;
            return false;
        }
    }
    if (phase_ == Phase::Arrived && yellowPending_) { // seg3:1128: seg5:7CD2 picks the card to drop
        const int slot = aiLeastValuableCard(aiState(steerless), player);
        if (slot >= 0) discardForCard(slot);
        collect();
    }
    if (phase_ == Phase::Arrived) {
        // State 13 (seg5:7079): the town plan of seg5:8F01, executed by seg1:49D4.
        const int field = players_[size_t(player)].field;
        if (atCardField()) { // state 12: seg5:7D85 / 7E4F, executed by seg3:4266 (sales first)
            for (const AiAction& a : aiCardShopPlan(aiState(steerless), player, cardShopOffers(deck_), rng_)) {
                if (a.kind == 1) {
                    for (int k = 0; k < 8; ++k)
                        if (deck_.typeOfSlot(hands_[size_t(player)][size_t(k)]) == a.value) { sellCard(k); break; }
                } else buyCard(a.value);
                collect();
            }
        } else if (townAt(field) >= 0) {
            const AiTownPlan plan = aiTownPlan(aiState(steerless), player, field);
            for (const AiAction& a : plan.actions) {
                if (a.kind == 2) playCard(a.value);
                else buyProperty(a.value);
                collect();
            }
        }
        endTurn();
        collect();
    }
    messages_ = log;
    return true;
}

} // namespace dtvr
