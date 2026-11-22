// SPDX-License-Identifier: GPL-3.0-only
// Tests for the SBBUTTON group: press / release, disabled buttons, focus walk.
#include "sb_button.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    // Two rows: ids 1, 2 at y 0; id 3 below id 1; id 2 disabled.
    SbButtonGroup g;
    g.reset({{1, {0, 0, 10, 10}, true}, {2, {20, 0, 10, 10}, false}, {3, {0, 20, 10, 10}, true}}, 3);
    CHECK(g.focus() == 3);
    g.reset(g.buttons(), 9); // unknown focus id: none
    CHECK(g.focus() == -1);
    CHECK(g.key(SDLK_RIGHT, 0, true).sound == -1); // nothing focused, nothing moves

    // Press and release on the same enabled button activates it with Plopp02.
    auto in = g.mouseDown(5, 5);
    CHECK(in.id == -1 && in.sound == -1 && g.focus() == 1);
    CHECK(g.pictureRect(g.buttons()[0]).x == 1 && g.pictureRect(g.buttons()[0]).y == 1);
    in = g.mouseUp(6, 6);
    CHECK(in.id == 1 && in.sound == 2);
    CHECK(g.pictureRect(g.buttons()[0]).x == 0);
    // Released outside: nothing.
    g.mouseDown(5, 5);
    CHECK(g.mouseUp(50, 50).id == -1);
    // A disabled button takes the focus, sounds 7 and is never activated.
    in = g.mouseDown(25, 5);
    CHECK(in.sound == 7 && g.focus() == 2);
    CHECK(g.mouseUp(25, 5).id == -1);
    CHECK(g.key(SDLK_SPACE, 0, true).id == -1);

    // Arrow walk: left from 2 to 1 (sound 8), down from 1 to 3, right from 3 nowhere.
    in = g.key(SDLK_LEFT, 0, true);
    CHECK(in.sound == 8 && g.focus() == 1);
    g.key(SDLK_DOWN, 0, true);
    CHECK(g.focus() == 3);
    CHECK(g.key(SDLK_RIGHT, 0, true).sound == -1 && g.focus() == 3);
    // Space activates, Return only when allowed, Escape / Backspace cancel.
    CHECK(g.key(SDLK_SPACE, 0, false).id == 3);
    CHECK(g.key(SDLK_RETURN, 0, false).id == -1 && g.key(SDLK_RETURN, 0, true).id == 3);
    CHECK(g.key(SDLK_ESCAPE, 0, true).cancel && g.key(SDLK_BACKSPACE, 0, true).cancel);
    g.setEnabled(2, true);
    g.setFocus(2);
    CHECK(g.key(SDLK_SPACE, 0, true).id == 2);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("sb_button_test: OK");
    return failures != 0;
}
