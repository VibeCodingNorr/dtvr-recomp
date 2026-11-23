// SPDX-License-Identifier: GPL-3.0-only
#include "menu_view.hpp"

namespace dtvr {
namespace {

constexpr int kWidth = 0xC0;   // CreateWindow width (seg5:5254)
constexpr int kRow = 32;       // height of the frames MENUN / MENUG
constexpr uint16_t kGrayed = 0x01, kDisabled = 0x02, kChecked = 0x08;

} // namespace

MenuView::MenuView(SDL_Renderer* renderer, const ScreenLayout& layout, TextRenderer& text, SpriteLookup sprite)
    : renderer_(renderer), layout_(layout), text_(text), sprite_(std::move(sprite)) {}

void MenuView::open(const std::string& name, std::vector<NeMenuItem> items) {
    name_ = name;
    items_ = std::move(items);
    focus_ = 0; // SetProp(FOCUS, 0) on WM_CREATE (seg5:5715)
}

SDL_Rect MenuView::windowRect() const {
    const int h = (int(items_.size()) + 1) * kRow; // (count + 1) << 5; only `count` rows are painted
    return SDL_Rect{(layout_.width - kWidth) / 2, (layout_.height - h) / 2, kWidth, h};
}

SDL_Rect MenuView::box() const { return borderClient(windowRect(), BorderSize::Thick); }

int MenuView::choose(int index) const {
    if (index < 0 || size_t(index) >= items_.size()) return 0;
    const NeMenuItem& item = items_[size_t(index)];
    if (item.flags & (kGrayed | kDisabled)) { sound_ = 7; return 0; } // seg5:550D: only a sound
    sound_ = 2;
    return item.id;
}

int MenuView::itemAt(int x, int y) const {
    const SDL_Rect b = box();
    if (x < b.x || x >= b.x + b.w || y < b.y) return -1;
    const int row = (y - b.y) / kRow;
    return row < int(items_.size()) ? row : -1;
}

int MenuView::handle(const SDL_Event& event) {
    if (!isOpen()) return 0;
    const int count = int(items_.size());
    if (event.type == SDL_MOUSEMOTION) {
        const int row = itemAt(event.motion.x, event.motion.y);
        if (row >= 0) focus_ = row;
        return 0;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int row = itemAt(event.button.x, event.button.y);
        if (row < 0) return 0;
        focus_ = row;
        return choose(row);
    }
    if (event.type != SDL_KEYDOWN) return 0;
    switch (event.key.keysym.sym) { // seg5:5657
    case SDLK_ESCAPE: case SDLK_BACKSPACE: // seg5:543D: the checked item, if any
        for (int i = 0; i < count; ++i)
            if (items_[size_t(i)].flags & kChecked) { sound_ = 2; return items_[size_t(i)].id; }
        return 0;
    case SDLK_TAB: case SDLK_DOWN: focus_ = focus_ >= count - 1 ? 0 : focus_ + 1; sound_ = 8; return 0; // seg5:555B
    case SDLK_UP: focus_ = focus_ == 0 ? count - 1 : focus_ - 1; sound_ = 8; return 0;                   // seg5:55CD
    case SDLK_RETURN: case SDLK_KP_ENTER: return choose(focus_);                              // seg5:54C8
    default: return 0;
    }
}

void MenuView::draw() {
    if (!isOpen()) return;
    drawWindowBorder(renderer_, sprite_, windowRect(), BorderSize::Thick); // seg5:57ED
    const SDL_Rect b = box();
    // seg5:58B1: items from the top, each as tall as its frame; text at (+18, +7),
    // focused (+20, +9); Arial weight 600 height 16 (seg6:04F9), no shadow; colour
    // 0x43 for grayed / disabled, 0xA4 focused, 0x45 otherwise (seg5:5B2C).
    int y = b.y;
    for (int i = 0; i < int(items_.size()); ++i) {
        const bool focused = i == focus_;
        int h = kRow;
        if (SDL_Texture* frame = sprite_(focused ? "MENUG" : "MENUN")) {
            int w = 0;
            SDL_QueryTexture(frame, nullptr, nullptr, &w, &h);
            const SDL_Rect dst{b.x, y, w, h};
            SDL_RenderCopy(renderer_, frame, nullptr, &dst);
        }
        const NeMenuItem& item = items_[size_t(i)];
        SDL_Color color{67, 27, 0, 255};                                 // 0x45
        if (item.flags & (kGrayed | kDisabled)) color = {123, 95, 27, 255}; // 0x43
        else if (focused) color = {255, 239, 239, 255};                  // 0xA4
        const int shift = focused ? 2 : 0;
        const TextStyle style{16, true, false, false, false, 0, color};
        text_.draw(renderer_, ansiToUtf8(item.text), ScreenRect{b.x + 18 + shift, y + 7 + shift, b.w, h}, style);
        y += h;
    }
}

} // namespace dtvr
