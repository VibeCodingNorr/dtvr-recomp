// SPDX-License-Identifier: GPL-3.0-only
// The slot machine's animation records (seg2:6DCB, seg2:7671, seg2:7C0A, seg2:6AA2).
#include "slot_records.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static bool shows(const SlotRecords& s, const std::string& name) {
    for (const SlotFrame& f : s.frames()) if (f.name == name) return true;
    return false;
}

// Passes until `done` holds, counting the pass of the lever as 0.
template <typename F> static int passesUntil(SlotRecords& s, F done, int limit = 5000) {
    for (int p = 0; p < limit; ++p) {
        s.pass();
        if (done()) return p;
    }
    return -1;
}

int main() {
    // A frame stays delay + 1 passes: the lever DS:2292 (9 frames, delay 2) advances in
    // its first pass (countdown 0) and ends after 1 + 8 * 3 passes.
    {
        SlotRecords s({3, 4, 5});
        CHECK(s.state() == 1 && s.frames().empty());
        s.pullLever();
        s.pass();
        CHECK(shows(s, "slthad02") && s.state() == 2);
        s.pass(); s.pass();
        CHECK(shows(s, "slthad02"));
        s.pass();
        CHECK(shows(s, "slthad03"));
        CHECK(!s.stopReels()); // ignored while DS:2292 plays (seg2:6AA2)
        SlotRecords t({3, 4, 5});
        t.pullLever();
        const int start = passesUntil(t, [&] { return t.reelsStarted(); });
        CHECK(start == 24 && shows(t, "slthad10")); // the follow-up DS:2338 swings back
        // The reels start in the same pass and have advanced once: drehung2.
        CHECK(shows(t, "drehung2") && shows(t, "slt3anm2"));
    }
    // Reel i turns (i + 3) * 2 + 1 times (12 passes each, the first 9 after its start
    // pass), so its callback runs at start + 9 + 12 * (i + 3) * 2; it counts the reel,
    // and the last reel ends the gears and sets state 4. passesUntil counts from the
    // pass after the previous one.
    {
        SlotRecords s({2, 5, 1, 6, 3});
        s.pullLever();
        passesUntil(s, [&] { return s.reelsStarted(); });
        const int first = passesUntil(s, [&] { return s.sum() > 0; });
        CHECK(first == 9 + 12 * 6 - 1 && s.sum() == 2 && shows(s, "walze_2h") && shows(s, "count_0") && shows(s, "count_2"));
        CHECK(shows(s, "slt5anr1") || shows(s, "slt5anr2") || shows(s, "slt5anr3") || shows(s, "slt5anr4")
              || shows(s, "slt5anr5") || shows(s, "slt5anr6"));
        const int last = passesUntil(s, [&] { return s.state() == 4; });
        CHECK(last == (9 + 12 * 14) - (9 + 12 * 6) - 1 && s.sum() == 17); // reel 4 ends at start + 177
        // seg2:7E7C ends records 8-10, but seg2:6F69 only skips them: each gear keeps the
        // frame it drew in the previous pass (round 141; formerly taken to vanish).
        auto gears = [&] {
            std::vector<std::string> g;
            for (const SlotFrame& f : s.frames()) if (f.name.rfind("slt5an", 0) == 0) g.push_back(f.name);
            return g;
        };
        const std::vector<std::string> ended = gears();
        CHECK(ended.size() == 3);
        passesUntil(s, [] { return false; }, 200);
        CHECK(gears() == ended);
        CHECK(shows(s, "walze_3") && shows(s, "count_1") && shows(s, "count_7")); // faces stay
        CHECK(shows(s, "slthad19") && !shows(s, "drehung1") && !shows(s, "drehung4")); // the lever too
    }
    // Five reels: each stopped reel slows the gears by one pass per frame.
    {
        SlotRecords s({1, 1, 1, 1, 1});
        s.pullLever();
        passesUntil(s, [&] { return s.reelsStarted(); });
        std::string before;
        for (const SlotFrame& f : s.frames()) if (f.name.rfind("slt5anl", 0) == 0) before = f.name;
        int changes = 0;
        for (int p = 0; p < 60; ++p) { // delay 4: a new frame every 5 passes
            s.pass();
            for (const SlotFrame& f : s.frames())
                if (f.name.rfind("slt5anl", 0) == 0 && f.name != before) { before = f.name; ++changes; }
        }
        CHECK(changes == 12);
    }
    // Three reels: slt3anm plays its 13 frames once (delay 5); the record ends after
    // 12 * 6 passes but its last frame slt3anm1 stays drawn (seg2:6F69 skips it).
    {
        SlotRecords s({1, 2, 3});
        s.pullLever();
        passesUntil(s, [&] { return s.reelsStarted(); });
        auto gear = [&] {
            for (const SlotFrame& f : s.frames()) if (f.name.rfind("slt3anm", 0) == 0) return f.name;
            return std::string();
        };
        const int last = passesUntil(s, [&] { return gear() == "slt3anm1"; });
        // The start pass shows frame 1 (index 1); index 12 follows 11 * 6 passes later.
        CHECK(last == 11 * 6 - 1);
        passesUntil(s, [] { return false; }, 300);
        CHECK(s.state() == 4 && gear() == "slt3anm1");
    }
    // Every record keeps the frame it last drew: after the count each reel shows its
    // face walze_<v>, the lever slthad19, the sum its two digits, nothing else.
    {
        SlotRecords s({6, 2});
        s.pullLever();
        passesUntil(s, [&] { return s.state() == 4; });
        passesUntil(s, [] { return false; }, 100);
        const std::vector<SlotFrame> f = s.frames();
        CHECK(f.size() == 5 && shows(s, "slthad19") && shows(s, "walze_6") && shows(s, "walze_2")
              && shows(s, "count_0") && shows(s, "count_8"));
    }
    // seg2:6AA2: a key while the reels turn ends every reel's current turn.
    {
        SlotRecords s({4, 4, 4, 4});
        s.pullLever();
        passesUntil(s, [&] { return s.reelsStarted(); });
        for (int p = 0; p < 15; ++p) s.pass(); // inside the second turn (passes 10..21)
        CHECK(s.stopReels() && s.state() == 3 && !s.stopReels());
        const int stop = passesUntil(s, [&] { return s.state() == 4; });
        CHECK(stop == 21 - 16 && s.sum() == 16);
    }

    // The window of a card's spin opens only after its film or message (seg3:1A72-1AAB,
    // 1B97-1BD0, 1D0B / 1DCE -> 1E50): any of them showing or queued holds it.
    CHECK(slotWindowMayOpen({}));
    CHECK(!slotWindowMayOpen({true, false, false, false}));
    CHECK(!slotWindowMayOpen({false, true, false, false}));
    CHECK(!slotWindowMayOpen({false, false, true, false}));
    CHECK(!slotWindowMayOpen({false, false, false, true}));

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("slot_records_test: OK");
    return failures != 0;
}
