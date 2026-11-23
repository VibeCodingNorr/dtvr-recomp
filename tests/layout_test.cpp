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
    // Player setup: slot 0 top left, slot 7 bottom right, star right, stop left (640).
    const SetupGeometry setup = setupGeometry(640);
    CHECK(same(setup.driver[0], 61, 73, 73, 73) && same(setup.driver[7], 334, 317, 73, 73));
    CHECK(same(setup.name[0], 148, 73, 160, 26) && same(setup.start, 491, 420, 66, 50) && setup.stop.x == 26);
    CHECK(setupGeometry(1024).driver[3].y == 507 && setupGeometry(800).name[4].x == 521);
    bool threw = false;
    try { layoutForCanvas(900); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);

    // Widescreen extension: 16:9 at 1024 x 768 -> 1365 -> 341 extra -> 340 (even).
    CHECK(l1024.extra() == 0 && l1024.resolution() == 0 && same(l1024.overview, 32, 32, 704, 704));
    CHECK(widescreenExtra(l1024, 1920, 1080) == 340 && widescreenExtra(l1024, 2560, 1440) == 340
          && widescreenExtra(l1024, 1920, 1200) == 204);
    CHECK(widescreenExtra(l1024, 1024, 768) == 0 && widescreenExtra(l1024, 1280, 1024) == 0);
    CHECK(widescreenExtra(l1024, 5120, 1440) == 768);      // 32:9 capped at 21:9 (1792)
    CHECK(widescreenExtra(l1024, 0, 0) == 0);
    const ScreenLayout wide = widenLayout(l1024, 340);
    CHECK(wide.width == 1364 && wide.height == 768 && wide.baseWidth == 1024 && wide.resolution() == 0);
    CHECK(same(wide.map, 32, 32, 1044, 704) && wide.viewport.w == 1028 && wide.viewport.h == 688);
    CHECK(same(wide.overview, 32 + 170, 32, 704, 704) && same(wide.stage(), 170, 0, 1024, 768));
    CHECK(wide.portrait().x == 815 + 340 && wide.playerInfo().x == 764 + 340 && wide.playerInfo().w == 220);
    CHECK(widenLayout(layoutForCanvas(640), 208).resolution() == 2);
    threw = false;
    try { widenLayout(l1024, 11); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("layout_test: OK");
    return failures != 0;
}
