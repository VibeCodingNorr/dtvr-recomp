// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "music_player.hpp"
#include "text.hpp"

namespace dtvr {

// Local audio overlay (F8), available above the original menu/options windows.
class MusicMenu {
public:
    MusicMenu(SDL_Renderer* renderer, int width, int height, MusicPlayer& player,
              std::filesystem::path preferences, std::vector<std::string> issues);
    bool open() const { return open_; }
    void toggle() { open_ = !open_; }
    // true if local settings changed; caller persists the original Midi switch.
    bool handle(const SDL_Event& event);
    void draw(TextRenderer& text, bool hint = false);
private:
    void changePack(int delta);
    void save();
    SDL_Renderer* renderer_;
    int width_, height_;
    MusicPlayer& player_;
    std::filesystem::path preferences_;
    std::vector<std::string> issues_;
    bool open_ = false;
};

} // namespace dtvr
