// SPDX-License-Identifier: GPL-3.0-only
#include "window_border.hpp"

namespace dtvr {

ClipGuard::ClipGuard(SDL_Renderer* renderer) : renderer_(renderer) {
    enabled_ = SDL_RenderIsClipEnabled(renderer_) == SDL_TRUE;
    SDL_RenderGetClipRect(renderer_, &clip_);
}

ClipGuard::~ClipGuard() { SDL_RenderSetClipRect(renderer_, enabled_ ? &clip_ : nullptr); }

SDL_Rect borderClient(const SDL_Rect& window, BorderSize size) {
    const int t = int(size); // seg6:1A91 ff.: left / top + t, right / bottom - t
    return SDL_Rect{window.x + t, window.y + t, window.w - 2 * t, window.h - 2 * t};
}

void drawWindowBorder(SDL_Renderer* renderer, const SpriteLookup& sprite, const SDL_Rect& window, BorderSize size) {
    if (!sprite) return;
    const int t = int(size);
    const std::string prefix = size == BorderSize::Thin ? "B" : "BS"; // DS:16A0 / DS:16B8
    auto piece = [&](int n, int& w, int& h) {
        SDL_Texture* tex = sprite(prefix + std::to_string(n));
        w = h = 0;
        if (tex) SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
        return tex;
    };
    // The window DC clips every piece to the window rectangle.
    const ClipGuard guard(renderer);
    SDL_RenderSetClipRect(renderer, &window);
    auto blit = [&](SDL_Texture* tex, int x, int y, int w, int h) {
        const SDL_Rect dst{window.x + x, window.y + y, w, h};
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
    };
    int w = 0, h = 0;
    if (SDL_Texture* top = piece(2, w, h); top && w > 0) // seg6:1BC8: x from t while x < width - t
        for (int x = t; x < window.w - t; x += w) blit(top, x, 0, w, h);
    if (SDL_Texture* bottom = piece(7, w, h); bottom && w > 0) // seg6:1C09: y = height - t
        for (int x = t; x < window.w - t; x += w) blit(bottom, x, window.h - t, w, h);
    if (SDL_Texture* left = piece(4, w, h); left && h > 0) // seg6:1C4E: y from t while y < height - t
        for (int y = t; y < window.h - t; y += h) blit(left, 0, y, w, h);
    if (SDL_Texture* right = piece(5, w, h); right && h > 0) // seg6:1C90: x = width - t
        for (int y = t; y < window.h - t; y += h) blit(right, window.w - t, y, w, h);
    if (SDL_Texture* c = piece(1, w, h)) blit(c, 0, 0, w, h);                         // seg6:1CD6
    if (SDL_Texture* c = piece(3, w, h)) blit(c, window.w - w, 0, w, h);              // seg6:1CFB
    if (SDL_Texture* c = piece(6, w, h)) blit(c, 0, window.h - h, w, h);              // seg6:1D27
    if (SDL_Texture* c = piece(8, w, h)) blit(c, window.w - w, window.h - h, w, h);   // seg6:1D54
}

} // namespace dtvr
