// SPDX-License-Identifier: GPL-3.0-only
// Tests for the event trigger, event tables and the opening of Eastern Europe.
#include "events.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    // Countdown: n * (10 - year / 32) + random(n).
    MsRandom rng(1), ref(1);
    CHECK(nextEventCountdown(3, 0, rng) == 30 + ref.random(3));
    CHECK(nextEventCountdown(2, 64, rng) >= 16 && nextEventCountdown(2, 64, rng) <= 18);

    // Table choice by progress; table 1 never yields robbers, table 4 is the only one with volcanoes.
    for (int i = 0; i < 300; ++i) {
        const EventKind early = chooseEvent(2, 10, 0, rng);
        CHECK(early != EventKind::Robbers && early != EventKind::Volcano && early != EventKind::Nessie);
        const EventKind mid = chooseEvent(2, 200, 100, rng);       // 120/2 <= 200, 500/2 > 100
        CHECK(mid != EventKind::Volcano && mid != EventKind::Drought && mid != EventKind::Storm);
    }
    bool volcano = false;
    for (int i = 0; i < 500; ++i) volcano |= chooseEvent(2, 999, 999, rng) == EventKind::Volcano;
    CHECK(volcano);
    CHECK(eventName(EventKind::Snow) == "Schnee");

    // East opening: more than (8 + 2n) % of the western value, or after year 40.
    CHECK(!eastShouldOpen(1000000, 120000, 2, 0) && eastShouldOpen(1000000, 120001, 2, 0));
    CHECK(eastShouldOpen(1000000, 0, 2, 41) && !eastShouldOpen(1000000, 0, 2, 40));
    CHECK(eastShouldOpen(1000000, 100001, 1, 0) && !eastShouldOpen(1000000, 100001, 3, 0));

    // Western value: kind-4/6 town fields without the mask.
    Board board;
    board.fields.resize(3);
    board.fields[0].x = 1; board.fields[0].kind = 6;
    board.fields[1].x = 2; board.fields[1].kind = 4; board.fields[1].flags = 0x4000;
    board.fields[2].x = 3; board.fields[2].kind = 1;
    std::vector<Town> towns(3);
    towns[0].x = 1; towns[0].entries[0].value = 1000; towns[0].entries[3].value = 500;
    towns[1].x = 2; towns[1].entries[0].value = 7000;
    towns[2].x = 3; towns[2].entries[0].value = 9000;
    CHECK(westPropertyValue(board, towns, 0x4000) == 1500);
    CHECK(westPropertyValue(board, towns, 0) == 8500);

    // Barrier links from a tile with flag 0x20 between two fields.
    std::vector<uint8_t> pit(0x4000, 0);
    pit[(0x1000 + 9) * 2] = 0x20;
    TileProperties props(pit);
    std::vector<uint8_t> mapData(262144, 0);
    const size_t cell = ((size_t(1) * 256 + 0) * 256 + 2) * 2; // layer 1, x 2, y 0
    mapData[cell] = 9;
    WorldMap map(mapData);
    Board line;
    line.fields.resize(2);
    line.fields[0].x = 1; line.fields[0].next[1] = 1;
    line.fields[1].x = 4; line.fields[1].next[3] = 0;
    const auto barred = barrierLinks(line, map, props);
    CHECK(barred.size() == 8 && barred[0 * 4 + 1] && barred[1 * 4 + 3] && !barred[0]);

    // Robbers: first active player above 500,000 x inflation with a right-hand link.
    std::vector<PlayerSlot> players(8);
    players[0].status = players[2].status = players[4].status = 1;
    players[0].field = players[2].field = players[4].field = 0;
    std::vector<double> cash(8, 0);
    cash[0] = 400000; cash[2] = 600000; cash[4] = 900000;
    CHECK(robberyVictim(players, line, cash, 1.0, {}) == 2);
    CHECK(robberyVictim(players, line, cash, 2.0, {}) == -1);
    std::vector<bool> sea(8, false);
    sea[2] = true;
    CHECK(robberyVictim(players, line, cash, 1.0, sea) == 4);
    players[4].field = 1;                                           // no right-hand link
    CHECK(robberyVictim(players, line, cash, 1.0, sea) == -1);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("events_test: OK");
    return failures != 0;
}
