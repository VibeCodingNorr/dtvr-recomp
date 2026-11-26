// SPDX-License-Identifier: GPL-3.0-only
// The horn check of rally.exe seg2:2676 while driving: when the moving vehicle
// reaches a field where another driver in play stands, random(1) is drawn
// (seg6:1343) and on 0 the mover's horn (sound 10 + player / 2) plays on channel 2.
// Called after Plopp01 when the vehicle drives on (seg2:1F81) or stops before its
// last field (seg2:209F); the stop on the last field (Carb01, seg2:2082) has none.
#include "game.hpp"
#include "rules.hpp"

#include <cstdio>
#include <memory>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// Ring of 13 blue fields linked left / right; the towns are only needed for the
// stage destination.
static Board ring(std::vector<Town>& towns) {
    Board b;
    b.fields.resize(13);
    for (int i = 0; i < 13; ++i) {
        Field& f = b.fields[size_t(i)];
        f.x = uint8_t(i * 10); f.kind = 1;
        f.next = {Field::kNone, int16_t((i + 1) % 13), Field::kNone, int16_t((i + 12) % 13)};
    }
    b.fields[0].kind = 6;
    b.fields[12].kind = 6;
    towns.resize(2);
    towns[0].x = 0; towns[0].name = "London";
    towns[1].x = 120; towns[1].name = "Rom";
    return b;
}

static bool hasCue(const std::vector<SoundCue>& cues, int channel, int sound) {
    for (const SoundCue& c : cues)
        if (c.channel == channel && c.sound == sound) return true;
    return false;
}

// The draws `game` makes are exactly those of `expected` (same generator state).
static bool sameDraws(Game& game, const MsRandom& expected) { return game.sessionRandom().state() == expected.state(); }

