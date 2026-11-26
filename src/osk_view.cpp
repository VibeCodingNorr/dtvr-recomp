// SPDX-License-Identifier: GPL-3.0-only
#include "osk_view.hpp"

#include "recomp_menu.hpp" // keyboardTexts: the recomp-only labels

#include <algorithm>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr SDL_Color kText{67, 27, 0, 255};  // palette colour 0x45, the dialogs' text
constexpr SDL_Color kBlack{0, 0, 0, 255};   // SBINPUT sets no text colour
constexpr int kMargin = 12;

void fill(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color c, Uint8 alpha) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, alpha);
    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

} // namespace

ScreenKeyboardView::ScreenKeyboardView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout,
                                       TextRenderer& text)
    : renderer_(renderer), root_(root), layout_(layout), text_(text) {}

void ScreenKeyboardView::open(int slot, const std::string& name, size_t maxChars) {
    keyboard_.open(name, maxChars);
    slot_ = slot;
    sound_ = -1;
    open_ = true;
}

ScreenKeyboardView::Geometry ScreenKeyboardView::geometry() const {
    Geometry g;
    g.keyW = layout_.baseWidth * 7 / 128; // 56 / 43 / 35 at 1024 / 800 / 640
    g.keyH = g.keyW * 4 / 5;
    g.gap = std::max(3, g.keyW / 12);
    constexpr int cols = ScreenKeyboard::kColumns, rows = ScreenKeyboard::kRows;
    const int gridW = cols * g.keyW + (cols - 1) * g.gap, gridH = rows * g.keyH + (rows - 1) * g.gap;
    const int nameH = g.keyH, helpH = std::max(14, g.keyH / 2);
    const int clientW = gridW + 2 * kMargin;
    const int clientH = kMargin + nameH + 2 * g.gap + gridH + 2 * g.gap + helpH + kMargin;
    constexpr int border = int(BorderSize::Thin);
    const int w = clientW + 2 * border, h = clientH + 2 * border;
    g.window = SDL_Rect{(layout_.width - w) / 2, (layout_.height - h) / 2, w, h};
    g.client = borderClient(g.window, BorderSize::Thin);
    g.left = g.client.x + kMargin;
    g.name = SDL_Rect{g.left, g.client.y + kMargin, gridW, nameH};
    g.top = g.name.y + nameH + 2 * g.gap;
    g.help = SDL_Rect{g.left, g.top + gridH + 2 * g.gap, gridW, helpH};
    return g;
}

SDL_Rect ScreenKeyboardView::cellRect(const Geometry& g, const ScreenKeyboard::Cell& cell) const {
    return SDL_Rect{g.left + cell.column * (g.keyW + g.gap), g.top + cell.row * (g.keyH + g.gap),
                    cell.span * g.keyW + (cell.span - 1) * g.gap, g.keyH};
}

int ScreenKeyboardView::cellAt(int x, int y) const {
    const Geometry g = geometry();
    const SDL_Point at{x, y};
    const auto& cells = keyboard_.cells();
    for (size_t i = 0; i < cells.size(); ++i) {
        const SDL_Rect r = cellRect(g, cells[i]);
        if (SDL_PointInRect(&at, &r)) return int(i);
    }
    return -1;
}

ScreenKeyboardView::Result ScreenKeyboardView::apply(const ScreenKeyboard::Result& r) {
    if (r.sound >= 0) sound_ = r.sound;
    if (r.kind == ScreenKeyboard::Result::Kind::Done) { open_ = false; return Result::Done; }
    if (r.kind == ScreenKeyboard::Result::Kind::Cancel) { open_ = false; return Result::Cancel; }
    return Result::None;
}

ScreenKeyboardView::Result ScreenKeyboardView::accept() {
    if (!open_) return Result::None;
    sound_ = 2; // Plopp02 as the Fertig key
    open_ = false;
    return Result::Done;
}

void ScreenKeyboardView::toggleShift() {
    if (open_) apply(keyboard_.toggleShift());
}

ScreenKeyboardView::Result ScreenKeyboardView::handle(const SDL_Event& event) {
    if (!open_) return Result::None;
    using Move = ScreenKeyboard::Move;
    if (event.type == SDL_KEYDOWN) {
        auto move = [&](Move m) { if (keyboard_.move(m)) sound_ = 8; return Result::None; }; // seg1:24E5
        switch (event.key.keysym.sym) {
        case SDLK_UP: case SDLK_KP_8: return move(Move::Up);
        case SDLK_DOWN: case SDLK_KP_2: return move(Move::Down);
        case SDLK_LEFT: case SDLK_KP_4: return move(Move::Left);
        case SDLK_RIGHT: case SDLK_KP_6: return move(Move::Right);
        case SDLK_RETURN: case SDLK_KP_ENTER: return apply(keyboard_.press());
        case SDLK_BACKSPACE: return apply(keyboard_.erase());
        case SDLK_ESCAPE: // like the SBBUTTON cancel (seg1:0F77), without a sound
            open_ = false;
            return Result::Cancel;
        default: return Result::None; // Space arrives as SDL_TEXTINPUT from a real keyboard
        }
    }
    if (event.type == SDL_TEXTINPUT) return apply(keyboard_.type(event.text.text));
    if (event.type == SDL_MOUSEMOTION) {
        if (const int cell = cellAt(event.motion.x, event.motion.y); cell >= 0) keyboard_.setFocus(cell);
        return Result::None;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int cell = cellAt(event.button.x, event.button.y);
        if (cell < 0) return Result::None;
        keyboard_.setFocus(cell);
        return apply(keyboard_.press());
    }
    return Result::None;
}

