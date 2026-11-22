// SPDX-License-Identifier: GPL-3.0-only
// Synthetic and malformed-input tests for europe.inf / europe.str / *.tow parsing.
#include "board.hpp"

#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>

using namespace dtvr;
using Bytes = std::vector<uint8_t>;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static void put16(Bytes& d, size_t at, unsigned v) { d[at] = uint8_t(v); d[at + 1] = uint8_t(v >> 8); }

static bool throws(const std::function<void()>& f) {
    try { f(); } catch (const std::runtime_error&) { return true; }
    return false;
}

// Two fields: 0 at (3,4) links right to 1 at (5,4); field 1 is a town with a list.
static Bytes sampleStr() {
    Bytes d(32 + 16 + 4, 0);
    d[0] = 3; d[1] = 4;
    for (int k = 0; k < 4; ++k) { put16(d, 2 + k * 2, 0xFFFF); put16(d, 18 + k * 2, 0xFFFF); }
    put16(d, 4, 1);           // field 0 right -> 1
    d[16] = 5; d[17] = 4;
    put16(d, 24, 0);          // field 1 left -> 0
    put16(d, 26, 6); put16(d, 28, 32); put16(d, 30, 0x4000);
    std::memcpy(d.data() + 32, "Ort", 3);
    put16(d, 46, 2); put16(d, 48, 7); put16(d, 50, 9);
    return d;
}

int main() {
    CHECK(throws([] { parseBoardInfo(Bytes(3)); }));
    { Bytes inf{0xBC, 0x02, 0x73, 0x00}; auto i = parseBoardInfo(inf); CHECK(i.fields == 700 && i.towns == 115); }

    const auto board = parseBoard(sampleStr(), 2);
    CHECK(board.fields.size() == 2);
    CHECK(board.fields[0].x == 3 && board.fields[0].y == 4 && board.fields[0].next[1] == 1);
    CHECK(board.fields[1].next[3] == 0 && board.fields[1].kind == 6 && board.fields[1].flags == 0x4000);
    CHECK(board.lists.count(32) && board.lists.at(32).name == "Ort");
    CHECK((board.lists.at(32).values == std::vector<uint16_t>{7, 9}));

    CHECK(throws([] { parseBoard(sampleStr(), 4); }));                                    // table too short
    CHECK(throws([] { auto d = sampleStr(); put16(d, 4, 5); parseBoard(d, 2); }));        // neighbour out of range
    CHECK(throws([] { auto d = sampleStr(); put16(d, 24, 0xFFFF); parseBoard(d, 2); }));  // one-sided link
    CHECK(throws([] { auto d = sampleStr(); put16(d, 46, 9); parseBoard(d, 2); }));       // list truncated
    CHECK(throws([] { auto d = sampleStr(); d.resize(40); parseBoard(d, 2); }));          // list header truncated
    CHECK(throws([] { auto d = sampleStr(); put16(d, 28, 34); parseBoard(d, 2); }));      // offset not a list start
    { auto d = sampleStr(); put16(d, 28, uint16_t(d.size())); CHECK(!throws([&] { parseBoard(d, 2); })); } // EOF = empty

    Bytes tow(0x176, 0);
    put16(tow, 0, 86); put16(tow, 2, 96);
    std::memcpy(tow.data() + 6, "B\x94rse", 5);
    std::memcpy(tow.data() + 0x22, "Caf\x82", 4);
    put16(tow, 0x22 + 0x18, 0x86A0); put16(tow, 0x22 + 0x1A, 0x0001); put16(tow, 0x22 + 0x20, 100);
    const auto towns = parseTowns(tow, 1);
    CHECK(towns[0].x == 86 && towns[0].y == 96);
    CHECK(oemToUtf8(towns[0].name) == "B\xC3\xB6rse");
    CHECK(oemToUtf8(towns[0].entries[0].name) == "Caf\xC3\xA9");
    CHECK(towns[0].entries[0].value == 100000 && towns[0].entries[0].words[2] == 100);
    CHECK(throws([&] { parseTowns(tow, 2); }));
    // Name without terminator stops at its field size.
    std::memset(tow.data() + 6, 'A', 28);
    CHECK(parseTowns(tow, 1)[0].name.size() == 28);

    // Finish photos (seg7:28AD): eight characters, umlauts and spaces mapped.
    CHECK(finishPhotoName("Br\x81ssel") == "Brussel" && finishPhotoName("San Marino") == "San_Mari");
    CHECK(finishPhotoName("K\x94ln") == "Koln" && finishPhotoName("Amsterdam") == "Amsterda");

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("board_test: OK");
    return failures != 0;
}
