// SPDX-License-Identifier: GPL-3.0-only
// Dr. Drago's own vehicle (seg2:9F61 ff.): trail, direction, placement, following.
#include "drago_vehicle.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// A road along y = 10 from x = 2 to x = 20, a field every second cell.
static Board road() {
    Board b;
    for (int i = 0; i < 10; ++i) {
        Field f;
        f.x = uint8_t(2 + 2 * i);
        f.y = 10;
        if (i > 0) f.next[3] = int16_t(i - 1);
        if (i < 9) f.next[1] = int16_t(i + 1);
        b.fields.push_back(f);
    }
    return b;
}

static uint16_t land(int x, int y) { return y == 10 && x >= 2 && x <= 20 ? uint16_t(x % 2 == 0 ? 0x03 : 0x02) : 0; }

int main() {
    // Trail ring (seg2:4D3F / 4D9F / 4E5F / 4EE7).
    DragoTrail t;
    CHECK(t.empty() && !t.oneLeft() && t.count() == 0);
    CHECK(t.push(5) && !t.push(5) && t.count() == 1 && t.oneLeft() && t.front() == 5);
    CHECK(t.push(6) && t.push(7) && t.push(5) && t.push(8) && t.count() == 5);
    t.cutLoop(); // 5 comes back later: 6 and 7 are skipped
    CHECK(t.front() == 5 && t.count() == 2);
    CHECK(t.pop() && t.front() == 8 && t.oneLeft());
    t.cutLoop(); // no later copy: unchanged
    CHECK(t.front() == 8 && t.count() == 1);
    CHECK(t.pop() && t.empty() && !t.pop());
    for (int i = 0; i < DragoTrail::kSize + 5; ++i) t.push(i); // wraps around
    CHECK(t.count() == DragoTrail::kSize + 5);

    // Directions of seg2:AE77: 1 up clockwise to 8 up-left.
    CHECK(dragoDirection(0, 0, 0, 0) == 0);
    CHECK(dragoDirection(0, -5, 0, 0) == 1 && dragoDirection(5, -5, 0, 0) == 2 && dragoDirection(5, 0, 0, 0) == 3);
    CHECK(dragoDirection(5, 5, 0, 0) == 4 && dragoDirection(0, 5, 0, 0) == 5 && dragoDirection(-5, 5, 0, 0) == 6);
    CHECK(dragoDirection(-5, 0, 0, 0) == 7 && dragoDirection(-5, -5, 0, 0) == 8);

    const Board b = road();
    const DragoVehicle::CellProperties cell = land;
    // Placement (seg2:9CF9): behind a car facing right is its left link, 48 pixels.
    DragoVehicle d;
    d.place(0, 10 * 16, 160, 2, 4, b, cell);
    CHECK(d.placed() && d.x() == 10 * 16 - 48 && d.y() == 160);
    // Field 0 has no left link: the next side clockwise with a link (up, right: right).
    d.place(0, 2 * 16, 160, 2, 0, b, cell);
    CHECK(d.x() == 2 * 16 + 48 && d.y() == 160);
    {
        const auto s = d.sprites(0);
        CHECK(s.size() == 1 && std::strcmp(s[0].frame, "farrigt2") == 0 && s[0].y == 160 - 10);
    }

    // Following the trail: the holder drove from field 0 to field 6 (x 14).
    d.place(0, 2 * 16, 160, 3, 0, b, cell); // facing down: behind is up, none; right has a link
    for (int f = 0; f <= 6; ++f) d.trail().push(f);
    const int hx = 14 * 16, hy = 160;
    int passes = 0;
    while (passes < 500 && !(d.trail().empty() || d.trail().oneLeft())) {
        d.movePass(0, hx, hy, b, cell);
        ++passes;
    }
    CHECK(passes < 500 && d.y() == 160);
    CHECK(d.x() <= hx);
    for (int i = 0; i < 200; ++i) d.movePass(0, hx, hy, b, cell);
    CHECK(hx - d.x() <= 0x30 && d.direction() == 0); // stops within three cells
    {
        const auto s = d.sprites(0);
        CHECK(s.size() == 1 && std::strncmp(s[0].frame, "farrigt", 7) == 0);
    }

    // The blimp flies straight at the holder, diagonally too, and stands beside it.
    DragoVehicle z;
    z.place(2, 160, 160, 2, 4, b, cell);
    z.trail().clear();
    for (int i = 0; i < 400; ++i) z.movePass(2, 160 + 200, 160 + 100, b, cell);
    CHECK(std::abs(z.x() - 360) <= 0x30 && std::abs(z.y() - 260) <= 3);
    {
        const auto s = z.sprites(2);
        CHECK(s.size() == 2 && std::strncmp(s[0].frame, "faz", 3) == 0 && std::strncmp(s[1].frame, "zep", 3) == 0);
        CHECK(s[0].x == z.x() - 5 && s[0].y == z.y() - 0x2D && s[1].y == z.y() - 0x55);
    }

    // Frames: two passes for the first, then delay + 2 each (seg2:B0D6).
    DragoVehicle c;
    c.place(1, 10 * 16, 160, 2, 4, b, cell);
    CHECK(std::strcmp(c.sprites(1)[0].frame, "faleft1") == 0);
    c.animatePass();
    CHECK(std::strcmp(c.sprites(1)[0].frame, "faleft1") == 0);
    c.animatePass();
    CHECK(std::strcmp(c.sprites(1)[0].frame, "faleft2") == 0);
    for (int i = 0; i < 5; ++i) c.animatePass();
    CHECK(std::strcmp(c.sprites(1)[0].frame, "faleft3") == 0);

    if (failures) std::fprintf(stderr, "drago_vehicle_test: %d failures\n", failures);
    else std::puts("drago_vehicle_test: OK");
    return failures ? 1 : 0;
}
