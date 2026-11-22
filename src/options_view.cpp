// SPDX-License-Identifier: GPL-3.0-only
#include "options_view.hpp"

namespace fs = std::filesystem;

namespace dtvr {
namespace {

bool inside(const ScreenRect& r, int x, int y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }

} // namespace

OptionsView::OptionsView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout)
    : renderer_(renderer), root_(root), layout_(layout), geometry_(setupGeometry(layout.width)) {}

SDL_Texture* OptionsView::bitmap(const fs::path& relative) {
    const std::string key = relative.generic_string();
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        Texture t(nullptr, SDL_DestroyTexture);
        const fs::path path = findCaseInsensitive(root_, relative);
        if (!path.empty()) {
            auto surface = loadBmp(path);
            t.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        }
        it = cache_.emplace(key, std::move(t)).first;
    }
    return it->second.get();
}

OptionsView::Result OptionsView::handle(const SDL_Event& event) {
    if (!open_) return Result::None;
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int x = event.button.x, y = event.button.y;
        for (int i = 0; i < Settings::kCount; ++i)
            if (inside(geometry_.driver[size_t(i)], x, y)) { settings_.toggle(i); return Result::None; }
        if (inside(geometry_.start, x, y)) return Result::Accept;
        if (inside(geometry_.stop, x, y)) return Result::Cancel;
        return Result::None;
    }
    if (event.type == SDL_KEYDOWN) { // assumption: Enter accepts, Escape cancels
        if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) return Result::Accept;
        if (event.key.keysym.sym == SDLK_ESCAPE) return Result::Cancel;
    }
    return Result::None;
}

void OptionsView::draw() {
    if (!open_) return;
    const fs::path butt = fs::path("butt") / std::to_string(layout_.artDir);
    if (SDL_Texture* back = bitmap(butt / "d" / "menu.bmp")) SDL_RenderCopy(renderer_, back, nullptr, nullptr);
    auto blit = [&](const fs::path& file, const ScreenRect& r) {
        if (SDL_Texture* t = bitmap(file)) {
            const SDL_Rect dst{r.x, r.y, r.w, r.h};
            SDL_RenderCopy(renderer_, t, nullptr, &dst);
        }
    };
    for (int i = 0; i < Settings::kCount; ++i) // seg1:AFB2: "%s\%d\op_menu\%d%s.bmp"
        blit(butt / "op_menu" / (std::to_string(settings_.value[size_t(i)]) + Settings::switchName(i) + ".bmp"),
             geometry_.driver[size_t(i)]);
    blit(butt / "pl_menu" / "star.bmp", geometry_.start);
    blit(butt / "pl_menu" / "stop.bmp", geometry_.stop);
}

} // namespace dtvr
