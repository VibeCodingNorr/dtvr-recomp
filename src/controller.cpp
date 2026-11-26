// SPDX-License-Identifier: GPL-3.0-only
#include "controller.hpp"

#include <algorithm>
#include <utility>

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
    e.key.keysym.mod = k.mod;
    SDL_PushEvent(&e);
}

bool held(SDL_GameController* pad, SDL_GameControllerButton b) { return SDL_GameControllerGetButton(pad, b) != 0; }

// seg2:8C2B checks up, down, left, right in this order; a diagonal is vertical.
SDL_Keycode direction(bool up, bool down, bool left, bool right) {
    if (up) return SDLK_UP;
    if (down) return SDLK_DOWN;
    if (left) return SDLK_LEFT;
    if (right) return SDLK_RIGHT;
    return SDLK_UNKNOWN;
}

} // namespace

ControllerInput::ControllerInput() {
    initialized_ = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0;
    if (initialized_)
        for (int i = 0; i < SDL_NumJoysticks(); ++i) open(i);
}

ControllerInput::~ControllerInput() {
    for (SDL_GameController* pad : pads_) SDL_GameControllerClose(pad);
    if (initialized_) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void ControllerInput::open(int deviceIndex) {
    if (!SDL_IsGameController(deviceIndex)) return;
    const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(deviceIndex);
    for (SDL_GameController* pad : pads_) // SDL also reports the pads found at start-up as added
        if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) == id) return;
    if (SDL_GameController* pad = SDL_GameControllerOpen(deviceIndex)) {
        pads_.push_back(pad);
        devicesChanged_ = true;
    }
}

void ControllerInput::handleEvent(const SDL_Event& event) {
    if (!initialized_) return;
    if (event.type == SDL_CONTROLLERDEVICEADDED) {
        open(event.cdevice.which); // device index
    } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
        const auto gone = std::find_if(pads_.begin(), pads_.end(), [&](SDL_GameController* pad) {
            return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) == event.cdevice.which; // instance id
        });
        if (gone != pads_.end()) {
            SDL_GameControllerClose(*gone);
            pads_.erase(gone);
            devicesChanged_ = true;
        }
    }
}

PadState ControllerInput::state() const {
    bool up = false, down = false, left = false, right = false;
    bool rUp = false, rDown = false, rLeft = false, rRight = false;
    PadState s;
    for (SDL_GameController* pad : pads_) {
        const Sint16 x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        const Sint16 y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        up |= held(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) || y < -kStickThreshold;
        down |= held(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || y > kStickThreshold;
        left |= held(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || x < -kStickThreshold;
        right |= held(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || x > kStickThreshold;
        const Sint16 rx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
        const Sint16 ry = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
        rUp |= ry < -kStickThreshold;
        rDown |= ry > kStickThreshold;
        rLeft |= rx < -kStickThreshold;
        rRight |= rx > kStickThreshold;
        // Button 1 / 2 of the game port: A / B (south / east on other pads); the rest recomp.
        static constexpr std::pair<SDL_GameControllerButton, unsigned> kButtons[] = {
            {SDL_CONTROLLER_BUTTON_A, kPadA}, {SDL_CONTROLLER_BUTTON_B, kPadB},
            {SDL_CONTROLLER_BUTTON_X, kPadX}, {SDL_CONTROLLER_BUTTON_Y, kPadY},
            {SDL_CONTROLLER_BUTTON_BACK, kPadBack}, {SDL_CONTROLLER_BUTTON_START, kPadStart},
            {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, kPadLB}, {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, kPadRB}};
        for (const auto& [button, bit] : kButtons)
            if (held(pad, button)) s.buttons |= bit;
    }
    s.direction = direction(up, down, left, right);
    s.rightStick = direction(rUp, rDown, rLeft, rRight);
    return s;
}

PadOutput ControllerInput::update(Uint32 now, bool enabled, bool focused, const PadContext& context) {
    PadOutput out;
    if (pads_.empty() || !enabled || !focused) {
        out = input_.release(); // at once, not on the next timer tick
        nextTick_ = 0;
    } else {
        if (nextTick_ != 0 && !SDL_TICKS_PASSED(now, nextTick_)) return out;
        nextTick_ = now + kTickMs;
        const PadState s = state();
        if (devicesChanged_) input_.devicesChanged(s.buttons);
        out = input_.tick(s, context);
    }
    devicesChanged_ = false;
    for (const PadKey& k : out.keys) push(k);
    out.keys.clear();
    return out;
}

PadOutput ControllerInput::scripted(const PadState& state, const PadContext& context) {
    if (!input_.active()) input_.tick(PadState{}, context); // the neutral start
    PadOutput out = input_.tick(state, context);
    for (const PadKey& k : input_.tick(PadState{}, context).keys) out.keys.push_back(k); // released
    for (const PadKey& k : out.keys) push(k);
    out.keys.clear();
    return out;
}

} // namespace dtvr
