// SPDX-License-Identifier: GPL-3.0-only
#include "setup_view.hpp"

#include "osk.hpp"

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr char kLetters[kPlayerSlots] = {'d', 'f', 'e', 'm', 'u', 'j', 'i', 'v'}; // seg2:2D43

bool inside(const ScreenRect& r, int x, int y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }
SDL_Rect rect(const ScreenRect& r) { return SDL_Rect{r.x, r.y, r.w, r.h}; }
constexpr int kStar = 100, kStop = 101, kName = 200; // kName + slot: the SBINPUT fields

} // namespace

SetupView::SetupView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout, TextRenderer& text)
    : renderer_(renderer), root_(root), layout_(layout), text_(text), geometry_(setupGeometry(layout.baseWidth)) {}

void SetupView::open(const std::array<uint8_t, kPlayerSlots>& status, const std::array<std::string, kPlayerSlots>& names) {
    status_ = status;
    names_ = names;
    editing_ = -1;
    open_ = true;
    // In creation order, the z-order the arrow keys walk (seg1:2474 ff.): star (seg1:05C9),
    // stop (0687), then for the slots 7 down to 0 the name field SBINPUT (071E, id 0x139C
    // + slot) and the driver button (0830, id 0x1392 + slot).
    std::vector<SbButton> buttons;
    buttons.push_back({kStar, rect(geometry_.start), anyPlayer()}); // disabled without players
    buttons.push_back({kStop, rect(geometry_.stop), true});
    for (int p = kPlayerSlots - 1; p >= 0; --p) {
        buttons.push_back({kName + p, rect(geometry_.name[size_t(p)]), true, true});
        buttons.push_back({p, rect(geometry_.driver[size_t(p)]), true});
    }
    buttons_.reset(std::move(buttons), kStar); // seg1:0409: SetFocus(DS:A6E8) = the star (index 8, seg1:083B)
    sound_ = -1;
}

void SetupView::stopEditing() {
    if (editing_ >= 0) SDL_StopTextInput();
    editing_ = -1;
}

void SetupView::followFocus() { // a name field gets the keyboard focus through the walk
    if (!buttons_.fieldFocused()) { stopEditing(); return; }
    if (editing_ < 0) SDL_StartTextInput();
    editing_ = buttons_.focus() - kName;
}

void SetupView::close() {
    if (editing_ >= 0) SDL_StopTextInput();
    editing_ = -1;
    open_ = false;
}

void SetupView::setName(int slot, const std::string& name) {
    if (slot < 0 || slot >= kPlayerSlots) return;
    std::string& field = names_[size_t(slot)];
    field.clear();
    appendNameText(field, name, kNameFieldLimit); // the field's own rules (seg1:1F27 / 1F35)
}

std::array<std::string, kPlayerSlots> SetupView::names() const {
    std::array<std::string, kPlayerSlots> out;
    for (size_t p = 0; p < out.size(); ++p) out[p] = utf8Prefix(names_[p], kPlayerNameMax); // seg1:0BB7
    return out;
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

SDL_Rect SetupView::extraRect() const {
    const SDL_Rect stop = rect(geometry_.stop), star = rect(geometry_.start);
    const int x = stop.x + stop.w + 24, h = std::max(20, star.h / 3);
    return SDL_Rect{x, star.y + (star.h - h) / 2, star.x - 24 - x, h};
}

SetupView::Result SetupView::handle(const SDL_Event& event) {
    if (!open_) return Result::None;
    // The recomp extension's switch (no counterpart in rally.exe).
    const SDL_Rect extra = extraRect();
    bool toggle = event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F9 && editing_ < 0;
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const SDL_Point at{event.button.x, event.button.y};
        toggle = SDL_PointInRect(&at, &extra);
    }
    if (toggle) {
        extraCards_ = !extraCards_;
        sound_ = 2; // Plopp02 like a switch
        return Result::None;
    }
    SbButtonGroup::Input in;
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int x = event.button.x, y = event.button.y;
        for (int p = 0; p < kPlayerSlots; ++p)
            if (inside(geometry_.name[size_t(p)], x, y)) { // the SBINPUT field takes the focus
                buttons_.setFocus(kName + p);
                followFocus();
                return Result::None;
            }
        // The star is disabled with EnableWindow (seg1:0853): its clicks go to PLAYER,
        // which has no WM_LBUTTONDOWN handler (dispatch seg1:0CC8), so nothing happens.
        const SDL_Rect star = rect(geometry_.start);
        const SDL_Point at{x, y};
        if (!anyPlayer() && SDL_PointInRect(&at, &star)) return Result::None;
        in = buttons_.mouseDown(x, y); // a button takes the focus; elsewhere it stays
        followFocus();
    } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        in = buttons_.mouseUp(event.button.x, event.button.y);
    } else if (event.type == SDL_TEXTINPUT && editing_ >= 0) {
        appendNameText(names_[size_t(editing_)], event.text.text, kNameFieldLimit); // seg1:1F00
        return Result::None;
    } else if (event.type == SDL_KEYDOWN) {
        const SDL_Keycode key = event.key.keysym.sym;
        if (editing_ >= 0 && key == SDLK_BACKSPACE) { // seg1:1FB3: sound 8, or 7 when empty
            sound_ = eraseNameChar(names_[size_t(editing_)]);
            return Result::None;
        }
        if (editing_ >= 0) { // SBINPUT (seg1:21E5): other keys, Escape too, do nothing
            in = buttons_.fieldKey(key, event.key.keysym.mod);
            sound_ = in.sound;
            if (in.moved) followFocus();
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER) // posted on to PLAYER (seg1:2190)
                return anyPlayer() ? Result::Start : Result::None; // seg1:0C3B
            return Result::None;
        }
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) return anyPlayer() ? Result::Start : Result::None; // seg1:0C3B
        if (key == SDLK_ESCAPE) return Result::Cancel;
        in = buttons_.key(key, event.key.keysym.mod, false);
        if (in.moved) followFocus();
    }
    sound_ = in.sound;
    if (in.cancel) return Result::Cancel;
    if (in.id == kStar) return Result::Start;
    if (in.id == kStop) return Result::Cancel;
    if (in.id >= 0 && in.id < kPlayerSlots) { // seg1:0A79: 0 -> 1 -> 2 -> 0
        uint8_t& s = status_[size_t(in.id)];
        s = uint8_t(s >= 2 ? 0 : s + 1);
        buttons_.setEnabled(kStar, anyPlayer());
    }
    return Result::None;
}

