// SPDX-License-Identifier: GPL-3.0-only
// The options window OPTION of rally.exe (seg1:ACE7; round 39): butt/<n>/<lang>/
// menu.bmp, eight switches op_menu/<value><name>.bmp at the places of the player
// setup's drivers (switch i at slot i, table DS:1BD4) and star / stop of pl_menu.
#pragma once

#include "layout.hpp"
#include "sdl_assets.hpp"
#include "settings.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class OptionsView {
public:
    OptionsView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout);
    // seg1:ADEE keeps a copy of the values (DS:9478) for stop.
    void open(const Settings& settings) { settings_ = settings; open_ = true; }
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    const Settings& settings() const { return settings_; }

    enum class Result { None, Accept, Cancel };
    Result handle(const SDL_Event& event);
    void draw();

private:
    SDL_Texture* bitmap(const std::filesystem::path& relative);

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    ScreenLayout layout_;
    SetupGeometry geometry_;
    std::map<std::string, Texture> cache_;
    Settings settings_;
    bool open_ = false;
};

} // namespace dtvr
