// SPDX-License-Identifier: GPL-3.0-only
// The Blue Byte logo of the start (see logo_view.hpp).
#include "logo_view.hpp"

#include <iostream>

namespace fs = std::filesystem;

namespace dtvr {

void drawCanvasBackground(SDL_Renderer* renderer, SDL_Texture* background, const ScreenLayout& layout) {
    if (layout.extra() == 0) { SDL_RenderCopy(renderer, background, nullptr, nullptr); return; }
    // Widescreen extension: the bevelled edges keep their size, the wood between them
    // (horizontal grain) is stretched to the wide canvas.
    int w = 0, h = 0;
    SDL_QueryTexture(background, nullptr, nullptr, &w, &h);
    constexpr int kEdge = 16;
    const SDL_Rect leftSrc{0, 0, kEdge, h}, midSrc{kEdge, 0, w - 2 * kEdge, h}, rightSrc{w - kEdge, 0, kEdge, h};
    const SDL_Rect leftDst{0, 0, kEdge, layout.height}, midDst{kEdge, 0, layout.width - 2 * kEdge, layout.height},
        rightDst{layout.width - kEdge, 0, kEdge, layout.height};
    SDL_RenderCopy(renderer, background, &leftSrc, &leftDst);
    SDL_RenderCopy(renderer, background, &midSrc, &midDst);
    SDL_RenderCopy(renderer, background, &rightSrc, &rightDst);
}

namespace {

Texture loadArt(SDL_Renderer* renderer, const fs::path& root, const fs::path& relative) {
    Texture texture(nullptr, SDL_DestroyTexture);
    const fs::path path = findCaseInsensitive(root, relative);
    if (path.empty()) {
        std::cerr << "Grafik fehlt: " << relative.generic_string() << "\n";
        return texture;
    }
    try {
        // The 8-bit bitmap with its own colour table; BB.bmp has no pixel of colour 0,
        // the key of the blit seg6:0938 -> seg5:4FC6, so the copy is opaque.
        const Surface surface = loadBmp(path);
        texture.reset(SDL_CreateTextureFromSurface(renderer, surface.get()));
    } catch (const std::exception& e) {
        std::cerr << relative.generic_string() << ": " << e.what() << "\n";
    }
    return texture;
}

} // namespace

bool LogoView::show() {
    if (loadedDir_ != layout_.artDir) {
        logo_ = loadArt(renderer_, root_, pictPath(layout_.artDir, "BB.bmp")); // DS:00F8 / DS:0111
        background_ = loadArt(renderer_, root_, pictPath(layout_.artDir, "bk.bmp")); // the canvas DS:17E2
        loadedDir_ = layout_.artDir;
    }
    if (!shown_) shownAt_ = SDL_GetTicks();
    shown_ = logo_ != nullptr;
    return shown_;
}

void LogoView::draw() {
    if (!shown_) return;
    if (background_) drawCanvasBackground(renderer_, background_.get(), layout_);
    int w = 0, h = 0;
    SDL_QueryTexture(logo_.get(), nullptr, nullptr, &w, &h);
    // PICT: CreateWindow(0, 0, biWidth, biHeight) as a child of the canvas (seg6:0AE0),
    // centred by WM_CREATE (seg6:0B8E -> seg6:0000); WM_PAINT copies the DIB to (0, 0).
    const ScreenRect r = centredOnCanvas(layout_.width, layout_.height, w, h);
    const SDL_Rect dst{r.x, r.y, r.w, r.h};
    SDL_RenderCopy(renderer_, logo_.get(), nullptr, &dst);
}

void LogoView::present() {
    if (!shown_) return;
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    draw();
    SDL_RenderPresent(renderer_);
    SDL_PumpEvents(); // seg6:0B05 dispatches the queue once; here the events wait for the loop
}

} // namespace dtvr
