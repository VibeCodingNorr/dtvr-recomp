// SPDX-License-Identifier: GPL-3.0-only
// The monthly statement STAT of rally.exe (seg1:972F, procedure seg1:97F8, paint
// seg1:9AA2 / 9DE2; round R1): 590 x 452 pixels with the 16-pixel wooden border, centred
// on the canvas; a 558 x 420 client with ten KO_Z strips of sbs/buy.sbs copied opaque:
// the header (text 340 and the month, the labels 341-344) and the eight ranks with the
// driver's D_M ... frame and four amounts. Evidence in docs/formats.md "Monthly statement".
#pragma once

#include "layout.hpp"
#include "sdl_assets.hpp"
#include "stat_sheet.hpp"
#include "text.hpp"
#include "window_border.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class StatView {
public:
    StatView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
             const std::map<int, std::string>& texts, TextRenderer& text)
        : renderer_(renderer), root_(root), layout_(layout), texts_(texts), text_(text) {}
    void setSprites(SpriteLookup sprites) { borderSprites_ = std::move(sprites); } // BS1-BS8
    // seg1:9AA2 / 9DE2 paint the DIB once before ShowWindow: the values are those at opening.
    void open(const StatSheet& sheet, Uint32 now);
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    Uint32 openedAt() const { return openedAt_; }
    // seg1:98CA: WM_KEYDOWN Space, Return, Escape or Backspace; seg1:9A7A: WM_LBUTTONDOWN
    // (only inside the client; the border is non-client). True when it would close.
    bool closes(const SDL_Event& event) const;
    void draw();
    SDL_Rect window() const;

private:
    SDL_Texture* frame(const std::string& name);
    std::string text(int id) const;

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    const std::map<int, std::string>& texts_;
    TextRenderer& text_;
    SpriteLookup borderSprites_;
    std::map<std::string, Texture> frames_; // buy.sbs without a colour key (seg2:913A)
    int framesArt_ = -1;                    // the art directory of the palette loaded
    StatSheet sheet_;
    bool open_ = false;
    Uint32 openedAt_ = 0;
};

} // namespace dtvr
