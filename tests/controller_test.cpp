// SPDX-License-Identifier: GPL-3.0-only
// Tests for the joystick key repetition of seg2:8A30 and the recomp pad layer.
#include "pad_input.hpp"

#include <cstdio>
#include <utility>

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
    c.yIsF9 = true;
    p.tick({}, c);
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadY}, c);
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_F9) && !o.handCards);
}

// Round 2: name fields, the on-screen keyboard and the town dialog's Karte.
void dialogContexts() {
    PadInput p;
    PadContext c;
    c.aIsSpace = true;  // PLAYER
    c.yIsF9 = true;
    c.nameField = true; // a name field has the focus
    p.tick({}, c);
    auto o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA}, c);
    CHECK(o.keys.empty() && o.keyboard);             // no Return (would start the game), no Space
    CHECK(!p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA}, c).keyboard); // held: once
    p.tick({}, c);
    c.turnMenu = c.townCards = true; // even if a turn menu or town dialog were behind PLAYER
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadY}, c); // Y in a name field: nothing
    CHECK(o.keys.empty() && !o.handCards && !o.townCards && !o.keyboardShift && !o.keyboard);
    c.turnMenu = c.townCards = false;
    p.tick({}, c);
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA}, c);
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA | kPadB}, c); // the seg2:8B8E repeat of button 1
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_ESCAPE) && o.keyboard);
    p.tick({}, c);
    o = p.tick({SDLK_DOWN, SDLK_UNKNOWN, 0}, c);     // the d-pad still walks out of the field
    CHECK(o.keys.size() == 1 && o.keys[0].key == SDLK_DOWN && !o.keyboard);
    // The keyboard is open: A is Return again, Start is done, Y shift, X Backspace, B Escape.
    c = {};
    c.keyboard = true;
    p.tick({}, c);
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadA}, c);
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_RETURN) && !o.keyboard);
    p.tick({}, c);
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadStart}, c);
    CHECK(o.keys.empty() && o.keyboardAccept && !o.pauseMenu);
    p.tick({}, c);
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadY}, c);
    CHECK(o.keys.empty() && o.keyboardShift && !o.handCards);
    p.tick({}, c);
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadX}, c);
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_BACKSPACE));
    // Town dialog header: Y is Karte; with ONFIELD (never at once) the turn menu wins.
    c = {};
    c.townCards = true;
    p.tick({}, c);
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadY}, c);
    CHECK(o.keys.empty() && o.townCards && !o.handCards);
    // OPTION: Y is F9 (its recomp link), A is Space.
    c = {};
    c.aIsSpace = true;
    c.yIsF9 = true;
    p.tick({}, c);
    o = p.tick({SDLK_UNKNOWN, SDLK_UNKNOWN, kPadY}, c);
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_F9) && !o.townCards);
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

// The fixed assignment of pad rounds 1-3 (ControllerInput::state before the remapping):
// d-pad OR left stick per direction in seg2:8C2B order, the right stick alone, A..RB.
PadState previousState(const PadRaw& raw) {
    const auto has = [&](PadPhysical p) { return (raw.buttons & padBit(p)) != 0; };
    PadState s;
    const bool up = has(PadPhysical::DPadUp) || raw.leftStick == SDLK_UP;
    const bool down = has(PadPhysical::DPadDown) || raw.leftStick == SDLK_DOWN;
    const bool left = has(PadPhysical::DPadLeft) || raw.leftStick == SDLK_LEFT;
    const bool right = has(PadPhysical::DPadRight) || raw.leftStick == SDLK_RIGHT;
    s.direction = up ? SDLK_UP : down ? SDLK_DOWN : left ? SDLK_LEFT : right ? SDLK_RIGHT : SDLK_UNKNOWN;
    s.rightStick = raw.rightStick;
    const std::pair<PadPhysical, unsigned> bits[] = {
        {PadPhysical::A, kPadA}, {PadPhysical::B, kPadB}, {PadPhysical::X, kPadX}, {PadPhysical::Y, kPadY},
        {PadPhysical::Back, kPadBack}, {PadPhysical::Start, kPadStart}, {PadPhysical::LB, kPadLB},
        {PadPhysical::RB, kPadRB}};
    for (const auto& [p, bit] : bits)
        if (has(p)) s.buttons |= bit;
    return s;
}

bool same(const PadState& a, const PadState& b) {
    return a.direction == b.direction && a.rightStick == b.rightStick && a.buttons == b.buttons;
}

