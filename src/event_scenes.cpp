// SPDX-License-Identifier: GPL-3.0-only
#include "event_scenes.hpp"

namespace dtvr {
namespace {

// Sturm seg8:1C01: definition on the stack (delay 6, frames DS:5F0E.. ST1-ST6),
// sbs\sturm.sbs (DS:5F3A), repetitions 4 (seg8:1D80), position town * 16 + (8, -0x1D)
// (seg8:1D47 / 1D4C).
const EventScene kStorm{"sturm.sbs", {"ST1", "ST2", "ST3", "ST4", "ST5", "ST6"}, 6, 4, 8, -0x1D};

} // namespace

const EventScene* eventSceneForNotice(int text) {
    if (text >= 5041 && text <= 5043) return &kStorm; // 5041 + random(2) after the animation (seg8:1DFD)
    return nullptr;
}

int eventScenePasses(const EventScene& scene) {
    return int(scene.frames.size()) * (scene.delay > 0 ? scene.delay : 1) * scene.repetitions;
}

const char* eventSceneFrame(const EventScene& scene, int pass) {
    if (scene.frames.empty()) return "";
    const int delay = scene.delay > 0 ? scene.delay : 1;
    return scene.frames[size_t(pass / delay) % scene.frames.size()];
}

} // namespace dtvr
