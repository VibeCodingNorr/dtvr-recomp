// SPDX-License-Identifier: GPL-3.0-only
// Tests for the SBBUTTON group: press / release, disabled buttons, the focus walk
// over the siblings' z-order (seg1:2474 ff.) and SBINPUT fields in it (seg1:21E5).
#include "sb_button.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static void pressAndRelease() {
    // z-order 1, 2, 3: ids 1, 2 at y 0; id 3 below id 1; id 2 disabled.
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
    // Space activates, Return only when allowed, Escape / Backspace cancel.
    g.setFocus(3);
    CHECK(g.key(SDLK_SPACE, 0, false).id == 3);
    CHECK(g.key(SDLK_RETURN, 0, false).id == -1 && g.key(SDLK_RETURN, 0, true).id == 3);
    CHECK(g.key(SDLK_ESCAPE, 0, true).cancel && g.key(SDLK_BACKSPACE, 0, true).cancel);
    g.setEnabled(2, true);
    g.setFocus(2);
    CHECK(g.key(SDLK_SPACE, 0, true).id == 2);
}

static void siblingWalk() {
    // A row as SELPLAYER lays it out (seg2:53BD): created slot 0 first, on the right.
    // z-order 0, 1, 2 (the first created at the top) at x 200, 100, 0.
    SbButtonGroup g;
    g.reset({{0, {200, 5, 90, 90}, true}, {1, {100, 5, 90, 90}, true}, {2, {0, 5, 90, 90}, true}}, 2);
    // Right walks GW_HWNDPREV (towards the top) to the same top: 2 -> 1 -> 0, then nothing.
    auto in = g.key(SDLK_RIGHT, 0, true);
    CHECK(in.sound == 8 && in.moved && g.focus() == 1);
    g.key(SDLK_RIGHT, 0, true);
    CHECK(g.focus() == 0);
    in = g.key(SDLK_RIGHT, 0, true);
    CHECK(in.sound == -1 && !in.moved && g.focus() == 0); // no wrap-around
    // Left walks GW_HWNDNEXT; Tab is Right, Shift+Tab Left (seg1:1060).
    g.key(SDLK_LEFT, 0, true);
    CHECK(g.focus() == 1);
    g.key(SDLK_TAB, KMOD_LSHIFT, true);
    CHECK(g.focus() == 2);
    g.key(SDLK_TAB, 0, true);
    CHECK(g.focus() == 1);
    // No one in the same column: down takes the adjacent sibling towards the top,
    // up the one towards the bottom (seg1:262D / 270C); none at the ends.
    g.key(SDLK_DOWN, 0, true);
    CHECK(g.focus() == 0);
    CHECK(!g.key(SDLK_DOWN, 0, true).moved && g.focus() == 0);
    g.key(SDLK_UP, 0, true);
    CHECK(g.focus() == 1);
    g.key(SDLK_UP, 0, true);
    CHECK(g.focus() == 2);
    CHECK(!g.key(SDLK_UP, 0, true).moved && g.focus() == 2);

    // Two rows of the card window: z-order slot 0 .. 3 from the top (BringWindowToTop,
    // seg3:0A26), slot 3 top left, slot 2 top right, slot 1 bottom left, slot 0 bottom right.
    g.reset({{0, {100, 100, 90, 90}, true}, {1, {0, 100, 90, 90}, true}, {2, {100, 0, 90, 90}, true},
             {3, {0, 0, 90, 90}, true}}, 0);
    g.key(SDLK_UP, 0, true); // same left towards the bottom: slot 2
    CHECK(g.focus() == 2);
    g.key(SDLK_LEFT, 0, true); // same top towards the bottom: slot 3
    CHECK(g.focus() == 3);
    CHECK(!g.key(SDLK_LEFT, 0, true).moved);
    g.key(SDLK_DOWN, 0, true); // same left towards the top: slot 1
    CHECK(g.focus() == 1);
    g.key(SDLK_RIGHT, 0, true); // same top towards the top: slot 0
    CHECK(g.focus() == 0);
    // The first match ends the search even when a nearer one lies further on.
    g.reset({{5, {0, 0, 10, 10}, true}, {6, {50, 0, 10, 10}, true}, {7, {100, 0, 10, 10}, true}}, 7);
    g.key(SDLK_RIGHT, 0, true);
    CHECK(g.focus() == 6);
}

static void fields() {
    // PLAYER's order: star, stop, then per slot the SBINPUT field and the driver.
    SbButtonGroup g;
    g.reset({{100, {500, 400, 50, 50}, true}, {101, {0, 400, 50, 50}, true},
             {201, {150, 100, 100, 20}, true, true}, {1, {50, 100, 50, 50}, true},
             {200, {150, 200, 100, 20}, true, true}, {0, {50, 200, 50, 50}, true}}, 100);
    g.setFocus(0);
    auto in = g.key(SDLK_RIGHT, 0, true); // the field of slot 0 has the same top
    CHECK(in.moved && g.focus() == 200 && g.fieldFocused());
    CHECK(g.key(SDLK_SPACE, 0, true).id == -1); // a field is never activated
    // SBINPUT: arrows walk too; Tab goes towards the top, Shift+Tab towards the bottom,
    // Return towards the top.
    in = g.fieldKey(SDLK_TAB, 0);
    CHECK(in.sound == 8 && g.focus() == 1 && !g.fieldFocused());
    g.setFocus(200);
    g.fieldKey(SDLK_TAB, KMOD_SHIFT);
    CHECK(g.focus() == 0);
    g.setFocus(201);
    g.fieldKey(SDLK_RETURN, 0);
    CHECK(g.focus() == 101);
    g.setFocus(201);
    g.fieldKey(SDLK_LEFT, 0); // same top towards the bottom: the driver of slot 1 comes first
    CHECK(g.focus() == 1);
    CHECK(!g.fieldKey(SDLK_ESCAPE, 0).moved && g.focus() == 1);
    // Clicks and the focus frame ignore fields.
    g.setFocus(200);
    CHECK(g.mouseDown(160, 205).sound == -1 && g.focus() == 200);
}

int main() {
    pressAndRelease();
    siblingWalk();
    fields();
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("sb_button_test: OK");
    return failures != 0;
}
