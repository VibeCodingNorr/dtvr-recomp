// SPDX-License-Identifier: GPL-3.0-only
#include "game.hpp"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace dtvr {
namespace {

std::string money(double v) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%+.0f", v);
    return buffer;
}

} // namespace

Game::Game(const Board& board, const std::vector<Town>& towns, int humans, int rounds, uint16_t seed,
           int defaultCards)
    : board_(board), towns_(towns), rng_(seed), economy_(towns, kPlayerSlots) {
    if (humans < 1 || humans > kPlayerSlots) throw std::invalid_argument("1 bis 8 Spieler");
    players_.resize(kPlayerSlots);
    origin_ = startField(board_, towns_);
    if (origin_ < 0) throw std::runtime_error("Startstadt LONDON nicht gefunden");
    for (int i = 0; i < humans; ++i) {
        players_[size_t(i)].status = 1;
        players_[size_t(i)].field = origin_;
    }
    remaining_ = rounds;
    current_ = firstActivePlayer(players_);
    // Deck (seg3:0000/01AD) and starting hands (seg3:0286, [Setup] defaultCards).
    deck_.shuffle(rng_);
    hands_.assign(kPlayerSlots, emptyHand());
    const int deal = startingCards(defaultCards);
    for (int p = 0; p < kPlayerSlots; ++p)
        if (players_[size_t(p)].status)
            for (int c = 0; c < deal; ++c) addToHand(hands_[size_t(p)], deck_.take());
    // New game: route mask 0x4000 (seg4:0E0B); first destination drawn from the start.
    destination_ = chooseDestination(board_, players_, origin_, origin_, 0x4000, rng_);
    messages_.push_back("Start in " + townName(origin_) + ", Ziel " + townName(destination_));
}

std::string Game::townName(int field) const {
    if (field < 0 || size_t(field) >= board_.fields.size()) return "?";
    const Field& f = board_.fields[size_t(field)];
    for (const Town& t : towns_)
        if (t.x == f.x && t.y == f.y) return oemToUtf8(t.name);
    return "Feld " + std::to_string(field);
}

int Game::townAt(int field) const {
    if (field < 0 || size_t(field) >= board_.fields.size()) return -1;
    const Field& f = board_.fields[size_t(field)];
    for (size_t i = 0; i < towns_.size(); ++i)          // seg2:276B matches coordinates
        if (towns_[i].x == f.x && towns_[i].y == f.y) return int(i);
    return -1;
}

bool Game::buyProperty(int entry) {
    messages_.clear();
    if (phase_ != Phase::Arrived) return false;
    const int field = players_[size_t(current_)].field;
    const int kind = board_.fields[size_t(field)].kind;
    const int town = townAt(field);
    if ((kind != 4 && kind != 6) || town < 0) return false;
    if (!economy_.buy(current_, town, entry, BuyMode::Normal)) return false;
    messages_.push_back("Spieler " + std::to_string(current_ + 1) + " kauft "
                        + oemToUtf8(towns_[size_t(town)].entries[size_t(entry)].name) + " in "
                        + townName(field) + " fuer " + money(-economy_.price(town, entry, BuyMode::Normal)));
    return true;
}

std::string Game::cardName(int deckSlot) const {
    const int type = deck_.typeOfSlot(deckSlot);
    return type < 0 ? "-" : cardTypes()[size_t(type)].name;
}

int Game::handCount(int player) const { // seg2:3C00
    int n = 0;
    for (auto s : hands_[size_t(player)]) n += s != Deck::kTaken;
    return n;
}

std::vector<int> Game::activePlayers() const {
    std::vector<int> active;
    for (int p = 0; p < kPlayerSlots; ++p)
        if (players_[size_t(p)].status) active.push_back(p);
    return active;
}

