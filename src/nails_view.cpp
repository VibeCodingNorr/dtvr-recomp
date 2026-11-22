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

void NailsView::open(const std::array<bool, 4>& available) {
    if (!paper_) {
        if (const fs::path p = findCaseInsensitive(root_, fs::path("butt") / "n.bmp"); !p.empty()) { // DS:3204
            auto surface = loadBmp(p);
            paper_.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        }
    }
    available_ = available;
    selected_ = 0;
    for (int d = 0; d < 4; ++d)
        if (available_[size_t(d)]) selected_ = d + 1; // seg3:2366 / 239D / 23D4 / 240B
    open_ = selected_ != 0;                           // none: CreateWindow fails (seg3:241C)
}

SDL_Rect NailsView::box() const {
    return SDL_Rect{(layout_.width - kWidth) / 2, (layout_.height - kHeight) / 2, kWidth, kHeight};
}

int NailsView::handle(const SDL_Event& event) {
    if (!open_) return 0;
    auto select = [this](int d) { // seg3:1FF2 ff.: only an available direction
        if (d >= 1 && d <= 4 && available_[size_t(d - 1)]) selected_ = d;
    };
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) { // seg3:1F2E
        const SDL_Rect b = box();
        const int x = event.button.x - b.x, y = event.button.y - b.y;
        for (int d = 0; d < 4; ++d)
            if (inside(kArrows[d], x, y)) { select(d + 1); return 0; } // posts the arrow key
        // seg3:1FA4: client rect, left + 0xF0, inflated by (-40, -20), top + 0xBE: Ok.
        if (x >= 0x118 && x < 0x1FE && y >= 0xD2 && y < 0xF0) return selected_; // posts Enter
        return 0;
    }
    if (event.type != SDL_KEYDOWN) return 0; // no right button, no Escape (seg3:2769)
    switch (event.key.keysym.sym) {         // seg3:21B6
    case SDLK_UP: select(1); break;
    case SDLK_RIGHT: select(2); break;
    case SDLK_DOWN: select(3); break;
    case SDLK_LEFT: select(4); break;
    case SDLK_RETURN: case SDLK_KP_ENTER: return selected_; // seg3:2096
    default: break;
    }
    return 0;
}

void NailsView::draw(SDL_Texture* compass) {
    if (!open_) return;
    const SDL_Rect b = box();
    if (paper_) {
        const SDL_Rect src{0, 0, std::min(kWidth, 661), std::min(kHeight, 270)};
        SDL_RenderCopy(renderer_, paper_.get(), &src, &b);
    }
    // seg3:2557: the 96 x 96 map pixels around the player stretched to (0x19, 0x19, 0xC0, 0xC0).
    if (compass) {
        const SDL_Rect to{b.x + 0x19, b.y + 0x19, 0xC0, 0xC0};
        SDL_RenderCopy(renderer_, compass, nullptr, &to);
    }
    // seg3:2560 / 25C0: the available triangles in PALETTEINDEX(0), the selected one
    // in PALETTEINDEX(0xED) (black and red in the palette of n.bmp), seg3:27C8 Polygon.
    for (int d = 0; d < 4; ++d) {
        if (!available_[size_t(d)]) continue;
        const bool sel = d + 1 == selected_;
        SDL_SetRenderDrawColor(renderer_, sel ? 255 : 0, 0, 0, 255);
        const auto& t = kArrows[d];
        const int minY = std::min({t[0].y, t[1].y, t[2].y}), maxY = std::max({t[0].y, t[1].y, t[2].y});
        const int minX = std::min({t[0].x, t[1].x, t[2].x}), maxX = std::max({t[0].x, t[1].x, t[2].x});
        for (int y = minY; y <= maxY; ++y)
            for (int x = minX; x <= maxX; ++x)
                if (inside(t, x, y)) SDL_RenderDrawPoint(renderer_, b.x + x, b.y + y);
    }
    // seg3:25D2: text 510 in the client rect with left + 0xF0, inflated by (-10, -20),
    // Arial 18 weight 400, colour 0x45, DrawText 0x110; then text 114 (Ok) from
    // top + 0xBE in weight 600, white, centred (0x111).
    const ScreenRect area{b.x + 0xFA, b.y + 0x14, 0x21C - 0xFA, 0xF0 - 0x14};
    if (const auto it = texts_.find(510); it != texts_.end())
        text_.drawWrapped(renderer_, it->second, area, TextStyle{18, false, false, false, false, 0, SDL_Color{67, 27, 0, 255}});
    if (const auto it = texts_.find(114); it != texts_.end())
        text_.draw(renderer_, it->second, ScreenRect{area.x, b.y + 0xD2, area.w, 0xF0 - 0xD2},
                   TextStyle{18, true, false, true, false, 0, SDL_Color{255, 255, 255, 255}});
}

} // namespace dtvr
