// SPDX-License-Identifier: GPL-3.0-only
// The Blue Byte logo of the start (evidence in docs/formats.md "Start
// and Blue Byte logo"). The start-up function seg5:0000 shows it twice through
// seg6:0A4D(name, focus) with name DS:00F8 / DS:0111 = "BB.bmp" and focus 0: once
// at seg5:0151, after the canvas exists and before the intro, and again at seg5:0193
// after the intro (only with [Setup] ShowIntro). seg6:0A4D loads
// `<DS:7A>\<seg5:1827>\BB.bmp` (format DS:165C "%s\%d\%s": butt/<n>/BB.bmp), creates
// the window class PICT as a child of the canvas DS:40 at the bitmap's size, which
// WM_CREATE centres (seg6:0000), shows it, pumps the queued messages once (seg6:0B05)
// and returns: the logo stays while rally.exe goes on loading (the recomp adds a
// minimum time, kLogoMinimumMs). The intro removes it
// before its window appears (seg7:044D -> seg6:0B37), the set-up removes it before
// the title (seg5:020C -> seg6:0B37). There is no timer.
#pragma once

#include "layout.hpp"
#include "sdl_assets.hpp"

#include <SDL.h>

#include <filesystem>
#include <string>

namespace dtvr {

// seg6:0000: half the difference of the parent's and the window's sizes (C division,
// towards zero), not below 0.
inline ScreenRect centredOnCanvas(int canvasWidth, int canvasHeight, int width, int height) {
    const int x = (canvasWidth - width) / 2, y = (canvasHeight - height) / 2;
    return {x < 0 ? 0 : x, y < 0 ? 0 : y, width, height};
}

// seg6:0A4D builds "%s\%d\%s" (DS:165C) from the butt directory DS:7A, the art
// directory of seg5:1827 and the name.
inline std::filesystem::path pictPath(int artDir, const std::string& name) {
    return std::filesystem::path("butt") / std::to_string(artDir) / name;
}

// The canvas background butt/<n>/bk.bmp (canvas WM_PAINT seg8:01B7); in the widescreen
// extension the bevelled edges keep their size and the wood between them is stretched.
void drawCanvasBackground(SDL_Renderer* renderer, SDL_Texture* background, const ScreenLayout& layout);

// Recomp addition: the logo stays at least this long after each show (rally.exe has no
// timer; on 1995 machines its loading took seconds, the rebuild loads in about 0.2 s).
// A key or a left click ends it earlier, as WM_KEYDOWN / WM_LBUTTONDOWN of PICT do.
constexpr Uint32 kLogoMinimumMs = 2500;

// Keys that alone do not end the logo's wait.
inline bool isModifierKey(SDL_Keycode k) {
    return k == SDLK_LSHIFT || k == SDLK_RSHIFT || k == SDLK_LCTRL || k == SDLK_RCTRL || k == SDLK_LALT
           || k == SDLK_RALT || k == SDLK_LGUI || k == SDLK_RGUI || k == SDLK_MODE;
}

class LogoView {
public:
    LogoView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout)
        : renderer_(renderer), root_(root), layout_(layout) {}
    // seg6:0A4D("BB.bmp", 0); false without the file (seg5:3E04 fails, no window).
    bool show();
    // seg6:0B37: DestroyWindow of DS:163C.
    void hide() { shown_ = false; }
    bool shown() const { return shown_; }
    Uint32 shownAt() const { return shownAt_; } // SDL_GetTicks of the last show()
    // The canvas while loading: bk.bmp with the logo centred on it.
    void draw();
    // One presented frame of draw(): the logo stays on screen while loading goes on.
    void present();

private:
    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    const ScreenLayout& layout_; // shared; the art directory and the canvas size
    Texture logo_{nullptr, SDL_DestroyTexture};
    Texture background_{nullptr, SDL_DestroyTexture};
    int loadedDir_ = 0; // art directory of the loaded textures
    bool shown_ = false;
    Uint32 shownAt_ = 0;
};

} // namespace dtvr
