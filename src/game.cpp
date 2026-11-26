// SPDX-License-Identifier: GPL-3.0-only
#include "game.hpp"

#include "ai.hpp"
#include "save.hpp"

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
    : Game(board, towns, slots, rounds, MsRandom(seed), defaultCards, map, props, soloOpponent) {}

Game::Game(const Board& board, const std::vector<Town>& towns, const std::array<uint8_t, kPlayerSlots>& slots,
           int rounds, MsRandom random, int defaultCards, const WorldMap* map, const TileProperties* props,
           bool soloOpponent)
    : board_(board), towns_(towns), rng_(random), economy_(towns, kPlayerSlots), map_(map), props_(props) {
    if (map_ && props_) barriers_ = barrierLinks(board_, *map_, *props_);
    westValue_ = westPropertyValue(board_, towns_, 0x4000); // seg4:0E0B mask, then seg1:A15B
    loadedSeason_ = seasonForMonth(0);
    players_.resize(kPlayerSlots);
    origin_ = startField(board_, towns_);
    if (origin_ < 0) throw std::runtime_error("Startstadt LONDON nicht gefunden");
    // seg2:0184: every slot starts with the facing of the start field (seg2:5067).
    if (const int f = startFacing(board_.fields[size_t(origin_)])) lastDirection_.fill(f);
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
    auto setCash = [&](double value) { economy_.setCash(kSlot, value); }; // direct writes: +0x4A stays
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
    if (phase_ != Phase::Roll || spinPending() || (computerControls() && !aiActing_)) return false;
    // A human's Würfeln can be taken back while its slot window turns (seg2:69AD).
    rollUndo_ = spinPresentation_ && !computerControls() ? SaveCodec::encode(*this) : std::string();
    declineInvestment();
    int sum = 0;
    sum = spin(1, 5);
    messages_.push_back(playerName(current_) + " wuerfelt " + std::to_string(sum));
    moveAfterSpin(sum); // seg2:66B9 -> seg2:61B7 once the window has closed
    return true;
}

void Game::moveAfterSpin(int allowance) {
    if (spinPresentation_) pendingAllowance_ = allowance;
    else startMove(allowance);
}

bool Game::finishSpin() {
    if (!spinPending()) return false;
    messages_.clear();
    const int allowance = pendingAllowance_;
    pendingAllowance_ = -1;
    rollUndo_.clear();
    startMove(allowance); // seg2:66B9 -> seg2:61B7
    return true;
}

bool Game::cancelRoll(bool reelsDrawn) {
    if (!rollCancellable()) return false;
    std::string before;
    before.swap(rollUndo_);
    std::string error;
    if (!SaveCodec::decode(*this, before, error)) return false; // also ends the pending spin
    if (reelsDrawn) spinReels(rng_, 1, 5); // seg2:776A has drawn the reel
    messages_.push_back(playerName(current_) + " nimmt den Wurf zurueck");
    return true;
}

