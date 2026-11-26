// SPDX-License-Identifier: GPL-3.0-only
// Game controllers (Xbox and other SDL GameController pads) on the original joystick
// handling of rally.exe (round 79): a timer every 80 ms (SetTimer 0x50, seg2:8018)
// runs seg2:8A30, which, while a window of the program has the focus (GetFocus,
// seg2:8A35), reads joystick 1 (seg2:8C2B, game port seg10:0010) as one of Up / Down /
// Left / Right and two buttons and posts keys to the focused window: a new direction
// at once, the same direction again after three ticks and then every tick, button 1
// -> Return and button 2 -> Escape when newly pressed. Released, the steering
// direction DS:A77A is cleared. Evidence in docs/formats.md "Joystick".
// The other pad buttons are a recomp addition (pad_input.hpp, "Recomp pad layer").
#pragma once

#include "pad_input.hpp"

#include <SDL.h>

#include <vector>

namespace dtvr {

// Every connected SDL game controller, merged into joystick 1 (any pad may act, for
// hotseat). Feeds key events into the SDL queue, so menus, dialogs and driving see
// them like the keyboard; actions without a key are returned.
class ControllerInput {
public:
    ControllerInput();
    ~ControllerInput();
    ControllerInput(const ControllerInput&) = delete;
    ControllerInput& operator=(const ControllerInput&) = delete;
    // Device added / removed events.
    void handleEvent(const SDL_Event& event);
    // `enabled`: [Setup] Joystick != 0 (DS:A126 bit 0); `focused`: the window has the
    // input focus (seg2:8A35). Without either, or without a pad, a held direction is
    // released at once and nothing else happens. `capturing`: the Steuerung page waits
    // for a button (also with the option off): no keys, PadOutput::captured instead.
    PadOutput update(Uint32 now, bool enabled, bool focused, const PadContext& context, bool capturing = false);
    // Smoke tests (--press Pad:A ...): one press and release of the physical `raw` through
    // the same mapping and layer, without a device.
    PadOutput scripted(const PadRaw& raw, const PadContext& context, bool capturing);
    // The remapping of the Steuerung page; applies to every merged pad.
    void setMap(const PadMap& map) { map_ = map; }
    bool connected() const { return !pads_.empty(); }

private:
    void open(int deviceIndex);
    PadRaw raw() const;

    bool initialized_ = false;
    std::vector<SDL_GameController*> pads_;
    bool devicesChanged_ = false;
    PadInput input_;
    PadMap map_;
    PadCapture capture_;
    Uint32 nextTick_ = 0;
};

} // namespace dtvr
