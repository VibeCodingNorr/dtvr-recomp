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
struct ScenePoint { int x = 0, y = 0; };
struct ScenePhase {
    std::vector<const char*> frames;  // the definition's frame names
    int delay = 1;                    // definition word +0
    int repetitions = 0;              // record +0xA
    enum class Motion { None, Flight, Descend } motion = Motion::None;
    int waitBefore = 0, waitAfter = 0; // seg6:0F60(ms) around this phase (assumed ms)
    bool fixed = false;                // drawn at `at` (world pixels) instead of the place
    ScenePoint at{};
    bool recentre = false;             // seg6:3064 before the phase: on `camera`, which is
    bool cameraFixed = false;          // absolute, or added to the place's cell * 16
    ScenePoint camera{};
};

// seg1:2D40(7, "<data>\snd\<file>", loops) of the handler, played with the first frame
// of `phase` (round 135, docs/formats.md "Event sounds").
struct SceneSound { int phase = 0; const char* file = ""; int loops = 0; };
struct EventScene {
    const char* file;                 // in the sbs directory
    std::vector<ScenePhase> phases;   // played one after the other
    int dx = 0, dy = 0;               // added to the place's cell * 16
    int waitBefore = 0, waitAfter = 0; // seg6:0F60(ms) around the whole loop
    bool quakeCracks = false;          // positions from the earthquake tables instead
    bool nessieLake = false;           // the place is the lake cell of the variant's index
    std::vector<SceneSound> sounds;    // WAVs of butt/snd on channel 7
};

// One pass of 20 ms: the frame (nullptr while waiting) at world pixel positions,
// and a new map centre when `recentre` is set.
struct SceneStep {
    const char* frame = nullptr;
    std::vector<ScenePoint> at;
    bool recentre = false;
    ScenePoint camera{};
    const char* sound = nullptr;       // a WAV starts with this pass (SceneSound)
    int soundLoops = 0;
};

// The animation of an event message text, nullptr when it has none (yet).
const EventScene* eventSceneForNotice(int text);
// World pixel positions of the scene's records for an event place (map cell).
std::vector<ScenePoint> eventScenePositions(const EventScene& scene, int cellX, int cellY);
// Every pass in order for an event place. `variant` is the random(3) corner of
// seg8:517B or Nessie's place index;
// `viewWidth` / `viewHeight` the visible world area and `viewTop` its top world pixel
// row when the scene starts (zoomed in and centred already).
std::vector<SceneStep> eventSceneTimeline(const EventScene& scene, int cellX, int cellY, int variant, int viewWidth,
                                          int viewHeight, int viewTop);

} // namespace dtvr
