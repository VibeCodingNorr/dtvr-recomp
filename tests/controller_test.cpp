// SPDX-License-Identifier: GPL-3.0-only
// Tests for the joystick key repetition of seg2:8A30.
#include "controller.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    JoystickRepeater r;
    auto k = r.tick(SDLK_UP, 0);                       // new direction: at once
    CHECK(k.size() == 1 && k[0].key == SDLK_UP && k[0].down);
    for (int i = 0; i < 3; ++i) CHECK(r.tick(SDLK_UP, 0).empty()); // three ticks of waiting
    k = r.tick(SDLK_UP, 0);                            // then every tick
    CHECK(k.size() == 1 && k[0].key == SDLK_UP);
    CHECK(r.tick(SDLK_UP, 0).size() == 1);
    k = r.tick(SDLK_LEFT, 0);                          // a change releases the old key
    CHECK(k.size() == 2 && k[0].key == SDLK_UP && !k[0].down && k[1].key == SDLK_LEFT && k[1].down);
    k = r.tick(SDLK_UNKNOWN, 0);                       // centred: released (DS:A77A = 0)
    CHECK(k.size() == 1 && k[0].key == SDLK_LEFT && !k[0].down);
    CHECK(r.tick(SDLK_UNKNOWN, 0).empty());
    k = r.tick(SDLK_UNKNOWN, 1);                       // button 1: Return once
    CHECK(k.size() == 2 && k[0].key == SDLK_RETURN && k[0].down && !k[1].down);
    CHECK(r.tick(SDLK_UNKNOWN, 1).empty());            // held: nothing more
    k = r.tick(SDLK_UNKNOWN, 3);                       // button 2 added; the changed state repeats button 1 too
    CHECK(k.size() == 4 && k[2].key == SDLK_ESCAPE);
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("controller_test: OK");
    return failures != 0;
}
