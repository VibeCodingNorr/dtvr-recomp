// SPDX-License-Identifier: GPL-3.0-only
// Playing hand cards (rounds 7, 9, 10, 13; evidence in docs/formats.md).
#include "game.hpp"

#include <cstdio>

namespace dtvr {
namespace {

std::string amountText(double v) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.0f", v);
    return buffer;
}

} // namespace

bool Game::playCard(int handSlot, int target) {
    messages_.clear();
    // The card window allows category 2 before spinning (seg4:0951) and category 1
    // from the town dialog (seg1:54C1); seg3:0701 compares (category & DS:18).
    const bool inTown = phase_ == Phase::Arrived && townAt(players_[size_t(current_)].field) >= 0;
    if ((phase_ != Phase::Roll && !inTown) || handSlot < 0 || handSlot >= 8) return false;
    if (computerControls() && !aiActing_) return false; // the computer plays this turn
    auto& slot = hands_[size_t(current_)][size_t(handSlot)];
    const int type = deck_.typeOfSlot(slot);
    if (type < 0) return false;
    const std::string who = playerName(current_);
    const std::string card = cardTypes()[size_t(type)].name;
    if (players_[size_t(current_)].flags & kFlagCardsBlocked) {
        messages_.push_back(who + ": Karten blockiert (Text 1020)");
        return false;
    }
    const CardKind kind = cardKind(type);
    if ((cardTypes()[size_t(type)].category & (inTown ? 1 : 2)) != cardTypes()[size_t(type)].category) {
        messages_.push_back("Karte " + card + (inTown ? ": nur vor dem Drehen" : ": nur im Stadtdialog"));
        return false;
    }
    if (kind == CardKind::Property && propertyMode_ != 0xFF) { // seg1:549E: one mode at a time
        messages_.push_back("Karte " + card + ": eine Grundstueckskarte ist schon aktiv");
        return false;
    }
    if (kind == CardKind::Unsupported) {
        messages_.push_back("Karte " + card + ": Wirkung noch nicht nachgebaut");
        return false;
    }
    // Preconditions that must hold before the card is used.
    int victim = -1;
    if (cardNeedsTarget(kind)) {
        victim = resolveTarget(players_, current_, target);
        if (victim < 0) { messages_.push_back("Karte " + card + ": Zielspieler waehlen"); return false; }
    }
    if (kind == CardKind::Tschuess) {
        victim = resolveAnyTarget(players_, target);
        if (victim < 0) { messages_.push_back("Tschuess: Zielspieler waehlen"); return false; }
    }
    const int here = players_[size_t(current_)].field;
    if (kind == CardKind::Naegel && (target < 1 || target > 4
                                     || board_.fields[size_t(here)].next[size_t(target) - 1] == Field::kNone)) {
        messages_.push_back("Naegel: Richtung mit Strasse waehlen (1-4)");
        return false;
    }
    if (kind == CardKind::Flugzeug && (target < 0 || fieldOfTown(target) < 0)) {
        messages_.push_back("Flugzeug: Zielstadt waehlen");
        return false;
    }
    if (kind == CardKind::Schulden && !economy_.passDebt(current_, victim)) {
        messages_.push_back("Karte " + card + ": nur mit negativem Kontostand");
        return false;
    }
    declineInvestment();
    // seg3:0E83: with Verdoppeln active the card stays in the hand.
    if (doubleNext_ && kind != CardKind::Verdoppeln) doubleNext_ = false;
    else discard(current_, handSlot); // seg2:3C62
    messages_.push_back(who + " spielt " + card + (victim >= 0 ? " auf " + playerName(victim) : ""));
    switch (kind) {
    case CardKind::Movement: {
        const CardMove move = cardMove(type);
        int allowance = move.fields;
        if (move.reels) allowance += spin(move.reels, move.maximum);
        counter_ = MoveCounter(allowance);
        phase_ = Phase::Move;
        messages_.push_back(std::to_string(allowance) + " Felder");
        return true;
    }
    case CardKind::Fiskus: messages_.push_back("Steuer " + amountText(economy_.fiskus(victim))); break;
    case CardKind::Konto: economy_.balanceAccounts(activePlayers()); break;
    case CardKind::Zero: economy_.zero(current_); break;
    case CardKind::Schulden: break; // applied above
    case CardKind::Weihnachten: playWeihnachten(); return true; // ends the turn itself
    case CardKind::HauAb: playHauAb(); return true;
    case CardKind::Erdbeben: { // seg3:1BDE: damage in the town the player stands in
        rng_.random(2);
        const int town = townAt(here);
        if (town >= 0) damageTown(town, 37);
        break;
    }
    case CardKind::Naegel: { // seg3:2096: two cells from the player, duration 4n; the turn goes on
        const Field& f = board_.fields[size_t(here)];
        static constexpr int dx[4] = {0, 2, 0, -2}, dy[4] = {-2, 0, 2, 0};
        const int x = f.x + dx[target - 1], y = f.y + dy[target - 1];
        obstacles_.add({kNails, 4 * activePlayerCount(players_), x, y, here, target});
        messages_.push_back("Naegel liegen bei (" + std::to_string(x) + "," + std::to_string(y) + ")");
        return true;
    }
    case CardKind::Punch: playPunch(); break;
    case CardKind::Konferenz: { // seg3:2F5F: everybody comes to the player's position
        for (int p = 0; p < kPlayerSlots; ++p) {
            if (!players_[size_t(p)].status || p == current_) continue;
            players_[size_t(p)].field = here;
            if (p == drago_.holder) placeDrago(drago_, p, rng_); // seg3:30A9
        }
        messages_.push_back("Alle Mitspieler kommen zur Konferenz");
        break;
    }
    case CardKind::Flugzeug: { // seg3:31D5: fly to a town, then the town's field action
        players_[size_t(current_)].field = fieldOfTown(target);
        counter_ = MoveCounter(0);
        phase_ = Phase::Move;
        messages_.push_back("Flug nach " + townName(fieldOfTown(target)));
        arrive();
        return true;
    }
    case CardKind::Property: // seg3:134D-1481: the town dialog buys / sells in this mode
        propertyMode_ = type + 1;
        messages_.push_back("Kaufmodus " + card + " aktiv bis zum Zugende");
        return true;
    case CardKind::Tschuess: playTschuess(victim); break;
    case CardKind::Risiko: startRisiko(); return true;
    case CardKind::Verdoppeln: // seg3:3FBB: the next card is not used up
        doubleNext_ = true;
        messages_.push_back("Die naechste Karte bleibt auf der Hand");
        return true;
    default: { // status cards: flag on the target, then a normal one-reel spin
        players_[size_t(victim)].flags |= cardStatusFlag(kind);
        int sum = 0;
        sum = spin(1, 5);
        counter_ = MoveCounter(sum);
        phase_ = Phase::Move;
        messages_.push_back("wuerfelt " + std::to_string(sum));
        return true;
    }
    }
    advanceTurn(); // the handler calls nextPlayer (seg2:06FC)
    return true;
}

