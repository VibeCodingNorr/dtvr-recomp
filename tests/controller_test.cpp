// SPDX-License-Identifier: GPL-3.0-only
// Tests for the joystick key repetition of seg2:8A30 and the recomp pad layer.
#include "pad_input.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

namespace {

bool isPress(const std::vector<PadKey>& k, size_t i, SDL_Keycode key, Uint16 mod = KMOD_NONE) {
    return k.size() >= i + 2 && k[i].key == key && k[i].down && k[i + 1].key == key && !k[i + 1].down
           && k[i].mod == mod && k[i + 1].mod == mod;
}

void originalRepeater() {
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
    for (const PadKey& key : k) CHECK(key.mod == KMOD_NONE);
}

void originalLayerThroughPadInput() {
    PadInput p;
    PadContext c;
    CHECK(p.tick({}, c).keys.empty());                 // neutral start
    auto o = p.tick({SDLK_DOWN, SDLK_UNKNOWN, 0}, c);
    CHECK(o.keys.size() == 1 && o.keys[0].key == SDLK_DOWN && o.keys[0].down);
    for (int i = 0; i < 3; ++i) CHECK(p.tick({SDLK_DOWN, SDLK_UNKNOWN, 0}, c).keys.empty());
    CHECK(p.tick({SDLK_DOWN, SDLK_UNKNOWN, 0}, c).keys.size() == 1); // held d-pad repeats (also in Risiko)
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA}, c);
    CHECK(o.keys.size() == 3 && !o.keys[0].down && isPress(o.keys, 1, SDLK_RETURN));
    CHECK(p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA}, c).keys.empty());
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA | kPadB}, c); // seg2:8B8E quirk kept
    CHECK(o.keys.size() == 4 && isPress(o.keys, 0, SDLK_RETURN) && isPress(o.keys, 2, SDLK_ESCAPE));
    // A recomp button changes nothing in the original layer's button state.
    p.tick({}, c);
    p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA}, c);
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA | kPadRB}, c);
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_TAB));
}

void aAsSpace() {
    PadInput p;
    PadContext c;
    c.aIsSpace = true;
    p.tick({}, c);
    auto o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA}, c);
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_SPACE));
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA | kPadB}, c);
    CHECK(o.keys.size() == 4 && isPress(o.keys, 0, SDLK_SPACE) && isPress(o.keys, 2, SDLK_ESCAPE));
}

void recompButtons() {
    PadInput p;
    PadContext c;
    p.tick({}, c);
    auto o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadX}, c);
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_BACKSPACE));
    CHECK(p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadX}, c).keys.empty()); // edge only
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadLB}, c);
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_TAB, KMOD_LSHIFT));
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadRB}, c);
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_TAB));
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadStart}, c);
    CHECK(o.keys.empty() && o.pauseMenu && !o.handCards);
    CHECK(!p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadStart}, c).pauseMenu);
    // Back: Backspace only where it opens RIGHTBTN.
    p.tick({}, c);
    CHECK(p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadBack}, c).keys.empty());
    p.tick({}, c);
    c.backOpensMenu = true;
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadBack}, c);
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_BACKSPACE));
    // Y: Karte einsetzen in ONFIELD, F9 in PLAYER, nothing elsewhere.
    c = {};
    p.tick({}, c);
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadY}, c);
    CHECK(o.keys.empty() && !o.handCards);
    p.tick({}, c);
    c.turnMenu = true;
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadY}, c);
    CHECK(o.keys.empty() && o.handCards);
    c = {};
    c.setup = true;
    p.tick({}, c);
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadY}, c);
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_F9) && !o.handCards);
}

