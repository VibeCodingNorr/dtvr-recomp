// SPDX-License-Identifier: GPL-3.0-only
// Tests for the event animation table, the record step of seg2:6DCB and flights.
#include "event_scenes.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    const EventScene* storm = eventSceneForNotice(5042);
    CHECK(storm && std::strcmp(storm->file, "sturm.sbs") == 0 && storm->phases.size() == 1);
    if (storm) {
        const auto t = eventSceneTimeline(*storm, 0, 208, 208);
        // Countdown 0 at the start: the first pass already shows frame 1, each frame
        // stays delay + 1 = 7 passes; five frames, then four more cycles of six.
        CHECK(t.size() == size_t(7 * 5 + 4 * 7 * 6));
        CHECK(!t.empty() && std::strcmp(t[0].frame, "ST2") == 0 && std::strcmp(t[6].frame, "ST2") == 0);
        CHECK(t.size() > 35 && std::strcmp(t[7].frame, "ST3") == 0 && std::strcmp(t[35].frame, "ST1") == 0);
        CHECK(eventScenePositions(*storm, 2, 3).size() == 1 && eventScenePositions(*storm, 2, 3)[0].x == 2 * 16 + 8);
    }
    const EventScene* rock = eventSceneForNotice(5082);
    CHECK(rock && rock->phases.size() == 1 && rock->phases[0].frames.size() == 33);
    if (rock) CHECK(eventSceneTimeline(*rock, 0, 208, 208).size() == size_t(4 * 32)); // frames 1..32 once

    const EventScene* quake = eventSceneForNotice(5202);
    CHECK(quake && quake->quakeCracks && quake->waitBefore == 2000 && quake->waitAfter == 500);
    if (quake) {
        const auto one = eventScenePositions(*quake, 0xD3, 0xE0), two = eventScenePositions(*quake, 0x97, 0xCF);
        CHECK(one.size() == 1 && one[0].x == 0xD5 * 16 && one[0].y == 0xE2 * 16);
        CHECK(two.size() == 2 && two[1].x == 0x9D * 16 && two[1].y == 0xCB * 16);
        CHECK(eventScenePositions(*quake, 1, 1).empty());
    }

    // Rain: the cloud starts at (c, e) for corner 2 (c = e = 104 + 40) and moves 2
    // pixels per pass and axis until both are within 2; then the two plays.
    const EventScene* rain = eventSceneForNotice(5122);
    CHECK(rain && rain->flight && rain->phases.size() == 3 && rain->dy == -0x11);
    if (rain) {
        const auto t = eventSceneTimeline(*rain, 2, 208, 208);
        CHECK(!t.empty() && t[0].offset.x == 144 && t[0].offset.y == 144 && std::strcmp(t[0].frame, "R00") == 0);
        CHECK(t.size() > 72 && t[1].offset.x == 142 && t[71].offset.x == 2 && t[72].offset.x == 0);
        CHECK(t.size() > 73 && std::strcmp(t[72].frame, "R02") == 0); // phase 2 starts at frame 1
        const auto left = eventSceneTimeline(*rain, 0, 208, 100);
        CHECK(!left.empty() && left[0].offset.x == -144 && left[0].offset.y == -90);
        // y reaches its axis first and snaps; x keeps moving.
        CHECK(left.size() > 45 && left[45].offset.y == 0 && left[45].offset.x == -54);
    }
    CHECK(eventSceneForNotice(5481) && eventSceneForNotice(5243) && eventSceneForNotice(5021));
    CHECK(eventSceneForNotice(600) == nullptr && eventSceneForNotice(5141) == nullptr);
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("event_scenes_test: OK");
    return failures != 0;
}
