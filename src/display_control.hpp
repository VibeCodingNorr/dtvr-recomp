// SPDX-License-Identifier: GPL-3.0-only
// Window and scaling of the game canvas (recomb only, [Display]): the logical canvas
// size, fullscreen (F11 / Alt+Enter), whole-number scaling and text smoothing (F12).
// rally.exe uses none of these keys.
#pragma once

#include "layout.hpp"
#include "settings.hpp"

#include <SDL.h>

#include <string>

namespace dtvr {

class DisplayControl {
public:
    DisplayControl(SDL_Window* window, SDL_Renderer* renderer, const ScreenLayout& layout)
        : window_(window), renderer_(renderer), layout_(layout) {}
    // Window events and the display keys. True when the event was consumed; `settings`
    // then holds the changed settings when `changed` is set.
    bool handle(const SDL_Event& event, Settings& settings, bool& changed);
    // Brings window mode and scaling in line with the settings; false (with SDL's error
    // printed) when the window mode could not be changed.
    bool apply(const Settings& settings);
    bool fullscreen() const;
    // The shared layout changed size (widescreen switch): a window keeps its height
    // and takes the new aspect; the canvas texture is rebuilt at the next frame.
    void layoutChanged();
    // Saves the scaled output, borders included, as BMP; throws on failure.
    void screenshot(const std::string& path);
    // One frame of the canvas. At a fractional output scale every piece scaled on its
    // own lands on truncated pixels and adjoining pieces (window borders) show 1-pixel
    // gaps; the frame is then drawn into a canvas texture at the next whole multiple
    // and scaled to the output once, smoothly. Whole-number scales draw directly.
    void beginFrame();
    void endFrame();

private:
    void resetLogicalSize();
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    SDL_Texture* canvas_ = nullptr; // owned; see ~DisplayControl
    int canvasFactor_ = 0, canvasWidth_ = 0;
    bool drawing_ = false;
public:
    ~DisplayControl() { if (canvas_) SDL_DestroyTexture(canvas_); }
    DisplayControl(const DisplayControl&) = delete;
    DisplayControl& operator=(const DisplayControl&) = delete;
};

} // namespace dtvr
