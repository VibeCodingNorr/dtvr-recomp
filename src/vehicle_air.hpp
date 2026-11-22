// SPDX-License-Identifier: GPL-3.0-only
// The vehicles' plane transformation on sea routes (round 85): player record +0x2D
// (0 car, 1 rising, 2 flying, 3 landing) and +0x2E (height 0..0x18). A step towards a
// seaway cell (bits road + sea route, seg2:1EB5) chooses the plane frames ?f… and
// starts the rise (seg2:2B30 -> seg2:27EE); while airborne a land field (bits field +
// road) three cells ahead starts the landing (seg2:2866); the height changes by 2 per
// tick (seg2:1793 / 17EE). No SDL. Evidence in docs/formats.md "Planes".
#pragma once

#include <cstdint>

namespace dtvr {

struct VehicleAir {
    int state = 0;  // +0x2D
    int height = 0; // +0x2E: the frame is drawn this much higher (seg2:1878)
    bool plane() const { return state != 0; }
};

// A vehicle at rest on a field cell with these properties: flying over a sea field
// (field + water, 0x09), else on the ground.
VehicleAir restingAir(uint16_t fieldCell);
// One movement tick. `stepStart`: a step begins towards the cell `ahead`; `moving`:
// the vehicle moves this tick with `ahead3` the cell three steps ahead.
void airTick(VehicleAir& air, bool stepStart, uint16_t ahead, bool moving, uint16_t ahead3);

} // namespace dtvr
