// SPDX-License-Identifier: GPL-3.0-only
#include "options_view.hpp"

#include <algorithm>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kStar = 100, kStop = 101;

SDL_Rect rect(const ScreenRect& r) { return SDL_Rect{r.x, r.y, r.w, r.h}; }

} // namespace

OptionsView::OptionsView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout,
                         TextRenderer& text)
    : renderer_(renderer), root_(root), layout_(layout), text_(text), geometry_(setupGeometry(layout.width)) {}

SDL_Rect OptionsView::smoothingRect() const {
    const SDL_Rect stop = rect(geometry_.stop), star = rect(geometry_.start);
    const int x = stop.x + stop.w + 24, h = std::max(20, star.h / 3);
    return SDL_Rect{x, star.y + (star.h - h) / 2, star.x - 24 - x, h};
}

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

void OptionsView::open(const Settings& settings) {
    settings_ = settings;
    open_ = true;
    std::vector<SbButton> buttons;
    for (int i = 0; i < Settings::kCount; ++i) buttons.push_back({i, rect(geometry_.driver[size_t(i)]), true});
    buttons.push_back({kStar, rect(geometry_.start), true});
    buttons.push_back({kStop, rect(geometry_.stop), true});
    buttons_.reset(std::move(buttons), kStar); // seg1:AD49: SetFocus(DS:A6E8) = the star (index 8, seg1:AE1F)
    sound_ = -1;
}

OptionsView::Result OptionsView::handle(const SDL_Event& event) {
    if (!open_) return Result::None;
    SbButtonGroup::Input in;
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const SDL_Rect extra = smoothingRect();
        const SDL_Point at{event.button.x, event.button.y};
        if (SDL_PointInRect(&at, &extra)) { // recomb: Original -> canvas -> screen
            settings_.textSmoothing = (settings_.textSmoothing + 1) % 3;
            sound_ = 2;
            return Result::Toggled;
        }
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
        in = buttons_.mouseDown(event.button.x, event.button.y);
    else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
        in = buttons_.mouseUp(event.button.x, event.button.y);
    else if (event.type == SDL_KEYDOWN) {
        const SDL_Keycode key = event.key.keysym.sym;
        if (key >= SDLK_F1 && key <= SDLK_F8) in.id = int(key - SDLK_F1); // seg1:B60A: WM_COMMAND 0x1392 + n
        else if (key == SDLK_F10) in.id = kStar;                         // seg1:B5F5: WM_COMMAND 0x139A
        else in = buttons_.key(key, event.key.keysym.mod, false);
    }
    sound_ = in.sound;
    if (in.cancel || in.id == kStar || in.id == kStop) return Result::Close; // seg1:B5D4 / B5E2 -> B539
    if (in.id >= 0) {
        settings_.toggle(in.id);
        return Result::Toggled;
    }
    return Result::None;
}

void OptionsView::draw() {
    if (!open_) return;
    const fs::path butt = fs::path("butt") / std::to_string(layout_.artDir);
    if (SDL_Texture* back = bitmap(butt / "d" / "menu.bmp")) SDL_RenderCopy(renderer_, back, nullptr, nullptr);
    for (const SbButton& b : buttons_.buttons()) {
        // seg1:AFB2: "%s\%d\op_menu\%d%s.bmp"; star / stop of pl_menu.
        const fs::path file = b.id == kStar ? butt / "pl_menu" / "star.bmp"
                            : b.id == kStop ? butt / "pl_menu" / "stop.bmp"
                            : butt / "op_menu" / (std::to_string(settings_.value[size_t(b.id)]) + Settings::switchName(b.id) + ".bmp");
        if (SDL_Texture* t = bitmap(file)) {
            const SDL_Rect pic = buttons_.pictureRect(b);
            SDL_RenderSetClipRect(renderer_, &b.rect);
            SDL_RenderCopy(renderer_, t, nullptr, &pic);
            SDL_RenderSetClipRect(renderer_, nullptr);
        }
    }
    buttons_.drawFocus(renderer_);
    // Recomb addition: the text smoothing as a label in the dialog colour (0x45).
    static const char* const kModes[3] = {"Original", "Bildgr\u00f6\u00dfe", "Bildschirmaufl\u00f6sung"};
    const SDL_Rect extra = smoothingRect();
    text_.draw(renderer_, std::string("Schrift: ") + kModes[settings_.textSmoothing % 3] + " (Erg\u00e4nzung, F12)",
               ScreenRect{extra.x, extra.y, extra.w, extra.h},
               TextStyle{std::max(12, extra.h * 2 / 3), true, false, false, false, 0, SDL_Color{67, 27, 0, 255}});
}

} // namespace dtvr