void defaultMapIsPreviousBehaviour() {
    const PadMap m;
    const SDL_Keycode sticks[] = {SDLK_UNKNOWN, SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT};
    bool all = true;
    for (unsigned b = 0; b < (1u << kPadPhysicalCount); ++b)
        for (SDL_Keycode l : sticks)
            for (SDL_Keycode r : {SDLK_UNKNOWN, SDLK_LEFT}) {
                const PadRaw raw{b, l, r};
                all = all && same(translate(m, raw), previousState(raw));
            }
    CHECK(all); // every button combination and stick: the same pad state as before
    // So the key stream of a whole sequence is identical too.
    PadInput before, after;
    PadContext c;
    const PadRaw seq[] = {{}, {padBit(PadPhysical::DPadDown)}, {padBit(PadPhysical::DPadDown)},
                          {padBit(PadPhysical::A)}, {padBit(PadPhysical::A) | padBit(PadPhysical::B)},
                          {padBit(PadPhysical::LB)}, {0, SDLK_LEFT}, {padBit(PadPhysical::Start)}, {}};
    for (const PadRaw& r : seq) {
        const PadOutput x = before.tick(previousState(r), c), y = after.tick(translate(m, r), c);
        CHECK(x.keys.size() == y.keys.size() && x.pauseMenu == y.pauseMenu);
        for (size_t i = 0; i < x.keys.size() && i < y.keys.size(); ++i)
            CHECK(x.keys[i].key == y.keys[i].key && x.keys[i].down == y.keys[i].down && x.keys[i].mod == y.keys[i].mod);
    }
    // The physical triggers and stick clicks do nothing by default.
    CHECK(translate(m, {padBit(PadPhysical::LT) | padBit(PadPhysical::RightStick)}).buttons == 0);
}

void remappedTranslation() {
    PadMap m;
    CHECK(m.assign(PadAction::Confirm, PadPhysical::RT) == PadAction::kCount); // a free button
    CHECK(translate(m, {padBit(PadPhysical::RT)}).buttons == kPadA);
    CHECK(translate(m, {padBit(PadPhysical::A)}).buttons == 0);
    // A taken button swaps: Cancel gets the d-pad's up, Up gets B.
    CHECK(m.assign(PadAction::Cancel, PadPhysical::DPadUp) == PadAction::Up);
    CHECK(translate(m, {padBit(PadPhysical::DPadUp)}).buttons == kPadB);
    CHECK(translate(m, {padBit(PadPhysical::DPadUp)}).direction == SDLK_UNKNOWN);
    CHECK(translate(m, {padBit(PadPhysical::B)}).direction == SDLK_UP);
    // The action's rules follow the action: the remapped Confirm is Space in PLAYER.
    PadInput p;
    PadContext c;
    c.aIsSpace = true;
    p.tick({}, c);
    auto o = p.tick(translate(m, {padBit(PadPhysical::RT)}), c);
    CHECK(o.keys.size() == 2 && isPress(o.keys, 0, SDLK_SPACE));
    // Its direction keeps the 80 ms repetition (three ticks of waiting, then every tick).
    o = p.tick(translate(m, {padBit(PadPhysical::B)}), c);
    CHECK(o.keys.size() == 1 && o.keys[0].key == SDLK_UP && o.keys[0].down);
    for (int i = 0; i < 3; ++i) CHECK(p.tick(translate(m, {padBit(PadPhysical::B)}), c).keys.empty());
    CHECK(p.tick(translate(m, {padBit(PadPhysical::B)}), c).keys.size() == 1);
    // Swapped sticks: the right one drives, the left one scrolls Umfeld.
    m.swapSticks = true;
    const PadState s = translate(m, {0, SDLK_LEFT, SDLK_DOWN});
    CHECK(s.direction == SDLK_DOWN && s.rightStick == SDLK_LEFT);
}

void capture() {
    PadCapture cap;
    const PadRaw a{padBit(PadPhysical::A)}, x{padBit(PadPhysical::X)}, none{};
    CHECK(cap.tick(none, false).translate);
    // A held when the wait starts is no choice; X pressed meanwhile is.
    auto r = cap.tick(a, true);
    CHECK(!r.translate && r.captured < 0);
    r = cap.tick({a.buttons | x.buttons}, true);
    CHECK(!r.translate && r.captured == int(PadPhysical::X));
    // After the wait the pad stays silent until everything is released.
    CHECK(!cap.tick({a.buttons | x.buttons}, false).translate);
    CHECK(!cap.tick({0, SDLK_UP}, false).translate); // a stick still pushed
    CHECK(cap.tick(none, false).translate);
    CHECK(cap.tick(a, false).translate);
    // The triggers can be chosen.
    cap.tick(none, true);
    CHECK(cap.tick({padBit(PadPhysical::LT)}, true).captured == int(PadPhysical::LT));
}

} // namespace

// The on-screen keyboard reads the driving stick's travel; a scripted key is full travel.
void keyboardStick() {
    PadMap m;
    PadRaw r{padBit(PadPhysical::A), SDLK_RIGHT, SDLK_UNKNOWN};
    r.leftX = 0.42f;
    r.rightY = -0.9f;
    OskPadRaw o = oskRaw(m, r);
    CHECK(o.buttons == padBit(PadPhysical::A) && o.stickX == 0.42f && o.stickY == 0);
    m.swapSticks = true; // the right stick drives
    o = oskRaw(m, r);
    CHECK(o.stickX == 0 && o.stickY == -0.9f);
    o = oskRaw(PadMap{}, {0, SDLK_UP, SDLK_UNKNOWN}); // --press without travel
    CHECK(o.stickX == 0 && o.stickY == -1);
}

int main() {
    originalRepeater();
    originalLayerThroughPadInput();
    aAsSpace();
    recompButtons();
    dialogContexts();
    rightStick();
    hazards();
    defaultMapIsPreviousBehaviour();
    remappedTranslation();
    capture();
    keyboardStick();
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("controller_test: OK");
    return failures != 0;
}
