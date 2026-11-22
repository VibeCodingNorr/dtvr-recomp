// SPDX-License-Identifier: GPL-3.0-only
// Game-level tests for the cards added in round 13 and the investment offer.
#include "estates.hpp"
#include "game.hpp"

#include <cstdio>
#include <memory>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// Ring of 13 fields ten cells apart: London (kind 6) at 0, Rom (kind 6) at 12,
// small towns (kind 4) with one property worth 10,000 between.
static Board ring(std::vector<Town>& towns) {
    Board b;
    b.fields.resize(13);
    for (int i = 0; i < 13; ++i) {
        Field& f = b.fields[size_t(i)];
        f.x = uint8_t(i * 10); f.kind = 4;
        f.next = {Field::kNone, int16_t((i + 1) % 13), Field::kNone, int16_t((i + 12) % 13)};
    }
    b.fields[0].kind = 6;
    b.fields[12].kind = 6;
    towns.resize(13);
    towns[0].x = 0; towns[0].name = "London";
    towns[1].x = 120; towns[1].name = "Rom";
    for (int i = 1; i < 12; ++i) {
        towns[size_t(i + 1)].x = uint16_t(i * 10);
        towns[size_t(i + 1)].name = "Stadt";
        towns[size_t(i + 1)].entries[0].name = "Haus";
        towns[size_t(i + 1)].entries[0].value = 10000;
    }
    return b;
}

// Spin and drive right until the move ends.
static void driveRight(Game& g) {
    g.roll();
    while (g.phase() == Game::Phase::Move) g.step(Direction::Right);
}

// First seed whose starting hand of `player` (8 cards) contains a card of `type`.
static std::unique_ptr<Game> gameWithCard(const Board& b, const std::vector<Town>& t, int players, int player,
                                          int type, int& slot) {
    for (uint16_t seed = 1; seed < 5000; ++seed) {
        auto g = std::make_unique<Game>(b, t, players, 0, seed, 8, nullptr, nullptr, false);
        g->setRandomEvents(false);
        for (int s = 0; s < 8; ++s)
            if (g->deck().typeOfSlot(g->hand(player)[size_t(s)]) == type) { slot = s; return g; }
    }
    return nullptr;
}

