// SPDX-License-Identifier: GPL-3.0-only
// Game-level tests for the cards added in round 13 and the investment offer.
#include "game.hpp"

#include <cstdio>
#include <memory>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// Ring of 13 fields: London (kind 6) at 0, Rom (kind 6) at 12, blue fields between.
static Board ring(std::vector<Town>& towns) {
    Board b;
    b.fields.resize(13);
    for (int i = 0; i < 13; ++i) {
        Field& f = b.fields[size_t(i)];
        f.x = uint8_t(i); f.kind = 1;
        f.next = {Field::kNone, int16_t((i + 1) % 13), Field::kNone, int16_t((i + 12) % 13)};
    }
    b.fields[0].kind = 6;
    b.fields[12].kind = 6;
    towns.resize(2);
    towns[0].x = 0; towns[0].name = "London";
    towns[1].x = 12; towns[1].name = "Rom";
    return b;
}

// First seed whose starting hand of `player` (8 cards) contains a card of `type`.
static std::unique_ptr<Game> gameWithCard(const Board& b, const std::vector<Town>& t, int players, int player,
                                          int type, int& slot) {
    for (uint16_t seed = 1; seed < 5000; ++seed) {
        auto g = std::make_unique<Game>(b, t, players, 0, seed, 8);
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
        CHECK(g->fieldOfTown(1) == 12 && g->fieldOfTown(5) == -1);
        CHECK(!g->playCard(slot, 9));
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

    // Investments need an offer.
    Game plain(board, towns, 1, 0, 1);
    CHECK(plain.investmentOffer() == EventKind::None && !plain.invest(1000));

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("game_cards_test: OK");
    return failures != 0;
}
