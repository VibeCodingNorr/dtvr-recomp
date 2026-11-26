// SPDX-License-Identifier: GPL-3.0-only
#include "pad_input.hpp"

#include <algorithm>

namespace dtvr {

// seg2:8A30 (docs/formats.md "Joystick"). The original posts WM_KEYDOWN with lParam 0
// (seg2:8B44 / 8B84 / 8BBC / 8BF4) and never a WM_KEYUP: a centred stick clears the
// steering direction DS:A77A instead (seg2:8B05). Here that clearing is the key-up.
std::vector<PadKey> JoystickRepeater::tick(SDL_Keycode direction, unsigned buttons) {
    std::vector<PadKey> keys;
    if (direction == SDLK_UNKNOWN) {
        if (previous_ != SDLK_UNKNOWN) {          // seg2:8AFA: counter 0, DS:A77A = 0
            repeat_ = 0;
            keys.push_back({previous_, false});
        }
    } else if (direction != previous_) {          // seg2:8B31: a new direction at once
        if (previous_ != SDLK_UNKNOWN) keys.push_back({previous_, false});
        keys.push_back({direction, true});
        repeat_ = 0;
    } else if (repeat_ < 3) {                     // seg2:8B5C: wait three ticks
        ++repeat_;
    } else {
        keys.push_back({direction, true});        // seg2:8B71: then every tick
    }
    if ((buttons & 1) && buttons != previousButtons_) { // seg2:8B8E: button 1 -> Return
        keys.push_back({SDLK_RETURN, true});
        keys.push_back({SDLK_RETURN, false});
    }
    if ((buttons & 2) && buttons != previousButtons_) { // seg2:8BC6: button 2 -> Escape
        keys.push_back({SDLK_ESCAPE, true});
        keys.push_back({SDLK_ESCAPE, false});
    }
    previous_ = direction;
    previousButtons_ = buttons;
    return keys;
}

namespace {

void press(std::vector<PadKey>& keys, SDL_Keycode key, Uint16 mod = KMOD_NONE) {
    keys.push_back({key, true, mod});
    keys.push_back({key, false, mod});
}

} // namespace

PadOutput PadInput::tick(const PadState& state, const PadContext& context) {
    PadOutput out;
    if (!active_) { // neutral start: what is held now was pressed before the pad counted
        active_ = true;
        suppressed_ = state.buttons;
    }
    suppressed_ &= state.buttons; // released buttons count again
    const unsigned buttons = state.buttons & ~suppressed_;
    const unsigned pressed = buttons & ~previous_;
    previous_ = buttons;

    // The original layer (unchanged): direction, A = button 1, B = button 2.
    out.keys = original_.tick(state.direction, buttons & (kPadA | kPadB));
    if (context.nameField) { // RECOMP: A on a name field opens the on-screen keyboard, no key
        const auto ret = [](const PadKey& k) { return k.key == SDLK_RETURN; };
        for (const PadKey& k : out.keys) out.keyboard = out.keyboard || (ret(k) && k.down);
        out.keys.erase(std::remove_if(out.keys.begin(), out.keys.end(), ret), out.keys.end());
    } else if (context.aIsSpace) { // RECOMP: SBBUTTON windows without Return of their own (PLAYER, OPTION)
        for (PadKey& k : out.keys)
            if (k.key == SDLK_RETURN) k.key = SDLK_SPACE;
    }

    // RECOMP ADDITION: the other buttons, each when newly pressed.
    if (pressed & kPadX) press(out.keys, SDLK_BACKSPACE);
    if ((pressed & kPadBack) && context.backOpensMenu) press(out.keys, SDLK_BACKSPACE);
    if (pressed & kPadLB) press(out.keys, SDLK_TAB, KMOD_LSHIFT);
    if (pressed & kPadRB) press(out.keys, SDLK_TAB);
    if (pressed & kPadStart) {
        if (context.keyboard) out.keyboardAccept = true;
        else out.pauseMenu = true;
    }
    if (pressed & kPadY) {
        if (context.keyboard) out.keyboardShift = true;
        else if (context.nameField) {} // a name field has the input: Y is no card and no F9
        else if (context.turnMenu) out.handCards = true;
        else if (context.townCards) out.townCards = true;
        else if (context.yIsF9) press(out.keys, SDLK_F9);
        // Y does nothing elsewhere.
    }
    // The right stick scrolls Umfeld with the original repetition, as a scroll step
    // outside the key queue. Not in a tick with other keys: one of them may close Umfeld.
    const bool scroll = context.surroundings && out.keys.empty();
    for (const PadKey& k : rightStick_.tick(scroll ? state.rightStick : SDLK_UNKNOWN, 0))
        if (k.down) out.scroll = k.key;
    return out;
}

PadOutput PadInput::release() {
    PadOutput out;
    if (!active_) return out;
    out.keys = original_.tick(SDLK_UNKNOWN, 0); // key-up of a held direction
    rightStick_.tick(SDLK_UNKNOWN, 0);
    previous_ = 0;
    suppressed_ = 0;
    active_ = false;
    return out;
}

} // namespace dtvr
