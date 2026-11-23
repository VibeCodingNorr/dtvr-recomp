// SPDX-License-Identifier: GPL-3.0-only
// The player setup window PLAYER of rally.exe (seg1:02BD, procedure seg1:0A60;
// round 36): butt/<n>/pl_menu/menu.bmp, one button per driver with the picture
// <letter><status>.bmp, a name field per driver and the buttons star (start) and
// stop. Geometry from the tables DS:1BD4 / DS:1CC4; evidence in docs/formats.md
// "Player setup".
#pragma once

#include "game.hpp"
#include "layout.hpp"
#include "sb_button.hpp"
#include "sdl_assets.hpp"
#include "text.hpp"

#include <SDL.h>

#include <array>
#include <filesystem>
#include <map>
#include <string>

namespace dtvr {


class SetupView {
public:
    SetupView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
              TextRenderer& text);
    void open(const std::array<uint8_t, kPlayerSlots>& status, const std::array<std::string, kPlayerSlots>& names);
    void close();
    bool isOpen() const { return open_; }
    const std::array<uint8_t, kPlayerSlots>& status() const { return status_; }
    const std::array<std::string, kPlayerSlots>& names() const { return names_; }

    // Recomb extension (not in rally.exe): "Computer spielen alle Karten"
    // ([Extensions] ComputerExtraCards) between stop and star; a click or F9 (a key
    // PLAYER does not use, seg1:0C7A) toggles it.
    void setExtraCards(bool on) { extraCards_ = on; }
    bool extraCards() const { return extraCards_; }
    enum class Result { None, Start, Cancel };
    Result handle(const SDL_Event& event);
    // The SBBUTTON sound of the last handled event for channel 2, -1 none.
    int takeSound() { const int s = sound_; sound_ = -1; return s; }
    void draw();

private:
    SDL_Texture* bitmap(const std::string& file);
    bool anyPlayer() const;

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    TextRenderer& text_;
    SetupGeometry geometry_;
    std::map<std::string, Texture> cache_;
    bool open_ = false;
    std::array<uint8_t, kPlayerSlots> status_{};
    std::array<std::string, kPlayerSlots> names_{};
    int editing_ = -1; // name field with the keyboard focus
    SbButtonGroup buttons_; // ids: slot, kStar, kStop
    int sound_ = -1;
    bool extraCards_ = false;
    SDL_Rect extraRect() const;
    void stopEditing();
};

} // namespace dtvr
