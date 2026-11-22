// SPDX-License-Identifier: GPL-3.0-only
// The save dialog of rally.exe (seg8:B157, procedure seg8:B2F0, paint seg8:B6B4 /
// BA8A; round 33): 558 x 462 pixels, header, nine slot strips and a footer with
// Nein / Ok, all frames of sbs/ls.sbs. Evidence in docs/formats.md "Save dialog".
#pragma once

#include "layout.hpp"
#include "save_slots.hpp"
#include "text.hpp"

#include <SDL.h>

#include <functional>
#include <map>
#include <string>

namespace dtvr {

class SaveView {
public:
    using SpriteLookup = std::function<SDL_Texture*(const std::string&)>;
    SaveView(SDL_Renderer* renderer, const ScreenLayout& layout, const std::map<int, std::string>& texts,
             TextRenderer& text, SpriteLookup sprite);

    void open(bool saving) { open_ = true; saving_ = saving; button_ = kOk; }
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    int slot() const { return slot_; }
    bool saving() const { return saving_; }

    enum class Result { None, Close, Confirm };
    Result handle(const SDL_Event& event);
    void draw(const SaveSlots& slots);
    SDL_Rect box() const;

private:
    static constexpr int kOk = 6, kNo = 7; // DS:8D64

    SDL_Renderer* renderer_;
    ScreenLayout layout_;
    const std::map<int, std::string>& texts_;
    TextRenderer& text_;
    SpriteLookup sprite_;
    bool open_ = false;
    bool saving_ = true; // DS:A052: save (text 800) or load (801)
    int slot_ = 0;       // DS:A7B8, kept between openings
    int button_ = kOk;   // DS:8D64: highlighted footer button
};

} // namespace dtvr
