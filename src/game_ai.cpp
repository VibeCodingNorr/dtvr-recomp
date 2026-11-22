// SPDX-License-Identifier: GPL-3.0-only
// Computer turns (round 16): the seg5 state machine at seg5:6D20 reduced to its
// decisions; see docs/formats.md, "Computer players".
#include "game.hpp"

#include <algorithm>

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

std::string Game::playerName(int player) const {
    if (player >= 0 && player < kPlayerSlots && !names_[size_t(player)].empty()) return names_[size_t(player)];
    return "Spieler " + std::to_string(player + 1);
}

bool Game::humansLeft() const {
    for (const PlayerSlot& p : players_)
        if (p.status == 1) return true;
    return false;
}

bool Game::requestDemoEnd() {
    if (humansLeft() || phase_ == Phase::GameOver) return false;
    demoEnd_ = true;
    messages_.push_back("Das Spiel endet beim naechsten Spielerwechsel");
    return true;
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
    s.extraCards = computerExtraCards_;
    s.nailsDirections = nailsDirections();
    s.airports = airports_;
    // DS:22 holds the last distances of seg2:43A8; during play they come from the
    // stage destination with the route mask (approximation, see docs).
    for (int d : fieldDistances(board_, destination_, routeMask())) s.fieldDist.push_back(d < 0 ? 0xFFFF : uint16_t(d));
    return s;
}

bool Game::runComputerTurn() {
    const bool takeover = phase_ == Phase::Move || phase_ == Phase::Arrived; // Mitspieler (seg2:5E93)
    if ((phase_ != Phase::Roll && !takeover) || !computerControls()) return false;
    std::vector<std::string> log;
    bool changed = false;
    for (int guard = 0; guard < 10000; ++guard) {
        const bool done = computerAdvance();
        log.insert(log.end(), messages_.begin(), messages_.end());
        changed |= done;
        if (!done || !aiTurn_) break;
    }
    messages_ = log;
    return changed;
}

bool Game::computerAdvance() {
    struct Acting { bool& flag; explicit Acting(bool& f) : flag(f) { flag = true; } ~Acting() { flag = false; } } acting(aiActing_);
    std::vector<std::string> log;
    auto collect = [&] { log.insert(log.end(), messages_.begin(), messages_.end()); };
    auto finish = [&](bool result) { messages_ = log; return result; };
    if (!aiTurn_ && computerControls() && (phase_ == Phase::Move || phase_ == Phase::Arrived)) {
        // Mitspieler (seg2:5E93 -> seg5:6CE8 / 6BF2) handed a running human turn to the
        // computer: it drives the remaining fields (route as states 5 / 6) or, already
        // arrived, continues with its town / card field plan. Assumption: the original's
        // state machine restart is not traced for a turn in progress.
        aiTurn_ = true;
        aiPlayer_ = current_;
        aiSteerless_ = false;
        aiRoute_.clear();
        if (phase_ == Phase::Move) {
            const AiGraph graph = buildAiGraph(board_, routeMask(), obstacles_);
            const auto dist = aiDistances(graph, destination_);
            aiRoute_ = aiChooseRoute(aiState(false), aiPlayer_, counter_.allowance() - counter_.taken(), graph, dist,
                                     rng_);
            std::reverse(aiRoute_.begin(), aiRoute_.end());
        }
        return finish(true);
    }
    if (!aiTurn_) {
        if (phase_ != Phase::Roll || !computerControls()) return false;
        // Steuerlos: seg2:0A6F clears the bit and seg5:A1CD sets DS:8D2A = 1.
        aiSteerless_ = players_[size_t(current_)].flags & kFlagSteerless;
        aiPlayer_ = current_;
        declineInvestment(); // the computer's answer to an offer is not traced yet
        // State 1: card choice (seg5:7386); without a card the computer spins (state 2).
        const AiCardChoice card = aiChooseCard(aiState(aiSteerless_), aiPlayer_, obstacles_, rng_);
        if (card.slot < 0 || !playCard(card.slot, card.target)) roll();
        for (int guard = 0; phase_ == Phase::Risiko && guard < 1000; ++guard) risikoTick(); // presses inside
        players_[size_t(aiPlayer_)].flags &= uint16_t(~kFlagSteerless); // after roll: computerControls() still held
        collect();
        if (phase_ == Phase::Roll && current_ != aiPlayer_) return finish(true); // the card ended the turn
        aiTurn_ = true;
        aiRoute_.clear();
        if (phase_ == Phase::Move) {
            // States 5 and 6: plan the route over exactly the spun number of fields.
            const AiGraph graph = buildAiGraph(board_, routeMask(), obstacles_);
            const auto dist = aiDistances(graph, destination_);
            aiRoute_ = aiChooseRoute(aiState(aiSteerless_), aiPlayer_, counter_.allowance(), graph, dist, rng_);
            std::reverse(aiRoute_.begin(), aiRoute_.end()); // taken from the back
        }
        return finish(true);
    }
    if (phase_ == Phase::Move) { // state 6: one field per stage
        bool stepped = false;
        if (!aiRoute_.empty()) {
            const int next = aiRoute_.back();
            aiRoute_.pop_back();
            const int d = aiDirection(board_, players_[size_t(aiPlayer_)].field, next);
            stepped = d <= 3 && step(Direction(d + 1));
            collect();
        }
        if (!stepped) aiRoute_.clear();
        if (phase_ == Phase::Move && aiRoute_.empty()) { // no route: the state machine would wait
            log.push_back("Computer: keine Route gefunden");
            aiTurn_ = false;
            return finish(false);
        }
        return finish(true);
    }
    aiTurn_ = false;
    if (phase_ == Phase::Arrived && yellowPending_) { // seg3:1128: seg5:7CD2 picks the card to drop
        const int slot = aiLeastValuableCard(aiState(aiSteerless_), aiPlayer_);
        if (slot >= 0) discardForCard(slot);
        collect();
    }
    if (phase_ == Phase::Arrived) {
        // State 13 (seg5:7079): the town plan of seg5:8F01, executed by seg1:49D4.
        const int field = players_[size_t(aiPlayer_)].field;
        if (atCardField()) { // state 12: seg5:7D85 / 7E4F, executed by seg3:4266 (sales first)
            for (const AiAction& a : aiCardShopPlan(aiState(aiSteerless_), aiPlayer_, cardShopOffers(deck_), rng_)) {
                if (a.kind == 1) {
                    for (int k = 0; k < 8; ++k)
                        if (deck_.typeOfSlot(hands_[size_t(aiPlayer_)][size_t(k)]) == a.value) { sellCard(k); break; }
                } else buyCard(a.value);
                collect();
            }
        } else if (townAt(field) >= 0) {
            const AiTownPlan plan = aiTownPlan(aiState(aiSteerless_), aiPlayer_, field);
            for (const AiAction& a : plan.actions) {
                if (a.kind == 2) playCard(a.value);
                else if (a.kind == 3) sellProperty(a.value); // extension only
                else buyProperty(a.value);
                collect();
            }
        }
        endTurn();
        collect();
    }
    return finish(true);
}

} // namespace dtvr
