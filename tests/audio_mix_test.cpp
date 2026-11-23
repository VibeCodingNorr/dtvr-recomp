// SPDX-License-Identifier: GPL-3.0-only
// The mixing rule of one sample (round 132).
#include "audio_mix.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    // Below the knee the sources simply add up, as before.
    CHECK(mixSample(1000, 2000, -500) == 2500 && mixSample(0, 0, 0) == 0);
    CHECK(softLimit(kMixKnee) == kMixKnee && softLimit(-kMixKnee) == -kMixKnee);
    // Above it the sum bends towards full scale: monotonic, symmetric, never cut.
    int previous = kMixKnee;
    for (int s = kMixKnee + 1; s <= 120000; s += 97) {
        const int v = softLimit(s);
        CHECK(v >= previous && v <= 32767 && softLimit(-s) == -v);
        previous = v;
    }
    CHECK(softLimit(30000) < 30000 && softLimit(30000) > 28000);
    CHECK(softLimit(200000) >= 32700);
    // WaveMix clips its own channels first: loud effects alone stay at full scale.
    CHECK(mixSample(0, 90000, 0) == softLimit(32767));
    CHECK(mixSample(0, -90000, 0) == softLimit(-32768) || mixSample(0, -90000, 0) == -softLimit(32767));

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("audio_mix_test: OK");
    return failures != 0;
}
