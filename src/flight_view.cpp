// SPDX-License-Identifier: GPL-3.0-only
#include "flight_view.hpp"

#include <algorithm>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 0x114, kHeight = 0x100;      // CreateWindow seg3:3212
constexpr int kRowTop = 0x32, kRowHeight = 0x18, kRowWidth = 0xF0; // seg3:33C8
constexpr SDL_Color kBrown{67, 27, 0, 255}, kWhite{255, 255, 255, 255};

} // namespace

FlightView::FlightView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout,
                       const std::map<int, std::string>& texts, TextRenderer& text)
    : renderer_(renderer), root_(root), layout_(layout), texts_(texts), text_(text) {}

std::string FlightView::text(int id) const {
    const auto it = texts_.find(id);
    return it == texts_.end() ? std::string() : it->second;
}

void FlightView::open(bool eastOpen, bool lastStage) {
    if (!paper_) {
        if (const fs::path p = findCaseInsensitive(root_, fs::path("butt") / "f.bmp"); !p.empty()) { // DS:3252
            auto surface = loadBmp(p);
            paper_.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        }
    }
    eastOpen_ = eastOpen;
    lastStage_ = lastStage;
    focus_ = 0;
    open_ = true;
}

SDL_Rect FlightView::box() const {
    return SDL_Rect{(layout_.width - kWidth) / 2, (layout_.height - kHeight) / 2, kWidth, kHeight};
}

FlightView::Result FlightView::choose(int row) {
    Result r;
    if (row == 1 && lastStage_) { r.closed = true; return r; } // seg3:33F2: Bodennebel (text 8300)
    r.town = text(5602 + row);
    return r;
}

FlightView::Result FlightView::handle(const SDL_Event& event) {
    Result r;
    if (!open_) return r;
    const SDL_Rect b = box();
    auto rowAt = [&](int x, int y) {
        x -= b.x;
        y -= b.y;
        if (x < 0 || x >= kRowWidth || y < kRowTop) return -1;
        const int row = (y - kRowTop) / kRowHeight;
        return row < rows() ? row : -1;
    };
    if (event.type == SDL_MOUSEMOTION) {
        if (const int row = rowAt(event.motion.x, event.motion.y); row >= 0) focus_ = row;
    } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (const int row = rowAt(event.button.x, event.button.y); row >= 0) return choose(row);
    } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        r.cancel = true;
    } else if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_UP: focus_ = focus_ > 0 ? focus_ - 1 : rows() - 1; break;
        case SDLK_DOWN: focus_ = focus_ + 1 < rows() ? focus_ + 1 : 0; break;
        case SDLK_RETURN: case SDLK_KP_ENTER: return choose(focus_);
        case SDLK_ESCAPE: r.cancel = true; break;
        default: break;
        }
    }
    return r;
}

void FlightView::draw() {
    if (!open_) return;
    const SDL_Rect b = box();
    if (paper_) {
        int pw = 0, ph = 0;
        SDL_QueryTexture(paper_.get(), nullptr, nullptr, &pw, &ph);
        for (int y = 0; pw > 0 && ph > 0 && y < b.h; y += ph)
            for (int x = 0; x < b.w; x += pw) {
                const SDL_Rect src{0, 0, std::min(pw, b.w - x), std::min(ph, b.h - y)};
                const SDL_Rect dst{b.x + x, b.y + y, src.w, src.h};
                SDL_RenderCopy(renderer_, paper_.get(), &src, &dst);
            }
    }
    // seg3:36F6: Arial 20 weight 600 for the heading, 18 for the towns, colour 0x45,
    // centred (format 0x120); the focused town in white.
    TextStyle head{20, true, false, true, false, 0, kBrown};
    text_.draw(renderer_, text(5601), ScreenRect{b.x, b.y + 12, kRowWidth, kRowHeight}, head);
    for (int i = 0; i < rows(); ++i) {
        TextStyle row{18, true, false, true, false, 0, i == focus_ ? kWhite : kBrown};
        text_.draw(renderer_, text(5602 + i), ScreenRect{b.x, b.y + kRowTop + i * kRowHeight + 2, kRowWidth, kRowHeight}, row);
    }
}

} // namespace dtvr
