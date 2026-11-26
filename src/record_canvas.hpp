// SPDX-License-Identifier: GPL-3.0-only
// The 8-bit images of a scene window that draws animation records the way rally.exe
// does (finish ZIEL, victory SIEG): the backdrop DIB, the WinG DIB the records draw
// into and the window image, with the SBS files in their slots (DS:9D44 + 4 * slot)
// and RecordPainter for the dirty-rectangle passes. The window image becomes a texture
// with the backdrop bitmap's palette.
#pragma once

#include "record_paint.hpp"
#include "sdl_assets.hpp"

#include <SDL.h>

#include <filesystem>
#include <string>
#include <vector>

namespace dtvr {

// The pixels of an 8-bit surface (rows without the pitch padding).
PaintImage toPaintImage(const SDL_Surface* surface);

class RecordCanvas {
public:
    RecordCanvas(int width, int height, size_t records) : width_(width), height_(height), painter_(records) {}
    // An 8-bit bitmap of the canvas size becomes the backdrop; its palette is kept.
    bool setBackdrop(const SDL_Surface* bitmap);
    PaintImage& backdrop() { return backdrop_; }
    // seg6:3E38: the frames of an SBS file into `slot` (a missing or malformed file
    // leaves the slot empty, as the original's null handle).
    void loadFile(size_t slot, const std::filesystem::path& file);
    // seg7:1E3C: every frame of the slot's file takes the backdrop under (x, y) where
    // it holds 0xFF (seg7:1CE0).
    void fillFile(size_t slot, int x, int y);
    const PaintFrames& frames() const { return frames_; }
    // The window shows black (ZIEL / SIEG WM_PAINT before the first pass clears the DIB,
    // seg7:4FF4 / seg7:8302).
    void clear();
    // One pass: seg2:74E4 restore, seg2:6F69 draw changed records; `present` copies the
    // pass's rectangles to the window (seg2:72DE).
    void pass(const std::vector<PaintRecord>& records, bool present);
    // WM_PAINT over the whole client area: backdrop into the DIB, every record, the
    // DIB to the window (seg7:4E36 ff. / seg7:811B ff.).
    void paint(const std::vector<PaintRecord>& records);
    RecordPainter& painter() { return painter_; }
    const PaintImage& screen() const { return screen_; }
    // The window image as a texture (rebuilt by the calls above).
    SDL_Texture* texture(SDL_Renderer* renderer);

private:
    int width_, height_;
    PaintImage backdrop_, dib_, screen_;
    PaintFrames frames_;
    std::vector<std::vector<std::string>> slots_; // frame names per file slot
    RecordPainter painter_;
    std::vector<SDL_Color> palette_;
    Texture texture_{nullptr, SDL_DestroyTexture};
    bool dirty_ = true;
};

} // namespace dtvr
