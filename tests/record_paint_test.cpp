// SPDX-License-Identifier: GPL-3.0-only
#include "record_paint.hpp"

#include <cstdio>
#include <cstdlib>

using namespace dtvr;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}

uint8_t at(const PaintImage& i, int x, int y) { return i.pixels[size_t(y) * size_t(i.width) + size_t(x)]; }

// A 2 x 1 frame: left pixel `left`, right pixel transparent (0xFF).
PaintImage halfFrame(uint8_t left) {
    PaintImage f(2, 1, 0xFF);
    f.pixels[0] = left;
    return f;
}

struct Pass {
    RecordPainter painter{3};
    PaintImage backdrop{8, 4, 1}, dib{8, 4, 1}, screen{8, 4, 1};
    void run(const std::vector<PaintRecord>& records, const PaintFrames& frames) {
        painter.restore(backdrop, dib); // seg7:1C18
        painter.show(records, frames, dib); // seg7:1C46
        painter.present(dib, screen); // seg7:1C82
    }
};

} // namespace

int main() {
    // seg7:1E3C / 1CE0: the frame takes the backdrop where it is 0xFF; the backdrop stays.
    {
        PaintImage backdrop(4, 2, 7);
        backdrop.pixels[size_t(1) * 4 + 3] = 9;
        PaintImage frame = halfFrame(3);
        fillTransparent(frame, backdrop, 2, 1);
        check(at(frame, 0, 0) == 3 && at(frame, 1, 0) == 9, "transparent pixel takes the backdrop under it");
        check(at(backdrop, 2, 1) == 7 && at(backdrop, 3, 1) == 9, "backdrop unchanged");
    }

    // A keyed frame over the clean backdrop shows the backdrop through its holes, not
    // other frames of the same place (the intro bug: frames stamped into club.bmp).
    {
        Pass p;
        PaintFrames frames{{"A", halfFrame(5)}};
        p.run({{true, "a", 2, 1, true}}, frames);
        check(at(p.screen, 2, 1) == 5 && at(p.screen, 3, 1) == 1, "keyed frame: hole shows the backdrop");
        check(p.painter.drawn(0) == (PaintRect{2, 1, 4, 2}), "keyed rectangle kept (+0x2E)");
    }

    // A frame missing from the files draws nothing: the window keeps the last frame
    // (lt117 .. lt121 of DS:08C8 are not in leute1.sbs).
    {
        Pass p;
        PaintFrames frames{{"LT116", PaintImage(2, 1, 6)}};
        p.run({{true, "lt116", 0, 0, false}}, frames);
        p.run({{true, "lt117", 0, 0, false}}, frames);
        check(at(p.screen, 0, 0) == 6 && at(p.screen, 1, 0) == 6, "window keeps the last drawn frame");
        check(at(p.dib, 0, 0) == 6, "an unkeyed record is not restored in the DIB");
    }

    // A closed file (seg6:3F02): its keyed frames are no longer found; the DIB is
    // restored under the old rectangle, the window is not touched.
    {
        Pass p;
        PaintFrames frames{{"ZO10", PaintImage(1, 1, 5)}};
        p.run({{true, "zo10", 0, 0, true}}, frames);
        frames.clear();
        p.run({{true, "zo11", 0, 0, true}}, frames);
        check(at(p.dib, 0, 0) == 1 && at(p.screen, 0, 0) == 5, "closed frames: DIB restored, window kept");
    }

    // An unchanged frame name is not drawn again, even when the record moved; the DIB
    // loses a keyed record (seg2:74E4) while the window keeps it.
    {
        Pass p;
        PaintFrames frames{{"A", PaintImage(1, 1, 5)}};
        p.run({{true, "a", 0, 0, true}}, frames);
        p.run({{true, "a", 3, 0, true}}, frames);
        check(at(p.screen, 0, 0) == 5 && at(p.screen, 3, 0) == 1, "same name: not moved on the window");
        check(at(p.dib, 0, 0) == 1, "DIB restored under the keyed rectangle");
    }

    // A changed record redraws an unchanged record under its new frame (status 1);
    // only the changed record's rectangle reaches the window.
    {
        Pass p;
        PaintFrames frames{{"BACK", PaintImage(3, 1, 4)}, {"F1", halfFrame(8)}, {"F2", halfFrame(9)}};
        p.run({{true, "back", 0, 0, true}, {true, "f1", 1, 0, true}}, frames);
        check(at(p.screen, 1, 0) == 8 && at(p.screen, 2, 0) == 4, "first pass");
        p.run({{true, "back", 0, 0, true}, {true, "f2", 1, 0, true}}, frames);
        check(at(p.screen, 1, 0) == 9 && at(p.screen, 2, 0) == 4, "overlapped record redrawn under the change");
        check(at(p.screen, 0, 0) == 4 && at(p.dib, 0, 0) == 4, "redrawn in the DIB as a whole");
    }

    // WM_PAINT draws every record without touching the names: the next pass draws them again.
    {
        Pass p;
        PaintFrames frames{{"A", PaintImage(1, 1, 5)}};
        p.painter.paint({{true, "a", 1, 1, true}}, frames, p.backdrop, p.dib, p.screen, {0, 0, 8, 4});
        check(at(p.screen, 1, 1) == 5, "paint shows the record");
        p.screen.pixels.assign(p.screen.pixels.size(), 0);
        p.run({{true, "a", 1, 1, true}}, frames);
        check(at(p.screen, 1, 1) == 5, "first pass after the paint draws it (+0x36 still empty)");
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
