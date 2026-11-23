// SPDX-License-Identifier: GPL-3.0-only
// The vehicles' frame animation (round 124). Every idle pass of the game (seg2:1C37,
// 20 ms) calls seg2:4F7D -> seg2:16C1, which draws every vehicle in play with frame
// +0x2C of its animation +0x20 and then counts +0x1E down: when the old value was
// negative it reloads the definition's delay (4) and advances the frame (four
// frames, seg2:1941-19BD). The animation is set at the start (seg2:5142: mode 2, or
// mode 0 at sea), when a step starts (seg2:1EC0: mode 1, mode 0 on a sea route) and
// at the arrival (seg2:2101: mode 2); mode 2 uses the same frames ?up1-4 ... as mode 1
// (table DS:1994), so standing cars animate too. seg2:2B30 resets frame and countdown
// only when the animation changes. No SDL. Evidence in docs/formats.md "Vehicles".
#pragma once

#include <cstdint>

namespace dtvr {

// Frame index (0-based) drawn `passes` idle passes after the animation was set.
int vehicleFrame(uint32_t passes, int frames = 4, int delay = 4);

class VehicleAnimation {
public:
    // mode 0 plane, 1 driving, 2 standing (DS:1994 column); facing 1-4.
    static int key(int mode, int facing) { return mode * 8 + facing; }
    // seg2:2B30: a different animation starts at frame 0 with the countdown 0.
    void select(int animation, uint32_t pass) {
        if (animation != key_) { key_ = animation; start_ = pass; }
    }
    int frame(uint32_t pass) const { return vehicleFrame(pass - start_); }

private:
    int key_ = -1;
    uint32_t start_ = 0;
};

} // namespace dtvr
