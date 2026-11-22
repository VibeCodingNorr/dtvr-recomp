// SPDX-License-Identifier: GPL-3.0-only
// Game controllers (Xbox and other SDL GameController pads) on the original joystick
// handling of rally.exe (round 79): a timer every 80 ms (SetTimer 0x50, seg2:8018)
// runs seg2:8A30, which reads joystick 1 (seg2:8C2B, game port seg10:0010) as one of
// Up / Down / Left / Right and two buttons and posts keys to the focused window: a new
// direction at once, the same direction again after three ticks and then every
// tick, button 1 -> Return and button 2 -> Escape when newly pressed. Released, the
// steering direction DS:A77A is cleared. Evidence in docs/formats.md "Joystick".
#pragma once

#include <SDL.h>

#include <vector>

namespace dtvr {

// One synthesized key: pressed or released.
struct PadKey { SDL_Keycode key = SDLK_UNKNOWN; bool down = true; };

// seg2:8A30 for one joystick, SDL-free apart from key codes.
class JoystickRepeater {
public:
    // `direction` is SDLK_UP / DOWN / LEFT / RIGHT or SDLK_UNKNOWN; bit 0 of `buttons`
    // is button 1, bit 1 button 2.
    std::vector<PadKey> tick(SDL_Keycode direction, unsigned buttons);

private:
    SDL_Keycode previous_ = SDLK_UNKNOWN; // DS:94FC
    int repeat_ = 0;                      // DS:A0E2
    unsigned previousButtons_ = 0;        // DS:A0E6
};

// The first connected SDL game controller as joystick 1. Feeds key events into the
// SDL queue, so menus, dialogs and driving see them like the keyboard.
class ControllerInput {
public:
    ControllerInput();
    ~ControllerInput();
    ControllerInput(const ControllerInput&) = delete;
    ControllerInput& operator=(const ControllerInput&) = delete;
    // Device added / removed events.
    void handleEvent(const SDL_Event& event);
    // `enabled`: [Setup] Joystick != 0 (DS:A126 bit 0).
    void update(Uint32 now, bool enabled);
    bool connected() const { return pad_ != nullptr; }

private:
    void openFirst();
    SDL_Keycode direction() const;
    unsigned buttons() const;

    bool initialized_ = false;
    SDL_GameController* pad_ = nullptr;
    JoystickRepeater repeater_;
    Uint32 nextTick_ = 0;
};

} // namespace dtvr
