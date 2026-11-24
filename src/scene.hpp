// SPDX-License-Identifier: GPL-3.0-only
// Animation records of the full-screen scenes (finish, victory; round 46): the
// records of seg2:6DCB / 74E4 with a position, a start frame (+0xC) and a
// definition of delay and frames, advanced once per 20 ms step.
#pragma once

#include "scene_record.hpp"
#include "sdl_assets.hpp"

#include <SDL.h>

#include <map>
#include <string>
#include <vector>

namespace dtvr {

constexpr Uint32 kSceneStepMs = 20; // seg6:0CDF(20) per pass

// seg2:6DCB (round 145, the rule of rounds 70 / 128): per pass `old = countdown--`,
// and when old <= 0 the countdown reloads the delay and the frame index grows. A new
// record starts with countdown 0 at its start frame (+0xC), so the scenes' loops,
// which step before they show, show frame first + 1 in pass 0 and keep every frame
// delay + 1 passes.
int sceneFrameIndex(const SceneRecord& record, int pass);
const char* sceneFrame(const SceneRecord& record, int pass);
// The pass at whose start the record stands on frame `frame` with countdown
// `countdown` (0 <= countdown <= delay), as tests like seg7:3CE4 see it.
int scenePassAt(const SceneRecord& record, int frame, int countdown);
// Passes until the definition has run once (its last frame has stayed delay + 1
// passes and the index wraps), when seg2:6DCB takes the follow-up.
int sceneLength(const SceneRecord& record);
// Draws a frame by name with its top-left corner at (x, y); false when it is missing.
bool drawSceneFrame(SDL_Renderer* renderer, const std::map<std::string, Texture>& frames, const char* name, int x,
                    int y);
// Copies `src` into `dst` at (x, y), leaving out palette index 0xFF (seg2:9497).
void blitKeyed(SDL_Surface* src, SDL_Surface* dst, int x, int y);

} // namespace dtvr
