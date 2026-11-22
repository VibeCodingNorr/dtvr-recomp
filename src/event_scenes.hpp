// SPDX-License-Identifier: GPL-3.0-only
// Event animations on the map (rounds 69-73): the seg8 event handlers play animation
// records of their SBS set at the event place before the message appears (seg2:6D1B,
// loop seg8:6E0F: one pass per 20 ms, seg2:6DCB advances the record). Rain, snow and
// flood first fly in from a corner (seg8:517B, seg8:5243). No SDL.
// Evidence in docs/formats.md "Event animations".
#pragma once

#include <vector>

namespace dtvr {

// One animation definition played by a record until it is freed.
struct ScenePhase {
    std::vector<const char*> frames;  // the definition's frame names
    int delay = 1;                    // definition word +0
    int repetitions = 0;              // record +0xA
};

struct EventScene {
    const char* file;                 // in the sbs directory
    std::vector<ScenePhase> phases;   // played one after the other
    int dx = 0, dy = 0;               // added to the place's cell * 16
    int waitBefore = 0, waitAfter = 0; // seg6:0F60(ms) around the loop (assumed ms)
    bool quakeCracks = false;          // positions from the earthquake tables instead
    bool flight = false;               // phase 0 flies from a corner to the target
};

struct ScenePoint { int x = 0, y = 0; };
// One drawn pass: the frame and its offset from the scene positions.
struct SceneStep { const char* frame; ScenePoint offset; };

// The animation of an event message text, nullptr when it has none (yet).
const EventScene* eventSceneForNotice(int text);
// World pixel positions of the scene's records for an event place (map cell).
std::vector<ScenePoint> eventScenePositions(const EventScene& scene, int cellX, int cellY);
// Every drawn pass in order. `corner` is the random(3) of seg8:517B and
// `viewWidth` / `viewHeight` the visible world area for the flight's start.
std::vector<SceneStep> eventSceneTimeline(const EventScene& scene, int corner, int viewWidth, int viewHeight);

} // namespace dtvr
