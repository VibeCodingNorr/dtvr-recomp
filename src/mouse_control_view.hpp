// SPDX-License-Identifier: GPL-3.0-only
// The mouse steering cross MCTRL of rally.exe (round 78): a left click on the map
// (seg6:1798 -> seg4:0F52) opens a 64 x 64 window centred on the click; the cursor
// stays inside it (ClipCursor), moving it highlights a direction (procedure
// seg4:103B, state DS:8B93) and releasing the left button posts that key to the main
// window; the right button closes it. Frames M0 / M1 / MT / MR / MD / ML of
// sbs/assorted.sbs. Evidence in docs/formats.md "Mouse steering".
#pragma once

#include <SDL.h>

#include <functional>
#include <string>

namespace dtvr {

// seg4:10A3: the state for a point inside the 64 x 64 window: 1 centre (0x15..0x29),
// 2 up, 3 right, 4 down, 5 left (the triangles between the diagonals).
int mouseControlState(int x, int y);
// The key a state posts (table seg4:107F): Return, Up, Right, Down, Left; 0 none.
SDL_Keycode mouseControlKey(int state);

class MouseControlView {
public:
    using SpriteLookup = std::function<SDL_Texture*(const std::string&)>;
    MouseControlView(SDL_Renderer* renderer, SpriteLookup sprite) : renderer_(renderer), sprite_(std::move(sprite)) {}
    // Opens centred on a canvas point (seg4:0F87: click - 0x20).
    void open(int x, int y);
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    // The key to post on release of the left button, SDLK_UNKNOWN otherwise; `closed`
    // tells the caller the window went away (release or right button).
    SDL_Keycode handle(const SDL_Event& event, bool& closed);
    // `moveDone`: DS:9D40 == DS:A778, the centre shows M1 instead of M0 (seg4:12FC).
    void draw(bool moveDone);

private:
    SDL_Renderer* renderer_;
    SpriteLookup sprite_;
    SDL_Rect box_{};
    bool open_ = false;
    int state_ = 1; // DS:8B93, 1 after WM_CREATE (seg4:1219)
};

} // namespace dtvr