int main() {
    std::vector<Town> towns;
    const Board board = ring(towns);
    int slot = -1;

    // Nägel (18): obstacle two cells away in the chosen direction; the turn goes on.
    if (auto g = gameWithCard(board, towns, 1, 0, 18, slot)) {
        CHECK(!g->playCard(slot, 1));                 // no road upwards
        CHECK(g->playCard(slot, 2));
        CHECK(g->obstacles().count() == 1 && g->obstacles().slots()[0].type == kNails);
        CHECK(g->phase() == Game::Phase::Roll);
    } else CHECK(false);

    // Obstacle question (round 65, seg2:2404): a human with the clearing card is asked
    // (text 600, Yes / No, card picture); No keeps card and obstacle, Yes drives on.
    {
        std::unique_ptr<Game> g;
        int nails = -1;
        for (uint16_t seed = 1; seed < 20000 && !g; ++seed) {
            auto c = std::make_unique<Game>(board, towns, 1, 0, seed, 8, nullptr, nullptr, false);
            c->setRandomEvents(false);
            int n = -1, clear = -1;
            for (int s = 0; s < 8; ++s) {
                const int type = c->deck().typeOfSlot(c->hand(0)[size_t(s)]);
                if (type == 18 && n < 0) n = s;
                if (type == 38) clear = s;
            }
            if (n >= 0 && clear >= 0) { g = std::move(c); nails = n; }
        }
        CHECK(g != nullptr);
        if (g) {
            CHECK(g->playCard(nails, 2) && g->obstacles().count() == 1);
            g->takeNotices();
            int asked = 0;
            for (int guard = 0; guard < 200 && g->phase() != Game::Phase::Arrived && !asked; ++guard) {
                if (g->phase() == Game::Phase::Roll) g->roll();
                else if (!g->step(Direction::Right) && g->obstacleQuestion().card >= 0) asked = 1;
            }
            CHECK(asked && g->obstacleQuestion().card == 38 && g->obstacleQuestion().direction == Direction::Right);
            const auto notices = g->takeNotices();
            CHECK(!notices.empty() && notices.back().text == 600 && notices.back().buttons == 4
                  && notices.back().cardPicture == 38);
            const int field = g->playerField(0), cards = g->handCount(0);
            CHECK(!g->step(Direction::Right));                       // waits for the answer
            CHECK(!g->answerObstacle(false) && g->obstacleQuestion().card < 0);
            CHECK(g->obstacles().count() == 1 && g->handCount(0) == cards && g->playerField(0) == field);
            CHECK(!g->step(Direction::Right) && g->obstacleQuestion().card == 38); // asked again
            CHECK(g->answerObstacle(true) && g->obstacles().count() == 0 && g->handCount(0) == cards - 1);
            CHECK(g->playerField(0) != field);
        }
    }

    // Punch (20): the other player on the same field is pushed four fields against
    // the puncher's driving direction (initially right -> pushed left).
    if (auto g = gameWithCard(board, towns, 2, 0, 20, slot)) {
        CHECK(g->playCard(slot) && g->playerField(1) == 9);
        CHECK(g->currentPlayer() == 1);               // the turn ended
    } else CHECK(false);

    // Konferenz (21): everybody comes to the player's field.
    if (auto g = gameWithCard(board, towns, 2, 1, 21, slot)) {
        CHECK(g->roll());
        while (g->phase() == Game::Phase::Move) g->step(Direction::Right);
        CHECK(g->endTurn() && g->currentPlayer() == 1 && g->playerField(0) != 0);
        CHECK(g->playCard(slot) && g->playerField(0) == g->playerField(1) && g->playerField(1) == 0);
    } else CHECK(false);

    // Flugzeug (27): fly to Rom, the destination of the last stage -> game over.
    if (auto g = gameWithCard(board, towns, 1, 0, 27, slot)) {
        CHECK(g->fieldOfTown(1) == 12 && g->fieldOfTown(5) == 4 && g->fieldOfTown(20) == -1);
        CHECK(!g->playCard(slot, 99));
        CHECK(g->playCard(slot, 1) && g->playerField(0) == 12 && g->phase() == Game::Phase::GameOver);
    } else CHECK(false);

    // Verdoppeln (40): the next card stays in the hand.
    if (auto g = gameWithCard(board, towns, 1, 0, 40, slot)) {
        int move = -1;
        for (int s = 0; s < 8; ++s)
            if (s != slot && cardKind(g->deck().typeOfSlot(g->hand(0)[size_t(s)])) == CardKind::Movement) move = s;
        if (move >= 0) {
            CHECK(g->playCard(slot) && g->hand(0)[size_t(slot)] == Deck::kTaken);
            const uint8_t kept = g->hand(0)[size_t(move)];
            CHECK(g->playCard(move) && g->hand(0)[size_t(move)] == kept && g->phase() == Game::Phase::Move);
        }
    } else CHECK(false);

    // Erdbeben (17): the turn ends; no town properties are owned here.
    if (auto g = gameWithCard(board, towns, 2, 0, 17, slot)) {
        CHECK(g->playCard(slot) && g->currentPlayer() == 1);
    } else CHECK(false);

    // Kaufen (2): only from the town dialog; buys for 75 %, one mode at a time,
    // reset when the turn ends.
    if (auto g = gameWithCard(board, towns, 2, 0, 2, slot)) {
        CHECK(!g->playCard(slot));                    // category 1 not before spinning
        driveRight(*g);
        CHECK(g->phase() == Game::Phase::Arrived && g->townAt(g->playerField(0)) >= 2);
        const double before = g->cash(0);
        CHECK(g->playCard(slot) && g->propertyMode() == 3);
        CHECK(g->buyProperty(0) && g->cash(0) == before - 7500);
        CHECK(g->propertyMode() == 3);                // stays until the dialog closes
        CHECK(g->endTurn() && g->propertyMode() == 0xFF);
    } else CHECK(false);

    // Nimm (6): takes another player's property, then the mode ends at once.
    // Seed search: player 2 holds Nimm and both first spins reach the same town.
    bool nimm = false;
    for (uint16_t seed = 1; seed < 5000 && !nimm; ++seed) {
        Game g(board, towns, 2, 0, seed, 8);
        g.setRandomEvents(false);
        int card = -1;
        for (int s = 0; s < 8; ++s) if (g.deck().typeOfSlot(g.hand(1)[size_t(s)]) == 6) card = s;
        if (card < 0) continue;
        driveRight(g);
        if (g.phase() != Game::Phase::Arrived || !g.buyProperty(0)) continue;
        const int field = g.playerField(0);
        g.endTurn();
        if (g.currentPlayer() != 1) continue;
        driveRight(g);
        if (g.phase() != Game::Phase::Arrived || g.playerField(1) != field) continue;
        nimm = true;
        CHECK(g.playCard(card) && g.propertyMode() == 7);
        CHECK(g.buyProperty(0) && g.propertyMode() == 0xFF);
        CHECK(g.economy().town(g.townAt(field)).entries[0].owner == 1);
    }
    CHECK(nimm);

    // Tschüss (19): the target lands on a field outside the 40 x 30 view.
    if (auto g = gameWithCard(board, towns, 2, 0, 19, slot)) {
        CHECK(g->playCard(slot, 1) && g->currentPlayer() == 1);
        const int f = g->playerField(1);
        CHECK(f >= 3 && f <= 12);                     // x >= 30: the inflated cell misses x -20..20
    } else CHECK(false);

    // Card field (kind 5): every field between London and Rom is a card shop.
    {
        Board shop = board;
        for (int i = 1; i < 12; ++i) shop.fields[size_t(i)].kind = 5;
        Game g(shop, towns, 1, 0, 3, 3, nullptr, nullptr, false);
        g.setRandomEvents(false);
        CHECK(!g.atCardField() && !g.buyCard(9));
        driveRight(g);
        CHECK(g.atCardField() && g.cardOffers().size() == size_t(kCardTypes));
        const int before = g.handCount(0);
        const double cash = g.cash(0);
        CHECK(g.buyCard(29) && g.handCount(0) == before + 1 && g.cash(0) == cash - 10000); // 1 Feld
        int slot = -1;
        for (int s = 0; s < 8; ++s) if (g.deck().typeOfSlot(g.hand(0)[size_t(s)]) == 29) slot = s;
        const int remaining = g.deck().remaining();
        CHECK(slot >= 0 && g.sellCard(slot) && g.cash(0) == cash - 5000);
        CHECK(g.deck().remaining() == remaining + 1 && g.handCount(0) == before);
        CHECK(!g.buyCard(4) && g.cash(0) == cash - 5000); // Platin costs 500,000 > cash
    }

    // Yellow field with a full hand: discard first, then the card follows.
    {
        Board yb = board;
        for (int i = 1; i < 12; ++i) yb.fields[size_t(i)].kind = 3;
        Game g(yb, towns, 1, 0, 5, 8, nullptr, nullptr, false);
        g.setRandomEvents(false);
        CHECK(g.handCount(0) == 8);
        driveRight(g);
        CHECK(g.yellowCardPending() && !g.endTurn());
        const uint8_t dropped = g.hand(0)[2];
        CHECK(g.discardForCard(2) && !g.yellowCardPending() && g.handCount(0) == 8);
        CHECK(g.hand(0)[2] != dropped && g.endTurn());
    }

    // Risiko (26): press while lit doubles, in the dark loses everything, stop keeps.
    if (auto g = gameWithCard(board, towns, 2, 0, 26, slot)) {
        CHECK(g->playCard(slot) && g->phase() == Game::Phase::Risiko);
        const int steps = g->risiko().steps;
        CHECK(steps >= 1 && steps <= 6 && g->risiko().level == 2 && g->risiko().light == 2);
        CHECK(g->risikoTickMs() == 320);
        CHECK(g->risikoPress() && g->risiko().steps == 2 * steps && g->risiko().level == 3);
        CHECK(g->risikoTick() && g->risiko().light == 0);
        g->takeNotices();
        g->takeSounds();
        CHECK(g->risikoPress() && g->phase() == Game::Phase::Roll && g->currentPlayer() == 1); // lost
        bool dark = false, darkSound = false; // text 41002, sound 0x17 on channel 2 (seg7:6A4F)
        for (const Notice& n : g->takeNotices()) dark |= n.text == 41002;
        for (const SoundCue& c : g->takeSounds()) darkSound |= c.channel == 2 && c.sound == 23;
        CHECK(dark && darkSound);
    } else CHECK(false);
    if (auto g = gameWithCard(board, towns, 2, 0, 26, slot)) {
        CHECK(g->playCard(slot));
        const int steps = g->risiko().steps;
        CHECK(g->risikoPress() && g->risikoTick() && g->risikoTick());          // level 3, lit again
        CHECK(g->risiko().light == 3 && g->risikoPress() && g->risiko().steps == 3 * steps);
        CHECK(g->risikoStop() && g->phase() == Game::Phase::Move && g->allowance() == 3 * steps);
        const auto notices = g->takeNotices();
        CHECK(!notices.empty() && notices.back().text == 41001);
    } else CHECK(false);
    if (auto g = gameWithCard(board, towns, 2, 0, 26, slot)) {
        CHECK(g->playCard(slot));
        CHECK(g->setComputer(0, true) && !g->risikoPress());                   // only the computer presses
        int ticks = 0;
        while (g->phase() == Game::Phase::Risiko && ticks < 1000) { g->risikoTick(); ++ticks; }
        CHECK(g->phase() != Game::Phase::Risiko && ticks > 8);
    } else CHECK(false);

    // The slot machine presentation sees the reels of every spin (lastReels, spinSerial).
    {
        Game g(board, towns, 1, 0, 9, 1, nullptr, nullptr, false);
        const int serial = g.spinSerial();
        CHECK(g.roll() && g.spinSerial() == serial + 1 && g.lastReels().size() == 1);
        CHECK(g.lastReels()[0] == g.allowance() && g.allowance() >= 1 && g.allowance() <= 6);
    }

    // Town dialog rows (round 30): price, income = value * 0.001 * rate, yield = rate * 1.2;
    // in the Kaufen tab only affordable free entries are enabled.
    {
        std::vector<Town> t2 = towns;
        t2[2].entries[0].words[0] = 4;
        t2[2].entries[1].name = "Schloss";
        t2[2].entries[1].value = 900000; // more than the starting cash
        Game g(board, t2, 1, 0, 1, 1, nullptr, nullptr, false);
        auto rows = estateRows(g, 2, EstatesTab::None);
        CHECK(rows.size() == 2 && rows[0].name == "Haus" && rows[0].price == 10000 && rows[0].owner == -1);
        CHECK(rows[0].income == 10000 * 0.001 * 4 && rows[0].yieldPercent > 4.79 && rows[0].yieldPercent < 4.81);
        CHECK(!rows[0].disabled && !rows[1].disabled);
        rows = estateRows(g, 2, EstatesTab::Buy);
        CHECK(!rows[0].disabled && rows[1].disabled);
        rows = estateRows(g, 2, EstatesTab::Sell); // nothing owned
        CHECK(rows[0].disabled && rows[1].disabled);
        CHECK(estateRows(g, 99, EstatesTab::None).empty());
    }

    // Investments need an offer.
    Game plain(board, towns, 1, 0, 1);
    CHECK(plain.investmentOffer() == EventKind::None && !plain.invest(1000));

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("game_cards_test: OK");
    return failures != 0;
}
