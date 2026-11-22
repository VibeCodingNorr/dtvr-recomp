// SPDX-License-Identifier: GPL-3.0-only
// Tests for the movement rules recovered from rally.exe (see docs/formats.md).
#include "rules.hpp"

#include <cstdio>
#include <stdexcept>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// Grid board helper: fields laid out in a w x h grid, all orthogonal neighbours linked.
static Board grid(int w, int h) {
    Board b;
    b.fields.resize(size_t(w * h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            Field& f = b.fields[size_t(y * w + x)];
            f.x = uint8_t(x); f.y = uint8_t(y);
            f.next = {int16_t(y > 0 ? (y - 1) * w + x : -1), int16_t(x + 1 < w ? y * w + x + 1 : -1),
                      int16_t(y + 1 < h ? (y + 1) * w + x : -1), int16_t(x > 0 ? y * w + x - 1 : -1)};
        }
    return b;
}

int main() {
    // MS C rand: srand(1) gives 41, 18467, 6334 (well-known reference sequence).
    MsRandom rng(1);
    CHECK(rng.rand() == 41 && rng.rand() == 18467 && rng.rand() == 6334);
    rng.srand(1);
    CHECK(rng.random(0) == 0 && rng.random(-3) == 0);
    rng.srand(1);
    CHECK(rng.random(5) == 41 % 6);
    rng.srand(7);
    for (int v : spinReels(rng, 100, 5)) CHECK(v >= 1 && v <= 6);
    CHECK(spinReels(rng, 0, 5).empty());

    CHECK(opposite(Direction::Up) == Direction::Down && opposite(Direction::Left) == Direction::Right);
    CHECK(opposite(Direction::None) == Direction::None);

    // MoveCounter semantics of seg2:3620.
    MoveCounter blocked(0);
    CHECK(!blocked.step(Direction::Up));
    MoveCounter free(-1);
    CHECK(free.step(Direction::Up) && free.taken() == 0);
    MoveCounter two(2);
    CHECK(two.step(Direction::Right, false) && two.taken() == 0);   // probe does not count
    CHECK(two.step(Direction::Right) && two.taken() == 1);
    CHECK(two.step(Direction::Down) && two.taken() == 2);
    CHECK(!two.step(Direction::Right));                             // allowance used up
    CHECK(two.step(Direction::Up) && two.lastWasBacktrack() && two.taken() == 1); // undo
    CHECK(two.step(Direction::Right) && two.taken() == 2);
    CHECK(two.step(Direction::Left) && two.taken() == 1);
    CHECK(two.step(Direction::Left) && two.taken() == 0);           // back at the start
    CHECK(two.step(Direction::Left) && two.taken() == 1);           // new direction counts

    // TileProperties: size check, layer OR, outside cells read tile 0.
    bool threw = false;
    try { TileProperties p(std::vector<uint8_t>(100)); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    std::vector<uint8_t> pit(0x4000, 0);
    pit[(0x1000 + 0) * 2] = 0x08;     // tile 0 = water
    pit[(0x1000 + 5) * 2] = 0x02;     // tile 5 = road
    pit[(0x1000 + 7) * 2] = 0x01;     // tile 7 = field marker
    TileProperties props(pit);
    std::vector<uint8_t> mapData(262144, 0);
    auto put = [&](int layer, int x, int y, uint16_t v) {
        const size_t at = ((size_t(layer) * 256 + y) * 256 + x) * 2;
        mapData[at] = uint8_t(v); mapData[at + 1] = uint8_t(v >> 8);
    };
    put(0, 3, 3, 5); put(1, 3, 3, 7 | 0x4000);
    WorldMap map(mapData);
    CHECK(props.cell(map, 3, 3) == 0x03);
    CHECK(props.cell(map, 0, 0) == 0x08);
    CHECK(props.cell(map, -1, 300) == 0x08);

    // Distances, blocking mask and route tie-break on a 3 x 3 grid.
    Board b = grid(3, 3);
    auto d = fieldDistances(b, 0);
    CHECK(d[0] == 0 && d[4] == 2 && d[8] == 4);
    b.fields[1].flags = 0x4000;
    b.fields[3].flags = 0x4000;
    d = fieldDistances(b, 0, 0x4000);
    CHECK(d[1] == 1 && d[3] == 1 && d[2] == -1 && d[8] == -1);
    auto route = fieldRoute(b, 0, 8);                     // mask ignored by the route
    CHECK((route == std::vector<int>{0, 1, 2, 5, 8}));    // up/right/down/left order -> right first
    CHECK(fieldRoute(b, 8, 8) == std::vector<int>{8});
    CHECK(fieldRoute(b, 0, 99).empty());
    // A long chain beyond 80 levels stays unreached.
    Board chain = grid(100, 1);
    CHECK(fieldDistances(chain, 0)[80] == 80 && fieldDistances(chain, 0)[81] == -1);

    // Exact-step end fields without reversal.
    Board g = grid(3, 3);
    CHECK((fieldsAfterSteps(g, 4, 1) == std::vector<int>{1, 3, 5, 7}));
    CHECK((fieldsAfterSteps(g, 4, 2) == std::vector<int>{0, 2, 6, 8}));
    CHECK((fieldsAfterSteps(chain, 0, 3) == std::vector<int>{3}));
    CHECK(fieldsAfterSteps(chain, 99, 1).size() == 1 && fieldsAfterSteps(chain, 99, 2).size() == 1);
    CHECK(fieldsAfterSteps(chain, -1, 2).empty());

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("rules_test: OK");
    return failures != 0;
}
