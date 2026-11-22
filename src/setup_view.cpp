// SPDX-License-Identifier: GPL-3.0-only
#include "setup_view.hpp"

#include <cctype>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr char kLetters[kPlayerSlots] = {'d', 'f', 'e', 'm', 'u', 'j', 'i', 'v'}; // seg2:2D43
constexpr size_t kNameMax = 0x13 - 1; // GetWindowText(..., 0x13) (seg1:0BB7)

bool inside(const ScreenRect& r, int x, int y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }

size_t utf8Length(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) n += (c & 0xC0) != 0x80;
    return n;
}

} // namespace

SetupView::SetupView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout, TextRenderer& text)
    : renderer_(renderer), root_(root), layout_(layout), text_(text), geometry_(setupGeometry(layout.width)) {}

void SetupView::open(const std::array<uint8_t, kPlayerSlots>& status, const std::array<std::string, kPlayerSlots>& names) {
    status_ = status;
    names_ = names;
    editing_ = -1;
    open_ = true;
}

void SetupView::close() {
    if (editing_ >= 0) SDL_StopTextInput();
    editing_ = -1;
    open_ = false;
}

bool SetupView::anyPlayer() const { // seg2:0368: number of slots in play
    for (uint8_t s : status_) if (s) return true;
    return false;
}

SDL_Texture* SetupView::bitmap(const std::string& file) {
    auto it = cache_.find(file);
    if (it == cache_.end()) {
        Texture t(nullptr, SDL_DestroyTexture);
        const fs::path path =
            findCaseInsensitive(root_, fs::path("butt") / std::to_string(layout_.artDir) / "pl_menu" / file);
        if (!path.empty()) {
            auto surface = loadBmp(path);
            t.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        }
        it = cache_.emplace(file, std::move(t)).first;
    }
    return it->second.get();
}

SetupView::Result SetupView::handle(const SDL_Event& event) {
    if (!open_) return Result::None;
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int x = event.button.x, y = event.button.y;
        for (int p = 0; p < kPlayerSlots; ++p) {
            if (inside(geometry_.driver[size_t(p)], x, y)) { // seg1:0A79: 0 -> 1 -> 2 -> 0
                status_[size_t(p)] = uint8_t(status_[size_t(p)] >= 2 ? 0 : status_[size_t(p)] + 1);
                return Result::None;
            }
            if (inside(geometry_.name[size_t(p)], x, y)) {
                if (editing_ < 0) SDL_StartTextInput();
                editing_ = p;
                return Result::None;
            }
        }
        if (inside(geometry_.start, x, y) && anyPlayer()) return Result::Start; // disabled without players
        if (inside(geometry_.stop, x, y)) return Result::Cancel;
        if (editing_ >= 0) SDL_StopTextInput();
        editing_ = -1;
        return Result::None;
    }
    if (event.type == SDL_TEXTINPUT && editing_ >= 0) {
        std::string& name = names_[size_t(editing_)];
        for (const char* c = event.text.text; *c; ++c) {
            const bool lead = (static_cast<unsigned char>(*c) & 0xC0) != 0x80;
            if (lead && utf8Length(name) >= kNameMax) break;
            name += *c;
        }
        return Result::None;
    }
    if (event.type != SDL_KEYDOWN) return Result::None;
    const SDL_Keycode key = event.key.keysym.sym;
    if (editing_ >= 0 && key == SDLK_BACKSPACE) {
        std::string& name = names_[size_t(editing_)];
        while (!name.empty() && (static_cast<unsigned char>(name.back()) & 0xC0) == 0x80) name.pop_back();
        if (!name.empty()) name.pop_back();
        return Result::None;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) return anyPlayer() ? Result::Start : Result::None; // seg1:0C3B
    if (key == SDLK_ESCAPE) return Result::Cancel;
    return Result::None;
}

void SetupView::draw() {
    if (!open_) return;
    if (SDL_Texture* back = bitmap("menu.bmp")) SDL_RenderCopy(renderer_, back, nullptr, nullptr);
    auto blit = [&](const std::string& file, const ScreenRect& r) {
        if (SDL_Texture* t = bitmap(file)) {
            const SDL_Rect dst{r.x, r.y, r.w, r.h};
            SDL_RenderCopy(renderer_, t, nullptr, &dst);
        }
    };
    for (int p = 0; p < kPlayerSlots; ++p) {
        blit(std::string(1, kLetters[p]) + std::to_string(status_[size_t(p)]) + ".bmp", geometry_.driver[size_t(p)]);
        // SBINPUT draws its text itself (outside rally.exe); assumed: Arial weight 600
        // at two thirds of the field height, palette colour 0x45, a bar at the caret.
        const ScreenRect& r = geometry_.name[size_t(p)];
        const std::string shown = names_[size_t(p)] + (p == editing_ ? "|" : "");
        text_.draw(renderer_, shown, ScreenRect{r.x + 4, r.y + r.h / 6, r.w - 8, r.h},
                   TextStyle{r.h * 2 / 3, true, false, false, false, 0, SDL_Color{67, 27, 0, 255}});
    }
    blit("star.bmp", geometry_.start);
    blit("stop.bmp", geometry_.stop);
}

} // namespace dtvr
