// SPDX-License-Identifier: GPL-3.0-only
#include "board.hpp"

#include <stdexcept>

namespace dtvr {
namespace {

constexpr size_t kFieldSize = 16;
constexpr size_t kListHeader = 16; // name[14] + count
constexpr size_t kTownSize = 0x176;
constexpr size_t kTownEntrySize = 0x22;

uint16_t u16(const std::vector<uint8_t>& d, size_t at) {
    if (at > d.size() || d.size() - at < 2) throw std::runtime_error("abgeschnitten");
    return uint16_t(d[at] | d[at + 1] << 8);
}

std::string text(const std::vector<uint8_t>& d, size_t at, size_t size) {
    std::string out;
    for (size_t i = 0; i < size && d[at + i]; ++i) out.push_back(char(d[at + i]));
    return out;
}

} // namespace

BoardInfo parseBoardInfo(const std::vector<uint8_t>& data) {
    if (data.size() != 4) throw std::runtime_error("europe.inf: erwartet 4 Bytes");
    return {u16(data, 0), u16(data, 2)};
}

Board parseBoard(const std::vector<uint8_t>& d, uint16_t count) {
    if (d.size() / kFieldSize < count) throw std::runtime_error("europe.str: Feldtabelle abgeschnitten");
    Board board;
    board.fields.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const size_t at = i * kFieldSize;
        Field& f = board.fields[i];
        f.x = d[at];
        f.y = d[at + 1];
        for (size_t k = 0; k < 4; ++k) {
            f.next[k] = int16_t(u16(d, at + 2 + k * 2));
            if (f.next[k] != Field::kNone && (f.next[k] < 0 || f.next[k] >= count))
                throw std::runtime_error("europe.str: ungueltiger Nachbar bei Feld " + std::to_string(i));
        }
        f.kind = u16(d, at + 10);
        f.offset = u16(d, at + 12);
        f.flags = u16(d, at + 14);
    }
    for (size_t i = 0; i < count; ++i)
        for (size_t k = 0; k < 4; ++k) {
            const int16_t n = board.fields[i].next[k];
            if (n != Field::kNone && board.fields[size_t(n)].next[(k + 2) % 4] != int16_t(i))
                throw std::runtime_error("europe.str: einseitige Verbindung bei Feld " + std::to_string(i));
        }
    for (size_t at = size_t(count) * kFieldSize; at < d.size();) {
        if (d.size() - at < kListHeader) throw std::runtime_error("europe.str: Liste abgeschnitten");
        FieldList list{text(d, at, 14), {}};
        const size_t n = u16(d, at + 14);
        if ((d.size() - at - kListHeader) / 2 < n) throw std::runtime_error("europe.str: Liste abgeschnitten");
        for (size_t k = 0; k < n; ++k) list.values.push_back(u16(d, at + kListHeader + k * 2));
        if (at > 0xFFFF) throw std::runtime_error("europe.str: Liste jenseits 64 KiB");
        board.lists.emplace(uint16_t(at), std::move(list));
        at += kListHeader + n * 2;
    }
    for (size_t i = 0; i < count; ++i) {
        const uint16_t o = board.fields[i].offset;
        if (o && o != d.size() && !board.lists.count(o))
            throw std::runtime_error("europe.str: Feld " + std::to_string(i) + " zeigt nicht auf eine Liste");
    }
    return board;
}

std::vector<Town> parseTowns(const std::vector<uint8_t>& d, uint16_t count) {
    if (d.size() / kTownSize < count) throw std::runtime_error("Staedtedatei abgeschnitten");
    std::vector<Town> towns(count);
    for (size_t i = 0; i < count; ++i) {
        const size_t at = i * kTownSize;
        Town& t = towns[i];
        t.x = u16(d, at);
        t.y = u16(d, at + 2);
        t.unknown = u16(d, at + 4);
        t.name = text(d, at + 6, 28);
        for (size_t k = 0; k < t.entries.size(); ++k) {
            const size_t e = at + kTownEntrySize * (k + 1);
            auto& entry = t.entries[k];
            entry.name = text(d, e, 24);
            entry.value = uint32_t(u16(d, e + 0x18)) | uint32_t(u16(d, e + 0x1A)) << 16;
            for (size_t w = 0; w < 3; ++w) entry.words[w] = u16(d, e + 0x1C + w * 2);
        }
    }
    return towns;
}

std::string oemToUtf8(const std::string& in) {
    // Code page 850, bytes 0x80..0xFF.
    static const char16_t high[128] = {
        0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
        0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00F8, 0x00A3, 0x00D8, 0x00D7, 0x0192,
        0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x00AE, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
        0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x00C1, 0x00C2, 0x00C0, 0x00A9, 0x2563, 0x2551, 0x2557, 0x255D, 0x00A2, 0x00A5, 0x2510,
        0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x00E3, 0x00C3, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x00A4,
        0x00F0, 0x00D0, 0x00CA, 0x00CB, 0x00C8, 0x0131, 0x00CD, 0x00CE, 0x00CF, 0x2518, 0x250C, 0x2588, 0x2584, 0x00A6, 0x00CC, 0x2580,
        0x00D3, 0x00DF, 0x00D4, 0x00D2, 0x00F5, 0x00D5, 0x00B5, 0x00FE, 0x00DE, 0x00DA, 0x00DB, 0x00D9, 0x00FD, 0x00DD, 0x00AF, 0x00B4,
        0x00AD, 0x00B1, 0x2017, 0x00BE, 0x00B6, 0x00A7, 0x00F7, 0x00B8, 0x00B0, 0x00A8, 0x00B7, 0x00B9, 0x00B3, 0x00B2, 0x25A0, 0x00A0};
    std::string out;
    for (unsigned char c : in) {
        const char16_t u = c < 0x80 ? char16_t(c) : high[c - 0x80];
        if (u < 0x80) out.push_back(char(u));
        else if (u < 0x800) { out.push_back(char(0xC0 | u >> 6)); out.push_back(char(0x80 | (u & 0x3F))); }
        else { out.push_back(char(0xE0 | u >> 12)); out.push_back(char(0x80 | (u >> 6 & 0x3F))); out.push_back(char(0x80 | (u & 0x3F))); }
    }
    return out;
}

} // namespace dtvr
