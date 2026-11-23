// SPDX-License-Identifier: GPL-3.0-only
// The investment offer of rally.exe (seg8:6FBF, procedure seg8:7098; round 56): a
// 0x1BE x 0x8A window on butt/unipaper.bmp with prof.bmp or fuss.bmp, the offer
// text and Ja / Nein; after Ja the amount in steps of 1,000 (texts 5390 / 5391).
// Evidence in docs/formats.md "Investment offer".
#pragma once

#include "window_border.hpp"

#include "layout.hpp"
#include "sdl_assets.hpp"
#include "text.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class InvestView {
public:
    void setSprites(SpriteLookup sprites) { borderSprites_ = std::move(sprites); } // B1-B8
    InvestView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
               const std::map<int, std::string>& texts, TextRenderer& text);
    // `professor`: prof.bmp, else fuss.bmp; `offer`: the formatted offer text.
    void open(bool professor, const std::string& offer, double cash);
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    struct Result { bool done = false; int amount = 0; int sound = -1; }; // amount 0: declined
    Result handle(const SDL_Event& event);
    void draw();

private:
    SpriteLookup borderSprites_;
    SDL_Rect windowRect() const; // CreateWindow size, centred
    SDL_Rect box() const;        // the client area inside the border (seg8:7C63)
    SDL_Texture* bitmap(const std::string& file);
    std::string text(int id) const;
    Result enter();

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    const std::map<int, std::string>& texts_;
    TextRenderer& text_;
    std::map<std::string, Texture> cache_;
    bool open_ = false, professor_ = true, asking_ = false;
    std::string offer_;
    int choice_ = 0;   // DS:8D5A: 0 Ja, 1 Nein
    int amount_ = 1000;
    double cash_ = 0;
};

} // namespace dtvr
