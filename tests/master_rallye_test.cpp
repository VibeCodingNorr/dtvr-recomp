// SPDX-License-Identifier: GPL-3.0-only
// Round R3a: the last stage (Master-Rallye) of rally.exe. Before it seg2:5CDE puts the
// players in play on consecutive cells of a 12-cell table from random(12 - n), each
// with the start facing seg2:5067; seg2:5FCA / 9C97 give Dr. Drago to the leader of
// cash + property. The announcement seg7:372F(3) keeps its destination draw but sets
// DS:A10D to Brussels (seg7:4B89). seg2:04AD re-places Dr. Drago after Flugzeug.
// Also the obstacle cue seg2:21E5 (Carb01) and the counter probe before it.
#include "game.hpp"
#include "rules.hpp"

#include <cstdio>
#include <memory>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static const std::string kBrussels = "Br\x81ssel"; // europe_d.tow spelling (code page 850)

// Ring of 13 fields linked left / right: London (kind 6) at cell (0, 0), fields 1..12
// at the Master-Rallye cells in table order. Brussels (kind 6) is field 1, Rom
// (kind 6) field 12; field 5 also has a road up to field 13 (start facing 1).
static Board ring(std::vector<Town>& towns) {
    Board b;
    b.fields.resize(14);
    for (int i = 0; i < 13; ++i) {
        Field& f = b.fields[size_t(i)];
        f.kind = 1;
        f.next = {Field::kNone, int16_t((i + 1) % 13), Field::kNone, int16_t((i + 12) % 13)};
        if (i > 0) { f.x = kMasterStartCells[size_t(i - 1)].x; f.y = kMasterStartCells[size_t(i - 1)].y; }
    }
    b.fields[0].kind = b.fields[1].kind = b.fields[12].kind = 6;
    b.fields[5].next[0] = 13;
    b.fields[13] = Field{};
    b.fields[13].x = 1; b.fields[13].y = 1; b.fields[13].kind = 1;
    b.fields[13].next[2] = 5;
    towns.resize(3);
    towns[0].name = "London";
    towns[1].x = b.fields[1].x; towns[1].y = b.fields[1].y; towns[1].name = kBrussels;
    towns[2].x = b.fields[12].x; towns[2].y = b.fields[12].y; towns[2].name = "Rom";
    return b;
}

