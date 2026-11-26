// SPDX-License-Identifier: GPL-3.0-only
// The SBINPUT name rules, the recomp on-screen keyboard and the stage count steps.
#include "osk.hpp"
#include "stage_count.hpp"

#include <cstdio>
#include <set>

using namespace dtvr;
using Kind = ScreenKeyboard::Result::Kind;
using Move = ScreenKeyboard::Move;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

namespace {

void nameRules() {
    std::string name;
    CHECK(appendNameText(name, "Ab 1", 18) == 4 && name == "Ab 1");
    CHECK(appendNameText(name, "-_.!?", 18) == 0 && name == "Ab 1"); // not alphanumeric (seg1:1F35)
    CHECK(appendNameText(name, "äß", 18) == 2 && utf8Length(name) == 6); // Windows-1252 letters
    std::string full(18, 'x');
    CHECK(appendNameText(full, "y", 18) == 0 && full.size() == 18);  // seg1:1F27: at the limit
    std::string limit(17, 'x');
    CHECK(appendNameText(limit, "üz", 18) == 1 && utf8Length(limit) == 18 && limit.back() != 'z');
    std::string erase = "aö";
    CHECK(eraseNameChar(erase) == 8 && erase == "a");                  // a whole UTF-8 character
    CHECK(eraseNameChar(erase) == 8 && erase.empty());
    CHECK(eraseNameChar(erase) == 7 && erase.empty());                 // seg1:1FD8: Zaehler2
}

// Fix A: the field holds 19 characters (seg1:1D28), PLAYER keeps 18 (seg1:0BB7).
void fieldLimit() {
    CHECK(kNameFieldLimit == 19 && kPlayerNameMax == 18);
    std::string field(18, 'x');
    CHECK(appendNameText(field, "\xc3\xa4z", kNameFieldLimit) == 1 && utf8Length(field) == 19); // the 19th: ä
    CHECK(appendNameText(field, "y", kNameFieldLimit) == 0 && utf8Length(field) == 19);
    CHECK(utf8Prefix(field, kPlayerNameMax) == std::string(18, 'x'));          // the ä is cut whole
    CHECK(utf8Prefix("\xc3\xa4\xc3\xb6", 1) == "\xc3\xa4" && utf8Prefix("ab", 5) == "ab" && utf8Prefix("ab", 0).empty());
    ScreenKeyboard k;
    k.open(std::string(18, 'x'), kNameFieldLimit);
    CHECK(k.press().kind == Kind::Typed && utf8Length(k.text()) == 19);       // Q as the 19th
    CHECK(k.press().kind == Kind::Refused && utf8Length(k.text()) == 19);
}

// Fix B: IsCharAlphaNumeric (seg1:1F35) on the Windows-1252 byte of each character.
void ansiClasses() {
    CHECK(ansiFromCodePoint(U'A') == 'A' && ansiFromCodePoint(0xE4) == 0xE4);
    CHECK(ansiFromCodePoint(0x20AC) == 0x80 && ansiFromCodePoint(0x0160) == 0x8A && ansiFromCodePoint(0x0178) == 0x9F);
    CHECK(ansiFromCodePoint(0x0142) == -1 && ansiFromCodePoint(0x1F600) == -1 && ansiFromCodePoint(0x0100) == -1);
    int alnum = 0;
    for (int c = 0; c < 256; ++c) alnum += isCharAlphaNumeric1252(static_cast<unsigned char>(c));
    CHECK(alnum == 10 + 52 + 8 + 3 + 23 + 31 + 8); // digits, ASCII, 0x8x/0x9x, ª µ º, À-Ö, Ø-ö, ø-ÿ
    CHECK(!isCharAlphaNumeric1252(0xD7) && !isCharAlphaNumeric1252(0xF7) && !isCharAlphaNumeric1252(0xB2));
    CHECK(!isCharAlphaNumeric1252(0x80) && !isCharAlphaNumeric1252(0xA0) && !isCharAlphaNumeric1252(0x20));
    CHECK(isCharAlphaNumeric1252(0xDF) && isCharAlphaNumeric1252(0xFF) && isCharAlphaNumeric1252(0x9C));
    std::string name;
    CHECK(appendNameText(name, "\xc5\xa0\xc5\x93\xc2\xb5", 19) == 3);              // Š œ µ: letters
    CHECK(appendNameText(name, "\xc3\x97\xc3\xb7\xc2\xb2\xe2\x82\xac\xc2\xa0", 19) == 0); // × ÷ ² € NBSP
    CHECK(appendNameText(name, "\xc5\x82\xf0\x9f\x98\x80\xd0\x96", 19) == 0);     // ł, emoji, Ж: not in 1252
    CHECK(appendNameText(name, "\x80\xff\xc3", 19) == 0 && name == "\xc5\xa0\xc5\x93\xc2\xb5"); // malformed
    CHECK(appendNameText(name, "\t\n", 19) == 0 && appendNameText(name, " ", 19) == 1); // 0x20 only
}

void grid() {
    ScreenKeyboard k;
    // Every column of every row is covered by exactly one cell; every key is reachable.
    for (int row = 0; row < ScreenKeyboard::kRows; ++row)
        for (int col = 0; col < ScreenKeyboard::kColumns; ++col) CHECK(k.cellAt(row, col) >= 0);
    CHECK(k.cellAt(5, 0) < 0 && k.cellAt(0, 11) < 0);
    std::set<int> seen;
    k.open("", 18);
    for (int i = 0; i < 200; ++i) { // a walk of Right and Down visits every cell
        seen.insert(k.focus());
        k.move(i % 13 == 12 ? Move::Down : Move::Right);
    }
    CHECK(seen.size() == k.cells().size());
    // Only characters the name field accepts are offered.
    for (const auto& c : k.cells()) {
        if (c.kind != ScreenKeyboard::Kind::Char && c.kind != ScreenKeyboard::Kind::Space) continue;
        std::string t;
        CHECK(appendNameText(t, c.lower, 18) == 1 && appendNameText(t, c.upper, 18) == 1);
    }
}

void typing() {
    ScreenKeyboard k;
    k.open("", 18);
    CHECK(k.shift() && k.label(k.cells()[size_t(k.focus())]) == "Q"); // starts on Q, capitals
    auto r = k.press();
    CHECK(r.kind == Kind::Typed && r.sound == 2 && k.text() == "Q" && !k.shift()); // one capital
    CHECK(k.move(Move::Right) && k.press().kind == Kind::Typed && k.text() == "Qw");
    CHECK(k.move(Move::Left) && k.move(Move::Left) && k.label(k.cells()[size_t(k.focus())]) == "ü"); // wraps
    CHECK(k.move(Move::Up) && k.cells()[size_t(k.focus())].row == 0);
    CHECK(k.move(Move::Up) && k.cells()[size_t(k.focus())].kind == ScreenKeyboard::Kind::Done); // wraps to the last row
    CHECK(k.move(Move::Down) && k.cells()[size_t(k.focus())].row == 0 && k.cells()[size_t(k.focus())].column == 10);
    r = k.erase();
    CHECK(r.kind == Kind::Erased && r.sound == 8 && k.text() == "Q");
    k.erase();
    CHECK(k.text().empty() && k.shift()); // empty again: capitals
    r = k.erase();
    CHECK(r.kind == Kind::Refused && r.sound == 7);
    CHECK(k.toggleShift().kind == Kind::Shift && !k.shift());
    CHECK(k.type("a b!").kind == Kind::Typed && k.text() == "a b"); // a real keyboard, '!' dropped
    // The limit: the 19th character is refused.
    k.open(std::string(18, 'x'), 18);
    CHECK(!k.shift() && k.press().kind == Kind::Refused && k.text().size() == 18);
    // Space, Shift, Erase, Cancel and Done through the grid.
    k.open("Ab", 18);
    k.setFocus(k.cellAt(4, 5));
    CHECK(k.press().kind == Kind::Typed && k.text() == "Ab ");
    k.setFocus(k.cellAt(3, 1));
    CHECK(k.press().kind == Kind::Shift && k.shift());
    k.setFocus(k.cellAt(3, 10));
    CHECK(k.press().kind == Kind::Erased && k.text() == "Ab");
    k.setFocus(k.cellAt(4, 0));
    CHECK(k.press().kind == Kind::Cancel);
    k.setFocus(k.cellAt(4, 10));
    CHECK(k.press().kind == Kind::Done && k.text() == "Ab");
    // Up / Down keep the column across the wide keys.
    k.setFocus(k.cellAt(1, 4)); // T
    CHECK(k.move(Move::Right) && k.move(Move::Down) && k.move(Move::Down) && k.move(Move::Down));
    CHECK(k.cells()[size_t(k.focus())].kind == ScreenKeyboard::Kind::Space);
    CHECK(k.move(Move::Up) && k.label(k.cells()[size_t(k.focus())]).size() == 1 && k.cells()[size_t(k.focus())].column == 5);
}

void stageCount() {
    CHECK(parseStageCount("99") == 99 && parseStageCount(" 12x") == 12 && parseStageCount("12345") == 1234);
    CHECK(parseStageCount("-5") == -5 && parseStageCount("") == 0);
    CHECK(stepStageCount("99", 99, 1) == 100 && stepStageCount("99", 99, -10) == 89);
    CHECK(stepStageCount("300", 300, 1) == 300 && stepStageCount("1", 1, -50) == 1);   // the slider's range
    CHECK(stepStageCount("290", 290, 50) == 300 && stepStageCount("30", 30, -50) == 1);
    CHECK(stepStageCount("", 42, 1) == 43 && stepStageCount("0", 42, -1) == 41);       // invalid field: current
    CHECK(stepStageCount("999", 120, 10) == 130 && stepStageCount("abc", 0, 1) == 2);
}

} // namespace

int main() {
    nameRules();
    fieldLimit();
    ansiClasses();
    grid();
    typing();
    stageCount();
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("osk_test: OK");
    return failures != 0;
}
