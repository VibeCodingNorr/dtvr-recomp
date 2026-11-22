// SPDX-License-Identifier: GPL-3.0-only
// Animation records of the full-screen scenes (finish, victory; round 46): the
// records of seg2:6DCB / 74E4 with a position, a start frame (+0xC) and a
// definition of delay and frames, advanced once per 20 ms step.
#pragma once

#include "sdl_assets.hpp"

#include <SDL.h>

#include <map>
#include <string>
#include <vector>

namespace dtvr {

struct SceneRecord {
    int x = 0, y = 0, first = 0, delay = 1;
    std::vector<const char*> frames;
};

constexpr Uint32 kSceneStepMs = 20; // seg6:0CDF(20) per pass

// The frame shown after `step` passes (seg2:6DCB: next frame when the delay expires).
const char* sceneFrame(const SceneRecord& record, int step);
// Draws a frame by name with its top-left corner at (x, y); false when it is missing.
bool drawSceneFrame(SDL_Renderer* renderer, const std::map<std::string, Texture>& frames, const char* name, int x,
                    int y);
// Copies `src` into `dst` at (x, y), leaving out palette index 0xFF (seg2:9497).
void blitKeyed(SDL_Surface* src, SDL_Surface* dst, int x, int y);

} // namespace dtvr
