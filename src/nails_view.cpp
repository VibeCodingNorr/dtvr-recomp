// SPDX-License-Identifier: GPL-3.0-only
#include "nails_view.hpp"

#include <algorithm>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 0x226, kHeight = 0x104; // CreateWindow seg3:1D27
// seg3:1E72: the triangles (client coordinates) for up, right, down, left.
constexpr SDL_Point kArrows[4][3] = {{{0x78, 0x07}, {0x87, 0x16}, {0x69, 0x16}},
                                     {{0xDC, 0x69}, {0xEB, 0x78}, {0xDC, 0x87}},
                                     {{0x69, 0xDC}, {0x87, 0xDC}, {0x78, 0xEB}},
                                     {{0x06, 0x78}, {0x15, 0x69}, {0x15, 0x87}}};

bool inside(const SDL_Point (&t)[3], int x, int y) {
    auto side = [](SDL_Point a, SDL_Point b, int px, int py) {
        return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
    };
    const int d1 = side(t[0], t[1], x, y), d2 = side(t[1], t[2], x, y), d3 = side(t[2], t[0], x, y);
    const bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
    return !(neg && pos);
}

} // namespace

NailsView::NailsView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout,
                     const std::map<int, std::string>& texts, TextRenderer& text)
    : renderer_(renderer), root_(root), layout_(layout), texts_(texts), text_(text) {}

void NailsView::open() {
    if (!paper_) {
        if (const fs::path p = findCaseInsensitive(root_, fs::path("butt") / "n.bmp"); !p.empty()) { // DS:3204
            auto surface = loadBmp(p);
            paper_.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        }
    }
    open_ = true;
}

SDL_Rect NailsView::box() const {
    return SDL_Rect{(layout_.width - kWidth) / 2, (layout_.height - kHeight) / 2, kWidth, kHeight};
}

int NailsView::handle(const SDL_Event& event) {
    if (!open_) return 0;
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const SDL_Rect b = box();
        for (int d = 0; d < 4; ++d)
            if (inside(kArrows[d], event.button.x - b.x, event.button.y - b.y)) return d + 1; // posts VK 0x26-0x28, 0x25
        return 0;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) return -2;
    if (event.type != SDL_KEYDOWN) return 0;
    switch (event.key.keysym.sym) {
    case SDLK_UP: return 1;
    case SDLK_RIGHT: return 2;
    case SDLK_DOWN: return 3;
    case SDLK_LEFT: return 4;
    case SDLK_ESCAPE: return -2;
    default: return 0;
    }
}

void NailsView::draw() {
    if (!open_) return;
    const SDL_Rect b = box();
    if (paper_) {
        const SDL_Rect src{0, 0, std::min(kWidth, 661), std::min(kHeight, 270)};
        SDL_RenderCopy(renderer_, paper_.get(), &src, &b);
    }
    // The arrows (their drawing is not traced; filled in the brown text colour).
    SDL_SetRenderDrawColor(renderer_, 67, 27, 0, 255);
    for (const auto& t : kArrows) {
        const int minY = std::min({t[0].y, t[1].y, t[2].y}), maxY = std::max({t[0].y, t[1].y, t[2].y});
        const int minX = std::min({t[0].x, t[1].x, t[2].x}), maxX = std::max({t[0].x, t[1].x, t[2].x});
        for (int y = minY; y <= maxY; ++y)
            for (int x = minX; x <= maxX; ++x)
                if (inside(t, x, y)) SDL_RenderDrawPoint(renderer_, b.x + x, b.y + y);
    }
    const auto it = texts_.find(510); // "In welche Richtung willst Du die Nägel werfen ?"
    if (it != texts_.end())
        text_.drawWrapped(renderer_, it->second, ScreenRect{b.x + 0x104, b.y + 0x60, kWidth - 0x104 - 16, kHeight - 0x60},
                          TextStyle{18, true, false, false, false, 0, SDL_Color{67, 27, 0, 255}});
}

} // namespace dtvr
