// SPDX-License-Identifier: GPL-3.0-only
// Round R3b: the distance array DS:22 (seg2:43A8) and Dr. Drago at a normal stage
// change (seg2:1495-1535). The placement reads DS:22 before seg2:154A rebuilds it, so
// it compares the distances from the destination just reached, unsigned: a field the
// search did not reach (0xFFFF) wins. Also the order of a sit-out's message and Dr.
// Drago's action (seg2:096F before seg2:0977).
#include "game.hpp"
#include "rules.hpp"

#include <cstdio>
#include <string>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// London (field 0, kind 6) -- right -- field 1 (kind 6); field 0 also has a one-way
// road down to field 2 (kind 6), which has no road back.
static Board fork(std::vector<Town>& towns) {
    Board b;
    b.fields.resize(3);
    for (int i = 0; i < 3; ++i) {
        b.fields[size_t(i)].kind = 6;
        b.fields[size_t(i)].x = uint8_t(i * 4);
    }
    b.fields[0].next = {Field::kNone, 1, 2, Field::kNone};
    b.fields[1].next[3] = 0;
    towns.resize(3);
    towns[0].name = "London";
    for (int i = 1; i < 3; ++i) { towns[size_t(i)].x = b.fields[size_t(i)].x; towns[size_t(i)].name = "T" + std::to_string(i); }
    return b;
}

int main() {
    // seg2:43F9-4428: every word 0xFFFF, the start 0; seg2:44AF: a frontier field with
    // flagset & DS:A10F is not expanded; seg2:4449: at most 0x50 levels.
    {
        Board chain;
        chain.fields.resize(84);
        for (int i = 0; i < 84; ++i) {
            if (i + 1 < 84) chain.fields[size_t(i)].next[1] = int16_t(i + 1);
            if (i > 0) chain.fields[size_t(i)].next[3] = int16_t(i - 1);
        }
        chain.fields[3].flags = 0x4000;
        auto t = distanceTable(chain, 0, 0);
        CHECK(t.size() == 84 && t[0] == 0 && t[80] == 80 && t[81] == 0xFFFF && t[83] == 0xFFFF);
        t = distanceTable(chain, 0, 0x4000);
        CHECK(t[3] == 3 && t[4] == 0xFFFF); // field 3 gets its distance but is not expanded
        t = distanceTable(chain, 5, 0x4000);
        CHECK(t[4] == 1 && t[3] == 2 && t[2] == 0xFFFF);
        CHECK(distanceTable(chain, -1, 0) == std::vector<uint16_t>(84, 0xFFFF));
    }

    // seg2:14B1-1535: unsigned, strictly larger, from 0, default the first in play.
    {
        std::vector<PlayerSlot> players(kPlayerSlots);
        const std::vector<uint16_t> table{0, 5, 5, 0xFFFF, 7};
        players[1].status = 1; players[1].field = 0;
        CHECK(stageDragoPlayer(players, table) == 1);           // one player: seg2:1A41
        players[2].status = 2; players[2].field = 1;
        players[5].status = 1; players[5].field = 2;
        CHECK(stageDragoPlayer(players, table) == 2);           // tie 5 / 5: the earlier slot
        players[6].status = 1; players[6].field = 4;
        CHECK(stageDragoPlayer(players, table) == 6);
        players[7].status = 1; players[7].field = 3;
        CHECK(stageDragoPlayer(players, table) == 7);           // 0xFFFF wins (jbe at seg2:1506)
        std::vector<PlayerSlot> atGoal(kPlayerSlots);
        atGoal[3].status = atGoal[4].status = 1;
        atGoal[3].field = atGoal[4].field = 0;
        CHECK(stageDragoPlayer(atGoal, table) == 3);            // all 0: the first in play
    }

    // The game: player 0 reaches field 1, the next destination is field 2 (the only one
    // left). Player 1 still stands in London: 1 field from the destination reached, but
    // unreachable from field 2 (the old rebuild then fell back to player 0).
    {
        std::vector<Town> towns;
        const Board board = fork(towns);
        int checked = 0;
        for (uint16_t seed = 1; seed < 4000 && checked < 4; ++seed) {
            Game g(board, towns, 2, 2, seed, 1, nullptr, nullptr, false);
            g.setRandomEvents(false);
            if (g.destination() != 1 || g.currentPlayer() != 0 || !g.roll() || g.allowance() != 1) continue;
            CHECK(g.distanceTable() == distanceTable(board, 1, 0x4000));
            CHECK(g.step(Direction::Right));
            // seg2:141A: the town dialog of the reached town comes first; the stage is unchanged.
            CHECK(g.fieldFollowUpPending() && g.remainingStages() == 2 && g.destination() == 1);
            CHECK(!g.endTurn()); // the announcement seg7:372F(1)
            CHECK(g.remainingStages() == 1 && g.destination() == 2 && g.origin() == 1);
            CHECK(g.dragoPlacements() == 0);
            CHECK(!g.endTurn()); // seg2:9C97
            CHECK(g.dragoPlacements() == 1 && g.drago().holder == 1 && !g.fieldFollowUpPending());
            CHECK(distanceTable(board, 2, 0x4000)[0] == 0xFFFF);
            CHECK(g.distanceTable()[0] == 0xFFFF && g.distanceTable()[2] == 0); // rebuilt (seg2:154A)
            ++checked;
        }
        CHECK(checked == 4);
    }

    // seg2:096F / 0A0D show the sit-out message before seg2:0977 / 0A15 lets Dr. Drago act;
    // without the announce hook the texts are collected as before.
    {
        for (uint16_t flag : {uint16_t(0x2), uint16_t(0x4)}) {
            std::vector<PlayerSlot> turn(kPlayerSlots);
            turn[0].status = turn[1].status = turn[2].status = 1;
            turn[1].flags = flag;
            Calendar cal;
            MsRandom rng(7), copy(7);
            std::vector<std::string> order;
            TurnHooks hooks;
            hooks.announce = [&](const SitOutText& t) { order.push_back("text " + std::to_string(t.text) + " " + std::to_string(t.player)); };
            hooks.afterSitOut = [&](int p) { order.push_back("drago " + std::to_string(p)); };
            hooks.afterPlayerChange = [&](int p) { order.push_back("event " + std::to_string(p)); };
            const auto tc = nextPlayer(turn, 0, cal, rng, hooks);
            const int text = (flag == 0x2 ? 1000 : 1010) + copy.random(2);
            CHECK(tc.player == 2 && tc.satOut == std::vector<int>{1} && tc.texts.empty());
            CHECK((order == std::vector<std::string>{"event 1", "text " + std::to_string(text) + " 1", "drago 1", "event 2"}));
            CHECK(rng.state() == copy.state());
            turn[1].flags = flag;
            hooks.announce = nullptr;
            const auto collected = nextPlayer(turn, 0, cal, rng, hooks);
            CHECK(collected.texts.size() == 1 && collected.texts[0].player == 1 && collected.texts[0].sound == -1);
        }
    }

    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
