// SPDX-License-Identifier: GPL-3.0-only
// Autodesk FLC / FLI animation decoder for the card films of rally.exe (round 83:
// butt/flc/karte1-4.flc, played by the FLIC window seg7:62AA). Bounds-checked, no SDL.
// Chunks: 4 COLOR_256, 11 COLOR_64, 7 DELTA_FLC, 12 DELTA_FLI, 13 BLACK, 15 BYTE_RUN,
// 16 FLI_COPY; others (e.g. 18 PSTAMP) are skipped. Evidence in docs/formats.md "Films".
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace dtvr {

class FlcFilm {
public:
    // Throws std::runtime_error on a malformed header.
    explicit FlcFilm(std::vector<uint8_t> data);
    int width() const { return width_; }
    int height() const { return height_; }
    int frames() const { return frames_; }      // without the ring frame
    int frameMs() const { return frameMs_; }    // FLC: ms, FLI: 1/70 s converted
    // Decodes frame `index` (0-based) onto the current picture; frames must be
    // decoded in order, after the last one `rewind` starts again from frame 1 (the
    // ring frame returns to frame 0's picture). False on malformed frame data.
    bool decode(int index);
    void rewind() { next_ = 0; }
    bool decodeNext();                           // the next frame in order, ring frame after the last
    const std::vector<uint8_t>& pixels() const { return pixels_; }
    const std::array<uint8_t, 768>& palette() const { return palette_; } // 8-bit RGB

private:
    bool chunk(uint16_t type, const uint8_t* p, size_t size);
    bool byteRun(const uint8_t* p, size_t size);
    bool deltaFlc(const uint8_t* p, size_t size);
    bool deltaFli(const uint8_t* p, size_t size);
    bool color(const uint8_t* p, size_t size, int shift);

    std::vector<uint8_t> data_;
    std::vector<size_t> offsets_; // frame chunk offsets including the ring frame
    int width_ = 0, height_ = 0, frames_ = 0, frameMs_ = 70;
    std::vector<uint8_t> pixels_;
    std::array<uint8_t, 768> palette_{};
    int next_ = 0;
};

} // namespace dtvr