void Game::playPunch() {
    // seg3:2D17: the first other active player on the same field is pushed up to
    // four fields against the puncher's driving direction (table at seg3:2D2B);
    // blocked directions turn clockwise, never into the puncher's own direction.
    const int here = players_[size_t(current_)].field;
    const int facing = lastDirection_[size_t(current_)]; // 1..4
    for (int p = 0; p < kPlayerSlots; ++p) {
        if (p == current_ || !players_[size_t(p)].status || players_[size_t(p)].field != here) continue;
        int at = here;
        int d = (facing + 1) % 4; // 0-based index of the opposite direction: 1->2, 2->3, 3->0, 4->1
        for (int stepCount = 0; stepCount < 4; ++stepCount) {
            for (int attempt = 0; attempt < 4; ++attempt) {
                const int16_t next = board_.fields[size_t(at)].next[size_t(d)];
                if (next != Field::kNone && !(board_.fields[size_t(next)].flags & routeMask())) { at = next; break; }
                d = d >= 3 ? 0 : d + 1;
                if (facing - d - 1 == 0) d = d >= 3 ? 0 : d + 1;
            }
        }
        players_[size_t(p)].field = at;
        messages_.push_back("Punch! " + playerName(p) + " landet auf " + townName(at));
        return;
    }
    messages_.push_back("Punch: niemand auf diesem Feld");
}

