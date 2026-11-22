// Synthetic and malformed-input tests for europe.map handling and tile composition.
#include "worldmap.hpp"

#include <cstdio>
#include <stdexcept>

using namespace dtvr;
using Bytes = std::vector<uint8_t>;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static void put(Bytes& d, int layer, int x, int y, uint16_t v) {
    const size_t at = ((size_t(layer) * 256 + y) * 256 + x) * 2;
    d[at] = uint8_t(v); d[at + 1] = uint8_t(v >> 8);
}

static Bytes emptyMap() {
    Bytes d(262144, 0);
    for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 256; ++x) { put(d, 0, x, y, WorldMap::kEmptyTile); put(d, 1, x, y, WorldMap::kEmptyTile); }
    return d;
}

static uint8_t px(const Bytes& img, int x, int y) { return img[size_t(y) * 4096 + x]; }

int main() {
    // Size validation.
    for (size_t n : {size_t(0), size_t(262143), size_t(262145)}) {
        bool threw = false;
        try { WorldMap m(Bytes(n, 0)); } catch (const std::runtime_error&) { threw = true; }
        CHECK(threw);
    }
    // Addressing and out-of-range reads.
    auto d = emptyMap();
    put(d, 1, 255, 2, 0x1234);
    WorldMap probe(d);
    CHECK(probe.cell(1, 255, 2) == 0x1234);
    CHECK(probe.cell(0, 255, 2) == WorldMap::kEmptyTile);
    CHECK(probe.cell(1, 256, 2) == 0 && probe.cell(2, 0, 0) == 0 && probe.cell(0, -1, 0) == 0);

    CHECK(seasonForMonth(0) == 3 && seasonForMonth(2) == 0 && seasonForMonth(5) == 1);
    CHECK(seasonForMonth(8) == 2 && seasonForMonth(11) == 3 && seasonForMonth(14) == 0);
    CHECK(seasonForMonth(-1) == 3);

    // splitSbb: tile k is image rows 16k..16k+15.
    Bytes strip(17 * 32, 0);
    for (int y = 0; y < 32; ++y) strip[size_t(y) * 17] = uint8_t(y + 1); // pitch 17, column 0
    auto tiles = splitSbb(strip.data(), 16, 32, 17);
    CHECK(tiles.size() == 2 && tiles[0][0] == 1 && tiles[1][0] == 17 && tiles[1][15 * 16] == 32);
    bool threw = false;
    try { splitSbb(strip.data(), 16, 30, 16); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);

    // Composition: tile 0 = solid 5, tile 1 = column 0 is 7, rest transparent (0).
    tiles.assign(2, Tile{});
    tiles[0].fill(5);
    for (int y = 0; y < 16; ++y) tiles[1][size_t(y) * 16] = 7;
    d = emptyMap();
    put(d, 0, 0, 0, 0);                                   // base tile
    put(d, 1, 0, 0, 1 | WorldMap::kMirror);               // overlay, mirrored: column 15
    put(d, 0, 1, 0, 1 | WorldMap::kForeground);           // layer 0 + bit 15: never drawn
    put(d, 1, 2, 0, 1 | WorldMap::kForeground);           // foreground pass
    put(d, 0, 3, 0, 999);                                 // missing tile: skipped
    auto img = renderWorld(WorldMap(d), tiles, true, 9);
    CHECK(px(img, 0, 0) == 5 && px(img, 15, 0) == 7 && px(img, 15, 15) == 7);
    CHECK(px(img, 16, 0) == 9);
    CHECK(px(img, 32, 3) == 7 && px(img, 33, 3) == 9);
    CHECK(px(img, 48, 0) == 9);

    // Tile 0x33 depends on the Views setting.
    tiles.assign(0x34, Tile{});
    tiles[0x33].fill(3);
    d = emptyMap();
    put(d, 1, 0, 0, WorldMap::kViewTile);
    CHECK(px(renderWorld(WorldMap(d), tiles, true), 0, 0) == 3);
    CHECK(px(renderWorld(WorldMap(d), tiles, false), 0, 0) == 0);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("worldmap_test: OK");
    return failures != 0;
}
