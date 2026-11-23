// SPDX-License-Identifier: GPL-3.0-only
#include "game.hpp"

#include "ai.hpp"

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

namespace {

std::array<uint8_t, kPlayerSlots> humanSlots(int humans) {
    if (humans < 1 || humans > kPlayerSlots) throw std::invalid_argument("1 bis 8 Spieler");
    std::array<uint8_t, kPlayerSlots> slots{};
    for (int i = 0; i < humans; ++i) slots[size_t(i)] = 1;
    return slots;
}

} // namespace

Game::Game(const Board& board, const std::vector<Town>& towns, int humans, int rounds, uint16_t seed,
           int defaultCards, const WorldMap* map, const TileProperties* props, bool soloOpponent)
    : Game(board, towns, humanSlots(humans), rounds, seed, defaultCards, map, props, soloOpponent) {}

Game::Game(const Board& board, const std::vector<Town>& towns, const std::array<uint8_t, kPlayerSlots>& slots,
           int rounds, uint16_t seed, int defaultCards, const WorldMap* map, const TileProperties* props,
           bool soloOpponent)
    : board_(board), towns_(towns), rng_(seed), economy_(towns, kPlayerSlots), map_(map), props_(props) {
    if (map_ && props_) barriers_ = barrierLinks(board_, *map_, *props_);
    westValue_ = westPropertyValue(board_, towns_, 0x4000); // seg4:0E0B mask, then seg1:A15B
    loadedSeason_ = seasonForMonth(0);
    players_.resize(kPlayerSlots);
    origin_ = startField(board_, towns_);
    if (origin_ < 0) throw std::runtime_error("Startstadt LONDON nicht gefunden");
    // [Player] P0..P7 of rally.ini (seg1:02BA): 0 empty, 1 human, 2 computer.
    for (int i = 0; i < kPlayerSlots; ++i) {
        if (slots[size_t(i)] > 2) throw std::invalid_argument("Platzstatus 0, 1 oder 2");
        players_[size_t(i)].status = slots[size_t(i)];
        if (slots[size_t(i)]) players_[size_t(i)].field = origin_;
    }
    if (activePlayerCount(players_) == 0) throw std::invalid_argument("mindestens ein Spieler");
    if (soloOpponent && activePlayerCount(players_) == 1) {
        // seg2:021F: one player alone gets a computer opponent in a random other
        // slot (random(7) until it differs), announced with text 961 + random(2).
        const int human = firstActivePlayer(players_);
        int slot;
        do slot = rng_.random(7); while (slot == human);
        players_[size_t(slot)].status = 2;
        players_[size_t(slot)].field = origin_;
        messages_.push_back("Computergegner auf Platz " + std::to_string(slot + 1));
        notice(961 + rng_.random(2), {playerArg(human), playerArg(slot)});
    }
    remaining_ = rounds;
    totalStages_ = rounds; // DS:A106 (seg4:0E29)
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

void Game::applyNameCodes() {
    constexpr int kSlot = 7; // seg2:634C: DS:94E2 = 7 for the duration
    auto setCash = [&](double value) { economy_.addMoney(kSlot, value - economy_.money(kSlot).cash); };
    if (names_[kSlot] == "Armin Gessert") setCash(500000.0); // seg2:6361: _fstrcmp, then DS:861E
    if (names_[kSlot] != "130663") return;                    // seg2:6397
    setCash(1000000.0);                                       // DS:8626
    Hand& hand = hands_[kSlot];
    for (int i = 0; i < 8; ++i) {                             // seg2:63C7: every occupied slot ...
        if (hand[size_t(i)] == Deck::kTaken) continue;
        for (uint8_t& free : hand)                            // ... draws into the first free one (seg2:4327)
            if (free == Deck::kTaken) { free = uint8_t(deck_.take()); break; }
    }
    names_[kSlot] = "Armin Gessert";                          // seg2:640F
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
    messages_.push_back(playerName(current_) + " kauft "
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

std::vector<int> Game::ranking() const {
    std::vector<int> order = activePlayers();
    auto worth = [&](int p) { return economy_.money(p).cash + economy_.money(p).propertyValue; };
    for (bool swapped = true; swapped && order.size() > 1;) { // seg1:8F64
        swapped = false;
        for (size_t i = 1; i < order.size(); ++i)
            if (worth(order[i]) > worth(order[i - 1])) {
                std::swap(order[i], order[i - 1]);
                swapped = true;
            }
    }
    return order;
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
    messages_.push_back(playerName(current_) + " verkauft "
                        + oemToUtf8(towns_[size_t(town)].entries[size_t(entry)].name));
    return true;
}

bool Game::startMove(int allowance) {
    counter_ = MoveCounter(allowance);
    phase_ = Phase::Move;
    int nailsCards = 0, ploughCards = 0; // seg3:3FFC(player, 0x27 / 0x25)
    for (uint8_t slot : hands_[size_t(current_)]) {
        const int type = deck_.typeOfSlot(slot);
        nailsCards += type == 38;
        ploughCards += type == 36;
    }
    const int reach = reachableSteps(board_, routeMask(), obstacles_, players_[size_t(current_)].field, nailsCards,
                                     ploughCards);
    if (reach != 0 && reach < allowance) { // seg2:61DA: shortened to what is reachable
        counter_ = MoveCounter(reach);
        if (reach == 1) {
            notice(6003, {playerArg(current_)}); // seg2:61F8
        } else {
            NoticeArg fields;
            fields.text = std::to_string(reach);
            notice(6005 + rng_.random(2), {playerArg(current_), fields}); // seg2:6238
        }
        messages_.push_back(playerName(current_) + " ist eingesperrt: nur " + std::to_string(reach) + " Felder");
        return true;
    }
    if (reach == 0 || allowance == 0) { // seg2:62A4: shut in completely, the turn ends
        notice(6009 + rng_.random(2), {playerArg(current_)}); // seg2:62BB
        messages_.push_back(playerName(current_) + " ist total eingesperrt");
        counter_ = MoveCounter(0);
        phase_ = Phase::Roll;
        advanceTurn(); // seg5:6CE8, seg2:06FC
        return false;
    }
    return true;
}

bool Game::roll() {
    messages_.clear();
    if (phase_ != Phase::Roll || (computerControls() && !aiActing_)) return false;
    declineInvestment();
    int sum = 0;
    sum = spin(1, 5);
    messages_.push_back(playerName(current_) + " wuerfelt " + std::to_string(sum));
    startMove(sum); // seg2:66B9 -> seg2:61B7
    return true;
}

bool Game::answerObstacle(bool yes) {
    if (question_.card < 0) return false;
    const Direction d = question_.direction;
    question_ = ObstacleQuestion{};
    if (!yes) return false;                     // seg2:24FF: only answer 6 uses the card
    obstacleConfirmed_ = true;
    const bool moved = step(d);
    obstacleConfirmed_ = false;
    return moved;
}

bool Game::step(Direction d) {
    messages_.clear();
    if (phase_ != Phase::Move || d == Direction::None || question_.card >= 0) return false;
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
        if (!computerControls() && !obstacleConfirmed_) { // seg2:24A6: humans are asked first
            question_ = ObstacleQuestion{card, d};
            Notice q{600, {}};                             // "Willst Du diese Karte wirklich einsetzen ?"
            q.buttons = 4;
            q.cardPicture = card;                          // DS:1A6C: <n>card_<number>.bmp
            notices_.push_back(std::move(q));
            return false;
        }
        messages_.push_back(playerName(current_) + " raeumt das Hindernis mit "
                            + cardName(hands_[size_t(current_)][size_t(slot)]));
        discard(current_, slot);                                // seg3:0E11 -> seg2:3C62
        obstacles_.remove(o);                                   // seg8:697C
    }
    if (!counter_.step(d, true)) return false;
    p.field = next;
    lastDirection_[size_t(current_)] = int(d);
    // seg2:206B: Carb01 when the last field of the move is reached, Plopp01 otherwise (channel 1).
    sound(1, counter_.taken() == counter_.allowance() ? 9 : 0);
    // seg2:B290 when the vehicle stops on a field: Dr. Drago changes hands.
    if (counter_.taken() == counter_.allowance()) {
        const int before = drago_.holder;
        if (passDragoOnMeeting(drago_, players_, current_) != before)
            messages_.push_back("Dr. Drago faehrt jetzt bei " + playerName(drago_.holder) + " mit");
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
    const std::string who = playerName(current_);
    switch (f.kind) {
    case 1:
    case 2: {
        const auto e = moneyField(f.kind, calendar_.month, calendar_.inflation, rng_);
        economy_.addMoney(current_, e.amount);
        sound(3, f.kind == 1 ? 14 : 1); // moneywin (seg2:0EF1) / Fate03 (seg2:1081)
        messages_.push_back(who + (f.kind == 1 ? " erhaelt " : " bezahlt ") + money(e.amount)
                            + " (Text " + std::to_string(e.textId) + ")");
        // seg2:0F05 / 1095: message with player and unsigned gain/loss amount.
        notice(e.textId, {playerArg(current_), moneyArg(f.kind == 1 ? e.amount : -e.amount)});
        dragoAct(current_);
        break;
    }
    case 3: {
        // seg2:10A5: top card of the deck (seg3:032D(0)); nothing if it is taken.
        if (deck_.peek() == Deck::kTaken) break;
        if (handCount(current_) == 8) { // seg3:1057: one card must be discarded first (text 403)
            yellowPending_ = true;
            messages_.push_back(who + ": Hand voll - erst eine Karte ablegen (Text 403)");
            notice(403);
            break;
        }
        giveYellowCard();
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
            finishMusic_ = r.gameOver || rng_.random(1) == 0 ? "z1" : "z2"; // seg2:11C5/12D8
            ++finishSerial_;
            finishField_ = field;
            finishPlayer_ = current_;
            economy_.addMoney(current_, r.reward);
            messages_.push_back(who + " erreicht das Ziel " + townName(field) + ": " + money(r.reward));
            if (r.gameOver) // seg2:1231: the Master-Rallye texts 354-356
                notice(354 + rng_.random(2), {playerArg(current_), moneyArg(r.reward)});
            else { // seg2:136F: texts 350-352 with stage number A106 - A104 + 1 and A106
                NoticeArg round, total;
                round.text = std::to_string(totalStages_ - remaining_ + 1);
                total.text = std::to_string(totalStages_);
                notice(350 + rng_.random(2), {playerArg(current_), round, total, moneyArg(r.reward)});
            }
            if (r.gameOver) {
                phase_ = Phase::GameOver;
                messages_.push_back("Spielende.");
                return;
            }
            remaining_ = r.remaining;
            // seg2:1486: the stage announcement seg7:372F(1) draws rand() % 5 (seg7:3B4E);
            // before the last stage seg7:372F(3) draws nothing.
            // seg7:372F first chooses the final destination with the old stage start DS:A10B
            // and the reached field DS:A10D (seg7:3755 -> seg5:1E2C; DS:A10B becomes the
            // field only afterwards, seg2:148E), then in mode 1 (not before the last stage)
            // draws rand() % 5 (seg7:3B4E). The light's jumps during the voices draw further
            // destinations whose number depends on the playback time; not modelled.
            const int next = chooseDestination(board_, players_, origin_, destination_, routeMask(), rng_);
            announceMode_ = remaining_ != 0 ? 1 : 3;
            announceVariant_ = remaining_ != 0 ? rng_.rand() % 5 : 3; // seg7:3B4E / 3B5F
            ++announceSerial_;
            origin_ = field;
            destination_ = next;
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

NoticeArg Game::townArg(int town) const {
    NoticeArg a;
    if (town >= 0 && size_t(town) < towns_.size()) a.text = oemToUtf8(towns_[size_t(town)].name);
    return a;
}

double Game::propertyPrice(const Economy::PropertyRef& p) const {
    if (p.town < 0 || size_t(p.town) >= towns_.size() || p.entry < 0 || p.entry > 9) return 0;
    return towns_[size_t(p.town)].entries[size_t(p.entry)].value;
}

int Game::spin(int reels, int maximum) {
    lastReels_ = spinReels(rng_, reels, maximum);
    ++spinSerial_;
    int sum = 0;
    for (int v : lastReels_) sum += v;
    return sum;
}

void Game::giveYellowCard() {
    const int slot = deck_.take();
    addToHand(hands_[size_t(current_)], slot);
    messages_.push_back(playerName(current_) + " bekommt die Karte " + cardName(slot));
    notice(4201 + rng_.random(13), {playerArg(current_)}); // seg2:1127
    dragoAct(current_);
}

bool Game::discardForCard(int handSlot) {
    messages_.clear();
    if (!yellowPending_ || handSlot < 0 || handSlot >= 8 || hands_[size_t(current_)][size_t(handSlot)] == Deck::kTaken)
        return false;
    messages_.push_back(playerName(current_) + " legt " + cardName(hands_[size_t(current_)][size_t(handSlot)]) + " ab");
    discard(current_, handSlot);
    yellowPending_ = false;
    giveYellowCard();
    return true;
}

bool Game::endTurn() {
    messages_.clear();
    if (phase_ != Phase::Arrived || yellowPending_) return false;
    if (!forcedSale_ && cash(current_) < 0 && sellableValue(current_) > 0) { // seg1:4E39
        forcedSale_ = true;
        messages_.push_back(playerName(current_) + " muss Besitz verkaufen");
        return false;
    }
    if (forcedSale_ && forcedSaleLocked()) return false; // seg1:5748: Zaehler2, it stays open
    forcedSale_ = false;
    advanceTurn();
    return true;
}

double Game::sellableValue(int player) const {
    double sum = 0;
    for (size_t t = 0; t < towns_.size(); ++t)
        for (size_t e = 0; e < 10; ++e) {
            const TownEntry& entry = towns_[t].entries[e];
            if (entry.name.empty() || entry.words[1] == 3) continue; // seg1:435E / 43C3
            const PropertyState& p = economy_.town(int(t)).entries[e];
            if (p.owner == uint8_t(player) && p.landlord == kNoOwner) sum += entry.value; // seg1:4382 / 43A2
        }
    return sum;
}

bool Game::forcedSaleLocked() const {
    return current_ >= 0 && cash(current_) < 0 && sellableValue(current_) > 0; // seg1:5857 ff.
}

bool Game::forcedSell(int town, int entry) {
    messages_.clear();
    if (!forcedSale_ || town < 0 || size_t(town) >= towns_.size() || entry < 0 || entry >= 10) return false;
    const PropertyState& p = economy_.town(town).entries[size_t(entry)];
    if (p.owner != uint8_t(current_) || p.landlord != kNoOwner) return false;
    economy_.sellWithFactor(current_, Economy::PropertyRef{town, entry}, 0.7); // DS:868E
    messages_.push_back(playerName(current_) + " verkauft "
                        + oemToUtf8(towns_[size_t(town)].entries[size_t(entry)].name) + " (70 %)");
    return true;
}

void Game::advanceTurn() {
    ++turnSerial_;
    doubleNext_ = false;
    propertyMode_ = 0xFF; // the town dialog closes (seg1:5399 / 5508 / 5767)
    declineInvestment();
    for (const auto& gone : obstacles_.tick()) { // seg8:61FE before the player loop (seg2:071B)
        if (gone.type == kProfessorInvestment || gone.type == kFootballInvestment) {
            int text = 0;
            const double payout = investmentPayout(gone.amount, rng_, &text); // seg8:652C / 6394
            const bool professor = gone.type == kProfessorInvestment;
            if (payout > 0) notice((professor ? 5341 : 5421) + text, {playerArg(gone.player), moneyArg(payout)});
            else notice((professor ? 5381 : 5441) + text, {playerArg(gone.player)});
            // seg8:65C0 / 66AC prof.bmp, seg8:6428 / 6514 fuss.bmp (butt directory)
            notices_.back().picture = professor ? Notice::Picture::Professor : Notice::Picture::Football;
            economy_.addMoney(gone.player, payout);
            messages_.push_back("Investition von " + playerName(gone.player)
                                + (payout > 0 ? ": doppelt zurueck!" : ": verloren"));
            continue;
        }
        messages_.push_back("Hindernis bei (" + std::to_string(gone.x) + "," + std::to_string(gone.y) + ") ist weg");
    }
    TurnHooks hooks;
    hooks.chargeCredits = [this] { economy_.chargeCredits(activePlayers()); };
    hooks.settleMonth = [this] { economy_.settleMonth(); };
    hooks.clearCredits = [this] { economy_.clearCredits(); };
    // seg8:88C6 draws rand() % 7 at once and rand() % 5, rand() % 4 when its window
    // is up (seg8:8C49 / 8C57), then rand() % 4 for each player's announcement
    // (seg8:8E38) and, in the closing part, rand() % 2 for its voice (seg8:9605)
    // unless the letter after the last player is 4 or 5 (docs/formats.md "Year balance").
    hooks.yearBalance = [this] {
        balanceChoice_ = rng_.rand() % 7;
        rng_.rand();
        balanceIntro_ = rng_.rand() % 4;
        // seg8:9075: a player's year is positive when cash + property value reaches the
        // value of the middle entry of the ranking (index (count - 1) / 2, seg1:8EE1).
        const std::vector<int> rank = ranking();
        auto worth = [&](int p) { return economy_.money(p).cash + economy_.money(p).propertyValue; };
        const double middle = rank.empty() ? 0 : worth(rank[(rank.size() - 1) / 2]);
        balance_.clear();
        int letter = balanceChoice_;
        for (int p : activePlayers()) { // seg8:8E38: one announcement per player in play
            BalanceEntry e;
            e.player = p;
            e.variant = rng_.rand() % 4;
            e.positive = !(middle - economy_.money(p).cash > economy_.money(p).propertyValue);
            e.letter = letter;
            letter = (letter + 1) % 7; // seg8:913D
            balance_.push_back(e);
        }
        // seg8:95E5: the closing pos_ voice; assumption: the show runs until the lever
        // is pulled (a human could close it earlier and skip the draw).
        balanceClosing_ = letter == 4 ? 5 : letter == 5 ? 4 : rng_.rand() % 2 + 4;
        ++balanceSerial_;
    };
    hooks.afterSitOut = [this](int player) { dragoAct(player); };
    hooks.dragoHolder = drago_.holder;
    hooks.afterPlayerChange = [this](int player) { eventTick(player); };
    timeLimitActive_ = false; // seg2:0716 -> seg2:3A8E: DS:12 = 0, the timer stops
    const auto change = nextPlayer(players_, current_, calendar_, rng_, hooks);
    for (int p : change.satOut) messages_.push_back(playerName(p) + " setzt aus");
    for (const auto& [text, p] : change.texts) {
        notice(text, {playerArg(p)});
        if (text >= 3251 && text < 3256) notices_.back().picture = Notice::Picture::Drago; // qp.bmp (seg2:C040)
        if (text >= 1000 && text < 1010) sound(5, 15);      // Zucker: Sfx08 (seg2:095B)
        else if (text >= 1010 && text < 1020) sound(5, 20); // Schlafen: Schlaf01 (seg2:09F9)
    }
    if (change.yearsEnded) messages_.push_back("Neues Jahr, Inflation " + std::to_string(calendar_.inflation));
    if (change.player < 0) { phase_ = Phase::GameOver; return; }
    if (demoEnd_ && !humansLeft()) { // seg2:0869: no human and DS:16 == 1
        phase_ = Phase::GameOver;
        messages_.push_back("Spiel beendet");
        return;
    }
    current_ = change.player;
    phase_ = Phase::Roll;
    messages_.push_back(playerName(current_) + " ist dran (Monat "
                        + std::to_string(calendar_.month % 12 + 1) + ")");
    if (dragoTurnStart(drago_, current_, calendar_.year, rng_)) { // seg2:049F
        music_.push_back("f" + std::to_string(drago_.form + 1)); // seg2:B3F6/B429/B442
        messages_.push_back("Dr. Drago wechselt in Form " + std::to_string(drago_.form + 1));
    }
}

} // namespace dtvr
