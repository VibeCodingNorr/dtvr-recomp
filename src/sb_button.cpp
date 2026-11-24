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

bool SbButtonGroup::fieldFocused() const {
    const int i = indexOf(focus_);
    return i >= 0 && buttons_[size_t(i)].field;
}

int SbButtonGroup::indexOf(int id) const {
    for (size_t i = 0; i < buttons_.size(); ++i)
        if (buttons_[i].id == id) return int(i);
    return -1;
}

SbButtonGroup::Input SbButtonGroup::activate(int index) const {
    Input in;
    if (index < 0 || buttons_[size_t(index)].field || !buttons_[size_t(index)].enabled) return in; // seg1:0F9C
    in.id = buttons_[size_t(index)].id;
    in.sound = 2; // Plopp02 on channel 2 (seg1:0FA5)
    return in;
}

SbButtonGroup::Input SbButtonGroup::mouseDown(int x, int y) {
    Input in;
    const SDL_Point at{x, y};
    for (const SbButton& b : buttons_) {
        if (b.field || !SDL_PointInRect(&at, &b.rect)) continue;
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

// SetFocus on a found sibling with sound 8 on channel 2 (seg1:24E5 ff.). The walk
// also requires a control id in 0x1388-0x1770 (seg1:24D3); every entry of the
// rebuilt groups is such an SB control.
bool SbButtonGroup::focusAt(int index) {
    if (index < 0 || index >= int(buttons_.size())) return false; // GetNextWindow: no sibling
    focus_ = buttons_[size_t(index)].id;
    return true;
}

// seg1:2474 (right) / 250A (left): GetNextWindow(GW_HWNDPREV / GW_HWNDNEXT) from the
// focused window to the first sibling with the same top. seg1:25A0 (down) / 267F (up):
// GW_HWNDPREV / GW_HWNDNEXT to the first one with the same left, and when there is
// none, the adjacent sibling in that direction. No wrap-around.
bool SbButtonGroup::walk(Walk direction) {
    const int from = indexOf(focus_);
    if (from < 0) return false;
    const bool towardsTop = direction == Walk::Right || direction == Walk::Down; // GW_HWNDPREV
    const int step = towardsTop ? -1 : 1;
    const bool row = direction == Walk::Right || direction == Walk::Left;
    const SDL_Rect& f = buttons_[size_t(from)].rect;
    for (int i = from + step; i >= 0 && i < int(buttons_.size()); i += step) {
        const SDL_Rect& r = buttons_[size_t(i)].rect;
        if (row ? r.y == f.y : r.x == f.x) return focusAt(i);
    }
    return row ? false : focusAt(from + step);
}

SbButtonGroup::Input SbButtonGroup::key(SDL_Keycode key, Uint16 mod, bool returnActivates) {
    Input in;
    bool moved = false;
    switch (key) {
    case SDLK_RIGHT: moved = walk(Walk::Right); break;
    case SDLK_LEFT: moved = walk(Walk::Left); break;
    case SDLK_DOWN: moved = walk(Walk::Down); break;
    case SDLK_UP: moved = walk(Walk::Up); break;
    case SDLK_TAB: moved = walk((mod & KMOD_SHIFT) ? Walk::Left : Walk::Right); break; // seg1:1060
    case SDLK_SPACE: return activate(indexOf(focus_));
    case SDLK_RETURN: case SDLK_KP_ENTER: return returnActivates ? activate(indexOf(focus_)) : in;
    case SDLK_ESCAPE: case SDLK_BACKSPACE: in.cancel = true; return in; // seg1:0F77
    default: return in;
    }
    if (moved) { in.sound = 8; in.moved = true; } // seg1:24E5
    return in;
}

SbButtonGroup::Input SbButtonGroup::fieldKey(SDL_Keycode key, Uint16 mod) {
    Input in;
    const int from = indexOf(focus_);
    bool moved = false;
    switch (key) {
    case SDLK_RIGHT: moved = walk(Walk::Right); break; // seg1:21AA
    case SDLK_LEFT: moved = walk(Walk::Left); break;   // seg1:21B8
    case SDLK_DOWN: moved = walk(Walk::Down); break;   // seg1:21C6
    case SDLK_UP: moved = walk(Walk::Up); break;       // seg1:21D4
    case SDLK_TAB: moved = from >= 0 && focusAt(from + ((mod & KMOD_SHIFT) ? 1 : -1)); break; // seg1:209E
    case SDLK_RETURN: case SDLK_KP_ENTER: moved = from >= 0 && focusAt(from - 1); break;      // seg1:2144
    default: return in;
    }
    if (moved) { in.sound = 8; in.moved = true; }
    return in;
}

SDL_Rect SbButtonGroup::pictureRect(const SbButton& button) const {
    const int shift = button.id == pressed_ ? 1 : 0; // seg1:183B
    return SDL_Rect{button.rect.x + shift, button.rect.y + shift, button.rect.w, button.rect.h};
}

void SbButtonGroup::drawFocus(SDL_Renderer* renderer) const {
    const int i = indexOf(focus_);
    if (i < 0 || buttons_[size_t(i)].field) return; // SBINPUT shows its caret instead
    const SDL_Rect& r = buttons_[size_t(i)].rect;
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for (int k = 0; k < 5 && 2 * k < r.w && 2 * k < r.h; ++k) {
        const SDL_Rect frame{r.x + k, r.y + k, r.w - 2 * k, r.h - 2 * k};
        SDL_RenderDrawRect(renderer, &frame);
    }
}

} // namespace dtvr
