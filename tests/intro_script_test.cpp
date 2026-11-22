// SPDX-License-Identifier: GPL-3.0-only
#include "intro_script.hpp"
#include "rules.hpp"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>

using namespace dtvr;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}

VoiceSyncRecord voice(const char* name, uint32_t ms) {
    VoiceSyncRecord r;
    r.name = std::string(name) + ".wav";
    r.length = ms * 11;
    r.entries.assign(10, {});
    r.talk.assign(10, {});
    return r;
}

struct Run {
    std::vector<std::string> voices;
    std::vector<std::pair<IntroEvent::Kind, int>> marks; // kind, pass within state
    int passes = 0;
    bool ended = false;
};

Run run(const std::vector<VoiceSyncRecord>* sync, IntroScript& s, int limit = 20000) {
    Run out;
    s.start();
    for (int i = 1; i <= limit && !s.finished(); ++i) {
        const IntroPass p = s.pass(uint32_t(i) * 20);
        out.passes = i;
        for (const auto& e : p.events) {
            if (e.kind == IntroEvent::Voice) out.voices.push_back(e.name);
            else out.marks.push_back({e.kind, s.passes()});
            if (e.kind == IntroEvent::End) out.ended = true;
        }
    }
    (void)sync;
    return out;
}

} // namespace

int main() {
    // The definitions match the data segment (spot checks; the full table was
    // compared with the executable's data segment).
    check(introDefinitionFrames(0x0614).size() == 18 && introDefinitionFrames(0x0614)[9] == "tu09", "door opening");
    check(introDefinitionFrames(0x0EDE).size() == 35 && introDefinitionFrames(0x0EDE)[34] == "rd014", "speaker");
    check(introDefinitionFrames(0x1234).empty(), "unknown definition");

    // Start: four stamps and records 3 to 7.
    {
        IntroScript s(nullptr);
        const auto events = s.start();
        check(events.size() == 4 && events[0].name == "paar2" && events[3].name == "paar1", "stamps");
        check(s.record(4).def == 0x08C8 && s.record(4).countdown == 1 && s.record(4).reps == 0x2706, "record 4");
        // The first pass draws frame 0 of every record, then steps.
        IntroPass p = s.pass(20);
        check(p.draw && p.frames.size() == 5 && p.frames[0].name == "tu00" && p.frames[1].name == "lt100", "first frames");
        check(s.record(3).frame == 0 && s.record(3).countdown == 999, "single frame record waits");
        // Record 4 (countdown 1): frame 0 for two passes, then lt101 for delay + 1 = 5 passes.
        p = s.pass(40);
        check(p.frames[1].name == "lt100", "countdown 1: second pass still frame 0");
        p = s.pass(60);
        check(p.frames[1].name == "lt101", "then frame 1");
    }

    // Voice timing: every voice 200 ms, rally07a long enough for the hat to fall.
    std::vector<VoiceSyncRecord> sync = {voice("rally01b", 200), voice("tuerauf1", 200), voice("tuerzu", 200),
                                         voice("rally02", 200),  voice("rally04", 200),  voice("rally05", 200),
                                         voice("rally07a", 6000), voice("blitz01", 200)};
    sync[6].cue = 2000 * 11; // the show master rises after 2 s
    {
        IntroScript s(&sync);
        const Run r = run(&sync, s);
        const std::vector<std::string> expected = {"rally01b", "tuerauf1", "tuerzu",   "rally02",
                                                   "rally04",  "rally05",  "rally07a", "blitz01"};
        check(r.voices == expected, "voice order");
        check(r.ended && s.finished() && s.state() == 11 && s.passes() == 0x22B, "ends at pass 555 of state 11");
        bool flash = false, wipe = false, stamp = false;
        for (const auto& [kind, pass] : r.marks) {
            if (kind == IntroEvent::Flash) flash = pass == 0x69;
            if (kind == IntroEvent::Wipe) wipe = pass == 0x131;
            if (kind == IntroEvent::StampFrame) stamp = pass == 0;
        }
        check(flash && wipe && stamp, "flash at 105, wipe at 305, door stamped on entering state 7");
        check(s.record(0).def == 0 && s.record(4).def == 0, "records cleared by the flash");
    }

    // Hat and Fate in state 10 (rally07a playing for 6 s = 300 passes).
    {
        IntroScript s(&sync);
        s.start();
        int i = 0;
        while (s.state() < 10 && i < 20000) s.pass(uint32_t(++i) * 20);
        s.pass(uint32_t(++i) * 20);
        check(s.record(0).def == 0x1164 && s.record(1).def == 0x113C, "hat and wall appear");
        int maxX = 0;
        bool fell = false;
        while (s.state() == 10 && i < 20000) {
            s.pass(uint32_t(++i) * 20);
            if (s.record(0).def == 0x1164 && s.record(0).x > maxX) maxX = s.record(0).x;
            if (s.record(0).def == 0x117E) fell = true;
        }
        check(maxX == 0x157, "the hat turns at x 0x158 (set to 0x157 in the same pass)");
        check(fell && s.record(1).def == 0x114A, "Fate falls behind the second wall piece");
        check(s.record(4).def == 0x10E0 && s.record(4).follow == 0x110A, "state 11: the show master sits down");
    }

    // Without voice timing every voice counts as finished at once.
    {
        IntroScript s(nullptr);
        const Run r = run(nullptr, s);
        check(r.ended && r.voices.size() == 8, "runs through without sync data");
    }

    // Wipe mode 4: 400 distinct cells, row limit, bottom from the width.
    {
        MsRandom rand(1);
        const auto cells = wipeMode4(480, 360, rand);
        std::set<std::pair<int, int>> seen;
        bool rows = true;
        for (size_t i = 0; i < cells.size(); ++i) {
            seen.insert({cells[i].left, cells[i].top});
            if (cells[i].top / 18 > int(i) / 20 + 2) rows = false;
        }
        check(cells.size() == 400 && seen.size() == 400, "every cell once");
        check(rows, "rows limited by progress");
        check(cells[0].bottom == (cells[0].top / 18 + 1) * 24, "bottom uses the width");
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
