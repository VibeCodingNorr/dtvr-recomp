// SPDX-License-Identifier: GPL-3.0-only
// Tests for the FLC decoder with a synthetic 4 x 2 film and malformed input.
#include "flc.hpp"

#include <cstdio>
#include <stdexcept>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static void put16(std::vector<uint8_t>& v, size_t at, int x) { v[at] = uint8_t(x); v[at + 1] = uint8_t(x >> 8); }
static void put32(std::vector<uint8_t>& v, size_t at, uint32_t x) { for (int i = 0; i < 4; ++i) v[at + size_t(i)] = uint8_t(x >> (8 * i)); }

// A frame chunk (0xF1FA) holding the given sub-chunks.
static std::vector<uint8_t> frame(const std::vector<std::pair<int, std::vector<uint8_t>>>& chunks) {
    std::vector<uint8_t> f(16, 0);
    for (const auto& [type, body] : chunks) {
        std::vector<uint8_t> c(6, 0);
        put32(c, 0, uint32_t(6 + body.size()));
        put16(c, 4, type);
        c.insert(c.end(), body.begin(), body.end());
        f.insert(f.end(), c.begin(), c.end());
    }
    put32(f, 0, uint32_t(f.size()));
    put16(f, 4, 0xF1FA);
    put16(f, 6, int(chunks.size()));
    return f;
}

int main() {
    std::vector<uint8_t> file(128, 0);
    put16(file, 4, 0xAF12);
    put16(file, 6, 2);   // two frames
    put16(file, 8, 4);   // width
    put16(file, 10, 2);  // height
    put32(file, 16, 50); // ms per frame
    // Frame 0: palette entry 1 = (10, 20, 30); BYTE_RUN: row 0 = 1 1 1 1, row 1 = 2 3 4 5.
    const auto f0 = frame({{4, {1, 0, 1, 1, 10, 20, 30}},
                           {15, {1, 4, 1, 1, 0xFC, 2, 3, 4, 5}}});
    // Frame 1: DELTA_FLC: one line, skip line 0 (0xFFFF = -1), then on row 1 one packet:
    // skip 2, literal word (9, 9).
    const auto f1 = frame({{7, {1, 0, 0xFF, 0xFF, 1, 0, 2, 1, 9, 9}}});
    // Ring frame: back to frame 0's picture (row 1 = 2 3 4 5).
    const auto ring = frame({{7, {1, 0, 0xFF, 0xFF, 1, 0, 2, 1, 4, 5}}});
    for (const auto* f : {&f0, &f1, &ring}) file.insert(file.end(), f->begin(), f->end());
    put32(file, 0, uint32_t(file.size()));
    put32(file, 80, 128);

    FlcFilm film(file);
    CHECK(film.width() == 4 && film.height() == 2 && film.frames() == 2 && film.frameMs() == 50);
    CHECK(film.decodeNext());
    CHECK(film.pixels()[0] == 1 && film.pixels()[3] == 1 && film.pixels()[4] == 2 && film.pixels()[7] == 5);
    CHECK(film.palette()[3] == 10 && film.palette()[4] == 20 && film.palette()[5] == 30);
    CHECK(film.decodeNext() && film.pixels()[6] == 9 && film.pixels()[7] == 9 && film.pixels()[4] == 2);
    CHECK(film.decodeNext() && film.pixels()[6] == 4 && film.pixels()[7] == 5); // ring frame
    CHECK(film.decodeNext() && film.pixels()[6] == 9);                          // then frame 1 again

    // Malformed: wrong magic, truncated frames, runs past the row.
    bool threw = false;
    std::vector<uint8_t> bad = file;
    put16(bad, 4, 0x1234);
    try { FlcFilm f(bad); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { FlcFilm f(std::vector<uint8_t>(file.begin(), file.begin() + 140)); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    std::vector<uint8_t> overrun = file;
    overrun[128 + 16 + 6 + 7 + 6 + 1] = 9; // first BYTE_RUN run length: 9 > width 4
    FlcFilm broken(overrun);
    CHECK(!broken.decodeNext());

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("flc_test: OK");
    return failures != 0;
}
