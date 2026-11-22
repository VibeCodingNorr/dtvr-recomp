// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace dtvr {

// europe.inf: uint16 field count (-> DS:9CFA), uint16 town count (-> DS:9C80).
struct BoardInfo {
    uint16_t fields = 0;
    uint16_t towns = 0;
};
BoardInfo parseBoardInfo(const std::vector<uint8_t>& data);

// One 16-byte record of europe.str. Member names follow the original debug
// output at DS:16DE ("oben", "rechts", "unten", "links", "whatisit", "offset",
// "flagset"). Coordinates are map cells (see docs/formats.md).
struct Field {
    static constexpr int16_t kNone = -1;
    uint8_t x = 0, y = 0;
    std::array<int16_t, 4> next{kNone, kNone, kNone, kNone}; // up, right, down, left
    uint16_t kind = 0;    // whatisit: 6 = town (seg6:23EA), 5 = card field (seg6:2487)
    uint16_t offset = 0;  // file offset of a FieldList, or 0
    uint16_t flags = 0;   // flagset; meaning of the bits is open
};

// Variable-length record after the field table: char name[14] (OEM code page),
// uint16 count, uint16 values[count]. Meaning of the values is open.
struct FieldList {
    std::string name;
    std::vector<uint16_t> values;
};

struct Board {
    std::vector<Field> fields;
    std::map<uint16_t, FieldList> lists; // keyed by file offset
};

// Parses europe.str with `count` fields. Validates neighbour indices, link
// symmetry and that every non-zero field offset starts a list or equals EOF.
Board parseBoard(const std::vector<uint8_t>& data, uint16_t count);

// 0x22-byte sub-record of a town; meanings beyond the name are open.
struct TownEntry {
    std::string name;    // OEM code page, at +0x00, 24 bytes
    uint32_t value = 0;  // +0x18
    std::array<uint16_t, 3> words{}; // +0x1C, +0x1E, +0x20
};

// One 0x176-byte record of europe_<lang>.tow (stride from seg6:241B).
struct Town {
    uint16_t x = 0, y = 0; // map cells, compared with DS:955E/9560 at seg6:2424
    uint16_t unknown = 0;  // +4
    std::string name;      // +6, 28 bytes, OEM code page
    std::array<TownEntry, 10> entries;
};
std::vector<Town> parseTowns(const std::vector<uint8_t>& data, uint16_t count);

// seg7:28AD: the photo's name from the first eight characters of the town's name
// (OEM code page): umlauts to their base letters, a space to '_'.
std::string finishPhotoName(const std::string& oemTownName);

// Converts OEM code page 850 text (as stored in the game files) to UTF-8.
std::string oemToUtf8(const std::string& text);

} // namespace dtvr
