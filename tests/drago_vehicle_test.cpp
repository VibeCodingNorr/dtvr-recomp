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
        const auto s = d.sprites();
        CHECK(s.size() == 1 && std::strcmp(s[0].frame, "farrigt2") == 0 && s[0].y == 160 - 10);
    }

    // Following the trail: the holder drove from field 0 to field 6 (x 14).
    d.place(0, 2 * 16, 160, 3, 0, b, cell); // facing down: behind is up, none; right has a link
    for (int f = 0; f <= 6; ++f) d.trail().push(f);
    const int hx = 14 * 16, hy = 160;
    const DragoView view{0, 0, 640, 480};
    const DragoContext at6{hx, hy, 6, b, cell, view};
    int passes = 0;
    while (passes < 500 && !(d.trail().empty() || d.trail().oneLeft())) {
        d.movePass(at6);
        ++passes;
    }
    CHECK(passes < 500 && d.y() == 160);
    CHECK(d.x() <= hx);
    for (int i = 0; i < 200; ++i) d.movePass(at6);
    CHECK(hx - d.x() <= 0x30 && d.direction() == 0); // stops within three cells
    {
        const auto s = d.sprites();
        CHECK(s.size() == 1 && std::strncmp(s[0].frame, "farrigt", 7) == 0);
    }

    // The blimp flies straight at the holder, diagonally too, and stands beside it.
    DragoVehicle z;
    z.place(2, 160, 160, 2, 4, b, cell);
    z.trail().clear();
    const DragoContext beside{160 + 200, 160 + 100, 4, b, cell, view};
    for (int i = 0; i < 400; ++i) z.movePass(beside);
    CHECK(std::abs(z.x() - 360) <= 0x30 && std::abs(z.y() - 260) <= 3);
    {
        const auto s = z.sprites();
        CHECK(s.size() == 2 && std::strncmp(s[0].frame, "faz", 3) == 0 && std::strncmp(s[1].frame, "zep", 3) == 0);
        CHECK(s[0].x == z.x() - 5 && s[0].y == z.y() - 0x2D && s[1].y == z.y() - 0x55);
    }

    // Frames: two passes for the first, then delay + 2 each (seg2:B0D6).
    DragoVehicle c;
    c.place(1, 10 * 16, 160, 2, 4, b, cell);
    CHECK(std::strcmp(c.sprites()[0].frame, "faleft1") == 0);
    c.animatePass();
    CHECK(std::strcmp(c.sprites()[0].frame, "faleft1") == 0);
    c.animatePass();
    CHECK(std::strcmp(c.sprites()[0].frame, "faleft2") == 0);
    for (int i = 0; i < 5; ++i) c.animatePass();
    CHECK(std::strcmp(c.sprites()[0].frame, "faleft3") == 0);

    // The start field of a form change (seg2:454E): the first field along the links
    // whose cell, inflated by three, misses the view.
    {
        const DragoView small{0, 0, 7 * 16, 400}; // cells 0..6 seen
        CHECK(dragoStartField(b, 0, 0, small) == 4);  // x 10: 10 - 3 = 7 is outside
        CHECK(dragoStartField(b, 0, 1, small) == 4);
        CHECK(dragoStartField(b, 9, 0, small) == 8);  // already outside: the first neighbour
        CHECK(dragoStartField(b, 0, 0, DragoView{0, 0, 640, 400}) == -1); // all in view
    }
    // A form change (seg2:9804 / 98D0): the bicycle leaves along the road to field 6,
    // which is out of a view of cells 0..6; the car comes back and the travel ends
    // near the holder at field 0.
    {
        DragoVehicle f;
        f.place(0, 2 * 16, 160, 2, 0, b, cell);
        const DragoView small{0, 0, 7 * 16, 400};
        const DragoContext home{2 * 16, 160, 0, b, cell, small};
        CHECK(f.startFormChange(1, 0, home) && f.travelling() && f.form() == 0);
        int guard = 0;
        bool sawBikeAway = false;
        while (f.travelling() && guard++ < 2000) {
            f.movePass(home);
            if (f.form() == 0 && f.x() > 5 * 16) sawBikeAway = true;
        }
        CHECK(!f.travelling() && f.form() == 1 && sawBikeAway);
        CHECK(std::abs(f.x() - 32) <= 0x30 && f.y() == 160);
        CHECK(std::strncmp(f.sprites()[0].frame, "fa", 2) == 0);
        CHECK(!f.startFormChange(1, 0, home)); // the same form: nothing to do
    }
    // Hau ab (seg2:B4E3): along the route to the new holder at field 9, then done.
    {
        DragoVehicle h;
        h.place(1, 2 * 16, 160, 2, 0, b, cell);
        const DragoContext there{20 * 16, 160, 9, b, cell, view};
        CHECK(h.startHandOver(0, there) && h.travelling());
        int guard = 0;
        while (h.travelling() && guard++ < 2000) h.movePass(there);
        CHECK(!h.travelling() && 20 * 16 - h.x() <= 0x30 && h.y() == 160);
    }

    if (failures) std::fprintf(stderr, "drago_vehicle_test: %d failures\n", failures);
    else std::puts("drago_vehicle_test: OK");
    return failures ? 1 : 0;
}
