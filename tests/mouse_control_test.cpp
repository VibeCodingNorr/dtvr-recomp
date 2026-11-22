// SPDX-License-Identifier: GPL-3.0-only
// Tests for the regions of the mouse steering cross (seg4:10A3).
#include "mouse_control_view.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    CHECK(mouseControlState(32, 32) == 1 && mouseControlState(0x15, 0x29) == 1);
    CHECK(mouseControlState(32, 2) == 2);   // top
    CHECK(mouseControlState(32, 61) == 4);  // bottom
    CHECK(mouseControlState(2, 32) == 5);   // left
    CHECK(mouseControlState(61, 32) == 3);  // right
    CHECK(mouseControlState(5, 10) == 5 && mouseControlState(60, 10) == 3); // upper corners split by the diagonal
    CHECK(mouseControlState(5, 55) == 5 && mouseControlState(60, 55) == 3);
    CHECK(mouseControlKey(1) == SDLK_RETURN && mouseControlKey(2) == SDLK_UP && mouseControlKey(5) == SDLK_LEFT);
    CHECK(mouseControlKey(0) == SDLK_UNKNOWN);
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("mouse_control_test: OK");
    return failures != 0;
}
