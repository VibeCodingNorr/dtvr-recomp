// SPDX-License-Identifier: GPL-3.0-only
// The record step of seg2:6DCB for the full-screen scenes (round 145).
#include "scene.hpp"

#include <cstdio>
#include <cstring>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// Direct simulation: step (old = countdown--; old <= 0 -> reload, next frame), then show.
static int simulate(const SceneRecord& r, int pass, int* countdownAtStart = nullptr) {
    int frame = r.first, countdown = 0;
    for (int p = 0; p <= pass; ++p) {
        if (p == pass && countdownAtStart) *countdownAtStart = countdown;
        const int old = countdown--;
        if (old <= 0) {
            countdown = r.delay;
            frame = (frame + 1) % int(r.frames.size());
        }
    }
    return frame;
}

int main() {
    // The lever DS:4A30: 23 frames, delay 3.
    SceneRecord pull{80, 159, 0, 3, {}};
    for (int i = 0; i < 23; ++i) pull.frames.push_back("f");
    for (int p = 0; p < 300; ++p) CHECK(sceneFrameIndex(pull, p) == simulate(pull, p));
    CHECK(sceneFrameIndex(pull, 0) == 1 && sceneFrameIndex(pull, 3) == 1 && sceneFrameIndex(pull, 4) == 2);
    // seg7:3CE4: frame 9 with countdown 1 at the start of pass 35 (seg8:94E7 alike).
    CHECK(scenePassAt(pull, 9, 1) == 35);
    int countdown = -9;
    CHECK(simulate(pull, 34) == 9 && simulate(pull, 35, &countdown) == 9 && countdown == 1);
    // The definition has run once after 88 passes (HEBEL03 from pass 88, seg8:9408).
    CHECK(sceneLength(pull) == 88 && sceneFrameIndex(pull, 87) == 22 && sceneFrameIndex(pull, 88) == 0);
    // A start frame (+0xC) and delay 0: one pass per frame from first + 1.
    const SceneRecord quick{0, 0, 2, 0, {"a", "b", "c", "d"}};
    CHECK(std::strcmp(sceneFrame(quick, 0), "d") == 0 && std::strcmp(sceneFrame(quick, 1), "a") == 0);
    for (int p = 0; p < 20; ++p) CHECK(sceneFrameIndex(quick, p) == simulate(quick, p));
    if (failures) std::fprintf(stderr, "scene_test: %d failures\n", failures);
    else std::puts("scene_test: OK");
    return failures ? 1 : 0;
}