int main() {
    std::vector<Town> towns;
    const Board board = ring(towns);

    // seg2:5067: the first direction with a link, 0 without links.
    {
        Field f;
        CHECK(startFacing(f) == 0);
        f.next[3] = 7;
        CHECK(startFacing(f) == 4);
        f.next[1] = 3;
        CHECK(startFacing(f) == 2);
        CHECK(startFacing(board.fields[5]) == 1 && startFacing(board.fields[4]) == 2);
    }

    // seg2:5CDE: slots in play in slot order on cells first, first + 1, ...; an empty
    // slot takes no cell; a cell without a field leaves the player but uses the cell.
    {
        std::vector<PlayerSlot> players(kPlayerSlots);
        players[1].status = 1;
        players[3].status = 2;
        players[6].status = 1;
        auto moved = masterStartFields(board, players, 4);
        CHECK(moved[1] == 5 && moved[3] == 6 && moved[6] == 7);
        CHECK(moved[0] == -1 && moved[2] == -1 && moved[7] == -1);
        Board missing = board;
        missing.fields[6].x = 2; // no field at table cell 5 any more
        moved = masterStartFields(missing, players, 4);
        CHECK(moved[1] == 5 && moved[3] == -1 && moved[6] == 7);
        Board twice = board;
        twice.fields[13].x = twice.fields[6].x; twice.fields[13].y = twice.fields[6].y;
        CHECK(masterStartFields(twice, players, 4)[3] == 13); // the loop does not stop at a match
    }

    // seg7:4B89 / 47AB: the kind-6 list in field order, the first town at the cell,
    // "BRÜSSEL" / "brüssel" with ASCII case folding first, then "brussels".
    {
        CHECK(lastStageDestination(board, towns, 12) == 1);
        std::vector<Town> t = towns;
        t[1].name = "BR\x9A" "SSEL";
        CHECK(lastStageDestination(board, t, 12) == 1);
        t[1].name = "Brussels";
        CHECK(lastStageDestination(board, t, 12) == 1);
        t[2].name = kBrussels; // a later "brüssel" wins over an earlier "brussels"
        CHECK(lastStageDestination(board, t, 0) == 12);
        t[1].name = "Bruessel";
        t[2].name = "Bruxelles";
        CHECK(lastStageDestination(board, t, 7) == 7); // not found: DS:A10D unchanged
        Board noTown = board;
        noTown.fields[1].kind = 4; // not in the destination list
        CHECK(lastStageDestination(noTown, towns, 12) == 12);
    }

    // The game: player 0 reaches Rom on the first of one stage. Draw order: the finish
    // music random(1), text 350 + random(2), the destination draw seg7:3755 (kept),
    // random(12 - 2) for the start cells, random(3) for Dr. Drago's countdown (seg2:B48D).
    {
        int checked = 0;
        for (uint16_t seed = 1; seed < 3000 && checked < 5; ++seed) {
            Game g(board, towns, 2, 1, seed, 1, nullptr, nullptr, false);
            g.setRandomEvents(false);
            if (g.destination() != 12 || g.currentPlayer() != 0 || !g.roll() || g.allowance() != 1) continue;
            CHECK(g.facing(0) == 2 && g.facing(1) == 2); // seg2:0184: London's first link
            MsRandom expected = g.sessionRandom();
            expected.random(1);
            expected.random(2);
            std::vector<PlayerSlot> at = g.players();
            at[0].field = 12;
            CHECK(chooseDestination(board, at, 0, 12, 0x4000, expected) >= 0);
            const int first = expected.random(10);
            expected.random(3);
            g.takeSounds();
            CHECK(g.step(Direction::Left));
            CHECK(g.sessionRandom().state() == expected.state());
            CHECK(g.announceMode() == 3 && g.remainingStages() == 0);
            CHECK(g.destination() == 1 && g.origin() == 12); // Brussels; DS:A10B = the reached field
            CHECK(g.playerField(0) == first + 1 && g.playerField(1) == first + 2);
            for (int p = 0; p < 2; ++p) {
                CHECK(g.facing(p) == startFacing(board.fields[size_t(g.playerField(p))]));
                CHECK(g.positionJumps(p) == 1);
            }
            CHECK(g.drago().holder == g.ranking().front() && g.drago().holder == 0); // 100,000 richer
            CHECK(g.dragoPlacements() == 1);
            ++checked;
        }
        CHECK(checked == 5);
    }

    // seg3:3302 -> seg2:04AD: Flugzeug moves the player as seg2:04AD; the holder of Dr.
    // Drago gets him placed anew (seg2:058C, call 9C97 at 0597).
    {
        bool done = false;
        for (uint16_t seed = 1; seed < 5000 && !done; ++seed) {
            Game g(board, towns, 2, 3, seed, 8, nullptr, nullptr, false);
            g.setRandomEvents(false);
            int plane = -1;
            for (int s = 0; s < 8; ++s)
                if (g.deck().typeOfSlot(g.hand(0)[size_t(s)]) == 27) plane = s;
            if (plane < 0 || g.currentPlayer() != 0) continue;
            g.startDrago(0);
            if (g.drago().holder != 0) continue;
            const int town = g.destination() == 1 ? 2 : 1; // not the destination
            const int placements = g.dragoPlacements();
            CHECK(g.playCard(plane, town));
            // seg3:3272: seg3:28ED first sets the player down out of view (seg3:2CBC), then
            // the flight lands (seg3:3302): two seg2:04AD calls, Dr. Drago placed twice.
            CHECK(g.playerField(0) == g.fieldOfTown(town) && g.positionJumps(0) == 2);
            CHECK(g.dragoPlacements() == placements + 2);
            done = true;
        }
        CHECK(done);
    }

    // seg2:1E61 before seg2:21D0: with the allowance used up the step towards an
    // obstacle is refused by the counter: no Carb01, no message, no question, no draw.
    // With a step left the vehicle meets the obstacle cell: Carb01 (channel 1, sound 9)
    // comes first (seg2:21E5), then seg2:2404 refuses (no clearing card here).
    {
        Board tri;
        tri.fields.resize(3);
        for (int i = 0; i < 3; ++i) {
            Field& f = tri.fields[size_t(i)];
            f.x = uint8_t(i * 10); f.kind = 1;
            f.next = {Field::kNone, int16_t((i + 1) % 3), Field::kNone, int16_t((i + 2) % 3)};
        }
        tri.fields[0].kind = 6;
        std::vector<Town> t(1);
        t[0].name = "London";
        int refused = 0, met = 0;
        for (uint16_t seed = 1; seed < 20000 && refused < 2; ++seed) {
            Game g(tri, t, 2, 0, seed, 8, nullptr, nullptr, false);
            g.setRandomEvents(false);
            g.setArrivalConfirm(true);
            int nails = -1, clearing = -1;
            for (int s = 0; s < 8; ++s) {
                const int type = g.deck().typeOfSlot(g.hand(0)[size_t(s)]);
                if (type == 18) nails = s;
                if (type == 39) clearing = s;
            }
            if (nails < 0 || clearing >= 0 || g.currentPlayer() != 0) continue;
            if (!g.playCard(nails, 2) || g.obstacles().count() != 1 || !g.roll()) continue; // link 0 -> 1
            if (g.allowance() != 2) continue;
            CHECK(g.step(Direction::Left) && g.step(Direction::Left) && g.playerField(0) == 1);
            g.takeSounds();
            MsRandom expected = g.sessionRandom();
            const int taken = g.stepsTaken();
            CHECK(!g.step(Direction::Left)); // 1 -> 0 carries the obstacle
            CHECK(g.messages().empty() && g.obstacleQuestion().card < 0 && g.takeSounds().empty());
            CHECK(g.sessionRandom().state() == expected.state() && g.stepsTaken() == taken);
            ++refused;
        }
        // With steps left: from field 0 straight onto the obstacle link.
        for (uint16_t seed = 1; seed < 20000 && met < 2; ++seed) {
            Game g(tri, t, 2, 0, seed, 8, nullptr, nullptr, false);
            g.setRandomEvents(false);
            g.setArrivalConfirm(true);
            int nails = -1, clearing = -1;
            for (int s = 0; s < 8; ++s) {
                const int type = g.deck().typeOfSlot(g.hand(0)[size_t(s)]);
                if (type == 18) nails = s;
                if (type == 39) clearing = s;
            }
            if (nails < 0 || clearing >= 0 || g.currentPlayer() != 0) continue;
            if (!g.playCard(nails, 2) || !g.roll()) continue;
            g.takeSounds();
            CHECK(!g.step(Direction::Right) && g.playerField(0) == 0);
            const auto cues = g.takeSounds();
            CHECK(!cues.empty() && cues.front().channel == 1 && cues.front().sound == 9);
            CHECK(!g.messages().empty());
            ++met;
        }
        CHECK(refused == 2 && met == 2);
    }

    return failures ? 1 : 0;
}
