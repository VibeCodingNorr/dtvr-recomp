// SPDX-License-Identifier: GPL-3.0-only
// RECOMP ADDITION (no counterpart in rally.exe): the game controller in the on-screen
// keyboard (src/osk.hpp). The original joystick timing (80 ms tick seg2:8A30, three
// ticks of waiting, then every tick; src/pad_input.cpp) stays for every original
// window; the keyboard is a recomp element and reacts like a modern console keyboard:
//   HoldRepeat       a step at once on the press, then after a delay a repetition
//                    that speeds up while held (injected time, no SDL);
//   StickHysteresis  the driving stick as four directions with an engage and a lower
//                    release threshold (no chatter), a diagonal as its dominant axis;
//   OskPad           the merged physical pad state through the PadMap's actions to
//                    keyboard commands, each button acting on its press.
// Timing values and bindings in docs/formats.md "On-screen keyboard feel".
#pragma once

#include "pad_map.hpp"

#include <cstdint>
#include <vector>

namespace dtvr {

// The repetition curve (milliseconds; assumptions tuned by feel, not from rally.exe).
struct HoldRepeatTiming {
    std::uint32_t delay = 250; // press -> first repetition
    std::uint32_t first = 110; // interval of the first repetition
    std::uint32_t fast = 40;   // interval once fully accelerated
    std::uint32_t ramp = 1000; // time of repeating until `fast` is reached (linear)
};

class HoldRepeat {
public:
    explicit HoldRepeat(HoldRepeatTiming timing = {}) : timing_(timing) {}
    // Steps due at `now`: 1 on the press itself, then the repetitions (at most one per
    // call; a late call does not burst). Releasing resets the curve.
    int update(bool held, std::uint32_t now);
    void reset() { held_ = false; }
    // The interval after `repeating` ms of repetition.
    std::uint32_t interval(std::uint32_t repeating) const;

private:
    HoldRepeatTiming timing_;
    bool held_ = false;
    std::uint32_t repeatStart_ = 0; // when the first repetition was due
    std::uint32_t next_ = 0;        // when the next step is due
};

enum class OskDirection { None, Up, Down, Left, Right };

// Stick travel in -1 .. 1 (SDL axes: y grows downwards).
class StickHysteresis {
public:
    static constexpr float kEngage = 0.5f, kRelease = 0.35f;
    OskDirection update(float x, float y);
    void reset() { current_ = OskDirection::None; }
    OskDirection current() const { return current_; }

private:
    OskDirection current_ = OskDirection::None;
};

enum class OskCommand { Up, Down, Left, Right, Press, Erase, Shift, Space, CaretLeft, CaretRight, FocusDone, Accept, Cancel };

// One reading of all pads: physical buttons (padBit) and the driving stick's travel.
struct OskPadRaw {
    unsigned buttons = 0;
    float stickX = 0, stickY = 0;
};

class OskPad {
public:
    // Every frame (and on every controller event) while the keyboard has the input.
    // The first call after reset() takes what is held as already pressed: the button
    // that opened the keyboard does not type.
    std::vector<OskCommand> tick(const PadMap& map, const OskPadRaw& raw, std::uint32_t now);
    void reset();
    bool active() const { return active_; }

private:
    bool active_ = false;
    unsigned previous_ = 0;   // physical buttons of the last tick
    unsigned suppressed_ = 0; // held at the start, ignored until released
    bool directionSuppressed_ = false;
    OskDirection direction_ = OskDirection::None;
    StickHysteresis stick_;
    HoldRepeat move_, erase_, caretLeft_, caretRight_;
};

} // namespace dtvr