int main() {
    std::vector<Town> towns;
    const Board board = ring(towns);

    // Back onto the start field where the other driver stands (a reversal, steps
    // left): one draw, the horn exactly on 0. Leaving towards an empty field: none.
    {
        int horns = 0, silent = 0;
        for (uint16_t seed = 1; seed < 400; ++seed) {
            Game g(board, towns, 2, 0, seed, 1, nullptr, nullptr, false);
            g.setRandomEvents(false);
            g.setArrivalConfirm(true);
            if (g.currentPlayer() != 0 || g.playerField(0) != g.playerField(1) || !g.roll() || g.allowance() < 2)
                continue;
            g.takeSounds();
            MsRandom expected = g.sessionRandom();
            CHECK(g.step(Direction::Right) && sameDraws(g, expected)); // field 1: nobody there
            const auto out = g.takeSounds();
            CHECK(out.size() == 1 && hasCue(out, 1, 0));
            const bool horn = expected.random(1) == 0;
            CHECK(g.step(Direction::Left) && g.playerField(0) == g.playerField(1));
            CHECK(sameDraws(g, expected));
            const auto back = g.takeSounds();
            CHECK(hasCue(back, 1, 0) && hasCue(back, 2, 10) == horn && back.size() == (horn ? 2u : 1u));
            if (!back.empty()) CHECK(back.front().channel == 1); // Plopp01 first (seg2:208D, then 209F)
            horn ? ++horns : ++silent;
        }
        CHECK(horns > 0 && silent > 0);
    }

    // Driving on over an occupied field draws; reaching it as the last field (Carb01)
    // does not. Player 0 drives first and waits on its last field; player 1 follows.
    {
        int passes = 0, lastOnes = 0;
        for (uint16_t seed = 1; seed < 2000 && (passes < 3 || lastOnes < 3); ++seed) {
            Game g(board, towns, 2, 0, seed, 1, nullptr, nullptr, false);
            g.setRandomEvents(false);
            g.setArrivalConfirm(true);
            if (g.currentPlayer() != 0 || !g.roll()) continue;
            const int a = g.allowance();
            while (g.stepsTaken() < a) g.step(Direction::Right);
            if (!g.pressReturn()) continue;
            if (g.phase() == Game::Phase::Arrived) g.endTurn();
            if (g.currentPlayer() != 1 || g.phase() != Game::Phase::Roll || !g.roll()) continue;
            const int b = g.allowance();
            if (b < a) continue;
            g.takeSounds();
            for (int i = 1; i <= b; ++i) {
                MsRandom expected = g.sessionRandom();
                const bool onOther = i == a, last = i == b;
                const bool horn = onOther && !last && expected.random(1) == 0;
                CHECK(g.step(Direction::Right) && g.playerField(1) == i);
                CHECK(sameDraws(g, expected)); // arrival waits for Return: no other draw
                const auto cues = g.takeSounds();
                CHECK(hasCue(cues, 1, last ? 9 : 0) && hasCue(cues, 2, 10) == horn); // player 1: 10 + 1 / 2
            }
            b > a ? ++passes : ++lastOnes;
        }
        CHECK(passes >= 3 && lastOnes >= 3);
    }

    // Computers (the headless path of computerAdvance) drive through the same step:
    // every field reached before the last one with another driver on it draws once.
    {
        int draws = 0, secondHorn = 0;
        const std::array<uint8_t, kPlayerSlots> slots{2, 2, 2, 0, 0, 0, 0, 0};
        for (uint16_t seed = 1; seed < 40; ++seed) {
            Game g(board, towns, slots, 0, seed);
            g.setRandomEvents(false);
            for (int guard = 0; guard < 400 && g.phase() != Game::Phase::GameOver; ++guard) {
                const bool moving = g.phase() == Game::Phase::Move;
                const int mover = g.currentPlayer(), taken = g.stepsTaken();
                MsRandom expected = g.sessionRandom();
                g.takeSounds();
                if (!g.computerAdvance()) break;
                if (!moving || g.phase() != Game::Phase::Move || g.currentPlayer() != mover) continue;
                if (g.stepsTaken() == taken) continue;
                const int at = g.playerField(mover);
                bool horn = false;
                for (int q = 0; q < kPlayerSlots; ++q)
                    if (q != mover && g.players()[size_t(q)].status && g.playerField(q) == at) {
                        horn = expected.random(1) == 0;
                        ++draws;
                        break;
                    }
                CHECK(sameDraws(g, expected));
                const auto cues = g.takeSounds();
                CHECK(hasCue(cues, 2, 10 + mover / 2) == horn); // player 2 honks Hupe02 (sound 11)
                if (horn && mover == 2) ++secondHorn;
            }
        }
        CHECK(draws > 0 && secondHorn > 0);
    }

    // Stopped by an obstacle on the road (seg2:2246) the vehicle can only turn back
    // (seg2:2189 -> 22E1) and reaches its field again: one horn check there. The
    // question for a clearing card draws only when it is declined.
    {
        int checked = 0;
        for (uint16_t seed = 1; seed < 6000 && checked < 4; ++seed) {
            auto g = std::make_unique<Game>(board, towns, 2, 0, seed, 8, nullptr, nullptr, false);
            g->setRandomEvents(false);
            int nails = -1;
            for (int s = 0; s < 8; ++s)
                if (g->deck().typeOfSlot(g->hand(0)[size_t(s)]) == 18) nails = s;
            if (nails < 0 || g->currentPlayer() != 0 || g->playerField(0) != g->playerField(1)) continue;
            if (!g->playCard(nails, 2) || g->obstacles().count() != 1 || !g->roll()) continue;
            const int field = g->playerField(0), taken = g->stepsTaken();
            g->takeSounds();
            MsRandom expected = g->sessionRandom();
            const bool refused = !g->step(Direction::Right);
            CHECK(refused && g->playerField(0) == field && g->stepsTaken() == taken);
            if (g->obstacleQuestion().card >= 0) {
                CHECK(sameDraws(*g, expected)); // asked first: no draw yet
                CHECK(!g->answerObstacle(false));
            }
            const bool horn = expected.random(1) == 0;
            CHECK(sameDraws(*g, expected));
            CHECK(hasCue(g->takeSounds(), 2, 10) == horn);
            ++checked;
        }
        CHECK(checked == 4);
    }

    return failures ? 1 : 0;
}
