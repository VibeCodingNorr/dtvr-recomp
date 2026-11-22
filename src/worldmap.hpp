#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace dtvr {

// europe.map: two layers of 256 x 256 little-endian uint16 cells, addressed as
// ((layer * 256 + y) * 256 + x) * 2 (rally.exe seg6:26B6). Evidence: docs/formats.md.
class WorldMap {
public:
    static constexpr int kSize = 256;
    static constexpr int kLayers = 2;
    static constexpr int kTile = 16;
    static constexpr uint16_t kTileMask = 0x0FFF;
    static constexpr uint16_t kMirror = 0x4000;     // horizontal flip (seg6:2D36)
    static constexpr uint16_t kForeground = 0x8000; // drawn in a separate pass
    static constexpr uint16_t kEmptyTile = 0x1F;    // never drawn (seg6:2C40)
    static constexpr uint16_t kViewTile = 0x33;     // drawn only with [Setup] Views != 0

    explicit WorldMap(const std::vector<uint8_t>& data); // throws on wrong size
    uint16_t cell(int layer, int x, int y) const;         // 0 outside the map, as in rally.exe

private:
    std::vector<uint16_t> cells_;
};

using Tile = std::array<uint8_t, WorldMap::kTile * WorldMap::kTile>; // top-down indices

// Season directory map/<n>/ for a month counter (seg5:186A: table indexed by month % 12).
int seasonForMonth(int month);

// Splits one SBB strip (16 x 16n, top-down indices; caller provides pitch * height
// bytes) into tiles in table order:
// tile 0 is the top block, matching the pointer table built at seg5:1A39.
std::vector<Tile> splitSbb(const uint8_t* pixels, int width, int height, int pitch);

// Renders the whole map into a 4096 x 4096 index buffer using the three passes of
// seg6:28D5: layer 0 (bit 15 clear), layer 1 (bit 15 clear), layer 1 (bit 15 set).
// Index 0 is transparent in every pass; uncovered pixels keep `background`.
std::vector<uint8_t> renderWorld(const WorldMap& map, const std::vector<Tile>& tiles,
                                 bool views = true, uint8_t background = 0);

} // namespace dtvr
