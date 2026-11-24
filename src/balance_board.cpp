// SPDX-License-Identifier: GPL-3.0-only
#include "balance_board.hpp"

#include "panel.hpp"

#include <algorithm>

namespace dtvr {
namespace {

constexpr double kStartCapital = 30000.0; // DS:85BE via seg2:033A
constexpr int kLeft = 0xBB;

// seg10:368A (_ftol): truncation, the low 32 bits.
int32_t ftol(double v) { return int32_t(uint32_t(uint64_t(int64_t(v)))); }

// The DWORD at player +0x66 + 4 * k for k >= 0: +0x66 is the year's rent, from +0x6A the
// worth per year (entries above 300 are not reached in a game; read as 0 here).
int32_t yearDword(const PlayerMoney& m, int k) {
    if (k == 0) return m.yearRent;
    if (k >= 1 && k <= kWorthYears) return m.worth[size_t(k - 1)];
    return 0;
}

std::string text(const std::map<int, std::string>& texts, int id) {
    const auto it = texts.find(id);
    return it == texts.end() ? std::string() : it->second;
}

// sprintf of "%Fs" / "%s" (seg10:18DC with the name at player +1).
std::string withName(const std::string& pattern, const std::string& name) {
    for (const char* f : {"%Fs", "%s"}) {
        const size_t at = pattern.find(f);
        if (at != std::string::npos) return pattern.substr(0, at) + name + pattern.substr(at + std::string(f).size());
    }
    return pattern;
}

} // namespace

int32_t balanceGrowth(const PlayerMoney& m, int year) {
    if (year >= 2) // seg8:9CD3: +0x66 + 4 * year minus +0x62 + 4 * year
        return int32_t(uint32_t(yearDword(m, year)) - uint32_t(yearDword(m, year - 1)));
    return int32_t(uint32_t(yearDword(m, year)) - uint32_t(ftol(kStartCapital))); // seg8:9C9A
}

BalanceSheet balanceSheet(const Game& game, int slot, const std::map<int, std::string>& texts) {
    BalanceSheet sheet;
    if (slot < 0 || slot >= kPlayerSlots) return sheet;
    const PlayerMoney& m = game.economy().money(slot);
    int top = 0x3C; // seg8:9BAA
    auto add = [&](int font, int id, bool hasValue, double value, bool coloured, int advance) {
        BalanceSheet::Line line;
        line.font = font;
        line.top = top;
        line.label = text(texts, id);
        if (hasValue) {
            line.value = formatMoney(value);   // seg6:137F
            line.negative = coloured && value < 0; // fcom with DS:8786 (0.0): 0xED below 0
        }
        sheet.lines.push_back(line);
        top += advance;
    };
    // seg8:9BF7: text 8001 with the driver's name, font 0, colour 0x45; top + 0x2D.
    BalanceSheet::Line title;
    title.font = 0;
    title.top = top;
    title.label = withName(text(texts, 8001), game.playerNames()[size_t(slot)]);
    sheet.lines.push_back(title);
    top += 0x2D;
    add(1, 8002, true, balanceGrowth(m, game.calendar().year), true, 0x19); // seg8:9D11
    add(1, 8003, true, m.yearRent, false, 0x19);                            // seg8:9DC4: always 0x45
    add(1, 8007, true, m.creditDebt, true, 0x37);                           // seg8:9E5D: +0x51A
    add(1, 8004, true, m.propertyValue, true, 0x19);                        // seg8:9F2A: +0x5A
    add(1, 8005, true, m.cash, true, 0x23);                                 // seg8:9FF2: +0x42
    add(2, 8006, true, m.cash + m.propertyValue, true, 0);                  // seg8:A0EF
    return sheet;
}

BalanceGraph balanceGraph(const Game& game) {
    // seg8:9942: the pens of the eight slots (COLORREF 0x02BBGGRR).
    static constexpr uint32_t kPens[8] = {0x0237530B, 0x022F2F2F, 0x020000C3, 0x026F6F73,
                                          0x021B53DB, 0x029F5B07, 0x0293379F, 0x024F6387};
    static constexpr char kFrames[8] = {'D', 'F', 'E', 'M', 'U', 'J', 'I', 'V'}; // seg2:2D43
    BalanceGraph g;
    const int years = std::min(game.calendar().year, kWorthYears); // DS:A102
    std::vector<int> active; // seg8:A22E / A32E: slots with a status byte (+0) != 0
    for (int p = 0; p < kPlayerSlots && size_t(p) < game.players().size(); ++p)
        if (game.players()[size_t(p)].status) active.push_back(p);
    // seg8:A1EE: maximum from 30000, minimum from 0.0 (DS:8786) over every worth shown.
    double hi = kStartCapital, lo = 0.0;
    for (int p : active)
        for (int i = 0; i < years; ++i) {
            const double v = game.economy().money(p).worth[size_t(i)];
            if (v > hi) hi = v;
            if (v < lo) lo = v;
        }
    const double scale = 190.0 / (hi - lo); // seg8:A2FC: DS:878E / (max - min)
    auto yOf = [&](double v) { return int(ftol((hi - v) * scale + 67.0)); }; // DS:8796
    int legend = 0;
    for (int p : active) {
        BalanceGraph::Polyline line;
        line.slot = p;
        line.colour = {uint8_t(kPens[p]), uint8_t(kPens[p] >> 8), uint8_t(kPens[p] >> 16)};
        line.points.push_back({kLeft, yOf(kStartCapital)}); // seg8:A36F MoveTo
        for (int i = 0; i < years; ++i) // seg8:A3B2: x = 0xBB + (i + 1) * 0xEB / DS:A102
            line.points.push_back({kLeft + int((int32_t(i + 1) * 0xEB) / years), yOf(game.economy().money(p).worth[size_t(i)])});
        g.players.push_back(line);
        g.legend.push_back({kFrames[p], {0xC0 + legend, 0x107}}); // seg8:A45E
        legend += 0x1E;
    }
    for (int k = 0; k < 0xBE; k += 5) // seg8:A4EE: ticks on the vertical axis
        g.ticks.push_back({{0xB7, k + 0x43}, {0xBF, k + 0x43}});
    for (int k = 0; k < 0xBE; k += 5) // seg8:A531: ticks along the zero line
        g.ticks.push_back({{k + kLeft, int(ftol(scale * hi + 64.0))}, {k + kLeft, int(ftol(scale * hi + 72.0))}});
    g.axes.push_back({{kLeft, 0x43}, {kLeft, 0x101}});                                    // seg8:A5D2
    g.axes.push_back({{kLeft, int(ftol(scale * hi + 68.0))}, {0x1A6, int(ftol(scale * hi + 68.0))}}); // seg8:A5EF
    return g;
}

} // namespace dtvr
