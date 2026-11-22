// SPDX-License-Identifier: GPL-3.0-only
// Scrolling of the map window as rally.exe does it (round 67): the origin DS:956A /
// 9568 in 16-pixel tiles is centred on a point (seg6:3064), kept inside the world
// (seg6:2B19) and moved by a large step when the driving vehicle comes near an edge
// (seg2:29EB -> seg6:30F8). No SDL. Evidence in docs/formats.md "Map scrolling".
#pragma once

namespace dtvr {

struct TilePoint { int x = 0, y = 0; };

// The visible world area DS:94DE / 94E0 in world pixels (map window - 16, halved
// with zoom) and the zoom flag DS:9C70.
struct MapViewSize { int width = 0, height = 0; bool zoomed = false; };

// seg6:2B19(1): x in [0, 0xFF - ceil(width / 16)], y in [0, 0x100 - ceil(height / 16)].
TilePoint clampOrigin(TilePoint origin, const MapViewSize& view);
// seg6:3064(px, py): origin = (p + 16 - size / 2) >> 4, then clamped.
TilePoint centredOrigin(int px, int py, const MapViewSize& view);
// seg2:29EB: when the vehicle rectangle (world pixels) leaves the view inflated by -32,
// one step (seg6:30F8) towards it, checked up, down, right, left; true when moved.
bool edgeScroll(TilePoint& origin, int left, int top, int right, int bottom, const MapViewSize& view);

} // namespace dtvr
