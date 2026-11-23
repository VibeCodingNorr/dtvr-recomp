// SPDX-License-Identifier: GPL-3.0-only
// Vehicle frame animation of seg2:16C1 (countdown +0x1E, frame +0x2C).
#include "vehicle_anim.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// Direct simulation of the original loop: draw, then count down / advance.
static int simulate(int passes) {
    int frame = 0, countdown = 0, drawn = 0;
    for (int p = 0; p <= passes; ++p) {
        drawn = frame;
        const int old = countdown--;
        if (old < 0) { countdown = 4; frame = (frame + 1) % 4; }
    }
    return drawn;
}

int main() {
    for (int n = 0; n < 200; ++n) CHECK(vehicleFrame(uint32_t(n)) == simulate(n));
    CHECK(vehicleFrame(0) == 0 && vehicleFrame(1) == 0 && vehicleFrame(2) == 1 && vehicleFrame(7) == 1 && vehicleFrame(8) == 2);
    CHECK(vehicleFrame(20) == 0); // four frames of six passes after the first two
    // The same animation keeps its phase; another one restarts at frame 0.
    VehicleAnimation a;
    a.select(VehicleAnimation::key(2, 3), 100);
    CHECK(a.frame(110) == vehicleFrame(10));
    a.select(VehicleAnimation::key(2, 3), 105);
    CHECK(a.frame(110) == vehicleFrame(10));
    a.select(VehicleAnimation::key(1, 3), 108);
    CHECK(a.frame(108) == 0 && a.frame(110) == 1);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("vehicle_anim_test: OK");
    return failures != 0;
}
