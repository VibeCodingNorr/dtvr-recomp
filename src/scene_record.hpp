// SPDX-License-Identifier: GPL-3.0-only
// The definition of an animation record of the full-screen scenes (no SDL): position,
// start frame (+0xC), delay and frame names, as the definitions in the data segment.
#pragma once

#include <vector>

namespace dtvr {

struct SceneRecord {
    int x = 0, y = 0, first = 0, delay = 1;
    std::vector<const char*> frames;
};

} // namespace dtvr
