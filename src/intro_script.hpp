// SPDX-License-Identifier: GPL-3.0-only
// The start intro of rally.exe (seg7:0000, round 89, no SDL): a state machine run
// once per 20 ms pass over 15 animation records (DS:30) in the window INTRO.
// Voices are timed by sync.dat (seg1:3E64 / 4071 / 414C). Evidence in
// docs/formats.md "Intro".
#pragma once

#include "voice_sync.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dtvr {

struct IntroFrame {
    std::string name; // SBS frame name as in the definitions (lower case)
    int x = 0, y = 0;  // top-left in the 480 x 360 DIB
};

struct IntroEvent {
    enum Kind {
        Voice,      // play butt/<lang>/voice/intro/<name>.wav (seg1:3579)
        StampFile,  // keyed blit of every frame of sbs/intro/<name>.sbs (seg7:1E3C)
        StampFrame, // keyed blit of the frame <name> (seg7:1F52)
        Flash,      // palette to white, backdrop <name>.bmp, back (seg5:3483 / 3623); subtitles stay off
        Wipe,       // backdrop butt/<lang>/<name>.bmp revealed by seg8:0E20 mode 4
        End         // the window closes (seg7:1C9B)
    };
    Kind kind = Voice;
    std::string name;
    int x = 0, y = 0;
};

struct IntroPass {
    std::vector<IntroEvent> events; // in the order of the pass
    bool draw = false;              // records drawn this pass (seg7:1BCE)
    std::vector<IntroFrame> frames; // the records as drawn, before the step
};

class IntroScript {
public:
    static constexpr int kRecords = 15;

    // `sync` may be null (no voice timing: every voice counts as finished).
    explicit IntroScript(const std::vector<VoiceSyncRecord>* sync);
    // The WM_CREATE stamps and records of seg7:0078 .. 0447.
    std::vector<IntroEvent> start();
    // One pass of the loop seg7:04D6 at `nowMs`.
    IntroPass pass(uint32_t nowMs);
    bool finished() const { return finished_; }
    int state() const { return state_; }
    int passes() const { return passes_; }

    // A record (0x40 bytes at DS:30): definitions by data-segment offset, 0 = none.
    struct Record {
        uint16_t def = 0, follow = 0;    // +0, +4
        int countdown = 0, reps = 0;     // +8, +0xA
        int frame = 0;                   // +0xC
        int x = 0, y = 0;                // +0x14, +0x16
    };
    const Record& record(int i) const { return records_[size_t(i)]; }

private:
    void set(int i, uint16_t def, int x, int y, int reps, int countdown = 0);
    void play(const char* name, uint32_t nowMs, std::vector<IntroEvent>& events);
    bool playing(uint32_t nowMs) const;
    bool talking(uint32_t nowMs) const;
    bool cue(uint32_t nowMs) const;
    void step();
    void moveHat();
    void logic(uint32_t nowMs, std::vector<IntroEvent>& events);

    const std::vector<VoiceSyncRecord>* sync_;
    const VoiceSyncRecord* voice_ = nullptr;
    uint32_t voiceStart_ = 0;
    std::array<Record, kRecords> records_{};
    int state_ = 0, passes_ = 0;
    bool doorClosed_ = false; // DS:12AC
    bool finished_ = false;
};

// The frame names of a definition (exposed for tests); empty for unknown offsets.
std::vector<std::string> introDefinitionFrames(uint16_t def);

// seg8:0E20 mode 4 on a client of width x height: the 400 cells of a 20 x 20 grid
// in reveal order, each as {left, top, right, bottom}. Drawn by `rand` (MS C rand):
// cell i is a random free cell whose row is at most i / 20 + 2. The bottom uses
// the width (as in rally.exe). A 20 ms wait precedes every 16th cell.
struct WipeCell { int left, top, right, bottom; };
class MsRandom;
std::vector<WipeCell> wipeMode4(int width, int height, MsRandom& rand);

} // namespace dtvr
