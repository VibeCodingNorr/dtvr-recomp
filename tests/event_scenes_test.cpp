// SPDX-License-Identifier: GPL-3.0-only
// Tests for the event animation table, the record step of seg2:6DCB, flights,
// the drought's descent and Nessie's two places.
#include "event_scenes.hpp"

#include <cstdio>
#include <cstring>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static bool frameIs(const SceneStep& s, const char* name) { return s.frame && std::strcmp(s.frame, name) == 0; }

int main() {
    const EventScene* storm = eventSceneForNotice(5042);
    CHECK(storm && std::strcmp(storm->file, "sturm.sbs") == 0 && storm->phases.size() == 1);
    if (storm) {
        const auto t = eventSceneTimeline(*storm, 2, 3, 0, 208, 208, 0);
        // Countdown 0 at the start: the first pass already shows frame 1, each frame
        // stays delay + 1 = 7 passes; five frames, then four more cycles of six.
        CHECK(t.size() == size_t(7 * 5 + 4 * 7 * 6));
        CHECK(t.size() > 35 && frameIs(t[0], "ST2") && frameIs(t[6], "ST2") && frameIs(t[7], "ST3")
              && frameIs(t[35], "ST1"));
        CHECK(!t.empty() && t[0].at.size() == 1 && t[0].at[0].x == 2 * 16 + 8 && t[0].at[0].y == 3 * 16 - 0x1D);
    }
    const EventScene* rock = eventSceneForNotice(5082);
    CHECK(rock && rock->phases.size() == 1 && rock->phases[0].frames.size() == 33);
    if (rock) CHECK(eventSceneTimeline(*rock, 0, 0, 0, 208, 208, 0).size() == size_t(4 * 32));

    const EventScene* quake = eventSceneForNotice(5202);
    CHECK(quake && quake->quakeCracks && quake->waitBefore == 2000 && quake->waitAfter == 500);
    if (quake) {
        const auto one = eventScenePositions(*quake, 0xD3, 0xE0), two = eventScenePositions(*quake, 0x97, 0xCF);
        CHECK(one.size() == 1 && one[0].x == 0xD5 * 16 && one[0].y == 0xE2 * 16);
        CHECK(two.size() == 2 && two[1].x == 0x9D * 16 && two[1].y == 0xCB * 16);
        CHECK(eventScenePositions(*quake, 1, 1).empty());
        const auto t = eventSceneTimeline(*quake, 0x97, 0xCF, 0, 208, 208, 0);
        CHECK(t.size() == size_t(100 + 6 * 17 + 25)); // waits of 100 and 25 passes around frames 1..17
        CHECK(t.size() > 100 && !t[99].frame && t[100].frame && t[100].at.size() == 2);
    }

    // Rain: the cloud starts at (c, e) for corner 2 (c = e = 104 + 40) from the target
    // and moves 2 pixels per pass and axis until both are within 2; then the two plays.
    const EventScene* rain = eventSceneForNotice(5122);
    CHECK(rain && rain->phases.size() == 3 && rain->phases[0].motion == ScenePhase::Motion::Flight);
    if (rain) {
        const int tx = 10 * 16, ty = 10 * 16 - 0x11;
        const auto t = eventSceneTimeline(*rain, 10, 10, 2, 208, 208, 0);
        CHECK(t.size() > 73 && frameIs(t[0], "R00") && t[0].at[0].x == tx + 144 && t[0].at[0].y == ty + 144);
        CHECK(t.size() > 73 && t[1].at[0].x == tx + 142 && t[71].at[0].x == tx + 2 && t[72].at[0].x == tx);
        CHECK(t.size() > 73 && frameIs(t[72], "R02")); // the next definition starts at frame 1
        const auto left = eventSceneTimeline(*rain, 10, 10, 0, 208, 100, 0);
        CHECK(left.size() > 45 && left[0].at[0].x == tx - 144 && left[0].at[0].y == ty - 90);
        CHECK(left.size() > 45 && left[45].at[0].y == ty && left[45].at[0].x == tx - 54); // y snapped first
    }

    // Drought: the sun sinks from viewTop - 0x14 by 2 per pass until within 0x32 of
    // the town row; the later definitions stay at that height.
    const EventScene* drought = eventSceneForNotice(5142);
    CHECK(drought && drought->phases.size() == 4);
    if (drought) {
        const auto t = eventSceneTimeline(*drought, 20, 30, 0, 208, 208, 400);
        CHECK(!t.empty() && t[0].at[0].x == 20 * 16 + 0x12 && t[0].at[0].y == 400 - 0x14);
        // 380 + 2n <= 480 - 50 first at n = 25: 25 drawn passes, then y = 430.
        CHECK(t.size() > 26 && t[24].at[0].y == 428 && t[25].at[0].y == 430 && frameIs(t[25], "S04"));
    }

    // Nessie: first at Loch Ness with a recentring before the 500 ms wait, then at the
    // place with a recentring on the place + 1 cell.
    const EventScene* nessie = eventSceneForNotice(5101);
    CHECK(nessie && nessie->phases.size() == 4);
    if (nessie) {
        const auto t = eventSceneTimeline(*nessie, 171, 81, 10, 208, 208, 0); // town of index 10, lake (179, 82)
        CHECK(t.size() > 26 && t[0].recentre && t[0].camera.x == 0x4D0 && t[0].camera.y == 0x240 && !t[0].frame);
        CHECK(t.size() > 26 && t[25].frame && t[25].at[0].x == 0x4D5 && t[25].at[0].y == 0x248);
        size_t second = 0;
        for (size_t i = 1; i < t.size() && !second; ++i) second = t[i].recentre ? i : 0;
        CHECK(second > 25 && t[second].camera.x == 180 * 16 && t[second].camera.y == 83 * 16);
        CHECK(second > 25 && t[second].frame && t[second].at[0].x == 179 * 16 + 0x12 && t[second].at[0].y == 82 * 16 + 0x17);
        CHECK(second > 50 && !t[second - 1].frame); // the 1000 ms wait before
    }
    CHECK(eventSceneForNotice(5481) && eventSceneForNotice(5243) && eventSceneForNotice(5021));
    CHECK(eventSceneForNotice(600) == nullptr && eventSceneForNotice(5001) == nullptr);
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("event_scenes_test: OK");
    return failures != 0;
}
