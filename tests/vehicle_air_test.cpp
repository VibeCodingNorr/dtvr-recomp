// SPDX-License-Identifier: GPL-3.0-only
// Tests for the plane transformation of the vehicles (seg2:1793, 27EE, 2866).
#include "vehicle_air.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    CHECK(restingAir(0x09).state == 2 && restingAir(0x09).height == 0x18); // sea field
    CHECK(!restingAir(0x03).plane());                                     // land field
    VehicleAir a;
    airTick(a, true, 0x02, true, 0x02);       // towards a road: stays a car
    CHECK(a.state == 0 && a.height == 0);
    airTick(a, true, 0x06, true, 0x06);       // towards a seaway: rises by 2
    CHECK(a.state == 1 && a.height == 2);
    for (int i = 0; i < 11; ++i) airTick(a, false, 0, true, 0x06);
    CHECK(a.state == 1 && a.height == 0x18);
    airTick(a, false, 0, true, 0x06);         // at the top: flying
    CHECK(a.state == 2);
    airTick(a, false, 0, true, 0x09);         // a sea field ahead: stays up
    CHECK(a.state == 2);
    airTick(a, false, 0, true, 0x03);         // a land field three cells ahead: lands
    CHECK(a.state == 3 && a.height == 0x16);
    for (int i = 0; i < 11; ++i) airTick(a, false, 0, true, 0x03);
    CHECK(a.state == 3 && a.height == 0);
    airTick(a, false, 0, false, 0);
    CHECK(a.state == 0 && !a.plane());
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("vehicle_air_test: OK");
    return failures != 0;
}
