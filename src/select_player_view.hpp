// SPDX-License-Identifier: GPL-3.0-only
// The target choice SELPLAYER of rally.exe (seg2:51EA(excluded); round 53): one
// button per eligible player with its picture butt/<n>/pl_menu/<letter><status>.bmp
// in a row. Evidence in docs/formats.md "Card target".
#pragma once

#include "window_border.hpp"

#include "layout.hpp"
#include "sb_button.hpp"
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
    void setSprites(SpriteLookup sprites) { borderSprites_ = std::move(sprites); } // B1-B8
    SelectPlayerView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout);
    // `candidates` in slot order with their status (1 human, 2 computer).
    void open(const std::vector<int>& candidates, const std::array<uint8_t, 8>& status);
    void close() { candidates_.clear(); }
    bool isOpen() const { return !candidates_.empty(); }
    // SBBUTTON input (seg1:0E78): `id` is the chosen slot, `sound` for channel 2.
    // There is no cancel: the window's procedure (dispatch seg2:598E) has no key or
    // right-button case and passes the buttons' WM_COMMAND 0xFFFF to DefWindowProc.
    SbButtonGroup::Input handle(const SDL_Event& event);
    void draw();

private:
    SpriteLookup borderSprites_;
    SDL_Rect windowRect() const; // CreateWindow size, centred
    SDL_Rect box() const;        // the client area inside the border (seg2:5969)
    SDL_Rect button(size_t index) const;
    SDL_Texture* bitmap(const std::string& file);

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    std::map<std::string, Texture> cache_;
    std::vector<int> candidates_;
    SbButtonGroup buttons_; // id = slot
    std::array<uint8_t, 8> status_{};
};

} // namespace dtvr
