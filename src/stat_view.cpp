// SPDX-License-Identifier: GPL-3.0-only
#include "stat_view.hpp"

#include "panel.hpp"

#include <exception>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 0x24E, kHeight = 0x1C4; // CreateWindow seg1:975B / 975E
constexpr int kRow = 0x2A;                     // KO_Z strips (seg1:9B54, 9E5A)
constexpr int kClientWidth = 0x22E;            // header rectangle right (seg1:9AC0)
constexpr SDL_Color kBrown{67, 27, 0, 255}, kWhite{255, 255, 255, 255}; // PALETTEINDEX 0x45, 0xFF

TextStyle style(int height, bool bold, int shadow, SDL_Color colour, bool right = false) {
    TextStyle s{height, bold, false, false, right, shadow, colour};
    return s;
}

} // namespace

SDL_Rect StatView::window() const {
    // seg1:976F passes two never-written locals as x, y; WM_CREATE centres the window on
    // the canvas with seg6:0000 (seg1:98BF).
    return SDL_Rect{(layout_.width - kWidth) / 2, (layout_.height - kHeight) / 2, kWidth, kHeight};
}

void StatView::open(const StatSheet& sheet, Uint32 now) {
    sheet_ = sheet;
    open_ = true;
    openedAt_ = now;
}

bool StatView::closes(const SDL_Event& event) const {
    if (!open_) return false;
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.mod & KMOD_ALT) return false; // WM_SYSKEYDOWN, not handled
        switch (event.key.keysym.sym) { // seg1:98CA: 0x20, 0x0D, 0x1B, 0x08 -> DestroyWindow
        case SDLK_SPACE: case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_ESCAPE: case SDLK_BACKSPACE: return true;
        default: return false;
        }
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) { // seg1:9A7A
        const SDL_Rect client = borderClient(window(), BorderSize::Thick);
        const SDL_Point p{event.button.x, event.button.y};
        return SDL_PointInRect(&p, &client) == SDL_TRUE;
    }
    return false;
}

SDL_Texture* StatView::frame(const std::string& name) {
    if (framesArt_ != layout_.artDir) { // the game palette DS:9CD2 of the resolution's art
        frames_.clear();
        framesArt_ = layout_.artDir;
        try {
            const fs::path palette = findCaseInsensitive(root_, fs::path("bmp") / std::to_string(layout_.artDir) / "info.bmp");
            if (!palette.empty()) {
                const Surface bmp = loadBmp(palette);
                // seg1:9812: "%s\buy.sbs" (DS:4089) in the sbs directory; seg2:913A copies
                // frames opaque, palette index 0 included.
                if (bmp && bmp->format->palette)
                    loadSbsTextures(renderer_, findCaseInsensitive(root_, fs::path("sbs") / "buy.sbs"),
                                    bmp->format->palette, frames_, -1);
            }
        } catch (const std::exception&) {
            frames_.clear();
        }
    }
    const auto it = frames_.find(upperAscii(name)); // seg6:406D ignores case
    return it == frames_.end() ? nullptr : it->second.get();
}

std::string StatView::text(int id) const {
    const auto it = texts_.find(id);
    return it == texts_.end() ? std::string() : it->second;
}

void StatView::draw() {
    if (!open_) return;
    const SDL_Rect win = window();
    drawWindowBorder(renderer_, borderSprites_, win, BorderSize::Thick); // seg1:9A0A -> seg6:1E83
    // WM_PAINT (seg1:992D): WinGBitBlt of the client size from (0, 0) of the 560 x 420 DIB.
    const SDL_Rect client = borderClient(win, BorderSize::Thick);
    const ClipGuard guard(renderer_);
    SDL_RenderSetClipRect(renderer_, &client);
    auto blit = [&](const std::string& name, int x, int y) { // seg2:913A
        SDL_Texture* t = frame(name);
        if (!t) return;
        int w = 0, h = 0;
        SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
        const SDL_Rect dst{client.x + x, client.y + y, w, h};
        SDL_RenderCopy(renderer_, t, nullptr, &dst);
    };
    const int cx = client.x, cy = client.y;
    // Header seg1:9AA2: KO_Z at y 0 and 42; Arial 24 weight 600 (seg6:04F9(24, 0, 600, 0)):
    // "%s %s" (DS:4099) of text 340 and the month 301 + A100 % 12 at (58, 10), format 0x100,
    // shadow 2, colour 0xFF.
    blit("KO_Z", 0, 0);
    blit("KO_Z", 0, kRow);
    text_.draw(renderer_, text(340) + " " + text(sheet_.monthText), ScreenRect{cx + 0x3A, cy + 10, kClientWidth - 0x3A, kRow},
               style(24, true, 2, kWhite));
    // Arial 20 weight 400 at y 52: 341 from x 58, 342 from 208, 343 from 328 (format 0x100),
    // 344 right-aligned to 548 (0x102); shadow 1, colour 0xFF.
    const TextStyle label = style(20, false, 1, kWhite);
    const int ly = cy + 10 + kRow;
    text_.draw(renderer_, text(341), ScreenRect{cx + 0x3A, ly, kClientWidth - 0x3A, kRow}, label);
    text_.draw(renderer_, text(342), ScreenRect{cx + 0x3A + 0x96, ly, kClientWidth - 0xD0, kRow}, label);
    text_.draw(renderer_, text(343), ScreenRect{cx + 0x148, ly, kClientWidth - 0x148, kRow}, label);
    text_.draw(renderer_, text(344), ScreenRect{cx + 0x148, ly, 0x224 - 0x148, kRow}, style(20, false, 1, kWhite, true));
    // Ranks seg1:9DE2: Arial 18 weight 400; per rank i a KO_Z strip at y (i + 2) * 42; with a
    // player the frame "%c_m" (DS:40A4, letter seg2:2D43) at (0, y) and from y + 10 the four
    // amounts of seg6:137F right-aligned to 142, 252, 392 and 542 (format 0x102, shadow 0,
    // colour 0x45).
    static constexpr char kLetters[8] = {'D', 'F', 'E', 'M', 'U', 'J', 'I', 'V'}; // seg2:2D43
    const TextStyle amount = style(18, false, 0, kBrown, true);
    for (size_t i = 0; i < sheet_.rows.size(); ++i) {
        const int y = int(i + 2) * kRow;
        blit("KO_Z", 0, y);
        const StatRow& row = sheet_.rows[i];
        if (row.player < 0 || row.player >= 8) continue; // DS:957A + i == 0xFF (seg1:9ED5)
        blit(std::string(1, kLetters[row.player]) + "_m", 0, y);
        const double values[4] = {row.income, row.rent, row.cash, row.wealth};
        const int rights[4] = {0x2A + 0x64, 0x2A + 0x64 + 0x6E, 0x2A + 0x64 + 0x6E + 0x8C, 0x2A + 0x64 + 0x6E + 0x8C + 0x96};
        for (int k = 0; k < 4; ++k)
            text_.draw(renderer_, formatMoney(values[k]), ScreenRect{cx, cy + y + 10, rights[k], 20}, amount);
    }
}

} // namespace dtvr
