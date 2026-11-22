// SPDX-License-Identifier: GPL-3.0-only
#include "card_view.hpp"

#include <cstdio>

namespace fs = std::filesystem;

namespace dtvr {

CardView::CardView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout)
    : renderer_(renderer), root_(root), layout_(layout) {}

bool CardView::open(const Game& game, int mask) {
    buttons_.clear();
    mask_ = mask;
    const int p = game.currentPlayer();
    if (p < 0) return false;
    std::array<bool, 8> occupied{};
    std::array<int, 8> types{};
    for (size_t s = 0; s < 8; ++s) {
        types[s] = game.deck().typeOfSlot(game.hand(p)[s]); // hand byte 0xFF: empty (seg3:09D8)
        occupied[s] = types[s] >= 0;
    }
    for (const CardButton& b : cardWindowLayout(layout_, occupied)) buttons_.push_back({b, types[size_t(b.slot)]});
    // seg3:0A77: SetFocus on every occupied slot 0..7 in turn, the last one keeps it.
    focus_ = -1;
    for (int s = 0; s < 8; ++s)
        if (occupied[size_t(s)]) focus_ = s;
    pressed_ = -1;
    return isOpen();
}

SDL_Texture* CardView::picture(int type) {
    auto it = pictures_.find(type);
    if (it == pictures_.end()) {
        // seg3:0488: "%s\%d\%c\%dcard_%.2d.bmp" with the art directory, the language
        // letter and the card type + 1.
        char name[32];
        std::snprintf(name, sizeof name, "%dcard_%02d.bmp", layout_.artDir, type + 1);
        Texture t(nullptr, SDL_DestroyTexture);
        const fs::path path = findCaseInsensitive(root_, fs::path("bmp") / std::to_string(layout_.artDir) / "d" / name);
        if (!path.empty()) {
            auto surface = loadBmp(path);
            t.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        }
        it = pictures_.emplace(type, std::move(t)).first;
    }
    return it->second.get();
}

bool CardView::enabled(int type) const {
    if (mask_ < 0 || type < 0) return true;
    const int category = cardTypes()[size_t(type)].category; // seg3:0701
    return (category & mask_) == category;
}

int CardView::indexOfSlot(int slot) const {
    for (size_t i = 0; i < buttons_.size(); ++i)
        if (buttons_[i].first.slot == slot) return int(i);
    return -1;
}

CardView::Input CardView::activate(int index) {
    Input in;
    if (index < 0 || !enabled(buttons_[size_t(index)].second)) return in; // seg1:0F9C
    in.slot = buttons_[size_t(index)].first.slot;
    in.sound = 2; // Plopp02 on channel 2 (seg1:0FA5)
    return in;
}

CardView::Input CardView::mouseDown(int x, int y) {
    Input in;
    for (const auto& [b, type] : buttons_) {
        const ScreenRect& r = b.rect;
        if (x < r.x || x >= r.x + r.w || y < r.y || y >= r.y + r.h) continue;
        focus_ = b.slot;
        if (enabled(type)) pressed_ = b.slot;
        else in.sound = 7;
        break;
    }
    return in;
}

CardView::Input CardView::mouseUp(int x, int y) {
    const int pressed = pressed_;
    pressed_ = -1;
    if (pressed < 0) return Input{};
    const int index = indexOfSlot(pressed);
    const ScreenRect& r = buttons_[size_t(index)].first.rect;
    if (x < r.x || x >= r.x + r.w || y < r.y || y >= r.y + r.h) return Input{}; // seg1:13F7
    return activate(index);
}

// seg1:2474 ff.: left / right take the neighbour in the same row, up / down the one
// in the same column (else the next button). Approximated by position: the original
// walks the sibling z-order (GetNextWindow).
bool CardView::moveFocus(int dx, int dy) {
    const int from = indexOfSlot(focus_);
    if (from < 0) return false;
    const ScreenRect& f = buttons_[size_t(from)].first.rect;
    int best = -1, bestDistance = 0;
    for (size_t i = 0; i < buttons_.size(); ++i) {
        const ScreenRect& r = buttons_[i].first.rect;
        const bool sameLine = dx ? r.y == f.y : r.x == f.x;
        const int d = dx ? (r.x - f.x) * dx : (r.y - f.y) * dy;
        if (!sameLine || d <= 0) continue;
        if (best < 0 || d < bestDistance) { best = int(i); bestDistance = d; }
    }
    if (best < 0 && dy) best = (from + (dy > 0 ? 1 : int(buttons_.size()) - 1)) % int(buttons_.size());
    if (best < 0 || best == from) return false;
    focus_ = buttons_[size_t(best)].first.slot;
    return true;
}

CardView::Input CardView::key(SDL_Keycode key, Uint16 mod) {
    Input in;
    if (!isOpen()) return in;
    bool moved = false;
    switch (key) {
    case SDLK_RIGHT: moved = moveFocus(1, 0); break;
    case SDLK_LEFT: moved = moveFocus(-1, 0); break;
    case SDLK_DOWN: moved = moveFocus(0, 1); break;
    case SDLK_UP: moved = moveFocus(0, -1); break;
    case SDLK_TAB: moved = moveFocus((mod & KMOD_SHIFT) ? -1 : 1, 0); break; // seg1:1060
    case SDLK_SPACE: case SDLK_RETURN: case SDLK_KP_ENTER: return activate(indexOfSlot(focus_));
    case SDLK_ESCAPE: case SDLK_BACKSPACE: in.cancel = true; return in; // seg1:0F77
    default: return in;
    }
    if (moved) in.sound = 8; // seg1:24E5
    return in;
}

void CardView::draw(SDL_Texture* disabled) {
    for (const auto& [b, type] : buttons_) {
        const SDL_Rect dst{b.rect.x, b.rect.y, b.rect.w, b.rect.h};
        const int shift = b.slot == pressed_ ? 1 : 0; // seg1:183B: pressed at (1, 1)
        const SDL_Rect pic{dst.x + shift, dst.y + shift, dst.w, dst.h};
        SDL_RenderSetClipRect(renderer_, &dst);
        if (SDL_Texture* t = picture(type)) SDL_RenderCopy(renderer_, t, nullptr, &pic);
        else {
            SDL_SetRenderDrawColor(renderer_, 67, 27, 0, 255);
            SDL_RenderDrawRect(renderer_, &dst);
        }
        if (disabled && !enabled(type)) SDL_RenderCopy(renderer_, disabled, nullptr, &dst); // seg2:9497 at (0, 0)
        SDL_RenderSetClipRect(renderer_, nullptr);
        if (b.slot == focus_) { // seg1:19B6: PS_INSIDEFRAME pen, width 5, red
            SDL_SetRenderDrawColor(renderer_, 255, 0, 0, 255);
            for (int i = 0; i < 5; ++i) {
                const SDL_Rect frame{dst.x + i, dst.y + i, dst.w - 2 * i, dst.h - 2 * i};
                SDL_RenderDrawRect(renderer_, &frame);
            }
        }
    }
}

} // namespace dtvr
