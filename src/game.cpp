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
           int defaultCards, const WorldMap* map, const TileProperties* props)
    : board_(board), towns_(towns), rng_(seed), economy_(towns, kPlayerSlots), map_(map), props_(props) {
    if (map_ && props_) barriers_ = barrierLinks(board_, *map_, *props_);
    westValue_ = westPropertyValue(board_, towns_, 0x4000); // seg4:0E0B mask, then seg1:A15B
    loadedSeason_ = seasonForMonth(0);
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
    destination_ = chooseDestination(board_, players_, origin_, origin_, routeMask(), rng_);
    messages_.push_back("Start in " + townName(origin_) + ", Ziel " + townName(destination_));
}

std::string Game::townName(int field) const {
    if (field < 0 || size_t(field) >= board_.fields.size()) return "?";
    const Field& f = board_.fields[size_t(field)];
    for (const Town& t : towns_)
        if (t.x == f.x && t.y == f.y) return oemToUtf8(t.name);
    return "Feld " + std::to_string(field);
}

bool Game::linkOpen(int field, Direction d) const {
    if (field < 0 || size_t(field) >= board_.fields.size() || d == Direction::None) return false;
    if (board_.fields[size_t(field)].next[size_t(d) - 1] == Field::kNone) return false;
    return eastOpen_ || barriers_.empty() || !barriers_[size_t(field) * 4 + size_t(d) - 1];
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
    // seg1:7DC5: modes 3-9 come from a property card; 2 (Verkaufen) and 0xFF buy normally.
    const BuyMode mode = propertyMode_ >= 3 && propertyMode_ <= 9 ? BuyMode(propertyMode_) : BuyMode::Normal;
    const double price = economy_.price(town, entry, mode);
    if (!economy_.buy(current_, town, entry, mode)) return false;
    if (mode == BuyMode::Rent || mode == BuyMode::Take) propertyMode_ = 0xFF; // seg1:7F87 / 802C
    messages_.push_back("Spieler " + std::to_string(current_ + 1) + " kauft "
                        + oemToUtf8(towns_[size_t(town)].entries[size_t(entry)].name) + " in "
                        + townName(field) + (mode == BuyMode::Credit ? " auf Kredit" : " fuer " + money(-price)));
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

bool Game::sellProperty(int entry) {
    messages_.clear();
    if (phase_ != Phase::Arrived) return false;
    const int town = townAt(players_[size_t(current_)].field);
    if (town < 0 || !economy_.sell(current_, town, entry, propertyMode_ == 2)) return false; // x1.5 with Verkaufen
    messages_.push_back("Spieler " + std::to_string(current_ + 1) + " verkauft "
                        + oemToUtf8(towns_[size_t(town)].entries[size_t(entry)].name));
    return true;
}

bool Game::roll() {
    messages_.clear();
    if (phase_ != Phase::Roll || (computerControls() && !aiActing_)) return false;
    declineInvestment();
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
    if (!linkOpen(p.field, d)) {
        messages_.push_back("Schlagbaum: Osteuropa ist noch geschlossen"); // seg2:2404
        return false;
    }
    if (const int o = obstacles_.onLink(board_, p.field, d); o >= 0) { // seg2:2404 with an obstacle
        const int card = clearingCard(obstacles_.slots()[size_t(o)].type);
        const int slot = card >= 0 ? findCard(current_, card) : -1;
        if (slot < 0) {
            messages_.push_back("Hindernis auf der Strasse - ohne passende Karte geht es nicht weiter");
            return false;
        }
        messages_.push_back("Spieler " + std::to_string(current_ + 1) + " raeumt das Hindernis mit "
                            + cardName(hands_[size_t(current_)][size_t(slot)]));
        discard(current_, slot);                                // seg3:0E11 -> seg2:3C62
        obstacles_.remove(o);                                   // seg8:697C
    }
    if (!counter_.step(d, true)) return false;
    p.field = next;
    lastDirection_[size_t(current_)] = int(d);
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
    propertyMode_ = 0xFF; // a new town dialog starts without a card mode (seg1:4FB9)
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
    case 5: // seg2:15AE -> seg3:4437: card shop until the turn ends
        messages_.push_back(who + ": Kartenfeld - Karten kaufen (Wert) oder verkaufen (halber Wert)");
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
            destination_ = chooseDestination(board_, players_, origin_, destination_, routeMask(), rng_);
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
    doubleNext_ = false;
    propertyMode_ = 0xFF; // the town dialog closes (seg1:5399 / 5508 / 5767)
    declineInvestment();
    for (const auto& gone : obstacles_.tick()) { // seg8:61FE before the player loop (seg2:071B)
        if (gone.type == kProfessorInvestment || gone.type == kFootballInvestment) {
            const double payout = investmentPayout(gone.amount, rng_); // seg8:652C / 6394
            economy_.addMoney(gone.player, payout);
            messages_.push_back("Investition von Spieler " + std::to_string(gone.player + 1)
                                + (payout > 0 ? ": doppelt zurueck!" : ": verloren"));
            continue;
        }
        messages_.push_back("Hindernis bei (" + std::to_string(gone.x) + "," + std::to_string(gone.y) + ") ist weg");
    }
    TurnHooks hooks;
    hooks.chargeCredits = [this] { economy_.chargeCredits(activePlayers()); };
    hooks.settleMonth = [this] { economy_.settleMonth(); };
    hooks.clearCredits = [this] { economy_.clearCredits(); };
    hooks.afterSitOut = [this](int player) { dragoAct(player); };
    hooks.dragoHolder = drago_.holder;
    hooks.afterPlayerChange = [this](int player) { eventTick(player); };
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
