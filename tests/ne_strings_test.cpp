// SPDX-License-Identifier: GPL-3.0-only
// RT_STRING reader on a synthetic NE file and malformed input.
#include "ne_strings.hpp"

#include <cstdio>
#include <stdexcept>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static void put16(std::vector<uint8_t>& d, size_t at, unsigned v) { d[at] = uint8_t(v); d[at + 1] = uint8_t(v >> 8); }

// MZ header, NE header at 0x40, resource table at NE+0x40, one RT_STRING block
// (id 0x8000 | 8 -> texts 112..127) at file offset 0x200 (shift 4 -> 0x20 units).
static std::vector<uint8_t> file() {
    std::vector<uint8_t> d(0x300, 0);
    d[0] = 'M'; d[1] = 'Z';
    put16(d, 0x3C, 0x40);
    d[0x40] = 'N'; d[0x41] = 'E';
    put16(d, 0x40 + 0x24, 0x40);   // resource table
    put16(d, 0x40 + 0x26, 0x60);   // resident names follow
    const size_t t = 0x80;
    put16(d, t, 4);                // shift
    put16(d, t + 2, 0x8006);       // RT_STRING
    put16(d, t + 4, 1);            // one resource
    put16(d, t + 10, 0x200 >> 4);  // offset
    put16(d, t + 12, 0x40 >> 4);   // length
    put16(d, t + 16, 0x8000 | 8);  // id 8: texts 112-127
    put16(d, t + 22, 0);           // end of types
    size_t p = 0x200;
    for (int i = 0; i < 16; ++i) {
        if (i == 8) { d[p++] = 5; for (char c : std::string("Hel\xdf" "t")) d[p++] = uint8_t(c); }
        else if (i == 15) { d[p++] = 2; d[p++] = 'Z'; d[p++] = '!'; }
        else d[p++] = 0;
    }
    return d;
}

// The synthetic file with an RT_MENU "ONFIELD" (popup with two items, the second
// a nested popup) as a named resource at 0x280.
static std::vector<uint8_t> menuFile() {
    auto d = file();
    const size_t t = 0x80;
    put16(d, t + 22, 0x8004);      // RT_MENU after the strings
    put16(d, t + 24, 1);
    put16(d, t + 30, 0x280 >> 4);
    put16(d, t + 32, 0x40 >> 4);
    put16(d, t + 36, 0x30);        // named: string at table + 0x30
    put16(d, t + 42, 0);
    d[t + 0x30] = 7;
    for (int i = 0; i < 7; ++i) d[t + 0x31 + i] = uint8_t("onfield"[i]);
    size_t p = 0x280 + 4;
    auto item = [&](unsigned flags, int id, const std::string& s) {
        put16(d, p, flags); p += 2;
        if (!(flags & 0x10)) { put16(d, p, unsigned(id)); p += 2; }
        for (char c : s) d[p++] = uint8_t(c);
        d[p++] = 0;
    };
    item(0x90, 0, "ONFIELD");
    item(0x08, 1030, "W\xfcrfeln");
    item(0x90, 0, "Sub");
    item(0x80, 7, "x");
    return d;
}

int main() {
    auto texts = readNeStrings(file());
    CHECK(texts.size() == 2 && texts.count(120) && texts.count(127));
    CHECK(ansiToUtf8(texts[120]) == "Hel\xc3\x9ft" && texts[127] == "Z!");
    CHECK(ansiToUtf8(std::string("\x80\xe4")) == "\xe2\x82\xac\xc3\xa4");    // euro sign, ä

    auto throws = [](std::vector<uint8_t> d) {
        try { readNeStrings(d); } catch (const std::runtime_error&) { return true; }
        return false;
    };
    CHECK(throws({}));
    auto bad = file(); bad[0x40] = 'P';
    CHECK(throws(bad));
    bad = file(); put16(bad, 0x80 + 10, 0x2F);       // resource beyond the end
    CHECK(throws(bad));
    auto clipped = file(); clipped[0x200 + 20] = 0x3F;        // last text runs past its block: clipped
    CHECK(!throws(clipped) && readNeStrings(clipped)[127].size() == 0x40 - 21);
    bad = file(); bad.resize(0x84);                   // table truncated
    CHECK(throws(bad));

    const auto menu = readNeMenu(menuFile(), "ONFIELD");
    CHECK(menu.size() == 2 && menu[0].id == 1030 && menu[0].flags == 0x08 && menu[0].text == "W\xfcrfeln");
    CHECK(menu[1].flags == 0x90 && menu[1].text == "Sub");
    CHECK(readNeMenu(menuFile(), "MAIN").empty());
    auto menuThrows = [](std::vector<uint8_t> d) {
        try { readNeMenu(d, "ONFIELD"); } catch (const std::runtime_error&) { return true; }
        return false;
    };
    auto broken = menuFile(); broken[0x280 + 4 + 2 + 8 + 4 + 10] = 'y';  // overwrite a terminator
    for (size_t i = 0x280 + 4; i < 0x2C0; ++i) if (broken[i] == 0 && i > 0x2A0) broken[i] = 'z';
    CHECK(menuThrows(broken));

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("ne_strings_test: OK");
    return failures != 0;
}
