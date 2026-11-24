// SPDX-License-Identifier: GPL-3.0-only
// The board of BILANZ as the window's WM_PAINT draws it into the WinG surface
// (seg8:9AB8 ff.; contents in balance_board.cpp, evidence in docs/formats.md
// "Year balance", round 149).
#include "balance_view.hpp"

#include <cstdlib>

namespace dtvr {
namespace {

// The points of a GDI line from `from` towards `to`, `to` itself excluded (LineTo).
std::vector<BoardPoint> linePoints(BoardPoint from, BoardPoint to) {
    std::vector<BoardPoint> out;
    const int dx = std::abs(to.x - from.x), dy = -std::abs(to.y - from.y);
    const int sx = from.x < to.x ? 1 : -1, sy = from.y < to.y ? 1 : -1;
    int err = dx + dy, x = from.x, y = from.y;
    while (x != to.x || y != to.y) {
        out.push_back({x, y});
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
    return out;
}

} // namespace

SDL_Color BalanceView::paletteColour(int index) const {
    if (index >= 0 && size_t(index) < palette_.size()) return palette_[size_t(index)];
    return SDL_Color{0, 0, 0, 255};
}

SDL_Color BalanceView::nearestColour(BalanceGraph::Rgb rgb) const {
    // PALETTERGB: the closest entry of the selected palette (DS:9CD2). The distance
    // measure of GDI's matching is not traced; the squared RGB distance is used.
    SDL_Color best{rgb.r, rgb.g, rgb.b, 255};
    long bestDistance = -1;
    for (const SDL_Color& c : palette_) {
        const long dr = long(c.r) - rgb.r, dg = long(c.g) - rgb.g, db = long(c.b) - rgb.b;
        const long d = dr * dr + dg * dg + db * db;
        if (bestDistance < 0 || d < bestDistance) { bestDistance = d; best = SDL_Color{c.r, c.g, c.b, 255}; }
    }
    return best;
}

void BalanceView::drawBoard(int ox, int oy) {
    if (board_ >= 0 && board_ < kPlayerSlots) drawSheet(sheets_[size_t(board_)], ox, oy); // seg8:9B93
    else if (board_ == kBoardGraph) drawGraph(ox, oy);                                    // seg8:A1E5
}

void BalanceView::drawSheet(const BalanceSheet& sheet, int ox, int oy) {
    if (!text_) return;
    const SDL_Color brown = paletteColour(0x45), red = paletteColour(0xED);
    for (const BalanceSheet::Line& line : sheet.lines) {
        // Font 0 is "MS Sans Serif" in rally.exe; TextRenderer has Arial / Liberation Sans.
        TextStyle style{line.font == 0 ? 18 : 16, line.font == 2, false, false, false, 0, brown};
        const ScreenRect rect{ox + sheet.left, oy + line.top, sheet.right - sheet.left, 0x3C + 0x100 - line.top};
        if (!line.label.empty()) text_->draw(renderer_, line.label, rect, style); // DrawText 0x100
        if (!line.value.empty()) {
            style.right = true; // 0x102
            style.color = line.negative ? red : brown;
            text_->draw(renderer_, line.value, rect, style);
        }
    }
}

void BalanceView::drawGraph(int ox, int oy) {
    auto plot = [&](const std::vector<BoardPoint>& points, int width) {
        for (const BoardPoint& p : points) {
            const SDL_Rect r{ox + p.x, oy + p.y, width, width};
            SDL_RenderFillRect(renderer_, &r);
        }
    };
    auto setColour = [&](SDL_Color c) { SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, 255); };
    for (const BalanceGraph::Polyline& line : graph_.players) { // pen width 2 (seg8:A33A)
        setColour(nearestColour(line.colour));
        for (size_t i = 1; i < line.points.size(); ++i) {
            plot(linePoints(line.points[i - 1], line.points[i]), 2);
            plot({line.points[i]}, 2);
        }
    }
    for (const BalanceGraph::Legend& l : graph_.legend) { // seg2:913A, 26 x 26 opaque
        const auto it = legend_.find(std::string(1, l.frame));
        if (it == legend_.end() || !it->second) continue;
        const SDL_Rect src{0, 0, 0x1A, 0x1A}, dst{ox + l.at.x, oy + l.at.y, 0x1A, 0x1A};
        SDL_RenderCopy(renderer_, it->second.get(), &src, &dst);
    }
    setColour(nearestColour(graph_.tickColour)); // width 1: LineTo leaves out the end point
    for (const BalanceGraph::Segment& s : graph_.ticks) plot(linePoints(s.from, s.to), 1);
    setColour(nearestColour(graph_.axisColour)); // width 2
    for (const BalanceGraph::Segment& s : graph_.axes) plot(linePoints(s.from, s.to), 2);
}

} // namespace dtvr
