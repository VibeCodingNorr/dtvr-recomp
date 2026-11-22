// SPDX-License-Identifier: GPL-3.0-only
// The target choice SELPLAYER of rally.exe (seg2:51EA(excluded); round 53): one
// button per eligible player with its picture butt/<n>/pl_menu/<letter><status>.bmp
// in a row. Evidence in docs/formats.md "Card target".
#pragma once

#include "layout.hpp"
#include "sdl_assets.hpp"

#include <SDL.h>

#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace dtvr {

class SelectPlayerView {
public:
    SelectPlayerView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout);
    // `candidates` in slot order with their status (1 human, 2 computer).
    void open(const std::vector<int>& candidates, const std::array<uint8_t, 8>& status);
    void close() { candidates_.clear(); }
    bool isOpen() const { return !candidates_.empty(); }
    // The chosen slot, -2 when cancelled (Escape / right button), -1 otherwise.
    int handle(const SDL_Event& event);
    void draw();

private:
    SDL_Rect box() const;
    SDL_Rect button(size_t index) const;
    SDL_Texture* bitmap(const std::string& file);

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    ScreenLayout layout_;
    std::map<std::string, Texture> cache_;
    std::vector<int> candidates_;
    std::array<uint8_t, 8> status_{};
};

} // namespace dtvr
