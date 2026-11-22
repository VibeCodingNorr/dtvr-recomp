// SPDX-License-Identifier: GPL-3.0-only
// Tests for the event animation table and timing.
#include "event_scenes.hpp"

#include <cstdio>
#include <cstring>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    const EventScene* storm = eventSceneForNotice(5042);
    CHECK(storm && std::strcmp(storm->file, "sturm.sbs") == 0 && storm->frames.size() == 6);
    CHECK(storm && eventScenePasses(*storm) == 6 * 6 * 4);              // 144 passes of 20 ms
    CHECK(storm && std::strcmp(eventSceneFrame(*storm, 0), "ST1") == 0);
    CHECK(storm && std::strcmp(eventSceneFrame(*storm, 6), "ST2") == 0);
    CHECK(storm && std::strcmp(eventSceneFrame(*storm, 36), "ST1") == 0); // second repetition
    CHECK(storm && storm->dx == 8 && storm->dy == -0x1D);
    CHECK(eventSceneForNotice(5021) == nullptr && eventSceneForNotice(600) == nullptr);
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("event_scenes_test: OK");
    return failures != 0;
}
