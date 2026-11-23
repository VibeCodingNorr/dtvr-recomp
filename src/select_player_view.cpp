// SPDX-License-Identifier: GPL-3.0-only
#include "select_player_view.hpp"

#include <algorithm>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr char kLetters[8] = {'d', 'f', 'e', 'm', 'u', 'j', 'i', 'v'}; // seg2:2D43
// seg2:5271: button size and extra height per resolution (1024, 800, 640).
constexpr int kSize[3] = {0x74, 0x5B, 0x49}, kExtra[3] = {0x14, 0x10, 0xE};

int resolution(int width) { return width == 1024 ? 0 : width == 800 ? 1 : 2; } // seg6:12FA

} // namespace

SelectPlayerView::SelectPlayerView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout)
    : renderer_(renderer), root_(root), layout_(layout) {}

void SelectPlayerView::open(const std::vector<int>& candidates, const std::array<uint8_t, 8>& status) {
    candidates_ = candidates;
    status_ = status;
    std::vector<SbButton> buttons;
    for (size_t i = 0; i < candidates_.size(); ++i) buttons.push_back({candidates_[i], button(i), true});
    // seg2:5505: SetFocus on each button as it is created; the last candidate keeps it.
    buttons_.reset(std::move(buttons), candidates_.empty() ? -1 : candidates_.back());
}

SDL_Rect SelectPlayerView::windowRect() const { // seg2:5343: n * (size + 5) + 20 by extra + size + 30
    const int r = resolution(layout_.width), n = int(candidates_.size());
    const int w = n * (kSize[r] + 5) + 0x14, h = kExtra[r] + kSize[r] + 0x1E;
    return SDL_Rect{(layout_.width - w) / 2, (layout_.height - h) / 2, w, h};
}

SDL_Rect SelectPlayerView::box() const { return borderClient(windowRect(), BorderSize::Thin); }

SDL_Rect SelectPlayerView::button(size_t index) const { // seg2:53BD: first candidate on the right
    const int r = resolution(layout_.width), n = int(candidates_.size());
    const SDL_Rect b = box();
    const int x = (kSize[r] + 5) * (n - 1) + 5 - int(index) * (kSize[r] + 5);
    return SDL_Rect{b.x + x, b.y + 5, kSize[r], kSize[r]};
}

SDL_Texture* SelectPlayerView::bitmap(const std::string& file) {
    auto it = cache_.find(file);
    if (it == cache_.end()) {
        Texture t(nullptr, SDL_DestroyTexture);
        if (const fs::path path = findCaseInsensitive(root_, file); !path.empty()) {
            auto surface = loadBmp(path);
            t.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        }
        it = cache_.emplace(file, std::move(t)).first;
    }
    return it->second.get();
}

SbButtonGroup::Input SelectPlayerView::handle(const SDL_Event& event) {
    SbButtonGroup::Input in;
    if (!isOpen()) return in;
    // Buttons with the ids 0x1392 + slot (seg2:54D6).
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
        return buttons_.mouseDown(event.button.x, event.button.y);
    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
        return buttons_.mouseUp(event.button.x, event.button.y);
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) in.cancel = true; // assumption
    if (event.type == SDL_KEYDOWN) return buttons_.key(event.key.keysym.sym, event.key.keysym.mod, true);
    return in;
}

void SelectPlayerView::draw() {
    if (!isOpen()) return;
    drawWindowBorder(renderer_, borderSprites_, windowRect(), BorderSize::Thin); // seg2:5969
    const SDL_Rect b = box();
    const std::string dir = "butt/" + std::to_string(layout_.artDir) + "/";
    if (SDL_Texture* paper = bitmap(dir + "p.bmp")) { // DS:1BA3
        int pw = 0, ph = 0;
        SDL_QueryTexture(paper, nullptr, nullptr, &pw, &ph);
        for (int y = 0; pw > 0 && ph > 0 && y < b.h; y += ph)
            for (int x = 0; x < b.w; x += pw) {
                const SDL_Rect src{0, 0, std::min(pw, b.w - x), std::min(ph, b.h - y)};
                const SDL_Rect dst{b.x + x, b.y + y, src.w, src.h};
                SDL_RenderCopy(renderer_, paper, &src, &dst);
            }
    } else {
        SDL_SetRenderDrawColor(renderer_, 200, 180, 140, 255);
        SDL_RenderFillRect(renderer_, &b);
    }
    for (size_t i = 0; i < candidates_.size(); ++i) { // DS:1B7A: pl_menu\%c%d.bmp
        const int slot = candidates_[i];
        const std::string file = dir + "pl_menu/" + kLetters[slot] + std::to_string(status_[size_t(slot)]) + ".bmp";
        const SbButton& b = buttons_.buttons()[i];
        const SDL_Rect pic = buttons_.pictureRect(b);
        SDL_RenderSetClipRect(renderer_, &b.rect);
        if (SDL_Texture* t = bitmap(file)) SDL_RenderCopy(renderer_, t, nullptr, &pic);
        SDL_RenderSetClipRect(renderer_, nullptr);
    }
    buttons_.drawFocus(renderer_);
}

} // namespace dtvr
