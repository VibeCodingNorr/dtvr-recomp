// SPDX-License-Identifier: GPL-3.0-only
// Geometry of the original game window (round 21, docs/formats.md "Game window").
// SDL-free: the canvas resolutions of seg5:1227, the map rectangle and the
// per-resolution element table of seg6:1140.
#pragma once

#include "edition.hpp"

#include <array>
#include <vector>

namespace dtvr {

struct ScreenRect { int x = 0, y = 0, w = 0, h = 0; };

struct ScreenLayout {
    // The EXE whose geometry tables apply (the setup table differs, see setupGeometry).
    Edition edition = Edition::GermanRetail;
    int width = 640, height = 480;  // canvas DS:9DC2 x DS:9D94 (wider in the widescreen extension)
    int baseWidth = 640;            // the original canvas width: 640, 800 or 1024
    int artDir = 6;                 // seg5:1827: butt/<n>, bmp/<n> (6 = 640, 8 = 800, 1 = 1024)
    ScreenRect map;                 // "The Map" child: DS:9C84..9C88 x DS:9C86..9C8A
    // seg6:12FA: 0 = 1024, 1 = 800, 2 = 640 (the art and font sizes).
    int resolution() const { return baseWidth == 1024 ? 0 : baseWidth == 800 ? 1 : 2; }
    // Widescreen extension (recomp only): the pixels added to the original canvas width.
    int extra() const { return width - baseWidth; }
    // The original 4:3 canvas centred in the wide one: full-canvas pictures (title,
    // PLAYER, OPTION, PICT) keep their size and coordinates there.
    ScreenRect stage() const { return {extra() / 2, 0, baseWidth, height}; }
    // OUTLINE (seg1:AAD4) has the original square map window, centred in a wide map.
    ScreenRect overview;
    // Its client area inside the 8-pixel border of seg6:1A68 (seg6:1763), the size of
    // the drawn area DS:94DE / 94E0 (round 112).
    ScreenRect mapClient() const { return {map.x + 8, map.y + 8, map.w - 16, map.h - 16}; }
    ScreenRect viewport;            // drawn world area inside the map window (DS:94DA..94E0)
    // seg6:1140(index) -> (a, b) for the current resolution; meanings in docs.
    std::array<std::array<int, 2>, 10> elements{};
    ScreenRect portrait() const;    // index 2 (seg2:2DAD), size = bitmap + 16 (seg2:2EEC)
    ScreenRect playerInfo() const;  // index 9 position, index 3 packed size (seg5:60C6)
};

// seg5:1227: the canvas is 640 x 480, 800 x 600 or 1024 x 768; the largest one
// that fits the screen (or the requested width) is used.
ScreenLayout layoutForScreen(int screenWidth, int screenHeight, int requestedWidth = 0); // German
// The same with the canvases of an edition profile (src/edition.hpp).
ScreenLayout layoutForScreen(const EditionProfile& profile, int screenWidth, int screenHeight, int requestedWidth = 0);
ScreenLayout layoutForCanvas(int canvasWidth); // 640, 800 or 1024

// Widescreen extension (recomp only, not in rally.exe): the canvas is widened by
// `extra` pixels; the map window and its world view grow by them and the right
// column (portrait, hand stack, player info) moves along. Heights stay original.
ScreenLayout widenLayout(const ScreenLayout& base, int extra);
// The extra width (even) that gives the canvas the aspect of a display; 0 when the
// display is not wider than 4:3. The aspect is capped at 21:9. The wide canvas is
// scaled to the display's height without whole-number steps (DisplayControl).
int widescreenExtra(const ScreenLayout& base, int displayWidth, int displayHeight);

// Card window seg3:0887: one button per occupied hand slot, the highest slot first,
// in rows of a per-count column number, centred on the canvas.
struct CardButton { int slot = 0; ScreenRect rect; };
std::vector<CardButton> cardWindowLayout(const ScreenLayout& layout, const std::array<bool, 8>& occupied);

// Player setup window PLAYER (round 36): rectangles for a canvas width (DS:1BD4: per resolution ten
// rectangles, slot 7 first, then star and stop; DS:1CC4: eight name fields). The US retail
// EXE has them at R DS:1BD6 / 1CC6 with three 640 name fields 1 px apart (layout.cpp).
struct SetupGeometry {
    std::array<ScreenRect, 8> driver{};
    std::array<ScreenRect, 8> name{};
    ScreenRect start, stop;
};
SetupGeometry setupGeometry(int canvasWidth, Edition edition = Edition::GermanRetail);

} // namespace dtvr
