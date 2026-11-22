// Synthetic and malformed-input tests for the SBSF parser.
#include "sbs.hpp"

#include <cstdio>
#include <cstring>
#include <stdexcept>

using dtvr::parseSbs;
using Bytes = std::vector<uint8_t>;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static void put16(Bytes& d, size_t at, unsigned v) { d[at] = uint8_t(v); d[at + 1] = uint8_t(v >> 8); }
static void put32(Bytes& d, size_t at, unsigned v) { put16(d, at, v & 0xffff); put16(d, at + 2, v >> 16); }

// One 3x2 frame named "AB": stride 4, rows stored bottom-up.
static Bytes sample() {
    Bytes d(20 + 14 + 8 + 4 * 2, 0);
    std::memcpy(d.data(), "SBSF", 4);
    put16(d, 4, 0x100); put16(d, 6, 1); put16(d, 10, 1); put16(d, 12, 8); put16(d, 18, 20);
    d[20] = 'A'; d[21] = 'B'; put32(d, 30, 34);
    put16(d, 34, 3); put16(d, 36, 2);
    const uint8_t rows[] = {4, 5, 6, 0xEE, 1, 2, 3, 0xEE}; // bottom row first, padding 0xEE
    std::memcpy(d.data() + 42, rows, sizeof rows);
    return d;
}

static bool throws(const Bytes& d) {
    try { parseSbs(d); } catch (const std::runtime_error&) { return true; }
    return false;
}

int main() {
    const auto file = parseSbs(sample());
    CHECK(file.frames.size() == 1);
    CHECK(file.frames[0].name == "AB");
    CHECK(file.frames[0].width == 3 && file.frames[0].height == 2);
    CHECK((file.frames[0].pixels == Bytes{1, 2, 3, 4, 5, 6}));

    CHECK(throws({}));
    { auto d = sample(); d[3] = 'X'; CHECK(throws(d)); }
    for (size_t n = 0; n < sample().size(); ++n) { auto d = sample(); d.resize(n); CHECK(throws(d)); }
    { auto d = sample(); put16(d, 6, 0xffff); CHECK(throws(d)); }
    { auto d = sample(); put16(d, 18, 0xffff); CHECK(throws(d)); }
    { auto d = sample(); put16(d, 18, 2); CHECK(throws(d)); }
    { auto d = sample(); put32(d, 30, 0xfffffff0u); CHECK(throws(d)); }
    { auto d = sample(); put16(d, 34, 0xffff); put16(d, 36, 0xffff); CHECK(throws(d)); }
    { auto d = sample(); put16(d, 34, 0); put16(d, 36, 0); CHECK(!throws(d)); }
    // Name without terminator stops at 10 bytes.
    { auto d = sample(); std::memset(d.data() + 20, 'Z', 10); CHECK(parseSbs(d).frames[0].name == "ZZZZZZZZZZ"); }

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("sbs_test: OK");
    return failures != 0;
}
