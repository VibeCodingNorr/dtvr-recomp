// SPDX-License-Identifier: GPL-3.0-only
// Text entry without a keyboard, SDL-free so it can be tested:
//   name rules   the SBINPUT field of rally.exe (seg1:1C71, WM_CHAR seg1:1F00, VK_BACK
//                seg1:1FB3) as the player names of PLAYER use it;
//   ScreenKeyboard  RECOMP ADDITION (no counterpart in rally.exe): the grid, cursor and
//                text of the on-screen keyboard that a game controller types names with
//                (src/osk_view.cpp draws it). Evidence and mapping in docs/formats.md
//                "Recomp pad layer".
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace dtvr {

// SBINPUT's length limit: WM_CREATE stores 0x13 in window word +0xC (seg1:1D28); WM_CHAR
// compares the text length with it (seg1:1F27), so a field holds and shows 19 characters.
// No caller sends the limit message 0x415 (seg1:2360 -> 1DB0), PLAYER neither.
constexpr size_t kNameFieldLimit = 0x13;
// PLAYER reads a field with GetWindowText(..., 0x13) (seg1:0BB7), which SBINPUT's
// WM_GETTEXT answers with lstrcpyn (seg1:1EF8): the player keeps 18 characters.
constexpr size_t kPlayerNameMax = 0x13 - 1;

// Characters (not bytes) of a UTF-8 string.
size_t utf8Length(const std::string& text);
// The first `maxChars` characters of a UTF-8 string (lstrcpyn on characters).
std::string utf8Prefix(const std::string& text, size_t maxChars);

// The Windows-1252 byte of a Unicode code point, -1 when the code page has none (such a
// character never reaches the window as WM_CHAR of the German Windows).
int ansiFromCodePoint(char32_t codePoint);
// USER.IsCharAlphaNumeric for a Windows-1252 byte (rally.exe calls it at seg1:1F35; the
// table belongs to Windows, not to rally.exe). Rebuilt as the Unicode letters and decimal
// digits of the code page: 0-9, A-Z, a-z, 0x83 0x8A 0x8C 0x8E 0x9A 0x9C 0x9E 0x9F, 0xAA
// 0xB5 0xBA, 0xC0-0xD6, 0xD8-0xF6, 0xF8-0xFF. Unverified: the superscripts 0xB2 0xB3
// 0xB9 count as no digit, and 0x8E / 0x9E (missing in the code page of Windows 3.1) as
// letters.
bool isCharAlphaNumeric1252(unsigned char ansi);

// WM_CHAR of SBINPUT (seg1:1F00): a character is appended when the text is shorter than
// `maxChars` (seg1:1F27) and IsCharAlphaNumeric accepts it or it is a space, 0x20
// (seg1:1F35 / 1F41); anything else is dropped without a sound (seg1:1FAA). `utf8` is
// checked per character in Windows-1252. Returns the number of characters appended.
size_t appendNameText(std::string& name, const std::string& utf8, size_t maxChars);

// VK_BACK of SBINPUT (seg1:1FB3): removes the last character and returns the sound for
// channel 2, 8 (seg1:1FFB); on an empty text nothing changes and it is 7, Zaehler2 (seg1:1FD8).
int eraseNameChar(std::string& name);

class ScreenKeyboard {
public:
    enum class Kind { Char, Shift, Erase, Space, Cancel, Done };
    struct Cell {
        Kind kind = Kind::Char;
        std::string lower, upper; // Kind::Char: what is typed (UTF-8)
        int row = 0, column = 0, span = 1; // grid position in kColumns columns
    };
    static constexpr int kColumns = 11, kRows = 5;

    ScreenKeyboard();
    // Starts with `text` (the field's name), the caret after it and the focus on the
    // first letter key.
    void open(std::string text, size_t maxChars);
    const std::string& text() const { return text_; }
    // RECOMP: the text caret in characters (0 .. utf8Length(text())). Typing inserts at
    // it and erasing removes the character before it, with SBINPUT's rules on the whole
    // text; at the end (where it opens) this is exactly SBINPUT's append / VK_BACK.
    size_t caret() const { return caret_; }
    // Moves the caret by one character; false at either end.
    bool moveCaret(bool right);
    const std::vector<Cell>& cells() const { return cells_; }
    int focus() const { return focus_; }
    bool shift() const { return shift_; }
    size_t maxChars() const { return max_; }
    // What a cell shows and types with the current shift state.
    const std::string& label(const Cell& cell) const { return shift_ ? cell.upper : cell.lower; }

    enum class Move { Up, Down, Left, Right };
    // Left / Right go to the neighbour in the row, Up / Down to the cell over the same
    // column; all wrap around. Returns false when the focus stayed (never in this grid).
    bool move(Move direction);
    void setFocus(int cell) { if (cell >= 0 && cell < int(cells_.size())) focus_ = cell; }
    int cellAt(int row, int column) const; // -1 outside the grid
    int cellOf(Kind kind) const;           // the first cell of `kind`, -1 none

    // `sound` for channel 2 (-1 none) as the SBBUTTON / SBINPUT originals would play it.
    struct Result {
        enum class Kind { None, Typed, Refused, Erased, Shift, Done, Cancel, Moved } kind = Kind::None;
        int sound = -1;
    };
    Result press();        // the focused key (A / Return / a click)
    Result erase();        // X / Backspace
    Result toggleShift();  // Y
    Result type(const std::string& utf8); // a real keyboard's characters (SDL_TEXTINPUT)
    // RECOMP pad shortcuts (docs/formats.md "On-screen keyboard feel"): the caret one
    // character left / right (sound 8 like a move, none at an end), a space at the
    // caret (as the key Leerzeichen) and the focus to the key Fertig (sound 8).
    Result caretStep(bool right);
    Result space();
    Result focusDone();

private:
    std::vector<Cell> cells_;
    std::string text_;
    size_t caret_ = 0; // characters before the caret
    size_t max_ = kNameFieldLimit;
    int focus_ = 0;
    int column_ = 0; // the column Up / Down keep
    bool shift_ = true; // names start with a capital letter
};

} // namespace dtvr
