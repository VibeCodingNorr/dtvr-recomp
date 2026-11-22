// SPDX-License-Identifier: GPL-3.0-only
// Tests for the event animation table and the record step of seg2:6DCB.
#include "event_scenes.hpp"

#include <cstdio>
#include <cstring>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    const EventScene* storm = eventSceneForNotice(5042);
    CHECK(storm && std::strcmp(storm->file, "sturm.sbs") == 0 && storm->frames.size() == 6);
    if (storm) {
        // Countdown 0 at the start: the first pass already moves to frame 1, which
        // then stays delay + 1 = 7 passes.
        CHECK(std::strcmp(eventSceneFrame(*storm, 0), "ST2") == 0);
        CHECK(std::strcmp(eventSceneFrame(*storm, 6), "ST2") == 0);
        CHECK(std::strcmp(eventSceneFrame(*storm, 7), "ST3") == 0);
        CHECK(std::strcmp(eventSceneFrame(*storm, 35), "ST1") == 0); // wrapped to frame 0
        // Five frames in the first cycle, four more full cycles of six (repetitions 4).
        CHECK(eventScenePasses(*storm) == 1 + 7 * 5 + 4 * 7 * 6 - 1);
        CHECK(eventSceneFrame(*storm, eventScenePasses(*storm))[0] == '\0');
    }
    const EventScene* rock = eventSceneForNotice(5082);
    CHECK(rock && std::strcmp(rock->file, "stein.sbs") == 0 && rock->frames.size() == 33 && rock->delay == 3);
    if (rock) CHECK(eventScenePasses(*rock) == 1 + 4 * 32 - 1); // one cycle, frames 1..32
    CHECK(eventSceneForNotice(5021) && eventSceneForNotice(5301) && eventSceneForNotice(5283));
    CHECK(eventSceneForNotice(600) == nullptr && eventSceneForNotice(5121) == nullptr);
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("event_scenes_test: OK");
    return failures != 0;
}
