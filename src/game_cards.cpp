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

bool Game::openCardWindow() {
    if (spinPending()) return false;
    if (current_ < 0 || !(players_[size_t(current_)].flags & kFlagCardsBlocked)) return true;
    notice(1020 + rng_.random(2), {playerArg(current_)});
    sound(5, 15); // Sfx08 (seg2:0B8D)
    messages_.push_back(playerName(current_) + ": Karten blockiert (Text 1020)");
    return false;
}

uint16_t Game::cellProperties(int x, int y) const {
    return map_ && props_ ? props_->cell(*map_, x, y) : 0;
}

bool Game::nailsInWater() const {
    if (current_ < 0) return false;
    // At rest the plane state 2 holds on sea fields (cell 0x09 = field + water); land
    // fields (0x03) are reached after the landing (seg2:2866) has ended it.
    const Field& f = board_.fields[size_t(players_[size_t(current_)].field)];
    if ((cellProperties(f.x, f.y) & TileProperties::kWater) != 0) return true;
    const auto d = nailsDirections();
    return !(d[0] || d[1] || d[2] || d[3]); // seg3:1D43: no window, the same message
}

std::array<bool, 4> Game::nailsDirections() const {
    std::array<bool, 4> available{};
    if (current_ < 0) return available;
    const Field& f = board_.fields[size_t(players_[size_t(current_)].field)];
    static constexpr int dx[4] = {0, 2, 0, -2}, dy[4] = {-2, 0, 2, 0}; // seg3:226E ff.
    for (size_t k = 0; k < 4; ++k)
        available[k] = f.next[k] != Field::kNone
                       && (cellProperties(f.x + dx[k], f.y + dy[k]) & TileProperties::kSeaRoute) == 0;
    return available;
}

int Game::timeLimitSecond() {
    if (!timeLimitActive_ || current_ < 0 || phase_ == Phase::GameOver) return -1; // seg2:3B22 ff.
    if (timeLimitLeft_ > 0) {
        if (--timeLimitLeft_ == 0) { // seg2:3B49: the time is up
            timeLimitOver_ = 0;
            return 0x22;
        }
        return -1;
    }
    // seg2:3B6D: 500 (DS:85FE) for ten seconds, 750 (DS:8606) for ten, then 1000 (DS:860E).
    const double fine = timeLimitOver_ < 10 ? 500.0 : timeLimitOver_ < 20 ? 750.0 : 1000.0;
    economy_.addMoney(current_, -fine); // seg2:5A47
    ++timeLimitOver_;
    return 0x15; // seg2:3BDF; seg5:607B(1) is not rebuilt
}

bool Game::playCard(int handSlot, int target) {
    messages_.clear();
    if (spinPending()) return false; // the slot window is modal
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
    if (!openCardWindow()) return false; // playing a card opens the card window first
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
    const bool water = kind == CardKind::Naegel && nailsInWater();
    if (kind == CardKind::Naegel && !water
        && (target < 1 || target > 4 || !nailsDirections()[size_t(target) - 1])) { // seg3:1FF2 ff.
        messages_.push_back("Naegel: Richtung mit Strasse waehlen (1-4)");
        return false;
    }
    if (kind == CardKind::Flugzeug && (target < 0 || fieldOfTown(target) < 0)) {
        messages_.push_back("Flugzeug: Zielstadt waehlen");
        return false;
    }
    // Schulden (seg3:1790): the card can be played at any balance; only a negative one
    // is passed on (seg3:17E0), and the message shows the amount, else 0.0 (DS:863E).
    double debt = 0;
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
        if (type >= 29 && type <= 34) notice(11001 + type, {playerArg(current_)}); // "n Felder": 11030-11035
        messages_.push_back(std::to_string(allowance) + " Felder");
        rollUndo_.clear(); // reel cards are not taken back (DS:9AD8 = 0)
        if (move.reels) moveAfterSpin(allowance); // seg2:6426 -> 66B9 -> 61B7 after the window
        else startMove(allowance); // seg3:3A2B ff.: no slot window
        if (type >= 29 && type <= 34 && timeLimitOn_) { // seg3:3AB1 ff. -> seg2:3ABD
            timeLimitLeft_ = (allowance + 2) * 5;
            timeLimitOver_ = 0;
            timeLimitActive_ = true;
        }
        return true;
    }
    case CardKind::Fiskus: { // card texts are 11001 + type (DS:9C71, seg3:1524)
        const double tax = economy_.fiskus(victim);
        messages_.push_back("Steuer " + amountText(tax));
        notice(11010, {playerArg(victim), moneyArg(tax < 0 ? -tax : tax), playerArg(current_)});
        break;
    }
    case CardKind::Konto: notice(11011, {moneyArg(economy_.balanceAccounts(activePlayers()))}); break;
    case CardKind::Zero: economy_.zero(current_); notice(11013, {playerArg(current_)}); break;
    case CardKind::Schulden: // seg3:17B6 target, seg3:17CF cash < 0 -> seg2:5A47 twice
        if (cash(current_) < 0) {
            debt = -cash(current_);
            economy_.passDebt(current_, victim);
        }
        notice(11012, {playerArg(victim), playerArg(current_), moneyArg(debt), playerArg(current_)});
        break;
    case CardKind::Weihnachten: playWeihnachten(); return true; // ends the turn itself
    case CardKind::HauAb: playHauAb(); return true;
    case CardKind::Erdbeben: { // seg3:1BDE: damage in the town the player stands in
        const int text = 5201 + rng_.random(2); // seg2:B74D(5201, 3) draws in any case
        const int town = townAt(here);
        if (town < 0) break; // seg3:1C27: no town, no message and no damage (round 147)
        notice(text, {townArg(town)});
        damageTown(town, 37);
        break;
    }
    case CardKind::Naegel: { // seg3:2096: two cells from the player, duration 4n; the turn goes on
        if (water) { // seg3:1CD8: Platsch, text 5500, no obstacle; the spin follows as usual
            sound(5, 0x12);
            notice(5500);
            messages_.push_back("Die Naegel fallen ins Wasser");
            return true;
        }
        films_.push_back({1, 1, "Naegel.wav"}); // seg3:1DA9 / 1DCE after the direction window
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
            if (p == drago_.holder) placeDrago(drago_, p, rng_), ++dragoPlacements_; // seg3:30A9
        }
        messages_.push_back("Alle Mitspieler kommen zur Konferenz");
        notice(11022);
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
        if (kind == CardKind::Zucker) films_.push_back({3, 1, "Zucker.wav"});    // seg3:1A25 / 1A72
        if (kind == CardKind::Schlafen) films_.push_back({2, 10, "Sab.wav"});    // seg3:1B4A / 1B97: ten times
        int sum = 0;
        sum = spin(1, 5);
        messages_.push_back("wuerfelt " + std::to_string(sum));
        rollUndo_.clear();
        moveAfterSpin(sum); // seg2:66B9 -> seg2:61B7 after the window
        return true;
    }
    }
    advanceTurn(); // the handler calls nextPlayer (seg2:06FC)
    return true;
}

void Game::playPunch() {
    films_.push_back({4, 1, "Punch.wav"}); // seg3:2EC2 / 2F17
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
