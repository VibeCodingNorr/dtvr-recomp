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
};

class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
    bool available() const { return available_; }
    const std::string& fontDescription() const { return description_; }
    // Draws one line of UTF-8 text inside `rect` (top-aligned, no clipping, as DT_NOCLIP).
    void draw(SDL_Renderer* renderer, const std::string& text, const ScreenRect& rect, const TextStyle& style);
    // DT_WORDBREAK: breaks at spaces so that every line fits the rectangle's width.
    void drawWrapped(SDL_Renderer* renderer, const std::string& text, const ScreenRect& rect, const TextStyle& style);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool available_ = false;
    std::string description_;
};

} // namespace dtvr
