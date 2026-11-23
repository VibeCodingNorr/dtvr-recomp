// SPDX-License-Identifier: GPL-3.0-only
#include "scripted_input.hpp"

#include <sstream>
#include <stdexcept>

namespace dtvr {

ScriptedInput::ScriptedInput(const std::string& keys) {
    std::istringstream in(keys);
    std::string name;
    while (std::getline(in, name, ',')) {
        if (name.empty()) continue;
        const SDL_Keycode key = SDL_GetKeyFromName(name.c_str());
        if (key == SDLK_UNKNOWN) throw std::invalid_argument("--press: unbekannte Taste " + name);
        keys_.push_back(key);
    }
}

void ScriptedInput::pump(int frame, int interval) {
    if (done() || interval <= 0 || frame < interval || frame % interval != 0) return;
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.state = SDL_PRESSED;
    event.key.keysym.sym = keys_[next_++];
    event.key.keysym.scancode = SDL_GetScancodeFromKey(event.key.keysym.sym);
    SDL_PushEvent(&event);
    event.type = SDL_KEYUP;
    event.key.state = SDL_RELEASED;
    SDL_PushEvent(&event);
}

} // namespace dtvr
