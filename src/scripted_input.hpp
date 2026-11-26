// SPDX-License-Identifier: GPL-3.0-only
// Scripted key presses for smoke tests (--press, developer option): the dummy video
// driver has no keyboard, so interactive flows are verified by posting SDL key events.
#pragma once

#include "pad_input.hpp"

#include <SDL.h>

#include <string>
#include <vector>

namespace dtvr {

class ScriptedInput {
public:
    // Comma-separated SDL key names ("Escape,Down,Return"); "Pad:<button>" presses a
    // game controller button through the pad layer (A, B, X, Y, Back, Start, LB, RB,
    // Up, Down, Left, Right); "ClickLeft:x:y" / "ClickRight:x:y" click at canvas pixels.
    // Throws on an unknown name.
    explicit ScriptedInput(const std::string& keys = {});
    // Posts the next key (down and up) every `interval` frames, the first at `interval`;
    // a pad entry waits in takePad() for the game window's pad tick.
    void pump(int frame, int interval = 10);
    bool done() const { return next_ >= entries_.size(); }
    // The scripted pad press of this frame, if any (taken once).
    bool takePad(PadState& state);

private:
    struct Entry {
        SDL_Keycode key = SDLK_UNKNOWN;
        bool pad = false;
        PadState state;
        int button = 0, x = 0, y = 0; // a mouse click
    };
    std::vector<Entry> entries_;
    size_t next_ = 0;
    bool padPending_ = false;
    PadState pad_;
};

} // namespace dtvr
