// SPDX-License-Identifier: GPL-3.0-only
// The monthly statement STAT (round R1): where the player change waits for it
// (seg2:07E1, seg2:12A3), what it shows (seg1:9AA2 / 9DE2) and that it draws nothing.
#include "game.hpp"
#include "save.hpp"
#include "save_slots.hpp"
#include "stat_sheet.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// A ring of 13 fields with the towns London (0) and Rom (12), blue fields elsewhere.
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

static void computers(Game& g) {
    for (int p = 0; p < kPlayerSlots; ++p)
        if (g.players()[size_t(p)].status) g.setComputer(p, true);
}

int main() {
    std::vector<Town> towns;
    const Board board = ring(towns);

    // Months 0..10: the change stops after the settlement with +0x4A / +0x52 intact and
    // the month not yet counted; closing STAT clears them (seg1:8CBA) and goes on.
    {
        Game headless(board, towns, 3, 40, 7, 1, nullptr, nullptr, false);
        Game watched(board, towns, 3, 40, 7, 1, nullptr, nullptr, false);
        computers(headless);
        computers(watched);
        watched.setStatementWindow(true);
        CHECK(watched.statementSerial() == 0 && watched.statementPlayer() == -1);
        for (int guard = 0; guard < 2000 && headless.statementSerial() == 0; ++guard) headless.runComputerTurn();
        for (int guard = 0; guard < 2000 && watched.phase() != Game::Phase::Statement; ++guard) watched.runComputerTurn();
        CHECK(headless.statementSerial() == 1 && headless.phase() != Game::Phase::Statement);
        CHECK(watched.phase() == Game::Phase::Statement && watched.statementSerial() == 1);
        CHECK(watched.calendar().month == 0 && headless.calendar().month == 1);
        CHECK(watched.statementPlayer() == 0); // DS:94E2: the first active slot
        CHECK(!watched.roll() && !watched.endTurn() && !watched.balanceFinish()); // modal
        const StatSheet sheet = statSheet(watched);
        CHECK(sheet.monthText == 301);
        const std::vector<int> rank = watched.ranking();
        CHECK(rank.size() == 3);
        double flows = 0;
        for (size_t i = 0; i < sheet.rows.size(); ++i) {
            const StatRow& row = sheet.rows[i];
            if (i >= rank.size()) { CHECK(row.player == -1); continue; }
            const PlayerMoney& m = watched.economy().money(rank[i]);
            CHECK(row.player == rank[i] && row.income == m.monthFlow && row.rent == m.monthlyIncome);
            CHECK(row.cash == m.cash && row.wealth == m.cash + m.propertyValue);
            if (i > 0) CHECK(sheet.rows[i - 1].wealth >= row.wealth);
            flows += m.monthFlow != 0;
        }
        CHECK(flows > 0); // blue fields paid during the month
        for (int p = 0; p < 3; ++p) CHECK(headless.economy().money(p).monthFlow == 0);
        { // no save while STAT holds the player change (it could not be loaded)
            const std::filesystem::path dir = std::filesystem::temp_directory_path() / "dtvr-statement-test";
            const SaveSlots slots(dir);
            std::string error;
            CHECK(!slots.save(0, watched, "x", error) && !error.empty());
            CHECK(!slots.saveAuto(watched, error));
            CHECK(!std::filesystem::exists(slots.file(0)) && !std::filesystem::exists(slots.autoFile()));
            std::filesystem::remove_all(dir);
        }
        CHECK(watched.statementFinish() && !watched.statementFinish());
        CHECK(watched.phase() != Game::Phase::Statement && watched.calendar().month == 1);
        for (int p = 0; p < 3; ++p)
            CHECK(watched.economy().money(p).monthFlow == 0 && watched.economy().money(p).monthlyIncome == 0);
        // STAT draws nothing: both games are the same, also later.
        CHECK(SaveCodec::encode(watched) == SaveCodec::encode(headless));
        for (int i = 0; i < 300 && headless.phase() != Game::Phase::GameOver; ++i) {
            headless.runComputerTurn();
            watched.runComputerTurn();
            for (int k = 0; k < 4 && watched.phase() == Game::Phase::Statement; ++k) watched.statementFinish();
            if (watched.phase() == Game::Phase::Balance) watched.balanceFinish();
        }
        CHECK(SaveCodec::encode(watched) == SaveCodec::encode(headless));
        CHECK(watched.statementSerial() == headless.statementSerial());
    }

    // December's change ends the year with BILANZ, not STAT (seg2:077D -> 0784 / 07DC).
    {
        Game g(board, towns, 2, 40, 9, 1, nullptr, nullptr, false);
        computers(g);
        for (int guard = 0; guard < 20000 && g.balanceSerial() == 0; ++guard) g.runComputerTurn();
        CHECK(g.balanceSerial() == 1 && g.calendar().month == 12 && g.statementSerial() == 11);
    }

    // After the Master-Rallye reward (seg2:12A3) STAT comes before the end (seg7:71C4):
    // two humans drive right around the ring until one stops on the last destination.
    {
        Game headless(board, towns, 2, 0, 1);
        Game watched(board, towns, 2, 0, 1);
        watched.setStatementWindow(true);
        headless.setRandomEvents(false); // no obstacle questions on the way
        watched.setRandomEvents(false);
        auto drive = [](Game& g) {
            if (g.phase() == Game::Phase::Arrived) g.endTurn();
            else if (g.phase() == Game::Phase::Roll) g.roll();
            else g.step(Direction::Right);
        };
        bool master = false;
        int monthly = 0;
        for (int guard = 0; guard < 5000 && watched.phase() != Game::Phase::GameOver; ++guard) {
            if (watched.phase() != Game::Phase::Statement) { drive(watched); continue; }
            const int months = watched.calendar().month;
            const int winner = watched.statementPlayer();
            const double flow = watched.economy().money(winner).monthFlow;
            CHECK(watched.statementFinish());
            if (watched.phase() != Game::Phase::GameOver) { ++monthly; continue; }
            master = true; // the Master-Rallye's STAT: the reward is in the month's flow
            CHECK(watched.calendar().month == months && winner == watched.finishPlayer());
            CHECK(flow >= 300000.0 * watched.calendar().inflation - 50000);
            CHECK(watched.economy().money(winner).monthFlow == flow); // not cleared after it
        }
        for (int guard = 0; guard < 5000 && headless.phase() != Game::Phase::GameOver; ++guard) drive(headless);
        CHECK(master);
        CHECK(monthly > 0);
        CHECK(headless.phase() == Game::Phase::GameOver);
        CHECK(headless.statementSerial() == watched.statementSerial());
        CHECK(SaveCodec::encode(watched) == SaveCodec::encode(headless));
    }

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("statement_test: OK");
    return failures != 0;
}