void SetupView::draw() {
    if (!open_) return;
    if (SDL_Texture* back = bitmap("menu.bmp")) SDL_RenderCopy(renderer_, back, nullptr, nullptr);
    for (const SbButton& b : buttons_.buttons()) {
        if (b.field) continue; // the names are drawn below
        const std::string file = b.id == kStar ? "star.bmp" : b.id == kStop ? "stop.bmp"
                               : std::string(1, kLetters[b.id]) + std::to_string(status_[size_t(b.id)]) + ".bmp";
        if (SDL_Texture* t = bitmap(file)) {
            const SDL_Rect pic = buttons_.pictureRect(b);
            SDL_RenderSetClipRect(renderer_, &b.rect);
            SDL_RenderCopy(renderer_, t, nullptr, &pic);
            SDL_RenderSetClipRect(renderer_, nullptr);
        }
    }
    buttons_.drawFocus(renderer_);
    for (int p = 0; p < kPlayerSlots; ++p) {
        // SBINPUT (seg1:1DC8, round 109): TextOut at (0, 0) of the field in Times New
        // Roman italic weight 600 at 20 / 17 / 15 (seg1:1C85 by seg6:12FA), no text
        // colour (black), transparent; the caret after the text.
        static constexpr int kInput[3] = {0x14, 0x11, 0xF};
        const int res = layout_.resolution();
        const ScreenRect& r = geometry_.name[size_t(p)];
        const std::string shown = names_[size_t(p)] + (p == editing_ ? "|" : "");
        TextStyle style{kInput[res], true, true, false, false, 0, SDL_Color{0, 0, 0, 255}};
        style.serif = true;
        text_.draw(renderer_, shown, ScreenRect{r.x, r.y, r.w, r.h}, style);
    }
    // Recomp extension: a box and its label in the text colour of the dialogs (0x45).
    const SDL_Rect extra = extraRect();
    const int side = extra.h * 3 / 4;
    const SDL_Rect box{extra.x, extra.y + (extra.h - side) / 2, side, side};
    SDL_SetRenderDrawColor(renderer_, 67, 27, 0, 255);
    SDL_RenderDrawRect(renderer_, &box);
    if (extraCards_) {
        const SDL_Rect fill{box.x + 3, box.y + 3, box.w - 6, box.h - 6};
        SDL_RenderFillRect(renderer_, &fill);
    }
    text_.draw(renderer_, "Computer spielen alle Karten (Erg\u00e4nzung, F9)",
               ScreenRect{box.x + side + 8, extra.y, extra.w - side - 8, extra.h},
               TextStyle{std::max(12, extra.h * 2 / 3), true, false, false, false, 0, SDL_Color{67, 27, 0, 255}});
}

} // namespace dtvr
