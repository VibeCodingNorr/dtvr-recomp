// SPDX-License-Identifier: GPL-3.0-only
// The animation tempo's presentation clock (docs/animation-tempo.md acceptance targets).
#include "presentation_clock.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    // Original speed follows real time; the first update only starts the clock.
    PresentationClock c;
    c.update(5000);
    CHECK(c.now() == 0);
    c.update(7000);
    CHECK(c.now() == 2000 && c.rate() == 1);
    // A two-second scene takes one second at 2x and half a second at 4x.
    PresentationClock fast;
    fast.setRate(2, 100);
    fast.update(1100);
    CHECK(fast.now() == 2000);
    PresentationClock faster;
    faster.setRate(4, 0);
    faster.update(500);
    CHECK(faster.now() == 2000);
    // A rate change counts the time before it at the old rate: no jump.
    PresentationClock change;
    change.update(0);
    change.update(300);
    change.setRate(4, 500); // 500 at 1x
    CHECK(change.now() == 500);
    change.update(600);     // + 100 at 4x
    CHECK(change.now() == 900);
    // Many short updates equal one long one.
    PresentationClock a, b;
    a.setRate(2, 0);
    b.setRate(2, 0);
    for (uint32_t t = 1; t <= 1000; ++t) a.update(t);
    b.update(1000);
    CHECK(a.now() == b.now() && a.now() == 2000);
    // Paused time (the recomp menu) is not added; resuming continues without a jump.
    PresentationClock p;
    p.update(0);
    p.update(100);
    p.setPaused(true, 200);
    p.update(5000);
    CHECK(p.now() == 200 && p.paused());
    p.setPaused(false, 9000);
    p.update(9100);
    CHECK(p.now() == 300 && !p.paused());
    // Unsupported rates fall back to the original speed; wrap-around of the real ticks.
    PresentationClock w;
    w.setRate(3, 0xFFFFFF00u);
    CHECK(w.rate() == 1 && !PresentationClock::validRate(0) && PresentationClock::validRate(4));
    w.update(0x100u);
    CHECK(w.now() == 0x200u);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("presentation_clock_test: OK");
    return failures != 0;
}
