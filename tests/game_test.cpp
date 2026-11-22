// SPDX-License-Identifier: GPL-3.0-only
// Tests for the hotseat game flow assembled from the recovered rules.
#include "game.hpp"

#include <cstdio>
#include <stdexcept>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// A ring of 13 fields: town "London" (kind 6) at 0, town "Rom" (kind 6) at 12,
// blue fields elsewhere. Fields are linked left/right, 12 wraps to 0.
static Board line(std::vector<Town>& towns) {
    Board b;
    b.fields.resize(13);
    for (int i = 0; i < 13; ++i) {
        Field& f = b.fields[size_t(i)];
        f.x = uint8_t(i); f.y = 0; f.kind = 1;
        f.next = {Field::kNone, int16_t((i + 1) % 13), Field::kNone, int16_t((i + 12) % 13)};
    }
    b.fields[0].kind = 6;
    b.fields[12].kind = 6;
    towns.resize(2);
    towns[0].x = 0; towns[0].name = "London";
    towns[1].x = 12; towns[1].name = "Rom";
    return b;
}

int main() {
    std::vector<Town> towns;
    const Board board = line(towns);
    bool threw = false;
    try { Game g(board, towns, 0, 1, 1); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);

    Game game(board, towns, 2, 0, 1);
    CHECK(game.origin() == 0 && game.destination() == 12 && game.currentPlayer() == 0);
    CHECK(game.phase() == Game::Phase::Roll);
    CHECK(!game.step(Direction::Right) && !game.endTurn());

    CHECK(game.roll() && game.phase() == Game::Phase::Move && !game.roll());
    const int rolled = game.allowance();
    CHECK(rolled >= 1 && rolled <= 6);
    CHECK(!game.step(Direction::Up) && !game.step(Direction::Down)); // no link
    // Step forward, back (undo), forward again until the allowance is used.
    CHECK(game.step(Direction::Right) && game.stepsTaken() == 1);
    if (rolled > 1) {
        CHECK(game.step(Direction::Left) && game.stepsTaken() == 0 && game.playerField(0) == 0);
        for (int i = 0; i < rolled; ++i) CHECK(game.step(Direction::Right));
    }
    CHECK(game.phase() == Game::Phase::Arrived && game.playerField(0) == rolled);
    CHECK(game.cash(0) > kStartingCash);                         // blue field
    CHECK(!game.step(Direction::Right));

    CHECK(game.endTurn() && game.currentPlayer() == 1 && game.phase() == Game::Phase::Roll);
    CHECK(game.calendar().month == 0);
    CHECK(game.roll());
    for (int i = 0; i < game.allowance(); ++i) game.step(Direction::Right);
    CHECK(game.endTurn() && game.currentPlayer() == 0 && game.calendar().month == 1);

    // Keep driving right around the ring; landing exactly on the destination with
    // 0 stages remaining ends the game (300,000 reward).
    for (int guard = 0; guard < 5000 && game.phase() != Game::Phase::GameOver; ++guard) {
        if (game.phase() == Game::Phase::Arrived) game.endTurn();
        else if (game.phase() == Game::Phase::Roll) game.roll();
        else CHECK(game.step(Direction::Right));
    }
    CHECK(game.phase() == Game::Phase::GameOver);
    CHECK(game.cash(game.currentPlayer()) >= kStartingCash + 300000.0);

    // With one stage left the first arrival continues with a new destination.
    Game stages(board, towns, 1, 1, 3);
    for (int guard = 0; guard < 5000 && stages.remainingStages() == 1; ++guard) {
        if (stages.phase() == Game::Phase::Arrived) stages.endTurn();
        else if (stages.phase() == Game::Phase::Roll) stages.roll();
        else stages.step(Direction::Right);
    }
    CHECK(stages.remainingStages() == 0 && stages.phase() != Game::Phase::GameOver);
    CHECK(stages.origin() == 12 && stages.destination() == 0);

    // Cards: a movement card from the starting hand replaces the spin.
    Game cardGame(board, towns, 1, 0, 1, 8);
    int moveSlot = -1, otherSlot = -1;
    for (int i = 0; i < 8; ++i) {
        const int type = cardGame.deck().typeOfSlot(cardGame.hand(0)[size_t(i)]);
        if (type < 0) continue;
        const CardMove m = cardMove(type);
        if ((m.reels || m.fields) && moveSlot < 0) moveSlot = i;
        if (!m.reels && !m.fields && otherSlot < 0) otherSlot = i;
    }
    CHECK(cardGame.deck().remaining() == 199 - 8);
    if (otherSlot >= 0) CHECK(!cardGame.playCard(otherSlot) && cardGame.phase() == Game::Phase::Roll);
    if (moveSlot >= 0) {
        const int type = cardGame.deck().typeOfSlot(cardGame.hand(0)[size_t(moveSlot)]);
        CHECK(cardGame.playCard(moveSlot) && cardGame.phase() == Game::Phase::Move);
        CHECK(cardGame.hand(0)[size_t(moveSlot)] == Deck::kTaken);
        const CardMove m = cardMove(type);
        if (m.fields) CHECK(cardGame.allowance() == m.fields);
        else CHECK(cardGame.allowance() >= m.reels && cardGame.allowance() <= m.reels * (m.maximum + 1));
        CHECK(!cardGame.roll() && !cardGame.playCard(moveSlot));
    }
    CHECK(!cardGame.playCard(-1) && !cardGame.playCard(8));

    // Yellow fields hand out the top card of the deck.
    Board yellow = board;
    for (int i = 1; i < 12; ++i) yellow.fields[size_t(i)].kind = 3;
    Game yellowGame(yellow, towns, 1, 0, 2, 1);
    auto held = [&] { int n = 0; for (auto s : yellowGame.hand(0)) n += s != Deck::kTaken; return n; };
    CHECK(held() == 1);
    const int top = yellowGame.deck().peek();
    yellowGame.roll();
    while (yellowGame.phase() == Game::Phase::Move) yellowGame.step(Direction::Right);
    CHECK(held() == 2 && yellowGame.deck().remaining() == 199 - 2);
    bool hasTop = false;
    for (auto s : yellowGame.hand(0)) hasTop |= s == top;
    CHECK(hasTop);

    // Town purchase: only after arriving in a town, entry must exist and be free.
    std::vector<Town> shopTowns = towns;
    shopTowns[0].entries[0].name = "Kiosk"; shopTowns[0].entries[0].value = 1000; shopTowns[0].entries[0].words[0] = 5;
    Game shop(board, shopTowns, 1, 99, 1);
    CHECK(shop.townAt(0) == 0 && shop.townAt(5) == -1 && !shop.buyProperty(0));
    for (int guard = 0; guard < 5000; ++guard) {
        if (shop.phase() == Game::Phase::Arrived && shop.playerField(0) == 0) break;
        if (shop.phase() == Game::Phase::Arrived) shop.endTurn();
        else if (shop.phase() == Game::Phase::Roll) shop.roll();
        else shop.step(Direction::Right);
    }
    CHECK(shop.phase() == Game::Phase::Arrived && shop.playerField(0) == 0);
    {
        const double before = shop.cash(0);
        CHECK(!shop.buyProperty(1) && shop.buyProperty(0) && shop.cash(0) == before - 1000);
        CHECK(!shop.buyProperty(0));
        CHECK(shop.economy().town(0).entries[0].owner == 0);
    }

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("game_test: OK");
    return failures != 0;
}
