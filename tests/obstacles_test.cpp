// SPDX-License-Identifier: GPL-3.0-only
// Tests for road obstacles, event places and property damage (round 12).
#include "economy.hpp"
#include "events.hpp"
#include "obstacles.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// Fields 0..3 on a row at x = 0, 4, 8, 12 (links of four cells), linked right/left.
static Board row() {
    Board b;
    b.fields.resize(4);
    for (int i = 0; i < 4; ++i) {
        b.fields[size_t(i)].x = uint8_t(i * 4);
        b.fields[size_t(i)].next = {Field::kNone, int16_t(i < 3 ? i + 1 : -1), Field::kNone, int16_t(i > 0 ? i - 1 : -1)};
    }
    return b;
}

int main() {
    CHECK(clearingCard(kRain) == 36 && clearingCard(kSnow) == 36 && clearingCard(kRockfall) == 36);
    CHECK(clearingCard(kNails) == 38 && clearingCard(0x400) == -1);

    const Board board = row();
    CHECK(linkAtCell(board, 2, 0).field == 0 && linkAtCell(board, 2, 0).dir == 2);
    CHECK(linkAtCell(board, 4, 0).field == -1);                    // a field cell, not inside a link
    CHECK(linkAtCell(board, 2, 1).field == -1);

    ObstacleTable t;
    CHECK(t.add({kRain, 2, 2, 0, 0, 2}) && t.count() == 1);
    CHECK(t.onLink(board, 0, Direction::Right) == 0 && t.onLink(board, 1, Direction::Left) == 0);
    CHECK(t.onLink(board, 1, Direction::Right) == -1);
    CHECK(t.add({kSnow, 5, 2, 0, 0, 2}) && t.count() == 1 && t.slots()[0].type == kSnow); // same cell
    // Countdown: 5 -> 0, then removed on the next tick.
    for (int i = 0; i < 5; ++i) CHECK(t.tick().empty());
    const auto gone = t.tick();
    CHECK(gone.size() == 1 && gone[0].type == kSnow && t.count() == 0);
    for (int i = 0; i < 30; ++i) CHECK(t.add({kRain, 1, 100 + i, 0, -1, 0}));
    CHECK(!t.add({kRain, 1, 200, 0, -1, 0}));                     // table full
    t.remove(3);
    CHECK(t.count() == 29);

    // Obstacle spot on the way of the only player to the destination.
    std::vector<PlayerSlot> players(8);
    players[0].status = 1;
    players[0].field = 0;
    MsRandom rng(1);
    const ObstacleSpot spot = pickObstacleSpot(board, players, 0, 3, 0, rng, nullptr, nullptr);
    CHECK(spot.field >= 1 && spot.field <= 3 && spot.y == 0);
    CHECK(spot.x == board.fields[size_t(spot.field)].x + (spot.dir == 2 ? 2 : -2));

    // Cyclic event places start at index 1; the volcano toggles starting with Hekla.
    EventPlaces places;
    const Cell n1 = nextNessiePlace(places);
    CHECK(n1.x == 63 && n1.y == 72);                                // Dublin
    for (int i = 0; i < 12; ++i) nextNessiePlace(places);
    CHECK(places.nessie == 0);                                      // 12 -> 0 wraps
    CHECK(nextVolcanoPlace(places).x == 15 && nextVolcanoPlace(places).x == 153);
    CHECK(nextQuakePlace(places).x == 151 && nextRockfallCell(places).x == 196);

    // Damage town selection by category with the owned-entry preference.
    std::vector<Town> towns(3);
    for (int t2 = 0; t2 < 3; ++t2) { towns[size_t(t2)].x = uint16_t(t2); towns[size_t(t2)].entries[0].name = "A"; }
    towns[0].entries[0].words[1] = 4;
    towns[2].entries[0].words[1] = 4;
    for (int i = 0; i < 20; ++i) {
        const int pick = pickDamageTown(towns, {0, 0, 1}, 4, rng);
        CHECK(pick == 2);                                           // only town 2 has an owner
        const int any = pickDamageTown(towns, {0, 1, 0}, 4, rng);
        CHECK(any == 0 || any == 2);                                // fallback: all with category 4
    }
    CHECK(pickDamageTown(towns, {1, 1, 1}, 7, rng) == -1);
    CHECK(townAtCell(towns, {2, 0}) == 2 && townAtCell(towns, {9, 9}) == -1);

    // Damage: uninsured owners lose entries 0..7; entries 8 and 9 are not checked.
    for (int e = 0; e < 10; ++e) { towns[0].entries[size_t(e)].name = "E"; towns[0].entries[size_t(e)].value = 1000; }
    Economy eco(towns, 3);
    eco.addMoney(0, 100000);
    eco.addMoney(1, 100000);
    CHECK(eco.buy(0, 0, 0, BuyMode::Normal) && eco.buy(0, 0, 9, BuyMode::Normal));
    CHECK(eco.buy(1, 0, 1, BuyMode::Normal) && eco.buy(2, 0, 2, BuyMode::Normal));
    CHECK(eco.buy(0, 0, 2, BuyMode::Rent));                         // 0 rents player 2's entry
    std::array<bool, 8> insured{};
    insured[1] = true;
    const auto used = eco.damageTown(0, insured);
    CHECK(used[1] && !used[0] && !used[2]);
    CHECK(eco.town(0).entries[0].owner == kNoOwner && eco.town(0).entries[9].owner == 0);
    CHECK(eco.town(0).entries[1].owner == 1);
    CHECK(eco.town(0).entries[2].owner == kNoOwner && eco.town(0).entries[2].landlord == kNoOwner);
    CHECK(eco.townHasOwner(0));

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("obstacles_test: OK");
    return failures != 0;
}
