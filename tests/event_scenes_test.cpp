// SPDX-License-Identifier: GPL-3.0-only
// Tests for the event animation table, the record step of seg2:6DCB, flights,
// the drought's descent and Nessie's two places.
#include "event_scenes.hpp"

#include <cstdio>
#include <string>
#include <utility>
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
    // Event WAVs (round 135): seg1:2D40(7, <data>\snd\<name>, loops) with the first frame
    // of a phase — after the earthquake's 2000 ms wait, after the flood's wave, Nessie's
    // E12 (one repetition) at Loch Ness, E12a / E12b with its last two phases.
    {
        auto sounds = [](const std::vector<SceneStep>& t) {
            std::vector<std::pair<size_t, std::string>> out;
            for (size_t i = 0; i < t.size(); ++i)
                if (t[i].sound) out.push_back({i, std::string(t[i].sound) + "/" + std::to_string(t[i].soundLoops)});
            return out;
        };
        const auto quake = sounds(eventSceneTimeline(*eventSceneForNotice(5201), 0x97, 0xCF, 0, 208, 208, 0));
        CHECK(quake.size() == 1 && quake[0].first == 100 && quake[0].second == "E5/0");
        const auto storm = sounds(eventSceneTimeline(*eventSceneForNotice(5041), 2, 3, 0, 208, 208, 0));
        CHECK(storm.size() == 1 && storm[0].first == 0 && storm[0].second == "E1/0");
        const auto floodT = eventSceneTimeline(*eventSceneForNotice(5241), 10, 10, 0, 208, 208, 0);
        const auto flood = sounds(floodT);
        CHECK(flood.size() == 1 && flood[0].second == "e7/0" && flood[0].first > 0
              && std::string(floodT[flood[0].first].frame) == "RG02"                  // a new record advances at once,
              && std::string(floodT[flood[0].first - 1].frame) == "RG00");          // after the flight of RG00
        const auto nessieT = eventSceneTimeline(*eventSceneForNotice(5101), 50, 50, 0, 208, 208, 0);
        const auto nessie = sounds(nessieT);
        CHECK(nessie.size() == 3 && nessie[0].first == 25 && nessie[0].second == "E12/1");
        CHECK(nessie.size() == 3 && std::string(nessieT[nessie[1].first].frame) == "NH09" && nessie[1].second == "E12a/0"
              && std::string(nessieT[nessie[1].first - 1].frame) == "NH01"
              && std::string(nessieT[nessie[2].first].frame) == "NH45" && nessie[2].second == "E12b/0"
              && std::string(nessieT[nessie[2].first - 1].frame) == "NH43");
        int withSound = 0;
        for (int text : {5041, 5021, 5141, 5101, 5281, 5201, 5121, 5241, 5481, 5081})
            withSound += eventSceneForNotice(text) && !eventSceneForNotice(text)->sounds.empty();
        CHECK(withSound == 10);
    }
    CHECK(eventSceneForNotice(5481) && eventSceneForNotice(5243) && eventSceneForNotice(5021));
    CHECK(eventSceneForNotice(600) == nullptr && eventSceneForNotice(5001) == nullptr);
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("event_scenes_test: OK");
    return failures != 0;
}
