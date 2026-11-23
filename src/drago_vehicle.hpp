// SPDX-License-Identifier: GPL-3.0-only
// Dr. Drago's own vehicle on the map (round 140): rally.exe moves him as a sprite of
// his own that follows the vehicle he rides with (seg2:9F61 from the movement tick
// seg2:1C71, the step seg2:A0A5 / A5AA, the frames seg2:AF38). Form 0 rides a bicycle
// (a boat on water, sbs/F_BIKE.sbs), form 1 a car with rocket flames that hovers over
// sea routes (F_CAR.sbs), form 2 hangs under a blimp and flies straight (F_BLIMP.sbs).
// Positions are world pixels like the vehicles' +0x16 / +0x18. The form change's drive
// from outside the view (seg2:9804, DS:9CFE) is not part of this yet. No SDL.
// Evidence in docs/formats.md "Dr. Drago's own vehicle on the map".
#pragma once

#include "board.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace dtvr {

// An animation record of the data segment: delay at +0, frame count at +3, frame
// names (far pointers) from +4.
struct DragoAnimation {
    int delay;
    std::vector<const char*> frames;
};

// The trail DS:2C: a ring of 1000 field numbers, head DS:28, tail DS:2A, count DS:26.
class DragoTrail {
public:
    static constexpr int kSize = 1000;
    void clear();                       // seg2:4CEB
    bool push(int field);               // seg2:4D3F: not when it repeats the last one
    bool pop();                         // seg2:4D9F: drop the oldest
    int front() const;                  // the oldest entry (DS:2C[DS:2A])
    int count() const { return count_; } // seg2:4EC4
    bool empty() const { return head_ == tail_; }
    bool oneLeft() const;               // seg2:4E5F
    void cutLoop();                     // seg2:4EE7: skip to a later copy of the oldest field

private:
    std::array<int16_t, kSize> buffer_{};
    int head_ = 0, tail_ = 0, count_ = 0;
};

// The direction from (x, y) towards (tx, ty), seg2:AE77: 0 there, 1 up, 2 up-right,
// 3 right ... 8 up-left.
int dragoDirection(int tx, int ty, int x, int y);

class DragoVehicle {
public:
    using CellProperties = std::function<uint16_t(int cellX, int cellY)>;
    struct Sprite {
        const char* frame;
        int x, y; // before the sprite list's (-16, -16) of seg2:3EB0
    };

    // seg2:9C97 / 9CF9: three cells beside the holder at (holderX, holderY) on its
    // field (seg2:4C7D), on the side behind its facing (1 up .. 4 left) or the next
    // side clockwise with a link.
    void place(int form, int holderX, int holderY, int holderFacing, int holderField, const Board& board,
               const CellProperties& cell);
    void reset() { placed_ = false; trail_.clear(); }
    bool placed() const { return placed_; }
    DragoTrail& trail() { return trail_; }
    const DragoTrail& trail() const { return trail_; }

    // One pass of seg2:9F61 and the height of the car's hop (seg2:AF38).
    void movePass(int form, int holderX, int holderY, const Board& board, const CellProperties& cell);
    // One pass of the frame countdowns (seg2:AF38 after drawing).
    void animatePass();
    // The frames drawn now: the main record, then the rockets or the blimp.
    std::vector<Sprite> sprites(int form) const;

    int x() const { return x_; }
    int y() const { return y_; }
    int direction() const { return dir_; }
    int facing() const { return face_; }

private:
    struct Running {
        const DragoAnimation* record = nullptr;
        int frame = 0, countdown = 0;
        void set(const DragoAnimation* r);
        void advance();
        const char* name() const;
    };
    void step(int form, int holderX, int holderY, const Board& board, const CellProperties& cell);
    void follow(int form, int dx, int dy, int holderX, int holderY, const Board& board);
    int towardsField(int field, const Board& board) const; // seg2:ADAB
    void setMain(const DragoAnimation* record);           // seg2:AC89
    bool checkLanding(int dir, const CellProperties& cell); // seg2:AB49

    bool placed_ = false;
    int x_ = 0, y_ = 0;
    int dir_ = 0;   // DS:3D78
    int face_ = 0;  // DS:3D76
    int steps_ = 3; // DS:3D8C
    int hop_ = 0;   // DS:9D2F: 1 rising, 2 hovering, 3 landing
    int height_ = 0; // DS:9D30
    int landX_ = 0, landY_ = 0; // DS:9518 / 9516
    Running main_, second_;
    DragoTrail trail_;
};

} // namespace dtvr
