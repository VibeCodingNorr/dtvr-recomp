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

} // namespace dtvr
