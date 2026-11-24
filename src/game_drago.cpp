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
int pickText(MsRandom& rng, int count) { return rng.random(count - 1); }

} // namespace

void Game::dragoAct(int player) {
    if (drago_.holder < 0 || drago_.holder != player) return;
    const DragoAction action = chooseDragoAction(drago_.form, rng_);
    const size_t firstNotice = notices_.size();
    const std::string who = "Dr. Drago (bei Spieler " + std::to_string(player + 1) + "): ";
    const double limit = 30000.0 * calendar_.inflation; // DS:8676 x inflation (seg2:B957)
    auto giveProperty = [&]() {      // seg2:BCAF
        const auto p = economy_.pickProperty(uint8_t(player), limit, rng_);
        if (p.town < 0) { // nothing to give away: buy instead (seg2:BD11)
            const auto free = economy_.pickProperty(kNoOwner, limit, rng_);
            if (free.town < 0) return;
            economy_.buyWithFactor(player, free, 1.5); // DS:8686
            notice(3101 + pickText(rng_, 3), {townArg(free.town), moneyArg(propertyPrice(free) * 1.5)});
            messages_.push_back(who + "kauft teuer ein Grundstueck");
            return;
        }
        economy_.sellWithFactor(player, p, 0.0); // DS:867E: given away
        notice(3001 + pickText(rng_, 11), {townArg(p.town)}); // seg2:BD42
        messages_.push_back(who + "verschenkt ein Grundstueck");
    };
    switch (action) {
    case DragoAction::Spend: { // seg2:BA92
        const int text = 3151 + pickText(rng_, 20); // seg2:BAD4
        const double sum = dragoAmount(calendar_.month, drago_.form, calendar_.inflation, rng_);
        economy_.addMoney(player, -sum);
        notice(text, {moneyArg(sum)});
        sound(5, rng_.random(1) == 0 ? 3 : 29); // seg2:BB5E: Lachen01 / Lachen02 (seg2:BB73 / BB84)
        messages_.push_back(who + "gibt " + amount(sum) + " aus");
        break;
    }
    case DragoAction::GiveMoney: { // seg2:BEA4
        const int text = 3201 + pickText(rng_, 5); // seg2:BEE6
        const double sum = dragoAmount(calendar_.month, drago_.form, calendar_.inflation, rng_);
        int to = economy_.poorest(activePlayers());
        if (to == player) to = randomOtherPlayer(players_, player, rng_);
        notice(text, {moneyArg(sum), playerArg(to)});
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
        notice(3051 + pickText(rng_, 8)); // seg2:BE70
        break;
    }
    case DragoAction::GiveProperty: giveProperty(); break;
    case DragoAction::Buy: { // seg2:BB95
        const auto free = economy_.pickProperty(kNoOwner, limit, rng_);
        if (free.town < 0) break;
        economy_.buyWithFactor(player, free, 1.5);
        notice(3101 + pickText(rng_, 3), {townArg(free.town), moneyArg(propertyPrice(free) * 1.5)}); // seg2:BC41
        messages_.push_back(who + "kauft teuer ein Grundstueck");
        break;
    }
    case DragoAction::Block: // seg2:BFB0
        players_[size_t(player)].flags |= kFlagDragoBlock;
        messages_.push_back(who + "blockiert den naechsten Zug");
        break;
    }
    // seg2:BAB5 / BBB8 / BCE1 / BE51 / BEC7: "%s\%d\qp.bmp" replaces the showmaster.
    for (size_t i = firstNotice; i < notices_.size(); ++i) notices_[i].picture = Notice::Picture::Drago;
    if (action == DragoAction::Block) notice(3351); // seg2:BFD6: text 3351, no picture (DS:3E2F "")
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
    ++dragoPlacements_;
    messages_.push_back("Dr. Drago steigt bei Spieler " + std::to_string(chosen + 1) + " ein");
}