void Game::discard(int player, int handSlot) {
    auto& slot = hands_[size_t(player)][size_t(handSlot)];
    if (slot == Deck::kTaken) return;
    deck_.giveBack(slot); // seg3:03B5
    slot = Deck::kTaken;
}

bool Game::atCardField() const {
    return phase_ == Phase::Arrived && board_.fields[size_t(players_[size_t(current_)].field)].kind == 5;
}

std::vector<CardOffer> Game::cardOffers() const {
    return atCardField() ? cardShopOffers(deck_) : std::vector<CardOffer>{};
}

bool Game::buyCard(int type) {
    // seg3:47F1: fewer than 8 cards and cash >= value; the card is swapped to the
    // draw position (seg3:59AA) and drawn into the first free slot (seg2:4327).
    messages_.clear();
    if (!atCardField()) return false;
    for (const CardOffer& o : cardShopOffers(deck_)) {
        if (o.type != type) continue;
        if (handCount(current_) >= 8 || cash(current_) < cardBuyPrice(type)) {
            messages_.push_back("Karte zu teuer oder Hand voll");
            return false;
        }
        deck_.swapWithPointer(o.position);
        addToHand(hands_[size_t(current_)], deck_.take());
        economy_.addMoney(current_, -cardBuyPrice(type)); // seg2:5A47
        messages_.push_back(playerName(current_) + " kauft die Karte " + cardTypes()[size_t(type)].name);
        return true;
    }
    return false;
}

bool Game::sellCard(int handSlot) {
    // seg3:4912: half the value (DS:8646); the card returns to the deck.
    messages_.clear();
    if (!atCardField() || handSlot < 0 || handSlot >= 8) return false;
    const int type = deck_.typeOfSlot(hands_[size_t(current_)][size_t(handSlot)]);
    if (type < 0) return false;
    discard(current_, handSlot);
    economy_.addMoney(current_, cardSellPrice(type)); // seg2:59E6
    messages_.push_back(playerName(current_) + " verkauft die Karte " + cardTypes()[size_t(type)].name);
    return true;
}

void Game::playTschuess(int target) {
    // seg3:28ED: the target is picked up and set down on a random field that is
    // not visible (seg2:47D2, then seg2:04AD); afterwards nextPlayer.
    const Field& at = board_.fields[size_t(players_[size_t(target)].field)];
    const int field = pickHiddenField(board_, routeMask(), viewAround(at), rng_);
    if (field < 0) return;
    players_[size_t(target)].field = field;
    messages_.push_back("Tschuess! " + playerName(target) + " landet bei " + townName(field));
}

bool Game::invest(int amount) {
    messages_.clear();
    if (offer_ == EventKind::None || amount < 1000 || amount % 1000 != 0 || amount > cash(current_)) return false;
    const uint16_t type = offer_ == EventKind::Professor ? kProfessorInvestment : kFootballInvestment;
    Obstacle entry;
    entry.type = type;
    entry.duration = activePlayerCount(players_); // seg8:7306
    entry.player = current_;
    entry.amount = amount;
    obstacles_.add(entry);
    economy_.addMoney(current_, -amount);       // seg8:735F
    messages_.push_back(playerName(current_) + " investiert " + std::to_string(amount));
    offer_ = EventKind::None;
    return true;
}

void Game::declineInvestment() { offer_ = EventKind::None; }

int Game::fieldOfTown(int town) const {
    if (town < 0 || size_t(town) >= towns_.size()) return -1;
    for (size_t i = 0; i < board_.fields.size(); ++i) {
        const Field& f = board_.fields[i];
        if ((f.kind == 4 || f.kind == 6) && f.x == towns_[size_t(town)].x && f.y == towns_[size_t(town)].y) return int(i);
    }
    return -1;
}

} // namespace dtvr
