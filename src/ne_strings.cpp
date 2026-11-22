// SPDX-License-Identifier: GPL-3.0-only
#include "ne_strings.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace dtvr {
namespace {

uint16_t word(const std::vector<uint8_t>& d, size_t at) {
    if (at + 2 > d.size()) throw std::runtime_error("NE-Datei zu kurz");
    return uint16_t(d[at] | d[at + 1] << 8);
}

// Resource table walk: calls `visit(type, idWord, start, size)` per resource.
template <typename Visit>
void forEachResource(const std::vector<uint8_t>& data, Visit visit) {
    if (data.size() < 0x40 || data[0] != 'M' || data[1] != 'Z') throw std::runtime_error("keine MZ-Datei");
    const size_t ne = size_t(word(data, 0x3C)) | size_t(word(data, 0x3E)) << 16;
    if (ne + 0x40 > data.size() || data[ne] != 'N' || data[ne + 1] != 'E') throw std::runtime_error("keine NE-Datei");
    if (word(data, ne + 0x24) == word(data, ne + 0x26)) return; // no resource table
    const size_t table = ne + word(data, ne + 0x24);
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
            if (start + size > data.size()) throw std::runtime_error("Ressource ausserhalb der Datei");
            visit(type, word(data, at + 6), table, start, size);
        }
    }
}

std::string upperAscii(std::string s) {
    for (char& c : s) c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

std::vector<NeMenuItem> readNeMenu(const std::vector<uint8_t>& data, const std::string& name) {
    constexpr uint16_t kRtMenu = 0x8004;
    std::vector<NeMenuItem> items;
    bool found = false;
    forEachResource(data, [&](uint16_t type, uint16_t id, size_t table, size_t start, size_t size) {
        if (found || type != kRtMenu || (id & 0x8000)) return;
        const size_t at = table + id;
        if (at >= data.size() || at + 1 + data[at] > data.size()) throw std::runtime_error("Ressourcenname ausserhalb");
        if (upperAscii(std::string(data.begin() + long(at + 1), data.begin() + long(at + 1 + data[at])))
            != upperAscii(name))
            return;
        found = true;
        const size_t end = start + size;
        size_t pos = start + 4; // MENUITEMTEMPLATEHEADER: version, offset
        auto text = [&]() {
            const auto zero = std::find(data.begin() + long(std::min(pos, end)), data.begin() + long(end), 0);
            if (zero == data.begin() + long(end)) throw std::runtime_error("Menuetext ohne Ende");
            std::string s(data.begin() + long(pos), zero);
            pos = size_t(zero - data.begin()) + 1;
            return s;
        };
        // "Last" flags of the open popups below the first one: a popup's END closes it,
        // and when that popup was itself the last item, its parent closes too.
        std::vector<bool> open;
        bool inFirst = false;
        for (;;) {
            if (pos + 2 > end) throw std::runtime_error("Menue ohne Ende");
            NeMenuItem item;
            item.flags = word(data, pos);
            pos += 2;
            const bool popup = item.flags & 0x10, last = item.flags & 0x80;
            if (!popup) {
                if (pos + 2 > end) throw std::runtime_error("Menue ohne Ende");
                item.id = word(data, pos);
                pos += 2;
            }
            item.text = text();
            if (!inFirst) { // top level: the first popup holds the items
                if (!popup) throw std::runtime_error("Menue ohne Untermenue");
                inFirst = true;
                continue;
            }
            if (open.empty()) items.push_back(item);
            if (popup) { open.push_back(last); continue; }
            if (!last) continue;
            bool done = true; // the END closes the current level and every "last" popup above
            while (!open.empty()) {
                const bool parentLast = open.back();
                open.pop_back();
                if (!parentLast) { done = false; break; }
            }
            if (done) break;
        }
    });
    return items;
}

std::map<int, std::string> readNeStrings(const std::vector<uint8_t>& data) {
    constexpr uint16_t kRtString = 0x8006;
    std::map<int, std::string> out;
    forEachResource(data, [&](uint16_t type, uint16_t id, size_t, size_t start, size_t size) {
        if (type != kRtString || !(id & 0x8000)) return;
        {
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
    });
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
