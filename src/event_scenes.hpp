// SPDX-License-Identifier: GPL-3.0-only
// Event animations on the map (round 69): the seg8 event handlers play an animation
// record of their SBS set at the event place before the message appears (seg2:6D1B,
// loop seg8:6E0F: one pass per 20 ms, seg2:6DCB advances the record). No SDL.
// Evidence in docs/formats.md "Event animations".
#pragma once

#include <vector>

namespace dtvr {

struct EventScene {
    const char* file;                 // in the sbs directory
    std::vector<const char*> frames;  // the animation definition's frame names
    int delay = 1;                    // passes per frame (definition word +0)
    int repetitions = 1;              // record +0xA
    int dx = 0, dy = 0;               // added to the place's cell * 16
};

// The animation of an event message text, nullptr when it has none (yet).
const EventScene* eventSceneForNotice(int text);
// Drawn passes until the record is freed (seg2:6DCB: each frame stays delay + 1 passes).
int eventScenePasses(const EventScene& scene);
// The frame drawn in pass `pass` (0-based; the record advances before drawing).
const char* eventSceneFrame(const EventScene& scene, int pass);

} // namespace dtvr
