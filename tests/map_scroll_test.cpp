// SPDX-License-Identifier: GPL-3.0-only
// Tests for the map scrolling rules of seg6:3064, seg6:2B19 and seg2:29EB.
#include "map_scroll.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    const MapViewSize view{416, 416, false}; // 26 x 26 tiles
    // Centre: (p + 16 - size / 2) >> 4.
    TilePoint o = centredOrigin(2000, 1000, view);
    CHECK(o.x == (2000 + 16 - 208) / 16 && o.y == (1000 + 16 - 208) / 16);
    // Clamp at the world edges: x <= 0xFF - 26, y <= 0x100 - 26, never negative.
    CHECK(centredOrigin(0, 0, view).x == 0 && centredOrigin(0, 0, view).y == 0);
    CHECK(centredOrigin(5000, 5000, view).x == 0xFF - 26 && centredOrigin(5000, 5000, view).y == 0x100 - 26);
    CHECK(clampOrigin({-3, 300}, MapViewSize{100, 100, false}).y == 0x100 - 7);

    // Edge scroll: inside the inner area nothing moves.
    o = {10, 10};
    CHECK(!edgeScroll(o, 160 + 100, 160 + 100, 160 + 116, 160 + 116, view) && o.x == 10 && o.y == 10);
    // Near the right edge: (416 >> 6) * 3 = 18 tiles to the right.
    CHECK(edgeScroll(o, 160 + 380, 160 + 100, 160 + 396, 160 + 116, view) && o.x == 28 && o.y == 10);
    // Up is checked before right.
    o = {10, 10};
    CHECK(edgeScroll(o, 160 + 380, 160 + 10, 160 + 396, 160 + 26, view) && o.x == 10 && o.y == 0); // clamped
    // Zoom: half the view, (208 & ~0x20) >> 5 = 6 tiles.
    const MapViewSize zoom{208, 208, true};
    o = {10, 10};
    CHECK(edgeScroll(o, 160 + 100, 160 + 190, 160 + 116, 160 + 206, zoom) && o.y == 16 && o.x == 10);
    // At the world edge no further move is reported.
    o = {0, 0};
    CHECK(!edgeScroll(o, 0, 0, 16, 16, view));

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("map_scroll_test: OK");
    return failures != 0;
}
