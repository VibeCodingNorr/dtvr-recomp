// SPDX-License-Identifier: GPL-3.0-only
// Tests for the map scrolling rules of seg6:3064, seg6:2B19, seg2:29EB and Umfeld
// (seg6:1798 / seg6:1991).
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

    // Umfeld step: (416 / 3) / 16 = 8 tiles; zoomed (208 / 3) / 16 = 4.
    CHECK(surroundingsStep(view) == 8 && surroundingsStep(zoom) == 4);
    // Keys: x and y both use the width (seg6:1915 ff. read DS:94DE).
    const MapViewSize wide{512, 416, false}; // step 10
    o = surroundingsMove({50, 50}, 0, 1, wide);
    CHECK(o.x == 50 && o.y == 60);
    o = surroundingsMove({3, 3}, -1, -1, view); // clamped by seg6:2B19
    CHECK(o.x == 0 && o.y == 0);
    o = surroundingsMove({225, 225}, 1, 1, view);
    CHECK(o.x == 0xFF - 26 && o.y == 0x100 - 26);
    // Clicks (seg6:1798): the middle of each strip.
    const TilePoint c{50, 50};
    o = surroundingsClick(c, 30, 208, view);   // left
    CHECK(o.x == 42 && o.y == 50);
    o = surroundingsClick(c, 208, 30, view);   // top
    CHECK(o.x == 50 && o.y == 42);
    o = surroundingsClick(c, 400, 208, view);  // right
    CHECK(o.x == 58 && o.y == 50);
    o = surroundingsClick(c, 208, 400, view);  // bottom
    CHECK(o.x == 50 && o.y == 58);
    // Centre and corner squares: nothing.
    o = surroundingsClick(c, 208, 208, view);
    CHECK(o.x == 50 && o.y == 50);
    o = surroundingsClick(c, 10, 10, view);
    CHECK(o.x == 50 && o.y == 50);
    o = surroundingsClick(c, 410, 410, view);
    CHECK(o.x == 50 && o.y == 50);
    // Inclusive bounds: x = 0x3C is still the left strip, x = 0x3D not; x = width is right.
    CHECK(surroundingsClick(c, 0x3C, 208, view).x == 42 && surroundingsClick(c, 0x3D, 208, view).x == 50);
    CHECK(surroundingsClick(c, 416, 208, view).x == 58 && surroundingsClick(c, 417, 208, view).x == 50);
    // (0x3C, 0x3C) lies on the left and the top strip: both axes move.
    o = surroundingsClick(c, 0x3C, 0x3C, view);
    CHECK(o.x == 42 && o.y == 42);
    // Zoomed: the caller divides by 2; the strips are in the halved view.
    o = surroundingsClick(c, 200, 104, zoom); // right strip of 208
    CHECK(o.x == 54 && o.y == 50);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("map_scroll_test: OK");
    return failures != 0;
}
