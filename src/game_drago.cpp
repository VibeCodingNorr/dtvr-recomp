// SPDX-License-Identifier: GPL-3.0-only
// Dr. Drago and the cards that involve him (round 10, evidence in docs/formats.md).
#include "game.hpp"

#include <cstdio>

namespace dtvr {
namespace {

std::string amount(double v) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.0f", v);
    return buffer;
}

// seg2:B74D: picks one of `count` texts with random(count - 1).
void pickText(MsRandom& rng, int count) { rng.random(count - 1); }

} // namespace

void Game::dragoAct(int player) {
    if (drago_.holder < 0 || drago_.holder != player) return;
    const DragoAction action = chooseDragoAction(drago_.form, rng_);
    const std::string who = "Dr. Drago (bei Spieler " + std::to_string(player + 1) + "): ";
    const double limit = 30000.0 * calendar_.inflation; // DS:8676 x inflation (seg2:B957)
    auto giveProperty = [&]() {      // seg2:BCAF
        const auto p = economy_.pickProperty(uint8_t(player), limit, rng_);
        if (p.town < 0) { // nothing to give away: buy instead (seg2:BD11)
            const auto free = economy_.pickProperty(kNoOwner, limit, rng_);
            if (free.town < 0) return;
            economy_.buyWithFactor(player, free, 1.5); // DS:8686
            pickText(rng_, 3);
            messages_.push_back(who + "kauft teuer ein Grundstueck");
            return;
        }
        economy_.sellWithFactor(player, p, 0.0); // DS:867E: given away
        pickText(rng_, 11);
        messages_.push_back(who + "verschenkt ein Grundstueck");
    };
    switch (action) {
    case DragoAction::Spend: { // seg2:BA92
        pickText(rng_, 20);
        const double sum = dragoAmount(calendar_.month, drago_.form, calendar_.inflation, rng_);
        economy_.addMoney(player, -sum);
        rng_.random(1); // seg2:BB5E: choice of the follow-up message
        messages_.push_back(who + "gibt " + amount(sum) + " aus");
        break;
    }
    case DragoAction::GiveMoney: { // seg2:BEA4
        pickText(rng_, 5);
        const double sum = dragoAmount(calendar_.month, drago_.form, calendar_.inflation, rng_);
        int to = economy_.poorest(activePlayers());
        if (to == player) to = randomOtherPlayer(players_, player, rng_);
        economy_.addMoney(player, -sum);
        economy_.addMoney(to, sum);
        messages_.push_back(who + "schenkt Spieler " + std::to_string(to + 1) + " " + amount(sum));
        break;
    }
    case DragoAction::TearCard: { // seg2:BDCA
        const int cards = handCount(player);
        if (cards == 0) { giveProperty(); break; }
        const int index = rng_.random(cards - 1);
        auto& slot = hands_[size_t(player)][size_t(index)];
        if (slot == Deck::kTaken) break; // the chosen slot may be empty
        messages_.push_back(who + "zerreisst die Karte " + cardName(slot));
        discard(player, index); // seg2:BE3A -> seg2:3C62
        pickText(rng_, 8);
        break;
    }
    case DragoAction::GiveProperty: giveProperty(); break;
    case DragoAction::Buy: { // seg2:BB95
        const auto free = economy_.pickProperty(kNoOwner, limit, rng_);
        if (free.town < 0) break;
        economy_.buyWithFactor(player, free, 1.5);
        pickText(rng_, 3);
        messages_.push_back(who + "kauft teuer ein Grundstueck");
        break;
    }
    case DragoAction::Block: // seg2:BFB0
        players_[size_t(player)].flags |= kFlagDragoBlock;
        messages_.push_back(who + "blockiert den naechsten Zug");
        break;
    }
}

void Game::placeDragoForStage() {
    // seg2:1495-1535: one active player -> first active; otherwise the player with
    // the largest field distance (assumed: distance to the new destination).
    int chosen = firstActivePlayer(players_);
    if (activePlayerCount(players_) > 1) {
        const auto dist = fieldDistances(board_, destination_);
        int best = 0;
        for (int p = 0; p < kPlayerSlots; ++p) {
            if (!players_[size_t(p)].status) continue;
            const int d = dist[size_t(players_[size_t(p)].field)];
            if (d > best) { best = d; chosen = p; }
        }
    }
    placeDrago(drago_, chosen, rng_);
    messages_.push_back("Dr. Drago steigt bei Spieler " + std::to_string(chosen + 1) + " ein");
}

bool Game::playWeihnachten() {
    const std::string who = playerName(current_);
    if (unsigned(calendar_.month) % 12 != 11) { // only in December (seg3:1174)
        pickText(rng_, 3);
        messages_.push_back(who + ": Weihnachten ist noch nicht - Zug verloren");
    } else if (handCount(current_) < 4 && deck_.peek() != Deck::kTaken) {
        pickText(rng_, 3);
        const int slot = deck_.take(); // seg2:4327
        addToHand(hands_[size_t(current_)], slot);
        messages_.push_back(who + " bekommt zu Weihnachten die Karte " + cardName(slot));
    } else {
        pickText(rng_, 3);
        const double gift = (rng_.random(10) + 10) * 1000.0 * calendar_.inflation; // DS:862E
        economy_.addMoney(current_, gift);
        messages_.push_back(who + " bekommt zu Weihnachten " + amount(gift));
    }
    advanceTurn(); // seg2:06FC
    return true;
}

void Game::playHauAb() {
    const int target = randomOtherPlayer(players_, current_, rng_); // always drawn (seg3:3F13)
    if (drago_.holder == current_ && target != current_) {          // seg2:B4E3
        drago_.holder = target;
        messages_.push_back("Dr. Drago muss zu Spieler " + std::to_string(target + 1));
    } else {
        messages_.push_back("Hau ab: Dr. Drago ist nicht bei Spieler " + std::to_string(current_ + 1));
    }
    advanceTurn();
}

} // namespace dtvr
