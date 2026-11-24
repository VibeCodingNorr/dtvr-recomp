// SPDX-License-Identifier: GPL-3.0-only
// The year balance's board (round 149): sheet values, the worth graph and the
// year counters of the settlement, on a synthetic board (no original data).
#include "balance_board.hpp"
#include "panel.hpp"

#include <cstdio>
#include <map>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// Ring of 13 fields with towns of one property each (as in save_test).
static Board ring(std::vector<Town>& towns) {
    Board b;
    b.fields.resize(13);
    for (int i = 0; i < 13; ++i) {
        Field& f = b.fields[size_t(i)];
        f.x = uint8_t(i * 10);
        f.kind = i % 3 == 0 ? 4 : i % 3 == 1 ? 3 : 1;
        f.next = {Field::kNone, int16_t((i + 1) % 13), Field::kNone, int16_t((i + 12) % 13)};
    }
    b.fields[0].kind = 6;
    b.fields[12].kind = 6;
    towns.resize(2);
    towns[0].x = 0; towns[0].name = "London";
    towns[1].x = 120; towns[1].name = "Rom";
    for (int i = 3; i < 12; i += 3) {
        Town t;
        t.x = uint16_t(i * 10);
        t.name = "Stadt";
        t.entries[0].name = "Haus";
        t.entries[0].value = 10000;
        t.entries[0].words = {4, 1, 0};
        towns.push_back(t);
    }
    return b;
}

int main() {
    // seg6:137F: "$%li", the first two characters swapped for a negative value.
    CHECK(formatMoney(30000) == "$30.000" && formatMoney(-1234567) == "-$1.234.567");
    CHECK(formatMoney(999) == "$999" && formatMoney(-0.5) == "0$" && formatMoney(1000.9) == "$1.000");

    // Vermögenszuwachs: +0x66 + 4 * year minus the entry before, or minus 30000.
    PlayerMoney m;
    m.yearRent = 700;
    m.worth[0] = 41000;
    m.worth[1] = 38000;
    CHECK(balanceGrowth(m, 0) == 700 - 30000); // the year's rent (+0x66) for DS:A102 = 0
    CHECK(balanceGrowth(m, 1) == 11000);
    CHECK(balanceGrowth(m, 2) == -3000);

    std::vector<Town> towns;
    const Board board = ring(towns);
    Game g(board, towns, 3, 2, 11, 3);
    for (int p = 0; p < 3; ++p) g.setComputer(p, true);
    g.setPlayerNames({"Ann", "Bob", "Cid", "", "", "", "", ""});
    g.setBalanceWindow(true);
    int balances = 0;
    for (int i = 0; i < 5000 && g.phase() != Game::Phase::GameOver && balances < 2; ++i) {
        if (g.phase() == Game::Phase::Balance) {
            ++balances;
            const int year = g.calendar().year;
            CHECK(year == balances);
            // At the balance the year's rent is still there (seg1:8BC6 follows it).
            const std::map<int, std::string> texts{{8001, "Sheet %Fs"}, {8002, "growth"}, {8003, "rent"},
                                                   {8004, "property"}, {8005, "cash"}, {8006, "worth"},
                                                   {8007, "credit"}};
            const BalanceSheet s = balanceSheet(g, 1, texts);
            const PlayerMoney& b = g.economy().money(1);
            CHECK(s.lines.size() == 7 && s.lines[0].label == "Sheet Bob" && s.lines[0].font == 0 && s.lines[0].top == 60);
            CHECK(s.lines[1].label == "growth" && s.lines[1].top == 105 && s.lines[1].value == formatMoney(balanceGrowth(b, year)));
            CHECK(s.lines[2].label == "rent" && s.lines[2].top == 130 && s.lines[2].value == formatMoney(b.yearRent) && !s.lines[2].negative);
            CHECK(s.lines[3].label == "credit" && s.lines[3].top == 155);
            CHECK(s.lines[4].label == "property" && s.lines[4].top == 210 && s.lines[4].value == formatMoney(b.propertyValue));
            CHECK(s.lines[5].label == "cash" && s.lines[5].top == 235 && s.lines[5].negative == (b.cash < 0));
            CHECK(s.lines[6].label == "worth" && s.lines[6].top == 270 && s.lines[6].font == 2);
            CHECK(b.worth[size_t(year - 1)] == int32_t(int32_t(b.propertyValue) + b.cash));
            // The graph: one line per slot in play, 1 + year points ending at x = 0xBB + 0xEB.
            const BalanceGraph gr = balanceGraph(g);
            CHECK(gr.players.size() == 3 && gr.legend.size() == 3);
            CHECK(gr.players[0].points.size() == size_t(year) + 1 && gr.players[0].points.back().x == 0xBB + 0xEB);
            CHECK(gr.legend[1].frame == 'F' && gr.legend[1].at.x == 0xC0 + 0x1E && gr.legend[1].at.y == 0x107);
            CHECK(gr.players[2].colour.r == 0xC3 && gr.players[2].colour.g == 0 && gr.players[2].colour.b == 0);
            CHECK(gr.ticks.size() == 2 * 38 && gr.axes.size() == 2 && gr.axes[0].to.y == 0x101);
            const int rentBefore = b.yearRent;
            g.balanceFinish();
            CHECK(g.economy().money(1).yearRent == 0 || rentBefore == 0);
            continue;
        }
        g.runComputerTurn();
    }
    CHECK(balances == 2);
    // A slot without a player shows an empty name.
    CHECK(balanceSheet(g, 5, {{8001, "Sheet %Fs"}}).lines[0].label == "Sheet ");

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("balance_board_test: OK");
    return failures != 0;
}
