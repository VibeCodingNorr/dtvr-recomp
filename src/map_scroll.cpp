// SPDX-License-Identifier: GPL-3.0-only
#include "map_scroll.hpp"

#include <algorithm>

namespace dtvr {
namespace {

constexpr int kTile = 16, kMargin = 32; // INFLATERECT(-0x20, -0x20) at seg2:2A1E

int floorDiv16(int v) { return v >= 0 ? v / kTile : -((-v + kTile - 1) / kTile); } // sar 4

} // namespace

TilePoint clampOrigin(TilePoint origin, const MapViewSize& view) {
    const int w = (view.width + 15) >> 4, h = (view.height + 15) >> 4;
    origin.x = std::max(0, std::min(origin.x, 0xFF - w));
    origin.y = std::max(0, std::min(origin.y, 0x100 - h));
    return origin;
}

TilePoint centredOrigin(int px, int py, const MapViewSize& view) {
    return clampOrigin(TilePoint{floorDiv16(px + 16 - view.width / 2), floorDiv16(py + 16 - view.height / 2)}, view);
}

bool edgeScroll(TilePoint& origin, int left, int top, int right, int bottom, const MapViewSize& view) {
    // seg6:30F8: with zoom (width & ~0x20) >> 5 tiles, otherwise (width >> 6) * 3.
    const int stepX = view.zoomed ? (view.width & ~0x20) >> 5 : (view.width >> 6) * 3;
    const int stepY = view.zoomed ? (view.height & ~0x20) >> 5 : (view.height >> 6) * 3;
    const int x0 = origin.x * kTile + kMargin, y0 = origin.y * kTile + kMargin;
    const int x1 = origin.x * kTile + view.width - kMargin, y1 = origin.y * kTile + view.height - kMargin;
    TilePoint moved = origin;
    if (top < y0) moved.y -= stepY;            // direction 1
    else if (bottom > y1) moved.y += stepY;    // 3
    else if (right > x1) moved.x += stepX;     // 2
    else if (left < x0) moved.x -= stepX;      // 4
    else return false;
    moved = clampOrigin(moved, view);
    const bool changed = moved.x != origin.x || moved.y != origin.y;
    origin = moved;
    return changed;
}

int surroundingsStep(const MapViewSize& view) {
    return view.width / 3 / kTile; // idiv 3, then cwd / and 0xF / sar 4: both toward zero
}

TilePoint surroundingsMove(TilePoint origin, int dx, int dy, const MapViewSize& view) {
    const int step = surroundingsStep(view);
    return clampOrigin(TilePoint{origin.x + dx * step, origin.y + dy * step}, view);
}

TilePoint surroundingsClick(TilePoint origin, int x, int y, const MapViewSize& view) {
    constexpr int kStrip = 0x3C;
    const int l = 0, t = 0, r = view.width, b = view.height; // DS:94DA / 94DC are 0 (seg5:13A3)
    const int step = surroundingsStep(view);
    TilePoint target = origin; // DS:96A2 / 96A4 equal the origin between loop passes
    if (y >= t + kStrip && y <= b - kStrip && x >= l && x <= l + kStrip) target.x = origin.x - step; // seg6:17C3
    if (y >= t && y <= t + kStrip && x >= l + kStrip && x <= r - kStrip) target.y = origin.y - step; // seg6:180D
    if (y >= t + kStrip && y <= b - kStrip && x >= r - kStrip && x <= r) target.x = origin.x + step; // seg6:1857
    if (y >= b - kStrip && y <= b && x >= l + kStrip && x <= r - kStrip) target.y = origin.y + step; // seg6:18A1
    return clampOrigin(target, view); // the loop at seg6:3D1C moves there and calls seg6:2B19(1)
}

} // namespace dtvr
