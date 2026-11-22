// SPDX-License-Identifier: GPL-3.0-only
#include "mouse_control_view.hpp"

#include <algorithm>

namespace dtvr {

int mouseControlState(int x, int y) {
    if (x >= 0x15 && x <= 0x29 && y >= 0x15 && y <= 0x29) return 1; // PtInRect (seg4:10AF)
    if (y < 0x14) {                                                 // upper band (seg4:10F0)
        if (y > x) return 5;
        if (0x40 - y < x) return 3;
        return 2;
    }
    const int mirrored = 0x3F - y;                                  // seg4:1151
    if (mirrored > x) return 5;
    if (0x40 - mirrored < x) return 3;
    return 4;
}

SDL_Keycode mouseControlKey(int state) {
    switch (state) {
    case 1: return SDLK_RETURN;
    case 2: return SDLK_UP;
    case 3: return SDLK_RIGHT;
    case 4: return SDLK_DOWN;
    case 5: return SDLK_LEFT;
    default: return SDLK_UNKNOWN;
    }
}

void MouseControlView::open(int x, int y) {
    box_ = SDL_Rect{x - 0x20, y - 0x20, 0x40, 0x40};
    state_ = 1;
    open_ = true;
}

SDL_Keycode MouseControlView::handle(const SDL_Event& event, bool& closed) {
    closed = false;
    if (!open_) return SDLK_UNKNOWN;
    if (event.type == SDL_MOUSEMOTION) {
        // ClipCursor to the window: positions outside count as its nearest edge.
        const int x = std::clamp(event.motion.x - box_.x, 0, 0x3F), y = std::clamp(event.motion.y - box_.y, 0, 0x3F);
        state_ = mouseControlState(x, y);
    } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        open_ = false; // seg4:11B3: destroyed, the key posted to the main window
        closed = true;
        return mouseControlKey(state_);
    } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        open_ = false; // seg4:11E1
        closed = true;
    }
    return SDLK_UNKNOWN;
}

void MouseControlView::draw(bool moveDone) {
    if (!open_) return;
    static const char* const kFrames[6] = {"M1", "M0", "MT", "MR", "MD", "ML"}; // seg4:104F
    const char* name = state_ == 1 && moveDone ? kFrames[0] : kFrames[std::clamp(state_, 0, 5)];
    if (SDL_Texture* t = sprite_(name)) {
        int w = 0, h = 0;
        SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
        const SDL_Rect dst{box_.x, box_.y, w, h};
        SDL_RenderCopy(renderer_, t, nullptr, &dst);
    }
}

} // namespace dtvr
