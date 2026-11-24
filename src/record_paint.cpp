// SPDX-License-Identifier: GPL-3.0-only
#include "record_paint.hpp"

#include <algorithm>
#include <cctype>

namespace dtvr {
namespace {

PaintRect frameRect(const PaintRecord& r, const PaintImage& f) { return {r.x, r.y, r.x + f.width, r.y + f.height}; }

bool intersect(const PaintRect& a, const PaintRect& b) { // USER.IntersectRect != 0
    if (a.empty() || b.empty()) return false;
    return std::max(a.left, b.left) < std::min(a.right, b.right) && std::max(a.top, b.top) < std::min(a.bottom, b.bottom);
}

PaintRect unite(const PaintRect& a, const PaintRect& b) { // USER.UnionRect of two non-empty rectangles
    return {std::min(a.left, b.left), std::min(a.top, b.top), std::max(a.right, b.right), std::max(a.bottom, b.bottom)};
}

} // namespace

const PaintImage* findPaintFrame(const PaintFrames& frames, const std::string& name) {
    std::string key = name;
    for (char& c : key) c = char(std::toupper(static_cast<unsigned char>(c)));
    const auto it = frames.find(key);
    return it == frames.end() ? nullptr : &it->second;
}

void fillTransparent(PaintImage& frame, const PaintImage& backdrop, int x, int y) {
    for (int row = 0; row < frame.height; ++row) {
        const int by = y + row;
        if (by < 0 || by >= backdrop.height) continue;
        for (int col = 0; col < frame.width; ++col) {
            const int bx = x + col;
            if (bx < 0 || bx >= backdrop.width) continue;
            uint8_t& p = frame.pixels[size_t(row) * size_t(frame.width) + size_t(col)];
            if (p == 0xFF) p = backdrop.pixels[size_t(by) * size_t(backdrop.width) + size_t(bx)]; // seg7:1DDF
        }
    }
}

void blitImage(const PaintImage& src, PaintImage& dst, int x, int y, bool keyed) {
    for (int row = 0; row < src.height; ++row) {
        const int dy = y + row;
        if (dy < 0 || dy >= dst.height) continue;
        for (int col = 0; col < src.width; ++col) {
            const int dx = x + col;
            if (dx < 0 || dx >= dst.width) continue;
            const uint8_t p = src.pixels[size_t(row) * size_t(src.width) + size_t(col)];
            if (!keyed || p != 0xFF) dst.pixels[size_t(dy) * size_t(dst.width) + size_t(dx)] = p;
        }
    }
}

void copyRect(const PaintImage& src, PaintImage& dst, PaintRect r) {
    r.left = std::max(r.left, 0);
    r.top = std::max(r.top, 0);
    r.right = std::min({r.right, src.width, dst.width});
    r.bottom = std::min({r.bottom, src.height, dst.height});
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x)
            dst.pixels[size_t(y) * size_t(dst.width) + size_t(x)] = src.pixels[size_t(y) * size_t(src.width) + size_t(x)];
}

bool RecordPainter::draw(const PaintRecord& record, const PaintFrames& frames, PaintImage& dib, State& state) {
    const PaintImage* f = findPaintFrame(frames, record.frame);
    if (!f) return false; // seg6:4217: nothing drawn, no rectangle changes
    blitImage(*f, dib, record.x, record.y, record.keyed);
    state.update = state.previous = PaintRect{}; // seg6:42FD / 4306
    const PaintRect now = frameRect(record, *f);
    if (!record.keyed) {
        state.update = now; // seg6:43DC; +0x2E stays
        return true;
    }
    if (intersect(now, state.drawn)) {
        state.update = unite(now, state.drawn); // seg6:4376
    } else {
        state.update = now;
        state.previous = state.drawn; // seg6:439B
    }
    state.drawn = now; // seg6:43B1
    return true;
}

void RecordPainter::restore(const PaintImage& backdrop, PaintImage& dib) {
    for (State& s : states_) {
        if (s.drawn.right == 0) continue; // seg2:752D
        s.drawn.left = std::max(s.drawn.left, 0);
        s.drawn.top = std::max(s.drawn.top, 0);
        s.drawn.right = std::min(s.drawn.right, dib.width);
        s.drawn.bottom = std::min(s.drawn.bottom, dib.height);
        copyRect(backdrop, dib, s.drawn);
    }
}

void RecordPainter::show(const std::vector<PaintRecord>& records, const PaintFrames& frames, PaintImage& dib) {
    const size_t n = std::min(records.size(), states_.size());
    std::vector<int> status(n, 0);
    for (size_t i = 0; i < n; ++i) // seg2:6FAF: a new frame name
        if (records[i].active && records[i].frame != states_[i].last) status[i] = 2;
    for (size_t i = 0; i < n; ++i) { // seg2:7037: records under a changed one
        if (status[i] != 2) continue;
        const PaintImage* f = findPaintFrame(frames, records[i].frame);
        if (!f) continue;
        const PaintRect changed = frameRect(records[i], *f);
        for (size_t j = 0; j < n; ++j) {
            if (!records[j].active || status[j] != 0) continue;
            const PaintImage* g = findPaintFrame(frames, records[j].frame);
            if (g && intersect(changed, frameRect(records[j], *g))) status[j] = 1;
        }
    }
    for (size_t i = 0; i < n; ++i) { // seg2:720B
        if (status[i] != 0) {
            states_[i].last = records[i].frame; // seg2:7274 lstrcpy before the draw
            draw(records[i], frames, dib, states_[i]);
        }
        if (status[i] != 2) states_[i].update = states_[i].previous = PaintRect{}; // seg2:72BD
    }
}

void RecordPainter::present(const PaintImage& dib, PaintImage& screen) {
    for (State& s : states_) {
        if (s.update.right != 0) copyRect(dib, screen, s.update); // seg2:7383
        if (s.previous.right != 0) copyRect(dib, screen, s.previous); // seg2:741A
        s.update = s.previous = PaintRect{};
    }
}

void RecordPainter::paint(const std::vector<PaintRecord>& records, const PaintFrames& frames,
                          const PaintImage& backdrop, PaintImage& dib, PaintImage& screen, PaintRect area) {
    copyRect(backdrop, dib, area); // seg7:2416
    const size_t n = std::min(records.size(), states_.size());
    for (size_t i = 0; i < n; ++i)
        if (records[i].active) draw(records[i], frames, dib, states_[i]); // seg7:248D
    copyRect(dib, screen, area); // seg7:24BE
}

} // namespace dtvr
