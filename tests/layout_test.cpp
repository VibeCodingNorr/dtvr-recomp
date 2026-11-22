// SPDX-License-Identifier: GPL-3.0-only
// Geometry of the original game window (seg5:1227, seg6:1140).
#include "layout.hpp"

#include <cstdio>
#include <stdexcept>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static bool same(const ScreenRect& r, int x, int y, int w, int h) { return r.x == x && r.y == y && r.w == w && r.h == h; }

int main() {
    const ScreenLayout l640 = layoutForCanvas(640);
    CHECK(l640.artDir == 6 && l640.height == 480);
    CHECK(same(l640.map, 16, 16, 448, 448) && same(l640.viewport, 0, 0, 432, 432));
    CHECK(same(l640.playerInfo(), 481, 329, 138, 135) && same(l640.portrait(), 510, 16, 80, 80));
    const ScreenLayout l800 = layoutForCanvas(800);
    CHECK(l800.artDir == 8 && same(l800.map, 16, 16, 560, 560) && l800.viewport.w == 544);
    CHECK(same(l800.playerInfo(), 595, 416, 176, 160) && same(l800.portrait(), 635, 16, 96, 96));
    const ScreenLayout l1024 = layoutForCanvas(1024);
    CHECK(l1024.artDir == 1 && same(l1024.map, 32, 32, 704, 704) && l1024.viewport.h == 688);
    CHECK(same(l1024.playerInfo(), 764, 537, 220, 200) && same(l1024.portrait(), 815, 32, 118, 118));
    // The largest canvas that fits the screen, or the requested one.
    CHECK(layoutForScreen(1920, 1080).width == 1024 && layoutForScreen(1000, 700).width == 800);
    CHECK(layoutForScreen(700, 500).width == 640 && layoutForScreen(320, 200).width == 640);
    CHECK(layoutForScreen(1920, 1080, 800).width == 800);
    // Card window: five cards at 640 in rows of three, the highest slot first.
    std::array<bool, 8> hand{};
    for (int s : {0, 2, 3, 6, 7}) hand[size_t(s)] = true;
    const auto cards = cardWindowLayout(l640, hand);
    CHECK(cards.size() == 5 && cards[0].slot == 7 && cards[4].slot == 0);
    // width 3 * 92 - 10 = 266 -> left 187 -> 186 (even); height 2 * 127 - 10 = 244 -> top 118
    CHECK(same(cards[0].rect, 186, 118, 82, 117) && cards[2].rect.x == 186 + 2 * 92 - 0 && cards[3].rect.y == 244); // 245 rounded down to even
    CHECK(cardWindowLayout(l640, std::array<bool, 8>{}).empty());
    bool threw = false;
    try { layoutForCanvas(900); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("layout_test: OK");
    return failures != 0;
}
