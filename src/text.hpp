// SPDX-License-Identifier: GPL-3.0-only
// Text as rally.exe draws it (GDI DrawText with Arial, seg2:314A), through
// SDL2_ttf when available. Arial is used if installed, otherwise the metrically
// compatible Liberation Sans. Without SDL2_ttf nothing is drawn.
#pragma once

#include "layout.hpp"

#include <SDL.h>

#include <map>
#include <memory>
#include <string>

namespace dtvr {

struct TextStyle {
    int pixelHeight = 12;   // CreateFont height (negative = character height)
    bool bold = true;       // weight 600 (seg6:04F9 callers)
    bool italic = false;
    bool center = false;    // DT_CENTER
    bool right = false;     // DT_RIGHT (format 0x102)
    int shadow = 0;         // seg2:314A: first in palette colour 0, moved up-left by this
    SDL_Color color{255, 255, 255, 255};
    bool serif = false;     // Times New Roman (SBINPUT, seg1:1D04) instead of Arial
};

class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
    bool available() const { return available_; }
    // 0 Original: no anti-aliasing; 1 anti-aliased at canvas size (default); 2 anti-aliased
    // at the output resolution: glyphs are rasterised at the renderer's scale and drawn
    // into the same canvas rectangle, so layout and line breaks stay those of the canvas.
    enum Smoothing { Original = 0, Canvas = 1, Screen = 2 };
    void setSmoothing(int mode) { smoothing_ = mode >= Original && mode <= Screen ? mode : Canvas; }
    int smoothing() const { return smoothing_; }
    const std::string& fontDescription() const { return description_; }
    // Draws one line of UTF-8 text inside `rect` (top-aligned, no clipping, as DT_NOCLIP).
    void draw(SDL_Renderer* renderer, const std::string& text, const ScreenRect& rect, const TextStyle& style);
    // GetTextExtent: width and height of one line; {0, pixelHeight} without a font.
    SDL_Point measure(const std::string& text, const TextStyle& style);
    // DT_WORDBREAK: breaks at spaces so that every line fits the rectangle's width.
    void drawWrapped(SDL_Renderer* renderer, const std::string& text, const ScreenRect& rect, const TextStyle& style);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool available_ = false;
    int smoothing_ = Canvas;
    std::string description_;
};

} // namespace dtvr
