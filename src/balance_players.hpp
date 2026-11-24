// SPDX-License-Identifier: GPL-3.0-only
// The players' part of the year balance BILANZ (no SDL): one pass of rally.exe's loop
// seg8:8CC5..9258 with the show master's record 0 (DS:30) stepped as seg2:6DCB.
// Evidence in docs/formats.md "Year balance" (round 148).
#pragma once

#include "game.hpp"
#include "scene_record.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace dtvr {

// The BILANZ record definitions of the data segment (delay word, frame count byte +3,
// frame name pointers from +4) with the positions seg8:8A48 ff. / 8F0F give them.
extern const SceneRecord kBilanzSpeaking;   // DS:4800 intro\redet, 16 frames, delay 3
extern const SceneRecord kBilanzFoot;       // DS:4798 red_foot
extern const SceneRecord kBilanzLever;      // DS:4AE8 HEBEL (record 2)
extern const SceneRecord kBilanzPull;       // DS:4A30 HEBEL05-13, 23 frames, delay 3
extern const SceneRecord kBilanzPullFollow; // DS:4968 HEBEL03 x 3, delay 9999
extern const SceneRecord kBilanzHold;       // DS:4938 HEBEL02, 03, 02, 03, 04, delay 3

// A running record (+0 definition, +4 follow-up, +8 countdown, +0xA repetitions,
// +0xC frame, +0x14 / +0x16 position).
struct ShowRecord {
    const SceneRecord* def = nullptr;
    const SceneRecord* follow = nullptr;
    int countdown = 0, repeats = 0, frame = 0, x = 0, y = 0;
    // seg2:6DCB: `old = countdown--`; when old <= 0 the countdown reloads the delay
    // and the frame grows; past the last frame it wraps to 0 and a repetition is used
    // up, without repetitions the follow-up replaces the definition (frame 0, the
    // countdown kept) or the record ends.
    void step();
    const char* name() const;
};

class BalancePlayers {
public:
    // What one pass asks of its surroundings, in the order rally.exe calls it.
    class Io {
    public:
        virtual ~Io() = default;
        virtual bool voicePlaying() = 0;  // seg1:3E64
        virtual bool talking() = 0;       // seg1:4071: inside a sync.dat talk interval
        virtual bool noHuman() = 0;       // seg2:5C5A
        virtual void stopVoice() = 0;     // seg1:38B7
        virtual void playVoice(const std::string& name) = 0; // seg1:3579 / 37EA, butt/<c>/voice/bilanz
        virtual void playHebel() = 0;     // seg1:3579 with %s\FX\HEBEL01.wav (DS:7D37)
        virtual int announce(size_t index) = 0; // seg8:8E38 rand() % 4: the Next_ variant
    };

    // seg8:8982 / 8C9C / 8CA1: state 0, pass counter 0, the first player; record 0 is
    // DS:4800 (seg8:8A48), record 2 the lever. The opening voice is already playing.
    void start(const std::vector<Game::BalanceEntry>& entries);
    // DS:8D5C (Enter, Space, left click), kept until a pass uses it.
    void click() { click_ = true; }
    bool clickPending() const { return click_; }
    // One pass without the end test (seg8:9253). Returns true when the show master
    // goes on after the last player (seg8:8E28): the closing part follows.
    bool pass(Io& io);

    const ShowRecord& master() const { return master_; }
    bool leverShown() const { return lever_; } // record 2, cleared at the first pull (seg8:8F42)
    int state() const { return state_; }       // [bp-0x6E]
    int counter() const { return counter_; }   // [bp-0x6C]
    size_t next() const { return next_; }      // the entry of the slot [bp-0x6]

private:
    std::vector<Game::BalanceEntry> entries_;
    ShowRecord master_;
    bool lever_ = true, click_ = false;
    int state_ = 0, counter_ = 0;
    size_t next_ = 0;
};

} // namespace dtvr
