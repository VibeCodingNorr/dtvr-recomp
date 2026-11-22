// SPDX-License-Identifier: GPL-3.0-only
// Tests for the computer players' driving decision and town plan (round 16).
#include "ai.hpp"
#include "game.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// A row of n fields linked left/right; kinds default to 1 (blue).
static Board row(int n) {
    Board b;
    b.fields.resize(size_t(n));
    for (int i = 0; i < n; ++i) {
        Field& f = b.fields[size_t(i)];
        f.x = uint8_t(i * 4);
        f.kind = 1;
        if (i + 1 < n) f.next[1] = int16_t(i + 1);
        if (i > 0) f.next[3] = int16_t(i - 1);
    }
    return b;
}

int main() {
    // Profiles per slot (DS:534A): slot 2 uses the strategy at DS:4E60.
    CHECK(aiProfile(2).strategy->destination == 1000 && aiProfile(2).strategy->randomRange == 1);
    CHECK(aiProfile(0).reserve[3] == 50000.0 && aiProfile(4).strategy->perFieldLast == 3);

    // Graph: masked fields and obstacle links disappear, BFS from the destination.
    Board b = row(6);
    b.fields[5].flags = 0x4000;
    ObstacleTable none;
    AiGraph g = buildAiGraph(b, 0x4000, none);
    CHECK(g[4][1] == Field::kNone && g[5][3] == Field::kNone && g[3][1] == 4);
    CHECK(aiDirection(b, 2, 3) == 1 && aiDirection(b, 2, 4) == 4);
    auto dist = aiDistances(g, 4);
    CHECK(dist[0] == 4 && dist[4] == 0 && dist[5] == 0xFFFF);
    ObstacleTable obstacles;
    Obstacle o;
    o.type = kSnow; o.duration = 3; o.x = 6; o.y = 0; o.field = 1; o.dir = 2;
    obstacles.add(o);
    g = buildAiGraph(b, 0, obstacles);
    CHECK(g[1][1] == Field::kNone && g[2][3] == Field::kNone && g[0][1] == 1);

    // Spare money: cash minus reserve x inflation; Dr. Drago's form raises the reserve.
    std::vector<Town> towns;
    Economy economy(towns, 8);
    std::vector<PlayerSlot> players(8);
    players[0].status = 2;
    AiState s;
    s.board = &b; s.towns = &towns; s.economy = &economy; s.players = &players;
    for (auto& h : s.handTypes) h.fill(-1);
    CHECK(aiSpareMoney(s, 0) == kStartingCash);
    s.dragoHolder = 0; s.dragoForm = 2;                 // reserve 50,000 > 30,000
    CHECK(aiSpareMoney(s, 0) == 0.0);
    s.dragoHolder = -1;

    // Route: four steps from field 0 on a row whose destination is field 4.
    Board line = row(8);
    line.fields[4].kind = 6;
    AiGraph lg = buildAiGraph(line, 0, none);
    auto ld = aiDistances(lg, 4);
    s.board = &line;
    s.destination = 4;
    s.remainingStages = 1;
    players[0].field = 0;
    MsRandom rng(1);
    auto route = aiChooseRoute(s, 0, 4, lg, ld, rng);
    CHECK(route.size() == 4 && route.back() == 4);
    // From field 6 with 2 steps only fields 4 and 8-... : the route ends on 4 (closer).
    players[0].field = 6;
    route = aiChooseRoute(s, 0, 2, lg, ld, rng);
    CHECK(route.size() == 2 && route.back() == 4);
    // Steuerlos: random scores, still a route of the full length.
    s.steerless = true;
    route = aiChooseRoute(s, 0, 3, lg, ld, rng);
    CHECK(route.size() == 3);
    s.steerless = false;

    // Town plan: free entries sorted by weight, bought while the spare money lasts.
    Board tb = row(2);
    tb.fields[1].kind = 4;
    std::vector<Town> tt(1);
    tt[0].x = 4; tt[0].name = "Stadt";
    for (int e = 0; e < 3; ++e) {
        tt[0].entries[size_t(e)].name = "E";
        tt[0].entries[size_t(e)].value = uint32_t(10000 * (e + 1));
        tt[0].entries[size_t(e)].words[0] = uint16_t(e);      // income factor: entry 2 scores highest
    }
    Economy te(tt, 8);
    AiState ts = s;
    ts.board = &tb; ts.towns = &tt; ts.economy = &te;
    AiTownPlan plan = aiTownPlan(ts, 0, 1);
    // Spare 30,000: entry 2 (30,000) is not below it, entry 1 (20,000) is, then 10,000 left
    // and entry 0 (10,000) is not below 10,000.
    CHECK(plan.purchases == 1 && plan.actions.size() == 1 && plan.actions[0].kind == 0 && plan.actions[0].value == 1);
    CHECK(aiTownPlan(ts, 0, 0).purchases == 0);           // not a town field
    ts.handTypes[0][3] = 4;                               // Platin: 10 % prices
    plan = aiTownPlan(ts, 0, 1);
    CHECK(plan.actions.size() == 4 && plan.actions[0].kind == 2 && plan.actions[0].value == 3 && plan.purchases == 3);

    // Game: a computer takes its turn alone; humans cannot spin for it.
    {
        Board gb = row(13);
        gb.fields[0].kind = 6;
        gb.fields[12].kind = 6;
        gb.fields[12].next[1] = 0;
        gb.fields[0].next[3] = 12;
        std::vector<Town> gt(2);
        gt[0].x = 0; gt[0].name = "London";
        gt[1].x = 48; gt[1].name = "Rom";
        Game game(gb, gt, 2, 0, 7);
        game.setRandomEvents(false);
        CHECK(game.setComputer(1, true) && game.isComputer(1) && !game.isComputer(0));
        CHECK(!game.runComputerTurn());                   // player 1 is human
        CHECK(game.roll());
        while (game.phase() == Game::Phase::Move) game.step(Direction::Right);
        CHECK(game.endTurn() && game.currentPlayer() == 1 && game.computerControls());
        CHECK(!game.roll());                              // the computer's turn
        CHECK(game.runComputerTurn());
        CHECK(game.currentPlayer() == 0 || game.phase() == Game::Phase::GameOver);
    }

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("ai_test: OK");
    return failures != 0;
}
