// SPDX-License-Identifier: GPL-3.0-only
#include "vehicle_anim.hpp"

namespace dtvr {

int vehicleFrame(uint32_t passes, int frames, int delay) {
    if (frames <= 0) return 0;
    // Countdown 0 after the reset: pass 0 leaves it at -1, pass 1 reloads the delay
    // and advances; from then on every delay + 2 passes (delay .. -1).
    if (passes < 2) return 0;
    const uint32_t advances = (passes - 2) / uint32_t(delay + 2) + 1;
    return int(advances % uint32_t(frames));
}

} // namespace dtvr