bool Game::answerObstacle(bool yes) {
    if (question_.card < 0) return false;
    const Direction d = question_.direction;
    question_ = ObstacleQuestion{};
    if (!yes) {                                 // seg2:24FF: only answer 6 uses the card
        honk(players_[size_t(current_)].field); // the vehicle stops and turns back (see step)
        return false;
    }
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
    // seg2:1E5B: the vehicle leaves the field only when seg2:3620(d, 0) allows the
    // step; barriers and obstacles (seg2:2404) are met on the road afterwards
    // (seg2:21D0 ff.), so a refused step never reaches them.
    if (!counter_.step(d, false)) return false;
    // seg2:21D0-21EB: in front of a cell with tile flag 0x20 (a closed border barrier,
    // or an obstacle tile, seg2:3305) Carb01 plays on channel 1 before seg2:2404
    // checks it. Only a road cell runs this (a field cell goes to the field branch,
    // seg2:1E00); every barrier cell lies at least two cells from both fields (data
    // check, R3a), so a road cell always comes first. The confirmed second call after
    // the card question is the same encounter.
    const bool barrier = !linkOpen(p.field, d);
    const int o = barrier ? -1 : obstacles_.onLink(board_, p.field, d);
    if ((barrier || o >= 0) && !obstacleConfirmed_) sound(1, 9);
    // seg2:2246 stops the vehicle in front of the barrier cell; it can only turn back
    // (seg2:2189 -> 22E1 undoes the step) and reaches its field again, where the
    // stop or pass at seg2:2021 / 1E49 runs the horn check seg2:2676.
    if (barrier) {
        messages_.push_back("Schlagbaum: Osteuropa ist noch geschlossen"); // seg2:2404
        honk(p.field);
        return false;
    }
    if (o >= 0) { // seg2:2404 with an obstacle
        const int card = clearingCard(obstacles_.slots()[size_t(o)].type);
        const int slot = card >= 0 ? findCard(current_, card) : -1;
        if (slot < 0) {
            messages_.push_back("Hindernis auf der Strasse - ohne passende Karte geht es nicht weiter");
            honk(p.field); // back on the field as at the barrier above
            return false;
        }
        if (!computerControls() && !obstacleConfirmed_) { // seg2:24A6: humans are asked first
            question_ = ObstacleQuestion{card, d};
            Notice q{600, {}};                             // "Willst Du diese Karte wirklich einsetzen ?"
            q.buttons = 4;
            q.cardPicture = card;                          // DS:1A6C: <n>card_<number>.bmp
            pushNotice(std::move(q));
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
    const bool last = counter_.taken() == counter_.allowance();
    sound(1, last ? 9 : 0);
    // seg2:1F81 (driving on) and seg2:209F (stopping before the last field): the horn
    // check follows Plopp01; the stop with Carb01 (seg2:2082) has none.
    if (!last) honk(next);
    // seg2:B290 when the vehicle stops on a field: Dr. Drago changes hands.
    if (counter_.taken() == counter_.allowance()) {
        const int before = drago_.holder;
        if (passDragoOnMeeting(drago_, players_, current_) != before)
            messages_.push_back("Dr. Drago faehrt jetzt bei " + playerName(drago_.holder) + " mit");
    }
    // Field-to-field movement keeps the vehicle grid-aligned; no dialog is modelled.
    // A human with a game window accepts the field with Return (pressReturn).
    if (arrivalTriggers(next, 0, 0, counter_.taken(), counter_.allowance(), false)
        && !(arrivalConfirm_ && !computerControls())) {
        returnEffects(); // the Return of seg5:6E12 (computer) or of a human at once
        arrive();
    }
    return true;
}

void Game::honk(int field) {
    // seg2:2676: DS:9C74 (set at the turn start seg2:048F, cleared by the first pass of
    // seg2:1CB0 that rests on the start field, seg2:215E) is always 0 by the first
    // step. Slots 0..7 in order, the mover skipped, any status != 0 (humans and
    // computers alike; Dr. Drago has no slot): the first one whose +0x3C equals the
    // field (DS:9592) draws random(1) (seg6:1343); on 0 sound 10 + mover / 2 on
    // channel 2 (seg1:2C9B). Only that first driver is checked.
    for (int q = 0; q < kPlayerSlots && size_t(q) < players_.size(); ++q) {
        if (q == current_ || !players_[size_t(q)].status || players_[size_t(q)].field != field) continue;
        if (rng_.random(1) == 0) sound(2, 10 + current_ / 2);
        return;
    }
}

bool Game::arrivalDue() const {
    return phase_ == Phase::Move && question_.card < 0
        && arrivalTriggers(players_[size_t(current_)].field, 0, 0, counter_.taken(), counter_.allowance(), false);
}

bool Game::pressReturn(bool aligned) { // seg2:0BC3
    messages_.clear();
    if (phase_ != Phase::Roll && phase_ != Phase::Move) return false;
    returnEffects();
    if (!aligned || !arrivalDue()) return false; // seg2:0D38-0D6F, else the view point (window)
    arrive();
    return true;
}

void Game::returnEffects() {
    // seg2:0CE8: DS:2EB0 = 0, Verdoppeln ends unused (its only other clear is the card
    // handler seg3:0E9F). seg2:0CEE: seg8:0362 kills the TimeOut timer DS:A7C8, which
    // only seg2:3ABD starts again. DS:10 (Steuerlos status, seg2:0CF9) is not modelled
    // that way here, and the computer's 1000 ms wait (seg2:0D2D) is presentation.
    doubleNext_ = false;
    timeLimitActive_ = false;
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
            sound(3, 15); // seg2:110D-1113: Sfx08 on channel 3 before seg3:1057
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
                openStatement(current_, true); // seg2:12A3: STAT before the victory (seg7:71C4)
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
            const int reached = destination_; // DS:A10D until seg7:372F stores the next one
            const int next = chooseDestination(board_, players_, origin_, destination_, routeMask(), rng_);
            announceMode_ = remaining_ != 0 ? 1 : 3;
            announceVariant_ = remaining_ != 0 ? rng_.rand() % 5 : 3; // seg7:3B4E / 3B5F
            ++announceSerial_;
            if (remaining_ != 0) {
                origin_ = field;                                   // seg2:1492
                destination_ = next;                               // seg7:3D7D / 3F46 / 45E7
            } else {
                // Mode 3 keeps its draw in [bp-0x38] only; DS:A10D becomes Brussels
                // (seg7:47AB / 4B89), then DS:A10B = the field (seg2:145F).
                destination_ = lastStageDestination(board_, towns_, destination_);
                origin_ = field;
            }
            messages_.push_back(std::string(r.lastStageNext ? "Letzte Etappe" : "Neue Etappe")
                                + ", Ziel " + townName(destination_));
            if (remaining_ != 0) placeDragoForStage(reached); // seg2:1495-1535, DS:22 not yet rebuilt
            else startMasterRallye();                  // seg2:1462 / 1467 / 146D
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
    const int text = 4201 + rng_.random(13); // seg2:1127
    sound(2, 19); // seg2:1175-117B: Hebel05 on channel 2, then the message seg2:1191
    notice(text, {playerArg(current_)});
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
    // DS:2EB0 (Verdoppeln) is not cleared at the player change: only Return (seg2:0CE8)
    // and the card handler (seg3:0E9F) write it.
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
    timeLimitActive_ = false; // seg2:0716 -> seg2:3A8E: DS:12 = 0, the timer stops
    const auto change = nextPlayer(players_, current_, calendar_, rng_, turnHooks());
    if (change.suspended) { // the year balance's show decides its remaining draws, or STAT
        suspendTurn(change);
        return;
    }
    completeTurn(change);
}

TurnHooks Game::turnHooks() {
    TurnHooks hooks;
    hooks.chargeCredits = [this] { economy_.chargeCredits(activePlayers()); };
    hooks.settleMonth = [this] { economy_.settleMonth(activePlayers(), calendar_.year); }; // DS:A102
    hooks.clearYearRent = [this] { economy_.clearYearRent(); };
    hooks.clearCredits = [this] { economy_.clearCredits(); };
    hooks.monthStatement = [this](int player) { return openStatement(player, false); }; // seg2:07E1
    hooks.clearMonth = [this] { economy_.clearMonth(); };                                // seg2:07E6
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
            e.variant = -1; // drawn when the show master announces the player (seg8:8E38)
            e.positive = !(middle - economy_.money(p).cash > economy_.money(p).propertyValue);
            e.letter = letter;
            letter = (letter + 1) % 7; // seg8:913D
            balance_.push_back(e);
        }
        balanceAnnounced_ = 0;
        balanceSpoken_ = 0;
        balanceClosed_ = false;
        ++balanceSerial_;
        // With a window the show's input decides the remaining draws (seg8:8E38 per
        // announced player, seg8:9605 at the closing lever; round 144): the change waits.
        if (balanceWindow_) return true;
        while (balanceAnnounce()) balanceSpeak(); // headless: the show watched to the end
        balanceClose();
        return false;
    };
    hooks.announce = [this](int text, int player) { announceSitOut(text, player); };
    hooks.afterSitOut = [this](int player) { dragoAct(player); };
    hooks.dragoHolder = drago_.holder;
    hooks.afterPlayerChange = [this](int player) { eventTick(player); };
    return hooks;
}

