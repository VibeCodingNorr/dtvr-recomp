// SPDX-License-Identifier: GPL-3.0-only
#include "save_view.hpp"

#include <algorithm>

namespace dtvr {
namespace {

constexpr int kWidth = 0x22E, kHeight = 0x1CE, kRow = 0x2A; // CreateWindow seg8:B26B
constexpr SDL_Color kBrown{67, 27, 0, 255}, kWhite{255, 255, 255, 255}; // 0x45, 0xFF

} // namespace

SaveView::SaveView(SDL_Renderer* renderer, const ScreenLayout& layout, const std::map<int, std::string>& texts,
                   TextRenderer& text, SpriteLookup sprite)
    : renderer_(renderer), layout_(layout), texts_(texts), text_(text), sprite_(std::move(sprite)) {}

SDL_Rect SaveView::box() const {
    return SDL_Rect{(layout_.width - kWidth) / 2, (layout_.height - kHeight) / 2, kWidth, kHeight};
}

SaveView::Result SaveView::handle(const SDL_Event& event) {
    if (!open_) return Result::None;
    const SDL_Rect b = box();
    auto enter = [&] { return button_ == kNo ? Result::Close : Result::Confirm; };
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button == SDL_BUTTON_RIGHT) return Result::Close; // seg8:B349: Backspace
        if (event.button.button != SDL_BUTTON_LEFT) return Result::None;
        const int x = event.button.x - b.x, y = event.button.y - b.y;
        if (x < 0 || x >= b.w || y < 0 || y >= b.h) return Result::None;
        // WM_LBUTTONDBLCLK posts Enter (seg8:B335), independently of hit testing.
        if (event.button.clicks == 2) return enter();
        if (y >= kRow && y < 10 * kRow) {
            // seg8:BA3B: PtInRect bottom = top + 41, leaving a one-pixel gap.
            if (y % kRow < kRow - 1) slot_ = y / kRow - 1;
            return Result::None;
        }
        // seg8:B974 / B9B4: only [0,150) and [458,558) are clickable.
        // The right region posts Enter; it retains the keyboard's selected button.
        if (y >= 10 * kRow && x < 0x96) return Result::Close;
        if (y >= 10 * kRow && x >= 0x1CA) return enter();
        return Result::None;
    }
    if (event.type != SDL_KEYDOWN) return Result::None;
    switch (event.key.keysym.sym) { // seg8:B578
    case SDLK_LEFT: button_ = kNo; break;
    case SDLK_RIGHT: button_ = kOk; break;
    case SDLK_UP: slot_ = std::max(0, slot_ - 1); break;
    case SDLK_DOWN: slot_ = std::min(kSaveSlots - 1, slot_ + 1); break;
    case SDLK_RETURN: case SDLK_KP_ENTER: return enter();
    case SDLK_ESCAPE: case SDLK_BACKSPACE: return Result::Close;
    default: break;
    }
    return Result::None;
}

void SaveView::draw(const SaveSlots& slots) {
    if (!open_) return;
    const SDL_Rect b = box();
    auto blit = [&](const char* name, int y) {
        if (SDL_Texture* t = sprite_(name)) {
            const SDL_Rect dst{b.x, b.y + y, kWidth, kRow};
            SDL_RenderCopy(renderer_, t, nullptr, &dst);
        }
    };
    auto text = [&](int id) {
        const auto it = texts_.find(id);
        return it == texts_.end() ? std::string() : it->second;
    };
    // Header seg8:BA8A: KO_Z, title 800 / 801 in Arial 22 weight 600, white, shadow 1.
    blit("KO_Z", 0);
    text_.draw(renderer_, text(saving_ ? 800 : 801), ScreenRect{b.x + 10, b.y + 10, kWidth - 20, kRow},
               TextStyle{22, true, false, false, false, 1, kWhite});
    // Slots seg8:B746: KO_ZI for the chosen slot, KO_Z otherwise, at (slot + 1) * 42;
    // the description in Arial 13 weight 400 at (15, slot * 42 + 52).
    for (int s = 0; s < kSaveSlots; ++s) {
        const bool chosen = s == slot_;
        blit(chosen ? "KO_ZI" : "KO_Z", (s + 1) * kRow);
        std::string label = slots.description(s);
        // GetPrivateProfileString default: German "leer" (DS:80D7), English string 11.
        if (label.empty()) label = emptySlotLabel(layout_.edition, text(11));
        text_.draw(renderer_, label, ScreenRect{b.x + 15, b.y + s * kRow + 0x34, kWidth - 15, 0x29},
                   TextStyle{13, false, false, false, false, 0, chosen ? kWhite : kBrown});
    }
    // Footer seg8:BBD2: KO_Z at y 420; text 116 (Nein) from x 30, text 114 (Ok) right
    // aligned to x 528, y + 8 + 10; the highlighted button (DS:8D64) in white.
    blit("KO_Z", 10 * kRow);
    const TextStyle no{22, true, false, false, false, 0, button_ == kNo ? kWhite : kBrown};
    TextStyle ok{22, true, false, false, false, 0, button_ == kOk ? kWhite : kBrown};
    ok.right = true;
    text_.draw(renderer_, text(116), ScreenRect{b.x + 0x1E, b.y + 10 * kRow + 8, 200, kRow}, no);
    text_.draw(renderer_, text(114), ScreenRect{b.x + 0x210 - 200, b.y + 10 * kRow + 8, 200, kRow}, ok);
}

} // namespace dtvr
