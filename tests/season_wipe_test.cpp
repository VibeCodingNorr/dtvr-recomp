// SPDX-License-Identifier: GPL-3.0-only
// The blind wipe of a season change (seg8:1624 -> seg8:0E20 mode 0) and the season
// changes the game reports for it.
#include "game.hpp"
#include "season_wipe.hpp"
#include "worldmap.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static Board ring(std::vector<Town>& towns) {
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

static int covered(const std::vector<BlindRun>& runs) {
    int rows = 0;
    for (const BlindRun& r : runs) rows += r.height;
    return rows;
}

int main() {
    // seg8:0E61 ff.: pass i, blind j with 0 <= i - j <= 19, top = (19j + i) * h / 400.
    {
        const std::vector<BlindStrip> s = blindStrips(400);
        CHECK(s.size() == size_t(kBlindStrips));
        CHECK(s[0].top == 0 && s[0].bottom == 2);        // pass 0: blind 0, line 0
        CHECK(s[1].top == 1 && s[2].top == 20);          // pass 1: blind 0 line 1, blind 1 line 0
        CHECK(s.back().top == 399 && s.back().bottom == 401); // pass 39: blind 19, line 19
        const std::vector<BlindStrip> half = blindStrips(200);
        CHECK(half[2].top == 10 && half.back().top == 199); // 20 * 200 / 400, 399 * 200 / 400
        CHECK(blindStripsAfter(0) == 0 && blindStripsAfter(1) == 1 && blindStripsAfter(2) == 3);
        CHECK(blindStripsAfter(20) == 210 && blindStripsAfter(40) == 400 && blindStripsAfter(99) == 400);
    }

    // Out blacks the old map strip by strip, In reveals the new one the same way.
    {
        SeasonWipe w;
        CHECK(!w.active(0) && w.blackRows(300, 0).empty());
        w.start(1000);
        CHECK(w.stage(1000) == SeasonWipe::Stage::Out && w.stripsShown(1000) == 1);
        CHECK(covered(w.blackRows(300, 1000)) == 2);
        const std::uint32_t outEnd = 1000 + SeasonWipe::kStageMs - 1;
        CHECK(w.stage(outEnd) == SeasonWipe::Stage::Out && covered(w.blackRows(300, outEnd)) == 300);
        const std::uint32_t inStart = 1000 + SeasonWipe::kStageMs;
        CHECK(w.stage(inStart) == SeasonWipe::Stage::In && covered(w.blackRows(300, inStart)) == 298);
        const std::uint32_t inEnd = 1000 + 2 * SeasonWipe::kStageMs - 1;
        CHECK(w.stage(inEnd) == SeasonWipe::Stage::In && covered(w.blackRows(300, inEnd)) == 0);
        CHECK(!w.active(1000 + 2 * SeasonWipe::kStageMs));
        w.start(5000);
        w.stop();
        CHECK(!w.active(5000));
    }

    // seg8:1624 reloads the tiles at the change into March (map/3 -> map/0): one change
    // per new season, none within a season.
    {
        std::vector<Town> towns;
        const Board board = ring(towns);
        Game g(board, towns, 3, 40, 7, 1, nullptr, nullptr, false);
        for (int p = 0; p < kPlayerSlots; ++p)
            if (g.players()[size_t(p)].status) g.setComputer(p, true);
        CHECK(g.seasonChanges() == 0);
        for (int guard = 0; guard < 4000 && g.calendar().month < 2; ++guard) {
            g.runComputerTurn();
            if (g.calendar().month < 2) CHECK(g.seasonChanges() == 0);
        }
        CHECK(g.calendar().month == 2 && g.seasonChanges() == 1);
        for (int guard = 0; guard < 4000 && g.calendar().month < 5 && g.phase() != Game::Phase::GameOver; ++guard) {
            g.runComputerTurn();
            if (g.calendar().month < 5) CHECK(g.seasonChanges() == 1);
        }
        CHECK(g.calendar().month == 5 && g.seasonChanges() == 2 && seasonForMonth(5) == 1);
    }

    if (failures) return 1;
    std::puts("season_wipe_test: OK");
    return 0;
}