std::string ScreenKeyboardView::caption(const ScreenKeyboard::Cell& cell) const {
    const KeyboardTexts& t = keyboardTexts();
    switch (cell.kind) {
    case ScreenKeyboard::Kind::Shift: return t.shift;
    case ScreenKeyboard::Kind::Erase: return t.erase;
    case ScreenKeyboard::Kind::Space: return t.space;
    case ScreenKeyboard::Kind::Cancel: return t.cancel;
    case ScreenKeyboard::Kind::Done: return t.done;
    case ScreenKeyboard::Kind::Char: break;
    }
    return keyboard_.label(cell);
}

SDL_Texture* ScreenKeyboardView::paper() {
    if (paperDir_ != layout_.artDir) { // butt/<n>/p1.bmp, the paper of TEMP (seg8:0B8B)
        paperDir_ = layout_.artDir;
        paper_.reset();
        if (const fs::path p = findCaseInsensitive(root_, fs::path("butt") / std::to_string(layout_.artDir) / "p1.bmp");
            !p.empty())
            if (auto surface = loadBmp(p)) paper_.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
    }
    return paper_.get();
}

void ScreenKeyboardView::draw() {
    if (!open_) return;
    const Geometry g = geometry();
    drawWindowBorder(renderer_, borderSprites_, g.window, BorderSize::Thin);
    { // the paper, tiled over the client area
        const ClipGuard guard(renderer_);
        SDL_RenderSetClipRect(renderer_, &g.client);
        if (SDL_Texture* p = paper()) {
            int pw = 0, ph = 0;
            SDL_QueryTexture(p, nullptr, nullptr, &pw, &ph);
            for (int y = g.client.y; pw > 0 && ph > 0 && y < g.client.y + g.client.h; y += ph)
                for (int x = g.client.x; x < g.client.x + g.client.w; x += pw) {
                    const SDL_Rect dst{x, y, pw, ph};
                    SDL_RenderCopy(renderer_, p, nullptr, &dst);
                }
        } else {
            fill(renderer_, g.client, SDL_Color{214, 190, 140, 255}, 255);
        }
    }
    const int res = layout_.resolution();
    static constexpr int kLabel[3] = {0x12, 0x10, 0xE};  // the dialog text sizes (seg8:0C0A)
    static constexpr int kInput[3] = {0x14, 0x11, 0xF};  // SBINPUT (seg1:1C85)
    // The name: a framed strip with the SBINPUT text and caret, the count at the right.
    fill(renderer_, g.name, kText, 28);
    SDL_SetRenderDrawColor(renderer_, kText.r, kText.g, kText.b, 255);
    SDL_RenderDrawRect(renderer_, &g.name);
    TextStyle input{kInput[res], true, true, false, false, 0, kBlack};
    input.serif = true;
    const SDL_Point inputSize = text_.measure("X", input);
    text_.draw(renderer_, keyboard_.text() + "|",
               ScreenRect{g.name.x + 8, g.name.y + (g.name.h - inputSize.y) / 2, g.name.w - 16, inputSize.y}, input);
    TextStyle count{kLabel[res], true, false, false, true, 0, kText};
    const SDL_Point countSize = text_.measure("0", count);
    text_.draw(renderer_, std::to_string(utf8Length(keyboard_.text())) + " / " + std::to_string(keyboard_.maxChars()),
               ScreenRect{g.name.x + 8, g.name.y + (g.name.h - countSize.y) / 2, g.name.w - 16, countSize.y}, count);
    // The keys: framed paper, the red PS_INSIDEFRAME of width 5 on the focus (seg1:19B6).
    const auto& cells = keyboard_.cells();
    for (size_t i = 0; i < cells.size(); ++i) {
        const ScreenKeyboard::Cell& cell = cells[i];
        const SDL_Rect r = cellRect(g, cell);
        const bool lit = cell.kind == ScreenKeyboard::Kind::Shift && keyboard_.shift();
        fill(renderer_, r, kText, lit ? 110 : 36);
        SDL_SetRenderDrawColor(renderer_, kText.r, kText.g, kText.b, 255);
        SDL_RenderDrawRect(renderer_, &r);
        TextStyle label{cell.kind == ScreenKeyboard::Kind::Char ? kLabel[res] + 2 : kLabel[res], true, false, true, false,
                        0, kText};
        const SDL_Point size = text_.measure("Ag", label);
        text_.draw(renderer_, caption(cell), ScreenRect{r.x, r.y + (r.h - size.y) / 2, r.w, size.y}, label);
        if (int(i) == keyboard_.focus()) {
            SDL_SetRenderDrawColor(renderer_, 255, 0, 0, 255);
            for (int k = 0; k < 5 && 2 * k < r.w && 2 * k < r.h; ++k) {
                const SDL_Rect frame{r.x + k, r.y + k, r.w - 2 * k, r.h - 2 * k};
                SDL_RenderDrawRect(renderer_, &frame);
            }
        }
    }
    TextStyle help{std::max(11, kLabel[res] - 3), true, false, true, false, 0, kText};
    text_.draw(renderer_, help_.empty() ? keyboardTexts().help : help_, ScreenRect{g.help.x, g.help.y, g.help.w, g.help.h}, help);
}

} // namespace dtvr
