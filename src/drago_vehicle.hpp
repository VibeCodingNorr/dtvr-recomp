// SPDX-License-Identifier: GPL-3.0-only
// Dr. Drago's own vehicle on the map (round 140): rally.exe moves him as a sprite of
// his own that follows the vehicle he rides with (seg2:9F61 from the movement tick
// seg2:1C71, the step seg2:A0A5 / A5AA, the frames seg2:AF38). Form 0 rides a bicycle
// (a boat on water, sbs/F_BIKE.sbs), form 1 a car with rocket flames that hovers over
// sea routes (F_CAR.sbs), form 2 hangs under a blimp and flies straight (F_BLIMP.sbs).
// Positions are world pixels like the vehicles' +0x16 / +0x18. A form change sends
// the old vehicle away along the roads to a field outside the view and brings the new
// one back (seg2:9804 / 98D0, round 141); Hau ab drives him to the new holder
// (seg2:B4E3). The game waits while he travels (DS:9CFE, DS:96A0). No SDL.
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
    int back() const;                   // the newest entry (DS:2C[DS:28 - 1])
    void cutLoop();                     // seg2:4EE7: skip to a later copy of the oldest field

private:
    std::array<int16_t, kSize> buffer_{};
    int head_ = 0, tail_ = 0, count_ = 0;
};

// The direction from (x, y) towards (tx, ty), seg2:AE77: 0 there, 1 up, 2 up-right,
// 3 right ... 8 up-left.
int dragoDirection(int tx, int ty, int x, int y);

using DragoCellProperties = std::function<uint16_t(int cellX, int cellY)>;

// The view in world pixels: DS:956A / 9568 × 16 and DS:94DE − 94DA / 94E0 − 94DC.
struct DragoView {
    int left = 0, top = 0, width = 0, height = 0;
};

// What a pass reads: the holder (+0x16 / +0x18, field +0x3C), the board, the cell
// properties (seg2:3260) and the view.
struct DragoContext {
    int holderX, holderY, holderField;
    const Board& board;
    const DragoCellProperties& cell;
    DragoView view;
};

// seg2:454E: the start field of a form change. A depth-first search over the links
// from the holder's field, each field trying the directions startSide, +1, +2, +3
// (mod 4; startSide = random(3)), takes the first field whose cell, inflated by three
// cells, misses the view in cells (seg2:46C8); -1 when there is none.
int dragoStartField(const Board& board, int holderField, int startSide, const DragoView& view);

class DragoVehicle {
public:
    using CellProperties = DragoCellProperties;
    struct Sprite {
        const char* frame;
        int x, y; // before the sprite list's (-16, -16) of seg2:3EB0
    };

    // seg2:9C97 / 9CF9: three cells beside the holder at (holderX, holderY) on its
    // field (seg2:4C7D), on the side behind its facing (1 up .. 4 left) or the next
    // side clockwise with a link.
    void place(int form, int holderX, int holderY, int holderFacing, int holderField, const Board& board,
               const CellProperties& cell);
    void reset() { placed_ = false; travel_ = Travel::None; trail_.clear(); }
    bool placed() const { return placed_; }
    int form() const { return form_; }  // DS:9CFD: the form drawn
    void setForm(int form) { form_ = form; }
    DragoTrail& trail() { return trail_; }
    const DragoTrail& trail() const { return trail_; }

    // seg2:9804(newForm): the start field (startSide = the game's random(3)) and the
    // route from the holder to it; false when there is none (the form then switches
    // in place).
    bool startFormChange(int newForm, int startSide, const DragoContext& ctx);
    // seg2:B4E3: Hau ab, the route from the old holder's field to ctx.holderField.
    bool startHandOver(int fromField, const DragoContext& ctx);
    // DS:9CFE: travelling; the game waits until it ends (DS:96A0 = 1).
    bool travelling() const { return travel_ != Travel::None; }

    // One pass of seg2:9F61 and the height of the car's hop (seg2:AF38).
    void movePass(const DragoContext& ctx);
    // One pass of the frame countdowns (seg2:AF38 after drawing).
    void animatePass();
    // The frames drawn now: the main record, then the rockets or the blimp.
    std::vector<Sprite> sprites() const;

    int x() const { return x_; }
    int y() const { return y_; }
    int direction() const { return dir_; }
    int facing() const { return face_; }

private:
    // DS:9566 with DS:94EC: leaving with a new form pending, coming back, Hau ab.
    enum class Travel { None, Leaving, Returning, HandOver };
    struct Running {
        const DragoAnimation* record = nullptr;
        int frame = 0, countdown = 0;
        void set(const DragoAnimation* r);
        void advance();
        const char* name() const;
    };
    void step(const DragoContext& ctx);
    void follow(int dx, int dy, const DragoContext& ctx);
    bool travelStep(int dx, int dy, const DragoContext& ctx); // the DS:9CFE branches of seg2:A0A5 / A8BB
    void complete(const DragoContext& ctx);                   // seg2:98D0
    bool route(int from, int to, const Board& board);         // seg2:9996
    bool visible(const DragoView& view) const;                // seg2:9ECB
    int towardsField(int field, const Board& board) const;    // seg2:ADAB
    void setMain(const DragoAnimation* record);               // seg2:AC89
    bool checkLanding(int dir, const CellProperties& cell);   // seg2:AB49

    bool placed_ = false;
    int form_ = 0;
    int x_ = 0, y_ = 0;
    int dir_ = 0;   // DS:3D78
    int face_ = 0;  // DS:3D76
    int steps_ = 3; // DS:3D8C
    int hop_ = 0;   // DS:9D2F: 1 rising, 2 hovering, 3 landing
    int height_ = 0; // DS:9D30
    int landX_ = 0, landY_ = 0; // DS:9518 / 9516
    Travel travel_ = Travel::None;
    int pendingForm_ = -1; // DS:94EC
    int startField_ = -1;  // DS:94D8
    Running main_, second_;
    DragoTrail trail_;
};

} // namespace dtvr
