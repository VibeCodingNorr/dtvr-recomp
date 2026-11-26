// SPDX-License-Identifier: GPL-3.0-only
#include "osk.hpp"

#include <utility>

namespace dtvr {

size_t utf8Length(const std::string& text) {
    size_t n = 0;
    for (unsigned char c : text) n += (c & 0xC0) != 0x80;
    return n;
}

std::string utf8Prefix(const std::string& text, size_t maxChars) {
    size_t chars = 0;
    for (size_t i = 0; i < text.size(); ++i)
        if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80 && chars++ == maxChars) return text.substr(0, i);
    return text;
}

namespace {

// 0x80-0x9F of Windows-1252 (as ansiToUtf8 in ne_strings.cpp); 0 marks an unused byte.
constexpr char16_t kAnsiHigh[32] = {
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039,
    0x0152, 0, 0x017D, 0, 0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178};

// One code point from UTF-8 at `i` (advanced past it); U+FFFD for a malformed sequence.
char32_t decodeUtf8(const std::string& s, size_t& i) {
    const unsigned char lead = static_cast<unsigned char>(s[i++]);
    int more = lead < 0x80 ? 0 : (lead & 0xE0) == 0xC0 ? 1 : (lead & 0xF0) == 0xE0 ? 2 : (lead & 0xF8) == 0xF0 ? 3 : -1;
    if (more < 0) { // a stray continuation byte or an invalid lead: skip its tail too
        while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
        return 0xFFFD;
    }
    char32_t cp = more == 0 ? lead : lead & (0x3F >> more);
    for (; more > 0; --more) {
        if (i >= s.size() || (static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) return 0xFFFD;
        cp = cp << 6 | (static_cast<unsigned char>(s[i++]) & 0x3F);
    }
    return cp;
}

} // namespace

int ansiFromCodePoint(char32_t cp) {
    if (cp < 0x80 || (cp >= 0xA0 && cp <= 0xFF)) return int(cp);
    for (int i = 0; i < 32; ++i)
        if (kAnsiHigh[i] != 0 && kAnsiHigh[i] == cp) return 0x80 + i;
    return -1;
}

bool isCharAlphaNumeric1252(unsigned char c) {
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return true;
    switch (c) {
    case 0x83: case 0x8A: case 0x8C: case 0x8E: case 0x9A: case 0x9C: case 0x9E: case 0x9F:
    case 0xAA: case 0xB5: case 0xBA:
        return true;
    default:
        return c >= 0xC0 && c != 0xD7 && c != 0xF7;
    }
}

size_t appendNameText(std::string& name, const std::string& utf8, size_t maxChars) {
    size_t added = 0;
    for (size_t i = 0; i < utf8.size();) {
        const size_t begin = i;
        const char32_t cp = decodeUtf8(utf8, i);
        if (utf8Length(name) >= maxChars) break;             // seg1:1F27: length = limit, ignored
        const int ansi = ansiFromCodePoint(cp);
        if (ansi < 0) continue;                               // no WM_CHAR in Windows-1252
        if (!isCharAlphaNumeric1252(static_cast<unsigned char>(ansi)) && ansi != 0x20) continue; // seg1:1F35 / 1F41
        name.append(utf8, begin, i - begin);
        ++added;
    }
    return added;
}

int eraseNameChar(std::string& name) {
    if (name.empty()) return 7; // seg1:1FD0: Zaehler2 on channel 2
    while (!name.empty() && (static_cast<unsigned char>(name.back()) & 0xC0) == 0x80) name.pop_back();
    if (!name.empty()) name.pop_back();
    return 8; // seg1:1FFB
}

namespace {

ScreenKeyboard::Cell character(const char* lower, const char* upper, int row, int column) {
    ScreenKeyboard::Cell c;
    c.lower = lower;
    c.upper = upper;
    c.row = row;
    c.column = column;
    return c;
}

ScreenKeyboard::Cell special(ScreenKeyboard::Kind kind, int row, int column, int span) {
    ScreenKeyboard::Cell c;
    c.kind = kind;
    c.row = row;
    c.column = column;
    c.span = span;
    return c;
}

} // namespace

// German typewriter order (QWERTZ) with the umlauts; only characters SBINPUT accepts.
ScreenKeyboard::ScreenKeyboard() {
    static const char* const kDigits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
    for (int i = 0; i < 10; ++i) cells_.push_back(character(kDigits[i], kDigits[i], 0, i));
    cells_.push_back(character("ß", "ß", 0, 10)); // ß has no capital in Windows-1252
    static const char* const kRow1[][2] = {{"q", "Q"}, {"w", "W"}, {"e", "E"}, {"r", "R"}, {"t", "T"}, {"z", "Z"},
                                           {"u", "U"}, {"i", "I"}, {"o", "O"}, {"p", "P"}, {"ü", "Ü"}};
    for (int i = 0; i < 11; ++i) cells_.push_back(character(kRow1[i][0], kRow1[i][1], 1, i));
    static const char* const kRow2[][2] = {{"a", "A"}, {"s", "S"}, {"d", "D"}, {"f", "F"}, {"g", "G"}, {"h", "H"},
                                           {"j", "J"}, {"k", "K"}, {"l", "L"}, {"ö", "Ö"}, {"ä", "Ä"}};
    for (int i = 0; i < 11; ++i) cells_.push_back(character(kRow2[i][0], kRow2[i][1], 2, i));
    cells_.push_back(special(Kind::Shift, 3, 0, 2));
    static const char* const kRow3[][2] = {{"y", "Y"}, {"x", "X"}, {"c", "C"}, {"v", "V"}, {"b", "B"}, {"n", "N"}, {"m", "M"}};
    for (int i = 0; i < 7; ++i) cells_.push_back(character(kRow3[i][0], kRow3[i][1], 3, 2 + i));
    cells_.push_back(special(Kind::Erase, 3, 9, 2));
    cells_.push_back(special(Kind::Cancel, 4, 0, 3));
    Cell space = special(Kind::Space, 4, 3, 5);
    space.lower = space.upper = " ";
    cells_.push_back(space);
    cells_.push_back(special(Kind::Done, 4, 8, 3));
}

void ScreenKeyboard::open(std::string text, size_t maxChars) {
    text_ = std::move(text);
    max_ = maxChars;
    focus_ = cellAt(1, 0);
    column_ = 0;
    shift_ = text_.empty();
}

int ScreenKeyboard::cellAt(int row, int column) const {
    for (size_t i = 0; i < cells_.size(); ++i) {
        const Cell& c = cells_[i];
        if (c.row == row && column >= c.column && column < c.column + c.span) return int(i);
    }
    return -1;
}

bool ScreenKeyboard::move(Move direction) {
    const Cell& from = cells_[size_t(focus_)];
    int next = -1;
    if (direction == Move::Left || direction == Move::Right) {
        const int column = direction == Move::Left ? (from.column + kColumns - 1) % kColumns
                                                   : (from.column + from.span) % kColumns;
        next = cellAt(from.row, column);
        if (next >= 0) column_ = cells_[size_t(next)].column;
    } else {
        // Keep the remembered column when it lies inside the current cell, else its start.
        if (column_ < from.column || column_ >= from.column + from.span) column_ = from.column;
        const int row = direction == Move::Up ? (from.row + kRows - 1) % kRows : (from.row + 1) % kRows;
        next = cellAt(row, column_);
    }
    if (next < 0 || next == focus_) return false;
    focus_ = next;
    return true;
}

ScreenKeyboard::Result ScreenKeyboard::type(const std::string& utf8) {
    Result r;
    if (appendNameText(text_, utf8, max_) == 0) {
        r.kind = Result::Kind::Refused;
        r.sound = 7; // recomp: Zaehler2 as the refused key (the field itself stays silent)
        return r;
    }
    r.kind = Result::Kind::Typed;
    return r;
}

ScreenKeyboard::Result ScreenKeyboard::erase() {
    Result r;
    r.sound = eraseNameChar(text_);
    r.kind = r.sound == 8 ? Result::Kind::Erased : Result::Kind::Refused;
    if (text_.empty()) shift_ = true;
    return r;
}

ScreenKeyboard::Result ScreenKeyboard::toggleShift() {
    shift_ = !shift_;
    Result r;
    r.kind = Result::Kind::Shift;
    r.sound = 2; // Plopp02 like an SBBUTTON (seg1:0FA5)
    return r;
}

ScreenKeyboard::Result ScreenKeyboard::press() {
    const Cell& cell = cells_[size_t(focus_)];
    Result r;
    switch (cell.kind) {
    case Kind::Shift: return toggleShift();
    case Kind::Erase: return erase();
    case Kind::Cancel: r.kind = Result::Kind::Cancel; r.sound = 2; return r;
    case Kind::Done: r.kind = Result::Kind::Done; r.sound = 2; return r;
    case Kind::Space: case Kind::Char: break;
    }
    r = type(label(cell));
    if (r.kind == Result::Kind::Typed) {
        r.sound = 2;                                   // the key as an SBBUTTON (seg1:0FA5)
        if (cell.kind == Kind::Char && cell.lower != cell.upper) shift_ = false; // one capital
    }
    return r;
}

} // namespace dtvr
