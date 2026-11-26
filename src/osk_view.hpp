// SPDX-License-Identifier: GPL-3.0-only
// RECOMP ADDITION (no counterpart in rally.exe): the on-screen keyboard over the player
// setup PLAYER, opened by the pad's A on a focused name field. It is drawn in the
// game's style from the installation: the wooden border B1-B8 of assorted.sbs
// (window_border), the paper butt/<n>/p1.bmp, keys framed in the dialog colour 0x45
// with the red SBBUTTON focus frame, the name in the SBINPUT font (Times New Roman
// italic). Grid and text rules in src/osk.hpp; mapping in docs/formats.md
// "Recomp pad layer".
#pragma once

#include "layout.hpp"
#include "osk.hpp"
#include "sdl_assets.hpp"
#include "text.hpp"
#include "window_border.hpp"

#include <SDL.h>

#include <filesystem>
#include <string>

namespace dtvr {

class ScreenKeyboardView {
public:
    ScreenKeyboardView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
                       TextRenderer& text);
    void setSprites(SpriteLookup sprites) { borderSprites_ = std::move(sprites); }
    // Edits a copy of `name` for the field of `slot`.
    void open(int slot, const std::string& name, size_t maxChars);
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    int slot() const { return slot_; }
    const std::string& text() const { return keyboard_.text(); }
    const ScreenKeyboard& keyboard() const { return keyboard_; }

    // Done: take text() into the field; Cancel: keep the old name. Either closes it.
    enum class Result { None, Done, Cancel };
    // Arrows (also the pad's), Return (A) presses the focused key, Backspace (X) erases,
    // Escape (B) cancels, characters of a real keyboard are typed, a left click presses
    // a key; the mouse pointer moves the focus.
    Result handle(const SDL_Event& event);
    Result accept();     // the pad's Start: done
    void toggleShift();  // the pad's Y
    void setHelp(std::string help) { help_ = std::move(help); } // the help line (default: the default map's)
    int takeSound() { const int s = sound_; sound_ = -1; return s; } // channel 2, -1 none
    void draw();

private:
    struct Geometry {
        SDL_Rect window, client, name, help;
        int keyW = 0, keyH = 0, gap = 0, left = 0, top = 0;
    };
    Geometry geometry() const;
    SDL_Rect cellRect(const Geometry& g, const ScreenKeyboard::Cell& cell) const;
    int cellAt(int x, int y) const;
    Result apply(const ScreenKeyboard::Result& r);
    std::string caption(const ScreenKeyboard::Cell& cell) const;
    SDL_Texture* paper();

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    TextRenderer& text_;
    SpriteLookup borderSprites_;
    Texture paper_{nullptr, SDL_DestroyTexture};
    int paperDir_ = -1; // the art directory paper_ was loaded from
    ScreenKeyboard keyboard_;
    bool open_ = false;
    int slot_ = -1;
    int sound_ = -1;
    std::string help_; // empty: keyboardTexts().help
};

} // namespace dtvr
