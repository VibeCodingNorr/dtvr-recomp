// SPDX-License-Identifier: GPL-3.0-only
#include "scripted_input.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace dtvr {

namespace {

// A physical button (PadMap names); Up / Down / Left / Right are the d-pad.
bool padState(const std::string& name, PadRaw& state) {
    static const std::pair<const char*, PadPhysical> kDirections[] = {
        {"Up", PadPhysical::DPadUp}, {"Down", PadPhysical::DPadDown},
        {"Left", PadPhysical::DPadLeft}, {"Right", PadPhysical::DPadRight}};
    for (const auto& [n, p] : kDirections)
        if (name == n) { state.buttons = padBit(p); return true; }
    PadPhysical p;
    if (!padPhysicalFromName(name, p)) return false;
    state.buttons = padBit(p);
    return true;
}

} // namespace

ScriptedInput::ScriptedInput(const std::string& keys) {
    std::istringstream in(keys);
    std::string name;
    while (std::getline(in, name, ',')) {
        if (name.empty()) continue;
        Entry e;
        if (name.rfind("Pad:", 0) == 0) {
            e.pad = true;
            if (!padState(name.substr(4), e.state)) throw std::invalid_argument("--press: unbekannte Pad-Taste " + name);
        } else if (name.rfind("Click", 0) == 0) { // ClickLeft:x:y / ClickRight:x:y (canvas pixels)
            const bool right = name.rfind("ClickRight:", 0) == 0;
            if (!right && name.rfind("ClickLeft:", 0) != 0) throw std::invalid_argument("--press: unbekannter Klick " + name);
            const std::string at = name.substr(right ? 11 : 10);
            const size_t colon = at.find(':');
            if (colon == std::string::npos) throw std::invalid_argument("--press: Klick ohne x:y " + name);
            e.button = right ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
            e.x = std::stoi(at.substr(0, colon));
            e.y = std::stoi(at.substr(colon + 1));
        } else {
            e.key = SDL_GetKeyFromName(name.c_str());
            if (e.key == SDLK_UNKNOWN) throw std::invalid_argument("--press: unbekannte Taste " + name);
        }
        entries_.push_back(e);
    }
}

bool ScriptedInput::takePad(PadRaw& state) {
    if (!padPending_) return false;
    padPending_ = false;
    state = pad_;
    return true;
}

void ScriptedInput::pump(int frame, int interval) {
    if (done() || interval <= 0 || frame < interval || frame % interval != 0) return;
    const Entry& entry = entries_[next_++];
    if (entry.pad) {
        pad_ = entry.state;
        padPending_ = true;
        return;
    }
    SDL_Event event{};
    if (entry.button != 0) { // a click: down and up at the same point
        event.type = SDL_MOUSEBUTTONDOWN;
        event.button.button = Uint8(entry.button);
        event.button.state = SDL_PRESSED;
        event.button.clicks = 1;
        event.button.x = entry.x;
        event.button.y = entry.y;
        SDL_PushEvent(&event);
        event.type = SDL_MOUSEBUTTONUP;
        event.button.state = SDL_RELEASED;
        SDL_PushEvent(&event);
        return;
    }
    event.type = SDL_KEYDOWN;
    event.key.state = SDL_PRESSED;
    event.key.keysym.sym = entry.key;
    event.key.keysym.scancode = SDL_GetScancodeFromKey(event.key.keysym.sym);
    SDL_PushEvent(&event);
    event.type = SDL_KEYUP;
    event.key.state = SDL_RELEASED;
    SDL_PushEvent(&event);
}

} // namespace dtvr
