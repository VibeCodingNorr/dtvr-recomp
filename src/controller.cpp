// SPDX-License-Identifier: GPL-3.0-only
#include "controller.hpp"

#include <algorithm>
#include <cmath>
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

// An axis as travel -1 .. 1; of several pads the largest one per axis counts.
void merge(float& into, Sint16 value) {
    const float v = std::max(-1.0f, float(value) / 32767.0f);
    if (std::fabs(v) > std::fabs(into)) into = v;
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

PadRaw ControllerInput::raw() const {
    bool up = false, down = false, left = false, right = false;
    bool rUp = false, rDown = false, rLeft = false, rRight = false;
    PadRaw r;
    for (SDL_GameController* pad : pads_) {
        const Sint16 x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        const Sint16 y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        up |= y < -kStickThreshold;
        down |= y > kStickThreshold;
        left |= x < -kStickThreshold;
        right |= x > kStickThreshold;
        const Sint16 rx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
        const Sint16 ry = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
        rUp |= ry < -kStickThreshold;
        rDown |= ry > kStickThreshold;
        rLeft |= rx < -kStickThreshold;
        rRight |= rx > kStickThreshold;
        merge(r.leftX, x);
        merge(r.leftY, y);
        merge(r.rightX, rx);
        merge(r.rightY, ry);
        // The physical buttons (pad_map.hpp); which action each triggers is the map's.
        static constexpr std::pair<SDL_GameControllerButton, PadPhysical> kButtons[] = {
            {SDL_CONTROLLER_BUTTON_A, PadPhysical::A}, {SDL_CONTROLLER_BUTTON_B, PadPhysical::B},
            {SDL_CONTROLLER_BUTTON_X, PadPhysical::X}, {SDL_CONTROLLER_BUTTON_Y, PadPhysical::Y},
            {SDL_CONTROLLER_BUTTON_BACK, PadPhysical::Back}, {SDL_CONTROLLER_BUTTON_START, PadPhysical::Start},
            {SDL_CONTROLLER_BUTTON_LEFTSTICK, PadPhysical::LeftStick},
            {SDL_CONTROLLER_BUTTON_RIGHTSTICK, PadPhysical::RightStick},
            {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, PadPhysical::LB}, {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, PadPhysical::RB},
            {SDL_CONTROLLER_BUTTON_DPAD_UP, PadPhysical::DPadUp}, {SDL_CONTROLLER_BUTTON_DPAD_DOWN, PadPhysical::DPadDown},
            {SDL_CONTROLLER_BUTTON_DPAD_LEFT, PadPhysical::DPadLeft},
            {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, PadPhysical::DPadRight}};
        for (const auto& [button, physical] : kButtons)
            if (held(pad, button)) r.buttons |= padBit(physical);
        // The triggers count as buttons past half way (assumption, like the sticks).
        if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > kStickThreshold)
            r.buttons |= padBit(PadPhysical::LT);
        if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > kStickThreshold)
            r.buttons |= padBit(PadPhysical::RT);
    }
    r.leftStick = direction(up, down, left, right);
    r.rightStick = direction(rUp, rDown, rLeft, rRight);
    return r;
}

PadOutput ControllerInput::update(Uint32 now, bool enabled, bool focused, const PadContext& context, bool capturing) {
    PadOutput out;
    if (pads_.empty() || !(enabled || capturing) || !focused) {
        out = input_.release(); // at once, not on the next timer tick
        osk_.reset();
        capture_.tick(PadRaw{}, false);
        nextTick_ = 0;
    } else {
        // RECOMP: the on-screen keyboard reads the pad at every call (each frame and each
        // controller event) instead of the 80 ms tick; the original layer pauses.
        const bool keyboard = context.keyboard && !capturing;
        if (!keyboard && nextTick_ != 0 && !SDL_TICKS_PASSED(now, nextTick_)) return out;
        nextTick_ = keyboard ? 0 : now + kTickMs;
        const PadRaw r = raw();
        const PadCapture::Result c = capture_.tick(r, capturing);
        if (!keyboard || !c.translate) osk_.reset();
        if (!c.translate) { // waiting for a button, or for everything released after it
            out = input_.release();
            out.captured = c.captured;
        } else if (keyboard) {
            out = input_.release(); // key-up of a held direction; afterwards a neutral start
            if (devicesChanged_) osk_.reset();
            out.keyboardCommands = osk_.tick(map_, oskRaw(map_, r), now);
        } else {
            const PadState s = translate(map_, r);
            if (devicesChanged_) input_.devicesChanged(s.buttons);
            out = input_.tick(s, context);
        }
    }
    devicesChanged_ = false;
    for (const PadKey& k : out.keys) push(k);
    out.keys.clear();
    return out;
}

PadOutput ControllerInput::scripted(const PadRaw& raw, const PadContext& context, bool capturing) {
    PadOutput out;
    if (capturing) { // the lowest pressed button is the choice
        for (int i = 0; i < kPadPhysicalCount; ++i)
            if (raw.buttons & (1u << unsigned(i))) { out.captured = i; break; }
        return out;
    }
    if (context.keyboard) { // the on-screen keyboard's path, as update() takes it
        const Uint32 now = SDL_GetTicks();
        if (!osk_.active()) osk_.tick(map_, {}, now); // the neutral start
        out.keyboardCommands = osk_.tick(map_, oskRaw(map_, raw), now);
        osk_.tick(map_, {}, now); // released
        return out;
    }
    osk_.reset();
    const PadState state = translate(map_, raw);
    if (!input_.active()) input_.tick(PadState{}, context); // the neutral start
    out = input_.tick(state, context);
    for (const PadKey& k : input_.tick(PadState{}, context).keys) out.keys.push_back(k); // released
    for (const PadKey& k : out.keys) push(k);
    out.keys.clear();
    return out;
}

} // namespace dtvr
