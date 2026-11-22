// SPDX-License-Identifier: GPL-3.0-only
#include "layout.hpp"

#include <stdexcept>

namespace dtvr {
namespace {

// seg6:1140: ten (a, b) pairs per resolution, as stored on the stack there.
constexpr std::array<std::array<int, 2>, 10> k640 = {{
    {7, 5}, {480, 100}, {510, 16}, {0x8A87, 329}, {0, 140}, {400, 360}, {50, 10}, {10, 10}, {185, 60}, {481, 329}}};
constexpr std::array<std::array<int, 2>, 10> k800 = {{
    {30, 20}, {600, 200}, {635, 16}, {0xB0A0, 416}, {0, 154}, {500, 400}, {100, 20}, {15, 15}, {200, 70}, {595, 416}}};
constexpr std::array<std::array<int, 2>, 10> k1024 = {{
    {150, 50}, {800, 250}, {815, 32}, {0xDCC8, 537}, {0, 215}, {600, 550}, {200, 50}, {20, 20}, {220, 85}, {764, 537}}};

} // namespace

ScreenRect ScreenLayout::portrait() const {
    const int side = width == 1024 ? 102 : width == 800 ? 80 : 64; // bmp/<n>/p1/*.bmp
    return {elements[2][0], elements[2][1], side + 16, side + 16};
}

ScreenRect ScreenLayout::playerInfo() const {
    const int packed = elements[3][0];
    return {elements[9][0], elements[9][1], packed >> 8 & 0xFF, packed & 0xFF};
}

ScreenLayout layoutForCanvas(int canvasWidth) {
    ScreenLayout l;
    switch (canvasWidth) {
    case 640: l.width = 640; l.height = 480; l.artDir = 6; l.elements = k640; break;
    case 800: l.width = 800; l.height = 600; l.artDir = 8; l.elements = k800; break;
    case 1024: l.width = 1024; l.height = 768; l.artDir = 1; l.elements = k1024; break;
    default: throw std::invalid_argument("Aufloesung 640, 800 oder 1024");
    }
    // seg5:134A: margin 16 (32 from 1024 wide); the map is square and its bottom
    // edge is (height - margin) rounded down to 16.
    const int margin = l.width >= 1024 ? 32 : 16;
    const int bottom = (l.height - margin) & ~0xF;
    l.map = {margin, margin, bottom - margin, bottom - margin};
    l.viewport = {0, 0, l.map.w - 16, l.map.h - 16}; // seg5:13AF: DS:94DE/94E0
    return l;
}

ScreenLayout layoutForScreen(int screenWidth, int screenHeight, int requestedWidth) {
    if (requestedWidth) return layoutForCanvas(requestedWidth);
    static constexpr int kWidths[3] = {640, 800, 1024}, kHeights[3] = {480, 600, 768};
    int chosen = 640;
    for (int i = 0; i < 3; ++i)
        if (screenWidth >= kWidths[i] && screenHeight >= kHeights[i]) chosen = kWidths[i];
    return layoutForCanvas(chosen);
}

std::vector<CardButton> cardWindowLayout(const ScreenLayout& layout, const std::array<bool, 8>& occupied) {
    // seg3:089B: card size by seg6:12FA (0 = 1024, 1 = 800, 2 = 640) and columns /
    // rows by the number of cards; 10 pixels between cards.
    static constexpr int kWidth[3] = {0x82, 0x66, 0x52}, kHeight[3] = {0xBB, 0x91, 0x75};
    static constexpr int kColumns[9] = {0, 1, 2, 3, 4, 3, 3, 4, 4}, kRows[9] = {0, 1, 1, 1, 1, 2, 2, 2, 2};
    const int res = layout.width == 1024 ? 0 : layout.width == 800 ? 1 : 2;
    int count = 0;
    for (bool o : occupied) count += o;
    std::vector<CardButton> out;
    if (count == 0) return out; // seg3:0922: no window
    const int w = kWidth[res], h = kHeight[res];
    const int left = (layout.width - (kColumns[count] * (w + 10) - 10)) / 2;
    const int top = (layout.height - (kRows[count] * (h + 10) - 10)) / 2;
    int x = left, y = top, column = 0;
    for (int slot = 7; slot >= 0; --slot) { // seg3:09AE
        if (!occupied[size_t(slot)]) continue;
        out.push_back({slot, ScreenRect{x & ~1, y & ~1, w, h}});
        if (kColumns[count] - 1 <= column) { column = 0; x = left; y += h + 10; } // seg3:0A41
        else { ++column; x += w + 10; }
    }
    return out;
}

namespace {

// DS:1BD4 (x, y, w, h), 10 per resolution 1024 / 800 / 640: slots 7..0, star, stop.
constexpr int kButtons[3][10][4] = {
    {{534, 507, 116, 116}, {534, 377, 116, 116}, {534, 247, 116, 116}, {534, 117, 116, 116},
     {98, 507, 116, 116}, {98, 377, 116, 116}, {98, 247, 116, 116}, {98, 117, 116, 116},
     {786, 670, 105, 80}, {41, 670, 105, 80}},
    {{417, 397, 91, 91}, {417, 295, 91, 91}, {417, 194, 91, 91}, {417, 92, 91, 91},
     {76, 397, 91, 91}, {76, 295, 91, 91}, {76, 194, 91, 91}, {76, 92, 91, 91},
     {614, 524, 82, 63}, {32, 524, 82, 63}},
    {{334, 317, 73, 73}, {334, 236, 73, 73}, {334, 155, 73, 73}, {334, 73, 73, 73},
     {61, 317, 73, 73}, {61, 236, 73, 73}, {61, 155, 73, 73}, {61, 73, 73, 73},
     {491, 420, 66, 50}, {26, 420, 66, 50}}};
// DS:1CC4: name fields, 8 per resolution, slots 7..0.
constexpr int kNames[3][8][4] = {
    {{660, 507, 220, 35}, {660, 377, 220, 35}, {660, 247, 220, 35}, {660, 117, 220, 35},
     {224, 507, 220, 35}, {224, 377, 220, 35}, {224, 247, 220, 35}, {224, 117, 220, 35}},
    {{521, 397, 180, 30}, {521, 295, 180, 30}, {521, 194, 180, 30}, {521, 92, 180, 30},
     {183, 397, 180, 30}, {183, 295, 180, 30}, {183, 194, 180, 30}, {183, 92, 180, 30}},
    {{416, 317, 160, 26}, {416, 235, 160, 26}, {416, 154, 160, 26}, {416, 74, 160, 26},
     {148, 317, 160, 26}, {148, 236, 160, 26}, {148, 155, 160, 26}, {148, 73, 160, 26}}};
ScreenRect rect(const int (&r)[4]) { return {r[0], r[1], r[2], r[3]}; }

} // namespace

SetupGeometry setupGeometry(int canvasWidth) {
    const int res = canvasWidth == 1024 ? 0 : canvasWidth == 800 ? 1 : 2; // seg6:12FA
    SetupGeometry g;
    for (int k = 0; k < 8; ++k) {
        g.driver[size_t(7 - k)] = rect(kButtons[res][k]); // seg1:0696: slot 7 first
        g.name[size_t(7 - k)] = rect(kNames[res][k]);
    }
    g.start = rect(kButtons[res][8]);
    g.stop = rect(kButtons[res][9]);
    return g;
}

} // namespace dtvr
