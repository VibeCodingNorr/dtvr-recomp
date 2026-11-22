// SPDX-License-Identifier: GPL-3.0-only
#include "card_view.hpp"

#include <cstdio>

namespace fs = std::filesystem;

namespace dtvr {

CardView::CardView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout)
    : renderer_(renderer), root_(root), layout_(layout) {}

bool CardView::open(const Game& game) {
    buttons_.clear();
    const int p = game.currentPlayer();
    if (p < 0) return false;
    std::array<bool, 8> occupied{};
    std::array<int, 8> types{};
    for (size_t s = 0; s < 8; ++s) {
        types[s] = game.deck().typeOfSlot(game.hand(p)[s]); // hand byte 0xFF: empty (seg3:09D8)
        occupied[s] = types[s] >= 0;
    }
    for (const CardButton& b : cardWindowLayout(layout_, occupied)) buttons_.push_back({b, types[size_t(b.slot)]});
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

int CardView::slotAt(int x, int y) const {
    for (const auto& [b, type] : buttons_) {
        const ScreenRect& r = b.rect;
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return b.slot;
    }
    return -1;
}

void CardView::draw() {
    for (const auto& [b, type] : buttons_) {
        const SDL_Rect dst{b.rect.x, b.rect.y, b.rect.w, b.rect.h};
        if (SDL_Texture* t = picture(type)) SDL_RenderCopy(renderer_, t, nullptr, &dst);
        else {
            SDL_SetRenderDrawColor(renderer_, 67, 27, 0, 255);
            SDL_RenderDrawRect(renderer_, &dst);
        }
    }
}

} // namespace dtvr
