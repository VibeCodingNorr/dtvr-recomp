// SPDX-License-Identifier: GPL-3.0-only
// Tests for the computer players' driving decision and town plan (round 16).
#include "ai.hpp"
#include "game.hpp"

#include <cstdio>
#include <stdexcept>

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
    CHECK(aiProfile(0).money->reserve[3] == 50000.0 && aiProfile(2).money->words[2] == 5 && aiProfile(4).strategy->perFieldLast == 3);

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

    // Forced sale (round 105, seg5:962A): own entries lowest score first, until the
    // cash plus value x 0.7 is no longer negative; category 3 is never sold.
    {
        AiState fs = ts;
        Economy fe(tt, 8);
        fs.economy = &fe;
        fe.addMoney(0, 100000);
        CHECK(fe.buy(0, 0, 0, BuyMode::Normal) && fe.buy(0, 0, 2, BuyMode::Normal));
        fe.addMoney(0, -fe.money(0).cash - 5000);         // 5,000 in debt
        auto plan2 = aiForcedSale(fs, 0);
        CHECK(plan2.size() == 1 && plan2[0].first == 0 && plan2[0].second == 0); // score 0 before 2; 7,000 covers it
        fe.addMoney(0, -20000);                           // 25,000 in debt: both
        plan2 = aiForcedSale(fs, 0);
        CHECK(plan2.size() == 2 && plan2[1].second == 2);
        std::vector<Town> agrar = tt;
        agrar[0].entries[0].words[1] = 3;                 // category 3 is kept
        AiState as = fs;
        as.towns = &agrar;
        plan2 = aiForcedSale(as, 0);
        CHECK(plan2.size() == 1 && plan2[0].second == 2);
    }

    // Recomp extension (round 100): Verkaufen in debt, Kredit without spare money.
    {
        AiState xs = ts;
        Economy xe(tt, 8);
        xs.economy = &xe;
        for (auto& h : xs.handTypes) h.fill(-1);
        xe.addMoney(0, -te.money(0).cash);                // cash 0: nothing spare
        xs.handTypes[0][2] = 8;                           // Kredit
        CHECK(aiTownPlan(xs, 0, 1).purchases == 0);       // original: never
        xs.extraCards = true;
        AiTownPlan p2 = aiTownPlan(xs, 0, 1);
        CHECK(p2.purchases == 1 && p2.actions.size() == 2 && p2.actions[0].kind == 2 && p2.actions[0].value == 2
              && p2.actions[1].kind == 0 && p2.actions[1].value == 2); // the best scored free entry
        xe.addMoney(0, 100000);
        CHECK(xe.buy(0, 0, 0, BuyMode::Normal));          // own entry 0
        xs.handTypes[0][2] = -1;
        xs.handTypes[0][5] = 1;                           // Verkaufen
        xe.addMoney(0, -xe.money(0).cash - 5000);         // 5,000 in debt
        p2 = aiTownPlan(xs, 0, 1);
        bool sells = false;
        for (const AiAction& a2 : p2.actions) sells = sells || a2.kind == 3;
        CHECK(p2.actions.size() >= 2 && p2.actions[0].kind == 2 && p2.actions[0].value == 5 && sells);
    }

    // Card choice (seg5:7386) on the row with the destination at field 4.
    {
        AiState cs = s;
        cs.board = &line;
        cs.steerless = false;
        cs.destination = 4;
        for (auto& h : cs.handTypes) h.fill(-1);
        cs.fieldDist.assign(line.fields.size(), 0xFFFF);
        auto fd = aiDistances(buildAiGraph(line, 0, none), 4);
        for (size_t i = 0; i < fd.size(); ++i) cs.fieldDist[i] = fd[i];
        players[0].field = 1;                             // three fields from the goal
        ObstacleTable clear;
        MsRandom cr(3);
        CHECK(aiChooseCard(cs, 0, clear, cr).slot == -1); // empty hand: spin
        cs.handTypes[0][5] = 31;                          // "3 Felder": exactly to the goal
        cs.handTypes[0][2] = 9;                           // Fiskus without a target
        AiCardChoice c = aiChooseCard(cs, 0, clear, cr);
        CHECK(c.slot == 5 && c.type == 31);
        players[0].flags = kFlagCardsBlocked;
        CHECK(aiChooseCard(cs, 0, clear, cr).slot == -1);
        players[0].flags = 0;
        cs.handTypes[0][5] = -1;
        cs.handTypes[0][6] = 35;                          // Hau ab while carrying Dr. Drago
        cs.dragoHolder = 0; cs.dragoForm = 1;
        c = aiChooseCard(cs, 0, clear, cr);
        CHECK(c.slot == 6);
        cs.dragoHolder = -1;
        CHECK(aiChooseCard(cs, 0, clear, cr).slot == -1);
        // Recomp extension (round 98): Nägel (type 18) never wins in rally.exe; with
        // extraCards a trailing opponent within 8 fields makes the computer throw them
        // away from the goal (field 2 -> left neighbour 1, direction 4).
        {
            const int keepField = players[3].field, keepStatus = players[3].status;
            players[0].field = 2;
            players[3].status = 1;
            players[3].field = 0;                         // two fields behind
            cs.handTypes[0].fill(-1);
            cs.handTypes[0][1] = 18;
            cs.nailsDirections = {false, true, false, true};
            CHECK(aiChooseCard(cs, 0, clear, cr).slot == -1); // original: never
            cs.extraCards = true;
            c = aiChooseCard(cs, 0, clear, cr);
            CHECK(c.slot == 1 && c.type == 18 && c.target == 4);
            cs.nailsDirections = {false, true, false, false}; // only towards the goal: no
            CHECK(aiChooseCard(cs, 0, clear, cr).slot == -1);
            cs.extraCards = false;
            cs.handTypes[0][1] = -1;
            players[0].field = 1;
            players[3].field = keepField;
            players[3].status = uint8_t(keepStatus);
        }
        // Recomp extension (round 99) on a 20-field row, destination 19, the computer at
        // field 2 (17 away): Risiko (type 26) far from the goal, Flugzeug (27) to the
        // airport two fields before the goal, Steuerlos (13) on a leader 7 away.
        {
            Board longRow = row(20);
            longRow.fields[19].kind = 6;
            std::vector<Town> airportTowns(1);
            airportTowns[0].x = longRow.fields[17].x;
            airportTowns[0].y = longRow.fields[17].y;
            AiState xs = cs;
            xs.board = &longRow;
            xs.towns = &airportTowns;
            xs.destination = 19;
            xs.airports[0] = 0;
            for (auto& h : xs.handTypes) h.fill(-1);
            const auto ld = aiDistances(buildAiGraph(longRow, 0, none), 19);
            xs.fieldDist.assign(ld.begin(), ld.end());
            const int keepField = players[0].field, keep3 = players[3].field, keepStatus = players[3].status;
            players[0].field = 2;
            players[3].status = 0;
            xs.handTypes[0][0] = 26;
            CHECK(aiChooseCard(xs, 0, clear, cr).slot == -1); // original: never
            xs.extraCards = true;
            CHECK(aiChooseCard(xs, 0, clear, cr).slot == 0);
            xs.handTypes[0][0] = 27;
            c = aiChooseCard(xs, 0, clear, cr);
            CHECK(c.slot == 0 && c.target == 0);
            xs.airports[0] = -1;                          // no airport: no flight
            CHECK(aiChooseCard(xs, 0, clear, cr).slot == -1);
            xs.handTypes[0][0] = 13;
            CHECK(aiChooseCard(xs, 0, clear, cr).slot == -1); // nobody ahead
            players[3].status = 1;
            players[3].field = 12;                        // 7 from the goal
            c = aiChooseCard(xs, 0, clear, cr);
            CHECK(c.slot == 0 && c.target == 3);
            players[0].field = keepField;
            players[3].field = keep3;
            players[3].status = uint8_t(keepStatus);
        }
        // Targets: Fiskus goes to the only other player; two candidates follow the odds.
        players[3].status = 1;
        CHECK(aiChooseTarget(cs, 0, 1, cr) == 3);
        players[3].flags = 0x2;                           // sitting out: not a target
        CHECK(aiChooseTarget(cs, 0, 1, cr) == -1);
        players[3].flags = 0;
        players[5].status = 1;
        int first = 0;
        for (int i = 0; i < 200; ++i) first += aiChooseTarget(cs, 0, 1, cr) == 3; // equal wealth: stable order
        CHECK(first > 100 && first < 200);                // slot 0 odds 13 of 0..15
        players[3].status = players[5].status = 0;
    }

    // Card worth (DS:521A) and the card to drop / sell.
    CHECK(aiCardRank(22, 0) == 10 && aiCardRank(6, 0) == 9 && aiCardRank(6, 25) == 11 && aiCardRank(6, 40) == 13);
    CHECK(aiCardRank(39, 0) == 0xFF && aiCardRank(39, 20) == 4 && aiCardRank(1, 0) == 0xFF);
    {
        AiState ks = s;
        for (auto& h : ks.handTypes) h.fill(-1);
        CHECK(aiLeastValuableCard(ks, 0) == -1);
        ks.handTypes[0][1] = 22;                          // 2 Walzen, worth 10
        ks.handTypes[0][4] = 0;                           // Weihnachten, worth 3
        CHECK(aiLeastValuableCard(ks, 0) == 4);
        ks.handTypes[0][6] = 1;                           // Verkaufen: no worth, dropped at once
        CHECK(aiLeastValuableCard(ks, 0) == 6);
        // Shop: the worthless card is sold, then affordable offers bought by worth.
        players[0].field = 0;
        ks.fieldDist.assign(ks.board->fields.size(), 0xFFFF);
        std::vector<CardOffer> offers{{0, 22}, {1, 30}, {2, 4}, {3, 29}}; // 2 Walzen, 2 Felder, Platin (too dear), 1 Feld (no worth)
        MsRandom kr(4);
        auto plan = aiCardShopPlan(ks, 0, offers, kr);
        CHECK(plan.size() == 3 && plan[0].kind == 1 && plan[0].value == 1);
        CHECK(plan.size() == 3 && plan[1].kind == 0 && plan[1].value == 22 && plan[2].kind == 0 && plan[2].value == 30);
    }

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

    // With the slot window (game window) the computer's move starts only when the window
    // has closed (finishSpin, seg2:66B9); seg2:66F9 then sets state 5 and the computer
    // plans and drives its route, exactly as without a window (regression: it spun and
    // then stood still).
    {
        Board gb = row(13);
        gb.fields[0].kind = 6;
        gb.fields[12].kind = 6;
        gb.fields[12].next[1] = 0;
        gb.fields[0].next[3] = 12;
        std::vector<Town> gt(2);
        gt[0].x = 0; gt[0].name = "London";
        gt[1].x = 48; gt[1].name = "Rom";
        const std::array<uint8_t, 8> slots{2, 2, 0, 0, 0, 0, 0, 0};
        Game plain(gb, gt, slots, 50, 7);
        Game shown(gb, gt, slots, 50, 7);
        plain.setRandomEvents(false);
        shown.setRandomEvents(false);
        shown.setSpinPresentation(true);
        const int start = shown.playerField(0);
        CHECK(shown.computerAdvance() && shown.spinPending() && shown.phase() == Game::Phase::Roll);
        CHECK(!shown.computerAdvance());                  // the window finishes the spin
        CHECK(shown.finishSpin() && shown.phase() == Game::Phase::Move && shown.computerTurnRunning());
        int stages = 0;
        while (shown.currentPlayer() == 0 && stages < 100 && shown.computerAdvance()) ++stages;
        CHECK(shown.playerField(0) != start);             // it drove
        CHECK(shown.currentPlayer() == 1 || shown.phase() == Game::Phase::GameOver);
        while (plain.currentPlayer() == 0 && plain.computerAdvance()) {}
        CHECK(plain.playerField(0) == shown.playerField(0) && plain.currentPlayer() == shown.currentPlayer());
        CHECK(plain.cash(0) == shown.cash(0));
    }

    // Slot setup ([Player] P0..P7) and the demo game of computers.
    {
        Board gb = row(13);
        gb.fields[0].kind = 6;
        gb.fields[12].kind = 6;
        gb.fields[12].next[1] = 0;
        gb.fields[0].next[3] = 12;
        std::vector<Town> gt(2);
        gt[0].x = 0; gt[0].name = "London";
        gt[1].x = 48; gt[1].name = "Rom";
        Game defaults(gb, gt, std::array<uint8_t, 8>{1, 2, 0, 0, 0, 0, 0, 0}, 0, 3);
        CHECK(!defaults.isComputer(0) && defaults.isComputer(1) && activePlayerCount(defaults.players()) == 2);
        CHECK(defaults.humansLeft() && !defaults.requestDemoEnd());
        bool threw = false;
        try { Game bad(gb, gt, std::array<uint8_t, 8>{3, 0, 0, 0, 0, 0, 0, 0}, 0, 1); } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
        threw = false;
        try { Game none(gb, gt, std::array<uint8_t, 8>{}, 0, 1); } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
        Game demo(gb, gt, std::array<uint8_t, 8>{2, 2, 2, 0, 0, 0, 0, 0}, 50, 9);
        demo.setRandomEvents(false);
        CHECK(!demo.humansLeft());
        for (int i = 0; i < 30; ++i) demo.runComputerTurn();
        CHECK(demo.phase() != Game::Phase::GameOver);     // a demo runs on
        CHECK(demo.requestDemoEnd());
        for (int i = 0; i < 3 && demo.phase() != Game::Phase::GameOver; ++i) demo.runComputerTurn();
        CHECK(demo.phase() == Game::Phase::GameOver);
    }

    // Reachable fields (round 80, seg5:9DE8): a row of 8 from field 2 -> 5 to the right
    // end (no turning back); nails between 4 and 5 block without a Nägel weg card.
    {
        const Board line = row(8);
        ObstacleTable none;
        CHECK(reachableSteps(line, 0, none, 2, 0, 0) == 5);
        CHECK(reachableSteps(line, 0, none, 7, 0, 0) == 7);
        ObstacleTable nails;
        Obstacle o;
        o.type = kNails; o.duration = 4; o.field = 4; o.dir = 2;
        nails.add(o);
        CHECK(reachableSteps(line, 0, nails, 2, 0, 0) == 2);      // 2 -> 3 -> 4, then blocked
        CHECK(reachableSteps(line, 0, nails, 2, 1, 0) == 5);      // the card clears it
        CHECK(reachableSteps(line, 0, nails, 2, 0, 1) == 2);      // a Schneepflug does not
        ObstacleTable rock;
        o.type = kRockfall;
        rock.add(o);
        CHECK(reachableSteps(line, 0, rock, 2, 3, 3) == 2);       // rockfall: no card helps
        CHECK(reachableSteps(line, 0, rock, 4, 0, 0) == 4);       // 4 -> 3 -> 2 -> 1 -> 0
        // A ring is a cycle: 30.
        Board ring = row(6);
        ring.fields[5].next[1] = 0;
        ring.fields[0].next[3] = 5;
        CHECK(reachableSteps(ring, 0, none, 2, 0, 0) == 30);
        // Masked fields (route mask) are no links: shut in completely.
        Board masked = row(3);
        masked.fields[0].flags = masked.fields[2].flags = 0x4000;
        CHECK(reachableSteps(masked, 0x4000, none, 1, 0, 0) == 0);
    }

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("ai_test: OK");
    return failures != 0;
}
