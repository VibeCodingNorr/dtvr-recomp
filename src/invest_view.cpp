// SPDX-License-Identifier: GPL-3.0-only
#include "invest_view.hpp"

#include "panel.hpp"

#include <algorithm>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 0x1BE, kHeight = 0x8A, kStep = 1000; // seg8:7027, DS:877E
constexpr SDL_Rect kYes{0x82, 0x5A, 0xA0 - 0x82, 0x6E - 0x5A}, kNo{0x154, 0x5A, 0x186 - 0x154, 0x6E - 0x5A}; // seg8:70BB / 70FF
constexpr SDL_Color kBrown{67, 27, 0, 255}, kWhite{255, 255, 255, 255};

} // namespace

InvestView::InvestView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout,
                       const std::map<int, std::string>& texts, TextRenderer& text)
    : renderer_(renderer), root_(root), layout_(layout), texts_(texts), text_(text) {}

std::string InvestView::text(int id) const {
    const auto it = texts_.find(id);
    return it == texts_.end() ? std::string() : it->second;
}

SDL_Texture* InvestView::bitmap(const std::string& file) {
    auto it = cache_.find(file);
    if (it == cache_.end()) {
        Texture t(nullptr, SDL_DestroyTexture);
        if (const fs::path p = findCaseInsensitive(root_, fs::path("butt") / file); !p.empty()) {
            auto surface = loadBmp(p);
            t.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        }
        it = cache_.emplace(file, std::move(t)).first;
    }
    return it->second.get();
}

void InvestView::open(bool professor, const std::string& offer, double cash) {
    professor_ = professor;
    offer_ = offer;
    cash_ = cash;
    asking_ = false;
    choice_ = 0; // seg8:75E2
    amount_ = kStep;
    open_ = true;
}

SDL_Rect InvestView::windowRect() const {
    return SDL_Rect{(layout_.width - kWidth) / 2, (layout_.height - kHeight) / 2, kWidth, kHeight};
}

SDL_Rect InvestView::box() const { return borderClient(windowRect(), BorderSize::Thin); }

InvestView::Result InvestView::enter() {
    Result r;
    r.sound = 2;
    if (!asking_) {
        if (choice_ == 1) { r.done = true; return r; } // Nein: declined
        asking_ = true;                                  // Ja: ask for the amount (text 5390)
        return r;
    }
    r.done = true;
    r.amount = amount_;
    return r;
}

InvestView::Result InvestView::handle(const SDL_Event& event) {
    Result r;
    if (!open_) return r;
    const SDL_Rect b = box();
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const SDL_Point at{event.button.x - b.x, event.button.y - b.y};
        if (SDL_PointInRect(&at, &kYes)) { choice_ = 0; return enter(); }
        if (SDL_PointInRect(&at, &kNo)) { choice_ = 1; return enter(); }
        return r;
    }
    if (event.type != SDL_KEYDOWN) return r;
    const int maximum = int(cash_ / kStep) * kStep;
    switch (event.key.keysym.sym) { // seg8:73AB
    case SDLK_LEFT: choice_ = 0; break;
    case SDLK_RIGHT: choice_ = 1; break;
    case SDLK_UP: amount_ = std::min(maximum, amount_ + kStep); break;
    case SDLK_DOWN: amount_ = std::max(kStep, amount_ - kStep); break;
    case SDLK_RETURN: case SDLK_KP_ENTER: return enter();
    case SDLK_ESCAPE: case SDLK_BACKSPACE: r.done = true; r.sound = 7; break; // seg8:714C: declined
    default: break;
    }
    return r;
}

void InvestView::draw() {
    if (!open_) return;
    drawWindowBorder(renderer_, borderSprites_, windowRect(), BorderSize::Thin); // seg8:7C63
    const SDL_Rect b = box();
    if (SDL_Texture* paper = bitmap("unipaper.bmp")) { // DS:71CD
        const SDL_Rect src{0, 0, kWidth, kHeight};
        SDL_RenderCopy(renderer_, paper, &src, &b);
    }
    if (SDL_Texture* picture = bitmap(professor_ ? "prof.bmp" : "fuss.bmp")) { // DS:71DD / 71E9 at (10, 10)
        const SDL_Rect dst{b.x + 10, b.y + 10, 102, 102};
        SDL_RenderCopy(renderer_, picture, nullptr, &dst);
    }
    const ScreenRect textArea{b.x + 122, b.y + 10, kWidth - 132, 0x5A - 14};
    const TextStyle body{16, true, false, false, false, 0, kBrown}; // seg8:76D0: Arial 16 weight 600, 0x45
    if (!asking_) {
        text_.drawWrapped(renderer_, offer_, textArea, body);
        TextStyle answer{20, true, false, true, false, 0, choice_ == 0 ? kWhite : kBrown};
        text_.draw(renderer_, text(115), ScreenRect{b.x + kYes.x, b.y + kYes.y, kYes.w, kYes.h}, answer);
        answer.color = choice_ == 1 ? kWhite : kBrown;
        text_.draw(renderer_, text(116), ScreenRect{b.x + kNo.x, b.y + kNo.y, kNo.w, kNo.h}, answer);
    } else {
        text_.drawWrapped(renderer_, text(5390), textArea, body);
        TextStyle amount{20, true, false, true, false, 0, kBrown};
        text_.draw(renderer_, text(5391) + " " + formatMoney(amount_), ScreenRect{b.x + 122, b.y + 0x5A, kWidth - 132, 24}, amount);
    }
}

} // namespace dtvr
