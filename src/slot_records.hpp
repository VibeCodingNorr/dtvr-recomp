// SPDX-License-Identifier: GPL-3.0-only
// The animation records of the slot machine window (round 128), advanced pass by
// pass like rally.exe: one pass of the loop seg2:65C4 (20 ms) runs seg2:6DCB over
// the records in index order. A record's countdown +8 is reloaded with the
// definition's delay and the frame advances when the *old* value was <= 0 (jg), so a
// frame stays delay + 1 passes. After the last frame the repetitions +0xA count down;
// at 0 the callback +0xE runs, then the follow-up +4 replaces the definition, or
// the record ends (seg2:6EB4-6F51). No SDL. Evidence in docs/formats.md "Slot gears".
//
// Records: 0 the lever (seg2:6A09: DS:2292, follow-up DS:2338, callback seg2:7671),
// 2 the sum display, 3 + i reel i (seg2:7671: DS:2398 drehung1-4, (i + 3) * 2
// repetitions, follow-up DS:2E3C[value], callback seg2:7C0A(i)), 8-10 the gears of
// three to five reels (seg2:78AB). seg2:7C0A raises the gears' delay by one per
// stopped reel (four and five reels) and ends them with the last reel.
//
// Drawing (round 141): every record is blitted opaque (+0xD = 0, seg6:41A2 -> seg2:913A)
// into the window's WinG surface, and seg2:6F69 draws only records that still have a
// definition. Nothing restores the housing in the loop (seg2:74E4 gets null bitmaps
// there, seg2:65D0), so an ended record - lever, reel, sum digit or gear - keeps the
// last frame it drew until a WM_PAINT copies the housing over everything (seg2:6C2B).
#pragma once

#include <array>
#include <string>
#include <vector>

namespace dtvr {

struct SlotFrame {
    std::string name; // frame of sbs/slot0.sbs or sbs/slot<n>.sbs
    int x = 0, y = 0; // top-left in the housing bitmap
};

class SlotRecords {
public:
    // `reels`: one to five values 1..6 (from the game).
    explicit SlotRecords(const std::vector<int>& reels);
    SlotRecords(const SlotRecords&) = delete; // records point into this object's definitions
    SlotRecords& operator=(const SlotRecords&) = delete;
    // State 1 -> 2: the lever record starts (seg2:6A09); it advances in this pass.
    void pullLever();
    // seg2:6AA2 (a key in state 2): every reel ends its current turn and stops; ignored
    // while the lever definition DS:2292 still plays. True when it took effect.
    bool stopReels();
    // One pass: seg2:6DCB over all records with their callbacks.
    void pass();

    int state() const { return state_; }        // DS:A780: 1 lever, 2 turning, 3 stopping, 4 counted
    bool reelsStarted() const { return started_; } // seg2:7671 has run
    int sum() const { return sum_; }             // the reels stopped so far (seg2:7C0A)
    // What the window shows after this pass, in record order: the last frame each
    // record drew, also after it ended (seg2:6F69 skips ended records, nothing erases
    // them). No two slot records overlap, so the order does not change the picture.
    std::vector<SlotFrame> frames() const;

private:
    struct Definition { int delay = 0; std::vector<std::string> frames; };
    enum class Callback { None, ReelsStart, ReelStop };
    struct Record {
        Definition* def = nullptr;
        Definition* follow = nullptr;
        int countdown = 0, repetitions = 0, frame = 0;
        Callback callback = Callback::None;
        int argument = 0;
        int x = 0, y = 0;
        std::string last; // the last frame shown (kept after the record ends)
    };
    void advance(int index);
    void startReels();      // seg2:7671
    void reelStopped(int i); // seg2:7C0A

    std::vector<int> values_;
    std::array<Record, 11> records_{};
    // Definitions the records point to; the gears' delays change (seg2:7E3A).
    Definition leverPull_, leverBack_, spin_, gearL_, gearR_, gearU_, gear4_, gear3_;
    std::array<Definition, 7> faces_{}; // DS:2E3C[value]: walze_<v>h, walze_<v>
    int state_ = 1, sum_ = 0, stoppedReels_ = 0;
    bool started_ = false;
};

} // namespace dtvr
