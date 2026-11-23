// SPDX-License-Identifier: GPL-3.0-only
// The wooden window border of rally.exe (round 112): the non-client handlers
// seg6:1A68 (8 pixels, frames B1-B8) and seg6:1E83 (16 pixels, BS1-BS8) of
// assorted.sbs. WM_NCCALCSIZE insets the client area by the thickness on every
// side; WM_NCPAINT tiles the top (B2) and bottom (B7) edges from x = t while
// x < width - t, the left (B4) and right (B5) edges from y = t while y < height - t,
// then draws the corners B1, B3, B6 and B8. Evidence in docs/formats.md
// "Window border".
#pragma once

#include <SDL.h>

#include <functional>
#include <string>

namespace dtvr {

using SpriteLookup = std::function<SDL_Texture*(const std::string&)>;

// 8 for seg6:1A68, 16 for seg6:1E83.
enum class BorderSize { Thin = 8, Thick = 16 };

// The client rectangle of a window with this border (WM_NCCALCSIZE).
SDL_Rect borderClient(const SDL_Rect& window, BorderSize size);

// Draws the border pieces into the outer `size` pixels of `window` (WM_NCPAINT).
void drawWindowBorder(SDL_Renderer* renderer, const SpriteLookup& sprite, const SDL_Rect& window, BorderSize size);

} // namespace dtvr