void Game::setPosition(int player, int field) {
    // seg2:04AD: +0x16 / +0x18 from the field's cell, +0x3C = +0x3E = field, then
    // seg2:5067 (start facing; a field always lies on its own cell, so the player is
    // never taken out of play there) and seg2:0590: seg2:9C97 for Dr. Drago's holder.
    players_[size_t(player)].field = field;
    if (const int f = startFacing(board_.fields[size_t(field)])) lastDirection_[size_t(player)] = f;
    ++jumps_[size_t(player)];
    if (player == drago_.holder) {
        placeDrago(drago_, player, rng_);
        ++dragoPlacements_;
    }
}

void Game::startMasterRallye() {
    // seg2:5CDE: first = random(12 - players in play) (seg2:5D51-5D66); the players in
    // play are put, in slot order, on the fields at consecutive cells of the table,
    // each with seg2:5067 (seg2:5E4D). Dr. Drago is not re-placed there.
    const int first = rng_.random(12 - activePlayerCount(players_));
    const auto fields = masterStartFields(board_, players_, first);
    for (int p = 0; p < kPlayerSlots; ++p) {
        if (fields[size_t(p)] < 0) continue;
        players_[size_t(p)].field = fields[size_t(p)];
        if (const int f = startFacing(board_.fields[size_t(fields[size_t(p)])])) lastDirection_[size_t(p)] = f;
        ++jumps_[size_t(p)];
    }
    // seg2:5FCA: seg2:5FF7 recomputes the property value +0x5A of every slot (the same
    // pass Economy::poorest makes before its ranking), seg1:8EE1 sorts the players in
    // play by +0x42 + +0x5A; DS:957A, the first, gets Dr. Drago (seg2:146D -> 9C97).
    economy_.poorest(activePlayers());
    const std::vector<int> order = ranking();
    if (order.empty()) return;
    placeDrago(drago_, order.front(), rng_);
    ++dragoPlacements_;
    messages_.push_back("Master-Rallye: alle Fahrer an den Start, Dr. Drago steigt bei Spieler "
                        + std::to_string(order.front() + 1) + " ein");
}

bool Game::playWeihnachten() {
    const std::string who = playerName(current_);
    if (unsigned(calendar_.month) % 12 != 11) { // only in December (seg3:1174)
        notice(13001 + pickText(rng_, 3)); // seg3:1189
        messages_.push_back(who + ": Weihnachten ist noch nicht - Zug verloren");
    } else if (handCount(current_) < 4 && deck_.peek() != Deck::kTaken) {
        notice(13006 + pickText(rng_, 3), {playerArg(current_)}); // seg3:1223
        const int slot = deck_.take(); // seg2:4327
        notices_.back().cardPicture = deck_.typeOfSlot(slot); // seg3:126F: the gift's card picture
        addToHand(hands_[size_t(current_)], slot);
        sound(2, 19); // Hebel05 (seg3:1257)
        messages_.push_back(who + " bekommt zu Weihnachten die Karte " + cardName(slot));
    } else {
        const int text = 13011 + pickText(rng_, 3); // seg3:1292
        const double gift = (rng_.random(10) + 10) * 1000.0 * calendar_.inflation; // DS:862E
        notice(text, {playerArg(current_), moneyArg(gift)});
        economy_.addMoney(current_, gift);
        messages_.push_back(who + " bekommt zu Weihnachten " + amount(gift));
    }
    advanceTurn(); // seg2:06FC
    return true;
}

void Game::playHauAb() {
    const int target = randomOtherPlayer(players_, current_, rng_); // always drawn (seg3:3F13)
    if (drago_.holder == current_ && target != current_) {          // seg2:B4E3
        dragoHandOverFrom_ = players_[size_t(current_)].field;
        ++dragoHandOvers_; // the map drives him along the route to the target
        drago_.holder = target;
        music_.push_back("f3"); // seg2:B53B (Hau ab), no new random draw
        messages_.push_back("Dr. Drago muss zu Spieler " + std::to_string(target + 1));
    } else {
        messages_.push_back("Hau ab: Dr. Drago ist nicht bei Spieler " + std::to_string(current_ + 1));
    }
    advanceTurn();
}

} // namespace dtvr
