// SPDX-License-Identifier: GPL-3.0-only
#include "sb_button.hpp"

#include <utility>

namespace dtvr {

void SbButtonGroup::reset(std::vector<SbButton> buttons, int focus) {
    buttons_ = std::move(buttons);
    focus_ = indexOf(focus) >= 0 ? focus : -1;
    pressed_ = -1;
}

void SbButtonGroup::setEnabled(int id, bool enabled) {
    if (const int i = indexOf(id); i >= 0) buttons_[size_t(i)].enabled = enabled;
}

int SbButtonGroup::indexOf(int id) const {
    for (size_t i = 0; i < buttons_.size(); ++i)
        if (buttons_[i].id == id) return int(i);
    return -1;
}

SbButtonGroup::Input SbButtonGroup::activate(int index) const {
    Input in;
    if (index < 0 || !buttons_[size_t(index)].enabled) return in; // seg1:0F9C
    in.id = buttons_[size_t(index)].id;
    in.sound = 2; // Plopp02 on channel 2 (seg1:0FA5)
    return in;
}

SbButtonGroup::Input SbButtonGroup::mouseDown(int x, int y) {
    Input in;
    const SDL_Point at{x, y};
    for (const SbButton& b : buttons_) {
        if (!SDL_PointInRect(&at, &b.rect)) continue;
        focus_ = b.id;                    // SetFocus unless focused (seg1:1337)
        if (b.enabled) pressed_ = b.id;   // bits 9 (seg1:136D)
        else in.sound = 7;                // seg1:1357
        break;
    }
    return in;
}

SbButtonGroup::Input SbButtonGroup::mouseUp(int x, int y) {
    const int index = indexOf(pressed_);
    pressed_ = -1;
    if (index < 0) return Input{};
    const SDL_Point at{x, y};
    if (!SDL_PointInRect(&at, &buttons_[size_t(index)].rect)) return Input{}; // seg1:13F7
    return activate(index);
}

// seg1:2474 ff.: left / right take the neighbour in the same row (same top), up /
// down the one in the same column (same left), else the adjacent sibling. The
// original walks the sibling z-order (GetNextWindow); here the nearest by position.
bool SbButtonGroup::moveFocus(int dx, int dy) {
    const int from = indexOf(focus_);
    if (from < 0) return false;
    const SDL_Rect& f = buttons_[size_t(from)].rect;
    int best = -1, bestDistance = 0;
    for (size_t i = 0; i < buttons_.size(); ++i) {
        const SDL_Rect& r = buttons_[i].rect;
        const bool sameLine = dx ? r.y == f.y : r.x == f.x;
        const int d = dx ? (r.x - f.x) * dx : (r.y - f.y) * dy;
        if (!sameLine || d <= 0) continue;
        if (best < 0 || d < bestDistance) { best = int(i); bestDistance = d; }
    }
    if (best < 0 && dy) best = (from + (dy > 0 ? 1 : int(buttons_.size()) - 1)) % int(buttons_.size());
    if (best < 0 || best == from) return false;
    focus_ = buttons_[size_t(best)].id;
    return true;
}

SbButtonGroup::Input SbButtonGroup::key(SDL_Keycode key, Uint16 mod, bool returnActivates) {
    Input in;
    bool moved = false;
    switch (key) {
    case SDLK_RIGHT: moved = moveFocus(1, 0); break;
    case SDLK_LEFT: moved = moveFocus(-1, 0); break;
    case SDLK_DOWN: moved = moveFocus(0, 1); break;
    case SDLK_UP: moved = moveFocus(0, -1); break;
    case SDLK_TAB: moved = moveFocus((mod & KMOD_SHIFT) ? -1 : 1, 0); break; // seg1:1060
    case SDLK_SPACE: return activate(indexOf(focus_));
    case SDLK_RETURN: case SDLK_KP_ENTER: return returnActivates ? activate(indexOf(focus_)) : in;
    case SDLK_ESCAPE: case SDLK_BACKSPACE: in.cancel = true; return in; // seg1:0F77
    default: return in;
    }
    if (moved) in.sound = 8; // seg1:24E5
    return in;
}

SDL_Rect SbButtonGroup::pictureRect(const SbButton& button) const {
    const int shift = button.id == pressed_ ? 1 : 0; // seg1:183B
    return SDL_Rect{button.rect.x + shift, button.rect.y + shift, button.rect.w, button.rect.h};
}

void SbButtonGroup::drawFocus(SDL_Renderer* renderer) const {
    const int i = indexOf(focus_);
    if (i < 0) return;
    const SDL_Rect& r = buttons_[size_t(i)].rect;
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for (int k = 0; k < 5 && 2 * k < r.w && 2 * k < r.h; ++k) {
        const SDL_Rect frame{r.x + k, r.y + k, r.w - 2 * k, r.h - 2 * k};
        SDL_RenderDrawRect(renderer, &frame);
    }
}

} // namespace dtvr
