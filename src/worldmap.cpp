#include "worldmap.hpp"

#include <stdexcept>

namespace dtvr {

WorldMap::WorldMap(const std::vector<uint8_t>& data) {
    const size_t count = size_t(kLayers) * kSize * kSize;
    if (data.size() != count * 2)
        throw std::runtime_error("europe.map: erwartet 262144 Bytes");
    cells_.resize(count);
    for (size_t i = 0; i < count; ++i)
        cells_[i] = uint16_t(data[i * 2] | data[i * 2 + 1] << 8);
}

uint16_t WorldMap::cell(int layer, int x, int y) const {
    if (layer < 0 || layer >= kLayers || x < 0 || x >= kSize || y < 0 || y >= kSize) return 0;
    return cells_[(size_t(layer) * kSize + y) * kSize + x];
}

int seasonForMonth(int month) {
    // Byte table built on the stack at seg5:187E..18A7, read as [bp-0xE + month % 12].
    static constexpr int table[12] = {3, 3, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3};
    return table[((month % 12) + 12) % 12];
}

std::vector<Tile> splitSbb(const uint8_t* pixels, int width, int height, int pitch) {
    if (!pixels || width != WorldMap::kTile || height <= 0 || height % WorldMap::kTile || pitch < width)
        throw std::runtime_error("SBB: unerwartete Abmessungen");
    std::vector<Tile> tiles(size_t(height / WorldMap::kTile));
    for (size_t t = 0; t < tiles.size(); ++t)
        for (int y = 0; y < WorldMap::kTile; ++y)
            for (int x = 0; x < WorldMap::kTile; ++x)
                tiles[t][size_t(y) * WorldMap::kTile + x] =
                    pixels[(t * WorldMap::kTile + y) * size_t(pitch) + x];
    return tiles;
}

std::vector<uint8_t> renderWorld(const WorldMap& map, const std::vector<Tile>& tiles,
                                 bool views, uint8_t background) {
    constexpr int T = WorldMap::kTile, side = WorldMap::kSize * T;
    std::vector<uint8_t> out(size_t(side) * side, background);
    struct Pass { int layer; bool foreground; };
    for (const Pass pass : {Pass{0, false}, Pass{1, false}, Pass{1, true}}) {
        for (int my = 0; my < WorldMap::kSize; ++my) {
            for (int mx = 0; mx < WorldMap::kSize; ++mx) {
                const uint16_t c = map.cell(pass.layer, mx, my);
                const unsigned index = c & WorldMap::kTileMask;
                if (index == WorldMap::kEmptyTile || (!views && index == WorldMap::kViewTile)) continue;
                if (bool(c & WorldMap::kForeground) != pass.foreground) continue;
                if (index >= tiles.size()) continue; // null pointer in the original table
                const bool mirror = c & WorldMap::kMirror;
                const auto& tile = tiles[index];
                for (int y = 0; y < T; ++y) {
                    uint8_t* row = &out[(size_t(my) * T + y) * side + size_t(mx) * T];
                    for (int x = 0; x < T; ++x) {
                        const uint8_t p = tile[size_t(y) * T + (mirror ? T - 1 - x : x)];
                        if (p != 0) row[x] = p;
                    }
                }
            }
        }
    }
    return out;
}

} // namespace dtvr
