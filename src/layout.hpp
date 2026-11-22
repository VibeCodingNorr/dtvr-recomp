// SPDX-License-Identifier: GPL-3.0-only
// Geometry of the original game window (round 21, docs/formats.md "Game window").
// SDL-free: the canvas resolutions of seg5:1227, the map rectangle and the
// per-resolution element table of seg6:1140.
#pragma once

#include <array>
#include <vector>

namespace dtvr {

struct ScreenRect { int x = 0, y = 0, w = 0, h = 0; };

struct ScreenLayout {
    int width = 640, height = 480;  // canvas DS:9DC2 x DS:9D94
    int artDir = 6;                 // seg5:1827: butt/<n>, bmp/<n> (6 = 640, 8 = 800, 1 = 1024)
    ScreenRect map;                 // "The Map" child: DS:9C84..9C88 x DS:9C86..9C8A
    ScreenRect viewport;            // drawn world area inside the map window (DS:94DA..94E0)
    // seg6:1140(index) -> (a, b) for the current resolution; meanings in docs.
    std::array<std::array<int, 2>, 10> elements{};
    ScreenRect portrait() const;    // index 2 (seg2:2DAD), size = bitmap + 16 (seg2:2EEC)
    ScreenRect playerInfo() const;  // index 9 position, index 3 packed size (seg5:60C6)
};

// seg5:1227: the canvas is 640 x 480, 800 x 600 or 1024 x 768; the largest one
// that fits the screen (or the requested width) is used.
ScreenLayout layoutForScreen(int screenWidth, int screenHeight, int requestedWidth = 0);
ScreenLayout layoutForCanvas(int canvasWidth); // 640, 800 or 1024

// Card window seg3:0887: one button per occupied hand slot, the highest slot first,
// in rows of a per-count column number, centred on the canvas.
struct CardButton { int slot = 0; ScreenRect rect; };
std::vector<CardButton> cardWindowLayout(const ScreenLayout& layout, const std::array<bool, 8>& occupied);

// Player setup window PLAYER (round 36): rectangles for a canvas width (DS:1BD4: per resolution ten
// rectangles, slot 7 first, then star and stop; DS:1CC4: eight name fields).
struct SetupGeometry {
    std::array<ScreenRect, 8> driver{};
    std::array<ScreenRect, 8> name{};
    ScreenRect start, stop;
};
SetupGeometry setupGeometry(int canvasWidth);

} // namespace dtvr
