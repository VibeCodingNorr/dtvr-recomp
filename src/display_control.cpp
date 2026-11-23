// SPDX-License-Identifier: GPL-3.0-only
#include "display_control.hpp"

#include "sdl_assets.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace dtvr {

bool DisplayControl::fullscreen() const { return (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0; }

void DisplayControl::resetLogicalSize() { SDL_RenderSetLogicalSize(renderer_, layout_.width, layout_.height); }

bool DisplayControl::apply(const Settings& settings) {
    bool ok = true;
    if (settings.fullscreen != fullscreen()) {
        if (SDL_SetWindowFullscreen(window_, settings.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
            std::cerr << "Vollbild: " << SDL_GetError() << "\n";
            ok = false;
        }
    }
    // Widescreen fills the display's height: whole-number steps would leave borders
    // on every side (1440 / 768 = 1.875 -> 1x).
    SDL_RenderSetIntegerScale(renderer_, settings.integerScale && layout_.extra() == 0 ? SDL_TRUE : SDL_FALSE);
    resetLogicalSize(); // new output size
    return ok;
}

void DisplayControl::beginFrame() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    float sx = 1, sy = 1;
    SDL_RenderGetScale(renderer_, &sx, &sy); // the logical size's scale of the output
    const float scale = std::min(sx, sy);
    const int factor = std::max(1, int(std::ceil(scale - 0.001f)));
    if (std::fabs(scale - float(factor)) < 0.001f) return; // whole number: no gaps
    if (!canvas_ || canvasFactor_ != factor) {
        if (canvas_) SDL_DestroyTexture(canvas_);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear"); // the one downscale of the frame
        canvas_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                    layout_.width * factor, layout_.height * factor);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        canvasFactor_ = factor;
        if (!canvas_) { std::cerr << "Leinwand: " << SDL_GetError() << "\n"; return; }
    }
    if (SDL_SetRenderTarget(renderer_, canvas_) != 0) return;
    SDL_RenderSetScale(renderer_, float(factor), float(factor));
    SDL_RenderSetViewport(renderer_, nullptr);
    SDL_RenderClear(renderer_);
    drawing_ = true;
}

void DisplayControl::endFrame() {
    if (!drawing_) return;
    drawing_ = false;
    SDL_SetRenderTarget(renderer_, nullptr); // back to the window with its logical size
    const SDL_Rect all{0, 0, layout_.width, layout_.height};
    SDL_RenderCopy(renderer_, canvas_, nullptr, &all);
}

void DisplayControl::screenshot(const std::string& path) {
    int outW = layout_.width, outH = layout_.height;
    SDL_GetRendererOutputSize(renderer_, &outW, &outH);
    Surface shot(SDL_CreateRGBSurfaceWithFormat(0, outW, outH, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
    // ReadPixels(NULL) reads the viewport; without the logical size it is the whole output.
    SDL_RenderSetLogicalSize(renderer_, 0, 0);
    SDL_RenderSetViewport(renderer_, nullptr);
    const bool read = shot && SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, shot->pixels, shot->pitch) == 0;
    resetLogicalSize();
    if (!read || SDL_SaveBMP(shot.get(), path.c_str()) != 0) throw std::runtime_error(SDL_GetError());
}

bool DisplayControl::handle(const SDL_Event& event, Settings& settings, bool& changed) {
    changed = false;
    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        resetLogicalSize(); // keep the canvas centred and scaled
        return false;       // others (focus) still see window events
    }
    if (event.type != SDL_KEYDOWN || event.key.repeat) return false;
    const SDL_Keycode key = event.key.keysym.sym;
    if (key == SDLK_F11 || (key == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT))) {
        Settings s = settings; // window / fullscreen ([Display] Fullscreen)
        s.fullscreen = !fullscreen();
        if (apply(s)) { settings = s; changed = true; }
        return true;
    }
    if (key == SDLK_F12) { // text smoothing ([Display] TextSmoothing)
        settings.textSmoothing = (settings.textSmoothing + 1) % 3;
        static const char* const kModes[3] = {"Original", "Bildgröße", "Bildschirmauflösung"};
        std::cout << "Schriftglättung: " << kModes[settings.textSmoothing] << "\n";
        changed = true;
        return true;
    }
    return false;
}

} // namespace dtvr
