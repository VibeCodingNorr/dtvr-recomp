// SPDX-License-Identifier: GPL-3.0-only
#include "ne_strings.hpp"

#include <algorithm>
#include <stdexcept>

namespace dtvr {
namespace {

uint16_t word(const std::vector<uint8_t>& d, size_t at) {
    if (at + 2 > d.size()) throw std::runtime_error("NE-Datei zu kurz");
    return uint16_t(d[at] | d[at + 1] << 8);
}

} // namespace

std::map<int, std::string> readNeStrings(const std::vector<uint8_t>& data) {
    constexpr uint16_t kRtString = 0x8006;
    if (data.size() < 0x40 || data[0] != 'M' || data[1] != 'Z') throw std::runtime_error("keine MZ-Datei");
    const size_t ne = size_t(word(data, 0x3C)) | size_t(word(data, 0x3E)) << 16;
    if (ne + 0x40 > data.size() || data[ne] != 'N' || data[ne + 1] != 'E') throw std::runtime_error("keine NE-Datei");
    std::map<int, std::string> out;
    const size_t table = ne + word(data, ne + 0x24);
    if (word(data, ne + 0x24) == word(data, ne + 0x26)) return out; // no resource table
    const unsigned shift = word(data, table);
    if (shift > 15) throw std::runtime_error("ungueltige Ressourcentabelle");
    size_t at = table + 2;
    for (;;) {
        const uint16_t type = word(data, at);
        if (type == 0) break;
        const uint16_t count = word(data, at + 2);
        at += 8;
        for (uint16_t i = 0; i < count; ++i, at += 12) {
            const size_t start = size_t(word(data, at)) << shift;
            const size_t size = size_t(word(data, at + 2)) << shift;
            const uint16_t id = word(data, at + 6);
            if (type != kRtString || !(id & 0x8000)) continue;
            if (start + size > data.size()) throw std::runtime_error("Ressource ausserhalb der Datei");
            const int base = ((id & 0x7FFF) - 1) * 16;
            size_t pos = start;
            for (int k = 0; k < 16 && pos < start + size; ++k) {
                // rally_d.dat has four blocks whose last length byte runs past the block
                // (ids 511, 1023, 10799, 41199 area); the loaded resource ends at the block,
                // so the text is clipped there.
                const size_t n = data[pos];
                const size_t end = std::min(pos + 1 + n, start + size);
                if (end > pos + 1) out[base + k] = std::string(data.begin() + long(pos + 1), data.begin() + long(end));
                pos += 1 + n;
            }
        }
    }
    return out;
}

std::string ansiToUtf8(const std::string& text) {
    // 0x80-0x9F of Windows-1252; 0xA0-0xFF equal Latin-1.
    static const char16_t kHigh[32] = {
        0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039,
        0x0152, 0xFFFD, 0x017D, 0xFFFD, 0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178};
    std::string out;
    for (unsigned char c : text) {
        const char32_t u = c < 0x80 ? c : c < 0xA0 ? kHigh[c - 0x80] : c;
        if (u < 0x80) out += char(u);
        else if (u < 0x800) { out += char(0xC0 | u >> 6); out += char(0x80 | (u & 0x3F)); }
        else { out += char(0xE0 | u >> 12); out += char(0x80 | (u >> 6 & 0x3F)); out += char(0x80 | (u & 0x3F)); }
    }
    return out;
}

} // namespace dtvr
