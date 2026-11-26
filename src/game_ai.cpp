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

bool Game::switchDriver(int player) {
    // seg4:0804: SELPLAYER (seg2:51EA(0xFF)) -> seg2:5E93. MAIN1 is reachable only through
    // ONFIELD's Hauptmenü (seg4:09DE), and ONFIELD opens at the turn start (seg2:0864 ->
    // 068F) and again after its own entries, never after the spin (seg4:0B76 closes it).
    if (phase_ != Phase::Roll || spinPending() || aiTurn_) return false;
    if (player < 0 || player >= kPlayerSlots || !players_[size_t(player)].status) return false;
    players_[size_t(player)].status = players_[size_t(player)].status == 2 ? 1 : 2; // seg2:5EB4-5EDD
    if (player == current_) {
        // seg2:5EEB-5EF5: seg5:6CE8 clears the state machine (DS:8CDC / 8CDE / 4E5C /
        // 4E5E / 8D2A = 0); seg5:6BF2 sets state 1 (card choice) when the player is now a
        // computer. computerAdvance starts there; a human simply goes on with ONFIELD.
        aiTurn_ = false;
        aiSteerless_ = false;
        aiRoute_.clear();
        aiRoutePlanned_ = false;
    }
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

int Game::escapePlayer() const {
    // DS:94E2: during STAT the new month's first slot (seg2:074E) or the winner, which the
    // rebuild keeps apart from current_ until the change completes.
    return phase_ == Phase::Statement ? statementPlayer_ : current_;
}

Game::EscapeAction Game::computerEscapeAction() const {
    // seg1:00D2 passes the input to seg1:01D7 only while seg2:5C02(DS:94E2) holds: the
    // status byte is 2 and a game runs (DS:94C4). Not traced for the year balance's own
    // loop (BILANZ takes Escape itself, seg8:99AC), so that phase is left out.
    const int p = escapePlayer();
    if (p < 0 || p >= kPlayerSlots || phase_ == Phase::GameOver || phase_ == Phase::Balance || !isComputer(p))
        return EscapeAction::None;
    if (!humansLeft() && demo_) return EscapeAction::DemoEnd; // seg1:0211-021D -> seg2:5CAD
    if (demo_) return EscapeAction::None;                    // seg1:0241: DS:14
    // seg1:024A: DS:10 holds the saved status of a Steuerlos driver from his turn start
    // (seg2:0A7F / 0AC8) until the Return of the arrival restores it (seg2:0CF9-0D15).
    // The rebuild's computer arrives at once, so the arrival phase counts as restored;
    // STAT comes before the turn start of its player (seg2:07E1), with DS:10 = 0.
    if (phase_ != Phase::Statement) {
        if (players_[size_t(p)].flags & kFlagSteerless) return EscapeAction::None;
        if (aiTurn_ && aiSteerless_ && phase_ != Phase::Arrived) return EscapeAction::None;
    }
    return EscapeAction::TakeOver;
}

bool Game::takeOverComputer() {
    if (computerEscapeAction() != EscapeAction::TakeOver) return false;
    const int p = escapePlayer();
    players_[size_t(p)].status = 1; // seg1:0261-026E
    // seg1:0277 -> seg5:6CE8: DS:8CDC / 8CDE / 4E5C / 4E5E / 8D2A = 0. seg5:6D20 acts no
    // more (seg2:5C02 fails), so the human goes on from the current phase.
    aiTurn_ = false;
    aiSteerless_ = false;
    aiRoute_.clear();
    aiRoutePlanned_ = false;
    messages_.push_back(playerName(p) + " wird von einem Menschen gesteuert");
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
    // DS:22 as the computer's code reads it (seg5:7386 / 7E4F / 9B0B): during a turn
    // it always holds the distances from DS:A10D with the mask DS:A10F (round R3b).
    s.fieldDist = distanceTable();
    return s;
}

bool Game::runComputerTurn() {
    if (phase_ != Phase::Roll || !computerControls()) return false;
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

void Game::planComputerRoute() {
    // State 5 (seg5:72C2): the route over exactly the spun number of fields (DS:A778);
    // state 6 drives it.
    const AiGraph graph = buildAiGraph(board_, routeMask(), obstacles_);
    const auto dist = aiDistances(graph, destination_);
    aiRoute_ = aiChooseRoute(aiState(aiSteerless_), aiPlayer_, counter_.allowance(), graph, dist, rng_);
    std::reverse(aiRoute_.begin(), aiRoute_.end()); // taken from the back
    aiRoutePlanned_ = true;
}

bool Game::computerAdvance() {
    struct Acting { bool& flag; explicit Acting(bool& f) : flag(f) { flag = true; } ~Acting() { flag = false; } } acting(aiActing_);
    std::vector<std::string> log;
    auto collect = [&] { log.insert(log.end(), messages_.begin(), messages_.end()); };
    auto finish = [&](bool result) { messages_ = log; return result; };
    if (spinPending()) return false; // the window finishes the spin (finishSpin)
    if (offer_ != EventKind::None && investmentWindow_) return false; // the window declines (seg6:0FB4)
    if (!aiTurn_) {
        if (phase_ != Phase::Roll || !computerControls()) return false;
        // Steuerlos: seg2:0A6F clears the bit and seg5:A1CD sets DS:8D2A = 1.
        aiSteerless_ = players_[size_t(current_)].flags & kFlagSteerless;
        aiPlayer_ = current_;
        declineInvestment(); // no window: the decline of its timeout at once (see setInvestmentWindow)
        // State 1: card choice (seg5:7386); without a card the computer spins (state 2).
        const AiCardChoice card = aiChooseCard(aiState(aiSteerless_), aiPlayer_, obstacles_, rng_);
        if (card.slot < 0 || !playCard(card.slot, card.target)) roll();
        for (int guard = 0; phase_ == Phase::Risiko && guard < 1000; ++guard) risikoTick(); // presses inside
        players_[size_t(aiPlayer_)].flags &= uint16_t(~kFlagSteerless); // after roll: computerControls() still held
        collect();
        if (phase_ == Phase::Roll && current_ != aiPlayer_) return finish(true); // the card ended the turn
        aiTurn_ = true;
        aiRoute_.clear();
        aiRoutePlanned_ = false;
        if (phase_ == Phase::Move) planComputerRoute(); // the move started at once (no slot window)
        return finish(true);
    }
    if (phase_ == Phase::Move && !aiRoutePlanned_) {
        // With the slot window the move starts only once that window has closed
        // (finishSpin: seg2:6426 -> seg2:66B9 -> 61B7); seg2:66F9 then sets the
        // computer's state 5 (seg5:71B3(5)), so the route is planned from here.
        planComputerRoute();
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
        // seg2:15A6 / 15B8 / 1438 ff.: Dr. Drago's action, or the announcement and his
        // placement at a reached destination, follow the dialog and precede seg2:1618.
        settleFieldFollowUp();
        collect();
        // seg1:4E39 -> seg1:4EA8: state 14 (seg5:70B5) sells by the plan of seg5:962A,
        // executed through the statistics window (seg1:50D1).
        if (cash(aiPlayer_) < 0 && sellableValue(aiPlayer_) > 0) {
            forcedSale_ = true;
            for (const auto& [town, entry] : aiForcedSale(aiState(aiSteerless_), aiPlayer_)) forcedSell(town, entry);
            collect();
        }
        endTurn();
        collect();
    }
    return finish(true);
}

} // namespace dtvr
