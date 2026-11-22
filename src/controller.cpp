// SPDX-License-Identifier: GPL-3.0-only
#include "controller.hpp"

namespace dtvr {
namespace {

constexpr Uint32 kTickMs = 0x50;        // SetTimer(…, 0x50, seg2:8A21) at seg2:8018
constexpr Sint16 kStickThreshold = 16384; // assumption: half way (the original calibrates)

void push(const PadKey& k) {
    SDL_Event e{};
    e.type = k.down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.state = k.down ? SDL_PRESSED : SDL_RELEASED;
    e.key.keysym.sym = k.key;
    e.key.keysym.scancode = SDL_GetScancodeFromKey(k.key);
    SDL_PushEvent(&e);
}

} // namespace

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

ControllerInput::ControllerInput() {
    initialized_ = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0;
    if (initialized_) openFirst();
}

ControllerInput::~ControllerInput() {
    if (pad_) SDL_GameControllerClose(pad_);
    if (initialized_) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void ControllerInput::openFirst() {
    for (int i = 0; i < SDL_NumJoysticks() && !pad_; ++i)
        if (SDL_IsGameController(i)) pad_ = SDL_GameControllerOpen(i);
}

void ControllerInput::handleEvent(const SDL_Event& event) {
    if (!initialized_) return;
    if (event.type == SDL_CONTROLLERDEVICEADDED && !pad_) {
        openFirst();
    } else if (event.type == SDL_CONTROLLERDEVICEREMOVED && pad_
               && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad_))) {
        SDL_GameControllerClose(pad_);
        pad_ = nullptr;
        openFirst();
    }
}

SDL_Keycode ControllerInput::direction() const {
    // seg2:8C2B checks up, down, left, right in this order.
    auto button = [&](SDL_GameControllerButton b) { return SDL_GameControllerGetButton(pad_, b) != 0; };
    const Sint16 x = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
    const Sint16 y = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
    if (button(SDL_CONTROLLER_BUTTON_DPAD_UP) || y < -kStickThreshold) return SDLK_UP;
    if (button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || y > kStickThreshold) return SDLK_DOWN;
    if (button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || x < -kStickThreshold) return SDLK_LEFT;
    if (button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || x > kStickThreshold) return SDLK_RIGHT;
    return SDLK_UNKNOWN;
}

unsigned ControllerInput::buttons() const {
    // Button 1 / 2 of the game port: A / B (south / east on other pads).
    return (SDL_GameControllerGetButton(pad_, SDL_CONTROLLER_BUTTON_A) ? 1u : 0u)
           | (SDL_GameControllerGetButton(pad_, SDL_CONTROLLER_BUTTON_B) ? 2u : 0u);
}

void ControllerInput::update(Uint32 now, bool enabled) {
    if (!pad_ || !enabled) return;
    if (nextTick_ != 0 && !SDL_TICKS_PASSED(now, nextTick_)) return;
    nextTick_ = now + kTickMs;
    for (const PadKey& k : repeater_.tick(direction(), buttons())) push(k);
}

} // namespace dtvr