bool Game::balanceAnnounce() { // seg8:8E38: the show master goes on to the next player
    if (balanceAnnounced_ >= balance_.size()) return false;
    balance_[balanceAnnounced_++].variant = rng_.rand() % 4;
    return true;
}

bool Game::balanceSpeak() { // seg8:9075..9153: pos_ / neg_, then the letter advances (913D)
    if (balanceSpoken_ >= balanceAnnounced_) return false;
    ++balanceSpoken_;
    return true;
}

bool Game::balanceClose() { // seg8:95E5..9613 at the closing lever
    if (balanceClosed_) return false;
    balanceClosed_ = true;
    // The letter [bp-0x7E] advances per spoken pos_ / neg_ (seg8:913D), not per Next_
    // (seg8:8E38): Escape between a player's Next_ and his result leaves it behind.
    const int letter = (balanceChoice_ + int(balanceSpoken_)) % 7;
    balanceClosing_ = letter == 4 ? 5 : letter == 5 ? 4 : rng_.rand() % 2 + 4;
    return true;
}

bool Game::balanceFinish() { // seg8:88C6 returns: seg1:8BC6, 8B41 and the rest of nextPlayer
    if (phase_ != Phase::Balance) return false;
    const TurnChange before = balanceTurn_;
    TurnChange rest = nextPlayer(players_, current_, calendar_, rng_, turnHooks(), before.resumePlayer);
    rest.satOut.insert(rest.satOut.begin(), before.satOut.begin(), before.satOut.end());
    rest.texts.insert(rest.texts.begin(), before.texts.begin(), before.texts.end());
    rest.monthsAdvanced += before.monthsAdvanced;
    rest.yearsEnded += before.yearsEnded;
    if (rest.suspended) {
        suspendTurn(rest);
        return true;
    }
    completeTurn(rest);
    return true;
}

