// SPDX-License-Identifier: GPL-3.0-only
// Scripted key presses for smoke tests (--press, developer option): the dummy video
// driver has no keyboard, so interactive flows are verified by posting SDL key events.
#pragma once

#include <SDL.h>

#include <string>
#include <vector>

namespace dtvr {

class ScriptedInput {
public:
    // Comma-separated SDL key names ("Escape,Down,Return"); throws on an unknown name.
    explicit ScriptedInput(const std::string& keys = {});
    // Posts the next key (down and up) every `interval` frames, the first at `interval`.
    void pump(int frame, int interval = 10);
    bool done() const { return next_ >= keys_.size(); }

private:
    std::vector<SDL_Keycode> keys_;
    size_t next_ = 0;
};

} // namespace dtvr
