// SPDX-License-Identifier: GPL-3.0-only
// The menu window MENU of rally.exe (create seg5:5214, procedure seg5:52E7, paint
// seg5:57F0; round 28): the items of a menu resource of the language file drawn as
// the SBS frames MENUN / MENUG (focused) with the item text, centred on the canvas
// and modal. Evidence in docs/formats.md "In-game menu".
#pragma once

#include "layout.hpp"
#include "ne_strings.hpp"
#include "text.hpp"

#include <SDL.h>

#include <functional>
#include <string>
#include <vector>

namespace dtvr {

class MenuView {
public:
    using SpriteLookup = std::function<SDL_Texture*(const std::string&)>;
    MenuView(SDL_Renderer* renderer, const ScreenLayout& layout, TextRenderer& text, SpriteLookup sprite);

    // Replaces an open menu (seg5:5214 closes the previous one first); focus on item 0.
    void open(const std::string& name, std::vector<NeMenuItem> items);
    void close() { name_.clear(); items_.clear(); }
    bool isOpen() const { return !items_.empty(); }
    const std::string& name() const { return name_; }
    int focus() const { return focus_; }
    // Mouse and keys while the menu holds the capture. Returns the command id of the
    // chosen item (WM_COMMAND to the main window), 0 when nothing was chosen.
    int handle(const SDL_Event& event);
    void draw();
    // The sound of the last handled event (seg5:5486 ff.: 2 Plopp02 chosen, 8 Taster01
    // moved, 7 Zaehler2 grayed), -1 for none; reading clears it.
    int takeSound() { const int s = sound_; sound_ = -1; return s; }
    // Client rectangle: 192 x (items + 1) * 32 (seg5:5733), centred (seg6:0000).
    SDL_Rect box() const;

private:
    int choose(int index) const; // id of an enabled item, else 0
    int itemAt(int x, int y) const;

    SDL_Renderer* renderer_;
    ScreenLayout layout_;
    TextRenderer& text_;
    SpriteLookup sprite_;
    std::string name_;
    std::vector<NeMenuItem> items_;
    int focus_ = 0;
    mutable int sound_ = -1;
};

} // namespace dtvr