void Game::completeTurn(const TurnChange& change) {
    announceChange(change);
    if (change.yearsEnded) messages_.push_back("Neues Jahr, Inflation " + std::to_string(calendar_.inflation));
    if (change.player < 0) { phase_ = Phase::GameOver; return; }
    if (demoEnd_ && !humansLeft()) { // seg2:0869: no human and DS:16 == 1
        phase_ = Phase::GameOver;
        messages_.push_back("Spiel beendet");
        return;
    }
    current_ = change.player;
    phase_ = Phase::Roll;
    // The turn start seg2:0407 of the new player (also after STAT or BILANZ held the
    // change): the window centres on him there (seg2:044E -> seg2:3216 -> seg6:3064).
    ++turnSerial_;
    messages_.push_back(playerName(current_) + " ist dran (Monat "
                        + std::to_string(calendar_.month % 12 + 1) + ")");
    beginTurn(); // seg2:07F4 / 0801 -> seg2:0407
}

void Game::beginTurn() {
    // seg2:041B: seg5:6CE8 clears the computer's state machine (seg5:6BF2 starts it again
    // from the card choice, computerAdvance with aiTurn_ false).
    aiTurn_ = false;
    aiSteerless_ = false;
    aiRoute_.clear();
    aiRoutePlanned_ = false;
    compactHand(hands_[size_t(current_)]); // seg2:0473 -> seg2:3CF1
    if (dragoTurnStart(drago_, current_, calendar_.year, rng_)) { // seg2:049F -> seg2:B3A4
        music_.push_back("f" + std::to_string(drago_.form + 1)); // seg2:B3F6/B429/B442
        // seg2:9804 -> 454E: the search for his start field draws random(3) (seg2:459C);
        // the form always differs, so it is always drawn.
        dragoStartSide_ = rng_.random(3);
        ++dragoFormChanges_;
        messages_.push_back("Dr. Drago wechselt in Form " + std::to_string(drago_.form + 1));
    }
}

} // namespace dtvr
