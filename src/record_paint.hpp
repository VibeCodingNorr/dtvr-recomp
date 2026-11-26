// SPDX-License-Identifier: GPL-3.0-only
// Drawing of animation records as rally.exe does it in its scene windows (no SDL):
// the records draw into a DIB over a backdrop and only changed rectangles reach
// the window. seg2:74E4 restores the backdrop under every record's last keyed
// rectangle, seg2:6F69 draws the records whose frame name changed (and those
// overlapping them) with seg6:41A2, seg2:72DE copies their rectangles to the
// window. A record whose frame name stays the same is not drawn again, and a
// frame missing from the SBS files draws nothing, so the window keeps what it
// showed. Evidence in docs/formats.md "Intro".
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace dtvr {

// An 8-bit indexed image, rows top-down without padding.
struct PaintImage {
    int width = 0, height = 0;
    std::vector<uint8_t> pixels;
    PaintImage() = default;
    PaintImage(int w, int h, uint8_t fill = 0) : width(w), height(h), pixels(size_t(w) * size_t(h), fill) {}
};

// A Windows RECT: left/top inclusive, right/bottom exclusive.
struct PaintRect {
    int left = 0, top = 0, right = 0, bottom = 0;
    bool empty() const { return right <= left || bottom <= top; }
    bool operator==(const PaintRect& o) const {
        return left == o.left && top == o.top && right == o.right && bottom == o.bottom;
    }
};

// A record as seg2:6F69 sees it after the step: active while its definition is set,
// the name of the current frame (+0xC of the definition), position +0x14 / +0x16
// and the flag +0xD (1: colour key 0xFF via seg2:9497, 0: plain copy seg2:913A).
struct PaintRecord {
    bool active = false;
    std::string frame;
    int x = 0, y = 0;
    bool keyed = false;
};

// Frames by upper-case name (seg6:406D finds the lower-case definition names in
// the upper-case SBS directories).
using PaintFrames = std::map<std::string, PaintImage>;
const PaintImage* findPaintFrame(const PaintFrames& frames, const std::string& name);

// seg7:1CE0 (for every frame of a file: seg7:1E3C): the frame's pixels of index 0xFF
// take the backdrop's pixel under the frame placed at (x, y). The backdrop is not
// changed; the frame becomes opaque.
void fillTransparent(PaintImage& frame, const PaintImage& backdrop, int x, int y);
// seg2:9497 (keyed, 0xFF left out) or seg2:913A (plain) of `src` into `dst` at (x, y);
// seg7:1F52 is the keyed form into the backdrop.
void blitImage(const PaintImage& src, PaintImage& dst, int x, int y, bool keyed);
// Copies the rectangle `r` (clipped) from `src` to the same place in `dst`.
void copyRect(const PaintImage& src, PaintImage& dst, PaintRect r);

class RecordPainter {
public:
    explicit RecordPainter(size_t records) : states_(records) {}
    void reset() { for (State& s : states_) s = State{}; }
    // seg2:74E4: the backdrop back into the DIB under the last keyed rectangle (+0x2E)
    // of every record slot, clamped to the DIB (the clamp is stored).
    void restore(const PaintImage& backdrop, PaintImage& dib);
    // seg2:6F69: records whose frame name differs from the last one drawn (+0x36) are
    // drawn; records whose current frame overlaps such a record's new frame are drawn
    // again; only the first kind keeps its window rectangles.
    void show(const std::vector<PaintRecord>& records, const PaintFrames& frames, PaintImage& dib);
    // seg2:72DE: the rectangles of this pass from the DIB to the window.
    void present(const PaintImage& dib, PaintImage& screen);
    // WM_PAINT seg7:2380 over `area`: backdrop into the DIB, every record drawn
    // (seg6:41A2, the names +0x36 stay), the area to the window.
    void paint(const std::vector<PaintRecord>& records, const PaintFrames& frames, const PaintImage& backdrop,
               PaintImage& dib, PaintImage& screen, PaintRect area);
    // Clears the name last drawn (+0x36) of slot `i`, so the next show() draws it as
    // changed (ZIEL clears the car's every pass, seg7:356E). Out-of-range slots are ignored.
    void forget(size_t i) { if (i < states_.size()) states_[i].last.clear(); }
    // The last keyed rectangle (+0x2E) of slot `i` (tests).
    PaintRect drawn(size_t i) const { return states_[i].drawn; }

private:
    struct State {
        std::string last;               // +0x36
        PaintRect update, previous;     // +0x1E, +0x26: window rectangles of this pass
        PaintRect drawn;                // +0x2E
    };
    bool draw(const PaintRecord& record, const PaintFrames& frames, PaintImage& dib, State& state); // seg6:41A2
    std::vector<State> states_;
};

} // namespace dtvr