// The right stick never produces SDL key events: only PadOutput::scroll, which the game
// window applies to Umfeld directly (it can never reach the map's heldArrow_).
void rightStick() {
    PadInput p;
    PadContext c;
    p.tick({}, c);
    auto o = p.tick({SDLK_UNKNOWN, SDLK_LEFT, 0}, c);
    CHECK(o.keys.empty() && o.scroll == SDLK_UNKNOWN); // not outside Umfeld
    c.surroundings = true;
    p.tick({}, c);
    o = p.tick({SDLK_UNKNOWN, SDLK_LEFT, 0}, c);
    CHECK(o.keys.empty() && o.scroll == SDLK_LEFT);    // at once
    for (int i = 0; i < 3; ++i) {                      // the original repetition
        o = p.tick({SDLK_UNKNOWN, SDLK_LEFT, 0}, c);
        CHECK(o.keys.empty() && o.scroll == SDLK_UNKNOWN);
    }
    o = p.tick({SDLK_UNKNOWN, SDLK_LEFT, 0}, c);
    CHECK(o.keys.empty() && o.scroll == SDLK_LEFT);
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, 0}, c);    // released: nothing
    CHECK(o.keys.empty() && o.scroll == SDLK_UNKNOWN);
    o = p.tick({SDLK_UNKNOWN, SDLK_UP, 0}, c);
    CHECK(o.keys.empty() && o.scroll == SDLK_UP);
    c.surroundings = false;                            // Umfeld closed while held
    o = p.tick({SDLK_UNKNOWN, SDLK_UP, 0}, c);
    CHECK(o.keys.empty() && o.scroll == SDLK_UNKNOWN);
    // Tester case: right stick held in Umfeld and B in the same tick: only Escape.
    PadInput q;
    c.surroundings = true;
    q.tick({}, c);
    o = q.tick({SDLK_UNKNOWN, SDLK_RIGHT, kPadB}, c);
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_ESCAPE) && o.scroll == SDLK_UNKNOWN);
    // Left stick in Umfeld stays the original arrow key; the right stick adds no key.
    o = q.tick({SDLK_LEFT, SDLK_RIGHT, 0}, c);
    CHECK(o.keys.size() == 1 && o.keys[0].key == SDLK_LEFT && o.scroll == SDLK_UNKNOWN);
    // Whatever the stick does over many ticks, no arrow key comes from it.
    PadInput r;
    r.tick({}, c);
    const SDL_Keycode dirs[] = {SDLK_UP, SDLK_UP, SDLK_RIGHT, SDLK_UNKNOWN, SDLK_DOWN, SDLK_DOWN, SDLK_DOWN, SDLK_DOWN, SDLK_DOWN};
    for (SDL_Keycode d : dirs) CHECK(r.tick({SDLK_UNKNOWN, d, 0}, c).keys.empty());
}

void hazards() {
    PadContext c;
    { // unplugged / option off / focus lost while driving: the held arrow is released
        PadInput p;
        p.tick({SDLK_RIGHT, SDLK_UNKNOWN, 0}, c);
        p.tick({SDLK_RIGHT, SDLK_UNKNOWN, 0}, c);
        auto o = p.release();
        CHECK(o.keys.size() == 1 && o.keys[0].key == SDLK_RIGHT && !o.keys[0].down);
        CHECK(!p.active() && p.release().keys.empty()); // once
        // Back again with the stick still held: the direction is new.
        o = p.tick({SDLK_RIGHT, SDLK_UNKNOWN, 0}, c);
        CHECK(o.keys.size() == 1 && o.keys[0].key == SDLK_RIGHT && o.keys[0].down);
    }
    { // a button held when the pad starts counting is no fresh press
        PadInput p;
        CHECK(p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA | kPadStart | kPadX}, c).keys.empty());
        auto o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA | kPadStart | kPadX}, c);
        CHECK(o.keys.empty() && !o.pauseMenu);
        p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadStart | kPadX}, c); // A released
        o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA | kPadStart | kPadX}, c);
        CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_RETURN) && !o.pauseMenu);
        // ... also after a release (option off and on again)
        p.release();
        o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA}, c);
        CHECK(o.keys.empty());
    }
    { // a pad connected with a button held
        PadInput p;
        p.tick({}, c);
        p.devicesChanged(kPadB);
        CHECK(p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadB}, c).keys.empty());
        p.tick({}, c);
        auto o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadB}, c);
        CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_ESCAPE));
    }
}

} // namespace

int main() {
    originalRepeater();
    originalLayerThroughPadInput();
    aAsSpace();
    recompButtons();
    rightStick();
    hazards();
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("controller_test: OK");
    return failures != 0;
}
