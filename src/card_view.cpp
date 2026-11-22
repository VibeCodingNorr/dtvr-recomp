// SPDX-License-Identifier: GPL-3.0-only
#include "card_view.hpp"

#include <cstdio>

namespace fs = std::filesystem;

namespace dtvr {

CardView::CardView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout)
    : renderer_(renderer), root_(root), layout_(layout) {}

bool CardView::open(const Game& game, int mask) {
    close();
    const int p = game.currentPlayer();
    if (p < 0) return false;
    std::array<bool, 8> occupied{};
    std::array<int, 8> types{};
    for (size_t s = 0; s < 8; ++s) {
        types[s] = game.deck().typeOfSlot(game.hand(p)[s]); // hand byte 0xFF: empty (seg3:09D8)
        occupied[s] = types[s] >= 0;
    }
    std::vector<SbButton> buttons;
    for (const CardButton& b : cardWindowLayout(layout_, occupied)) {
        const int type = types[size_t(b.slot)];
        const int category = cardTypes()[size_t(type)].category; // seg3:0701
        buttons.push_back({b.slot, SDL_Rect{b.rect.x, b.rect.y, b.rect.w, b.rect.h}, mask < 0 || (category & mask) == category});
        types_[b.slot] = type;
    }
    // seg3:0A77: SetFocus on every occupied slot 0..7 in turn, the last one keeps it.
    int focus = -1;
    for (int s = 0; s < 8; ++s)
        if (occupied[size_t(s)]) focus = s;
    buttons_.reset(std::move(buttons), focus);
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

void CardView::draw(SDL_Texture* disabled) {
    for (const SbButton& b : buttons_.buttons()) {
        const int type = types_[b.id];
        const SDL_Rect pic = buttons_.pictureRect(b);
        SDL_RenderSetClipRect(renderer_, &b.rect);
        if (SDL_Texture* t = picture(type)) SDL_RenderCopy(renderer_, t, nullptr, &pic);
        else {
            SDL_SetRenderDrawColor(renderer_, 67, 27, 0, 255);
            SDL_RenderDrawRect(renderer_, &b.rect);
        }
        if (disabled && !b.enabled) SDL_RenderCopy(renderer_, disabled, nullptr, &b.rect); // seg2:9497 at (0, 0)
        SDL_RenderSetClipRect(renderer_, nullptr);
    }
    buttons_.drawFocus(renderer_);
}

} // namespace dtvr