bool Game::playCard(int handSlot, int target) {
    messages_.clear();
    if (phase_ != Phase::Roll || handSlot < 0 || handSlot >= 8) return false;
    auto& slot = hands_[size_t(current_)][size_t(handSlot)];
    const int type = deck_.typeOfSlot(slot);
    if (type < 0) return false;
    const std::string who = "Spieler " + std::to_string(current_ + 1);
    const std::string card = cardTypes()[size_t(type)].name;
    if (players_[size_t(current_)].flags & kFlagCardsBlocked) {
        messages_.push_back(who + ": Karten blockiert (Text 1020)");
        return false;
    }
    const CardKind kind = cardKind(type);
    if (kind == CardKind::Unsupported) {
        messages_.push_back("Karte " + card + ": Wirkung noch nicht nachgebaut");
        return false;
    }
    int victim = -1;
    if (cardNeedsTarget(kind)) {
        victim = resolveTarget(players_, current_, target);
        if (victim < 0) {
            messages_.push_back("Karte " + card + ": Zielspieler waehlen");
            return false;
        }
    }
    if (kind == CardKind::Schulden && !economy_.passDebt(current_, victim)) {
        messages_.push_back("Karte " + card + ": nur mit negativem Kontostand");
        return false;
    }
    slot = Deck::kTaken;
    const std::string target_name = victim >= 0 ? " auf Spieler " + std::to_string(victim + 1) : "";
    messages_.push_back(who + " spielt " + card + target_name);
    switch (kind) {
    case CardKind::Movement: {
        const CardMove move = cardMove(type);
        int allowance = move.fields;
        if (move.reels) for (int v : spinReels(rng_, move.reels, move.maximum)) allowance += v;
        counter_ = MoveCounter(allowance);
        phase_ = Phase::Move;
        messages_.push_back(std::to_string(allowance) + " Felder");
        return true;
    }
    case CardKind::Fiskus: messages_.push_back("Steuer " + money(-economy_.fiskus(victim))); break;
    case CardKind::Konto: economy_.balanceAccounts(activePlayers()); break;
    case CardKind::Zero: economy_.zero(current_); break;
    case CardKind::Schulden: break; // applied above
    case CardKind::Weihnachten: playWeihnachten(); return true; // ends the turn itself
    case CardKind::HauAb: playHauAb(); return true;
    default: { // status cards: flag on the target, then a normal one-reel spin
        players_[size_t(victim)].flags |= cardStatusFlag(kind);
        int sum = 0;
        for (int v : spinReels(rng_, 1, 5)) sum += v;
        counter_ = MoveCounter(sum);
        phase_ = Phase::Move;
        messages_.push_back("wuerfelt " + std::to_string(sum));
        return true;
    }
    }
    advanceTurn(); // money cards: the handler calls nextPlayer (seg2:06FC)
    return true;
}

bool Game::sellProperty(int entry) {
    messages_.clear();
    if (phase_ != Phase::Arrived) return false;
    const int town = townAt(players_[size_t(current_)].field);
    if (town < 0 || !economy_.sell(current_, town, entry, false)) return false;
    messages_.push_back("Spieler " + std::to_string(current_ + 1) + " verkauft "
                        + oemToUtf8(towns_[size_t(town)].entries[size_t(entry)].name));
    return true;
}

bool Game::roll() {
    messages_.clear();
    if (phase_ != Phase::Roll) return false;
    int sum = 0;
    for (int v : spinReels(rng_, 1, 5)) sum += v;
    counter_ = MoveCounter(sum);
    phase_ = Phase::Move;
    messages_.push_back("Spieler " + std::to_string(current_ + 1) + " wuerfelt " + std::to_string(sum));
    return true;
}

bool Game::step(Direction d) {
    messages_.clear();
    if (phase_ != Phase::Move || d == Direction::None) return false;
    PlayerSlot& p = players_[size_t(current_)];
    const int16_t next = board_.fields[size_t(p.field)].next[size_t(d) - 1];
    if (next == Field::kNone) return false;
    if (!counter_.step(d, true)) return false;
    p.field = next;
    // seg2:B290 when the vehicle stops on a field: Dr. Drago changes hands.
    if (counter_.taken() == counter_.allowance()) {
        const int before = drago_.holder;
        if (passDragoOnMeeting(drago_, players_, current_) != before)
            messages_.push_back("Dr. Drago faehrt jetzt bei Spieler " + std::to_string(drago_.holder + 1) + " mit");
    }
    // Field-to-field movement keeps the vehicle grid-aligned; no dialog is modelled.
    if (arrivalTriggers(next, 0, 0, counter_.taken(), counter_.allowance(), false)) arrive();
    return true;
}

