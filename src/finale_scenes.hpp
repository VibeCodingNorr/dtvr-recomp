// SPDX-License-Identifier: GPL-3.0-only
// The animation records of the finish ZIEL (seg7:2628) and the victory SIEG
// (seg7:7250) pass by pass (no SDL): the record table DS:30 as their set-up writes it
// (definition, position, start frame +0xC, countdown +0x8, repetitions +0xA and the
// draw flag +0xD), the frame fills of seg7:1E3C, and the per-pass changes of their
// loops before seg2:6DCB steps the records. RecordPainter draws the result.
// Evidence in docs/formats.md "Finish sequence" and "Victory sequence".
#pragma once

#include "record_paint.hpp"
#include "scene_record.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace dtvr {

// One record of DS:30 as seg2:6DCB steps it. ZIEL and SIEG set no follow-up (+0x4)
// and no callback (+0xE), so a definition whose repetitions have run out is cleared
// and the record is no longer drawn.
struct SceneTrack {
    const SceneRecord* def = nullptr; // +0x0
    int x = 0, y = 0;                 // +0x14, +0x16
    int index = 0;                    // +0xC
    int countdown = 0;                // +0x8
    int repeats = 0;                  // +0xA
    bool keyed = false;               // +0xD bit 0
    // seg2:6DCB: old = countdown--; when old <= 0 the countdown reloads the delay and
    // the index grows; at the frame count it wraps to 0 and a repetition is used up.
    void step();
    // The record as seg2:6F69 / seg6:41A2 read it (frame name of definition[index]).
    PaintRecord paint() const;
};

// seg7:1E3C(file slot, x, y, backdrop): every frame of the file takes the backdrop's
// pixels under (x, y) where it holds 0xFF (seg7:1CE0).
struct SceneFill {
    size_t slot;
    int x, y;
};

class FinishScene {
public:
    static constexpr size_t kRecords = 16; // set by seg7:2C55..3531 (20 allocated, seg7:4D95)
    static constexpr size_t kCar = 8;
    static constexpr int kCarGone = -0xA0; // seg7:3546: cmp x, 0xFF60 / jge
    // DS:4BE8 in slot order under butt/sbs/ziel; slot 8 is the driver's car
    // `%cZielCar.sbs` (DS:4CD3, letter of seg2:2D43, seg7:2827).
    static std::vector<std::string> files(char driverLetter);
    static const std::vector<SceneFill>& fills(); // seg7:2ADE..2C1C
    FinishScene();
    // seg7:3542: the car has left the scene, the loop destroys the window (seg7:3555)
    // before this pass draws anything.
    bool carGone() const;
    // seg7:3564: the car 2 pixels left (seg7:356E also clears its +0x36, which the
    // caller does on its RecordPainter), then seg2:6DCB (seg7:35B6).
    void pass();
    int passes() const { return passes_; } // [bp-0x6], counted up to 0x7530
    std::vector<PaintRecord> records() const;
    const SceneTrack& track(size_t i) const { return tracks_[i]; }

private:
    std::vector<SceneTrack> tracks_;
    int passes_ = 0;
};

class VictoryScene {
public:
    static constexpr size_t kRecords = 11; // slots 0-6, 9 and 10 set (seg7:7516..7B86); 7, 8 stay empty
    static constexpr size_t kWinner = 9, kSpeaker = 10;
    static constexpr int kPassLimit = 5000; // seg7:7D4E: VK_RETURN after pass 0x1388
    // DS:5E6A under butt/sbs in slot order, empty names left out (seg7:7FF6): the
    // scene files, the winner's car (slot 7) and `x_winner.sbs` of the places (8-10).
    static std::vector<std::string> files(const std::vector<int>& ranking);
    static const std::vector<SceneFill>& fills(); // seg7:748E..750E
    // The definition of the winner record for a driver slot (DS:5D6C[slot + 1]) and its
    // x = 0xE4 - DS:5D8E[slot + 1] (seg7:795C..796E).
    static const SceneRecord& winner(int slot);
    // The second and third places' standing frame `<L>W00` (DS:5D9C) and where
    // seg7:1F52 stamps it: x = centre - width / 2, y = foot - height (seg7:79CB / 7A6C).
    static std::string placeFrame(int slot);
    static void placePosition(size_t place, int width, int height, int& x, int& y);
    // `winnerHeight`: the height of the winner's first frame (seg7:7994..79AB).
    VictoryScene(int winnerSlot, int winnerHeight);
    // seg7:7BFF..7C46 for the speaker (record 10, intro\redet.sbs) from the voice
    // SIEG.WAV, then seg2:6DCB (seg7:7C9F). `talking`: seg1:4071; `cue`: seg1:414C;
    // `playing`: seg1:3E64.
    void pass(bool talking, bool cue, bool playing);
    int passes() const { return passes_; } // [bp-0x4]
    bool done() const { return passes_ >= kPassLimit; }
    std::vector<PaintRecord> records() const;
    const SceneTrack& track(size_t i) const { return tracks_[i]; }

private:
    std::vector<SceneTrack> tracks_;
    int passes_ = 0;
};

} // namespace dtvr
