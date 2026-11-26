// SPDX-License-Identifier: GPL-3.0-only
// Drawing and input of the recomp menu (src/recomp_menu.hpp) over the darkened game:
// our own layout and colours; the panel uses the wood of butt/<n>/bk.bmp from the
// installation when it is there.
#pragma once

#include "layout.hpp"
#include "recomp_menu.hpp"
#include "sdl_assets.hpp"
#include "text.hpp"

#include <SDL.h>

#include <filesystem>
#include <string>
#include <vector>

namespace dtvr {

class RecompMenuView {
public:
    RecompMenuView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
                   TextRenderer& text, std::string version);
    RecompMenu& menu() { return menu_; }
    const RecompMenu& menu() const { return menu_; }
    bool isOpen() const { return menu_.isOpen(); }
    // Keys, controller keys and the mouse (canvas coordinates).
    RecompMenu::Action handle(const SDL_Event& event);
    void draw();

private:
    struct Geometry {
        SDL_Rect panel, header, sidebar, content, footer;
        SDL_Rect tableHead{}; // a table page's column headings
        std::vector<SDL_Rect> pages, rows;
        double scale = 1;
    };
    Geometry geometry() const;
    int px(double v) const;
    SDL_Rect sliderBar(const SDL_Rect& row) const; // the volume bar left of its percentage
    void drawToggle(const SDL_Rect& row, bool on, bool focused, double scale);
    void drawTable(const Geometry& g, const RecompMenu::Page& page);

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    TextRenderer& text_;
    std::string version_;
    RecompMenu menu_;
    Texture wood_{nullptr, SDL_DestroyTexture};
    bool woodTried_ = false;
};

} // namespace dtvr