void Game::arrive() {
    // seg2:0D38 / dispatch seg2:160C.
    phase_ = Phase::Arrived;
    const int field = players_[size_t(current_)].field;
    const Field& f = board_.fields[size_t(field)];
    const std::string who = "Spieler " + std::to_string(current_ + 1);
    switch (f.kind) {
    case 1:
    case 2: {
        const auto e = moneyField(f.kind, calendar_.month, calendar_.inflation, rng_);
        economy_.addMoney(current_, e.amount);
        messages_.push_back(who + (f.kind == 1 ? " erhaelt " : " bezahlt ") + money(e.amount)
                            + " (Text " + std::to_string(e.textId) + ")");
        dragoAct(current_);
        break;
    }
    case 3: {
        // seg2:10A5: top card of the deck (seg3:032D(0)); nothing if it is taken.
        if (deck_.peek() == Deck::kTaken) break;
        Hand& hand = hands_[size_t(current_)];
        if (std::find(hand.begin(), hand.end(), Deck::kTaken) == hand.end()) {
            messages_.push_back(who + ": Hand voll - erst eine Karte ablegen (Text 403)");
            break;
        }
        const int slot = deck_.take();
        addToHand(hand, slot);
        messages_.push_back(who + " bekommt die Karte " + cardName(slot));
        rng_.random(13); // text 4201 + random(13), seg2:1127
        dragoAct(current_);
        break;
    }
    case 5:
        messages_.push_back(who + ": Kartenfeld (noch nicht nachgebaut)");
        dragoAct(current_);
        break;
    case 4:
    case 6:
        if (field == destination_) {
            const auto r = reachDestination(remaining_, calendar_.inflation);
            economy_.addMoney(current_, r.reward);
            messages_.push_back(who + " erreicht das Ziel " + townName(field) + ": " + money(r.reward));
            if (r.gameOver) {
                phase_ = Phase::GameOver;
                messages_.push_back("Spielende.");
                return;
            }
            remaining_ = r.remaining;
            origin_ = field;
            destination_ = chooseDestination(board_, players_, origin_, destination_, 0x4000, rng_);
            messages_.push_back(std::string(r.lastStageNext ? "Letzte Etappe" : "Neue Etappe")
                                + ", Ziel " + townName(destination_));
            placeDragoForStage();
        } else {
            messages_.push_back(who + " in " + townName(field) + ": Grundstuecke kaufen moeglich");
            dragoAct(current_);
        }
        break;
    default: break;
    }
}

bool Game::endTurn() {
    messages_.clear();
    if (phase_ != Phase::Arrived) return false;
    advanceTurn();
    return true;
}

void Game::advanceTurn() {
    TurnHooks hooks;
    hooks.chargeCredits = [this] { economy_.chargeCredits(activePlayers()); };
    hooks.settleMonth = [this] { economy_.settleMonth(); };
    hooks.clearCredits = [this] { economy_.clearCredits(); };
    hooks.afterSitOut = [this](int player) { dragoAct(player); };
    hooks.dragoHolder = drago_.holder;
    const auto change = nextPlayer(players_, current_, calendar_, rng_, hooks);
    for (int p : change.satOut) messages_.push_back("Spieler " + std::to_string(p + 1) + " setzt aus");
    if (change.yearsEnded) messages_.push_back("Neues Jahr, Inflation " + std::to_string(calendar_.inflation));
    if (change.player < 0) { phase_ = Phase::GameOver; return; }
    current_ = change.player;
    phase_ = Phase::Roll;
    messages_.push_back("Spieler " + std::to_string(current_ + 1) + " ist dran (Monat "
                        + std::to_string(calendar_.month % 12 + 1) + ")");
    if (dragoTurnStart(drago_, current_, calendar_.year, rng_)) // seg2:049F
        messages_.push_back("Dr. Drago wechselt in Form " + std::to_string(drago_.form + 1));
}

} // namespace dtvr
