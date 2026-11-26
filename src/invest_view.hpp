// SPDX-License-Identifier: GPL-3.0-only
// The investment offer of rally.exe (seg8:6FBF, procedure seg8:7098; round 56): a
// 0x1BE x 0x8A window on butt/unipaper.bmp with prof.bmp or fuss.bmp, the offer
// text and Ja / Nein; after Ja the amount in steps of 1,000 (texts 5390 / 5391).
// Evidence in docs/formats.md "Investment offer".
#pragma once

#include "window_border.hpp"

#include "layout.hpp"
#include "sdl_assets.hpp"
#include "text.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class InvestView {
public:
    void setSprites(SpriteLookup sprites) { borderSprites_ = std::move(sprites); } // B1-B8
    InvestView(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
               const std::map<int, std::string>& texts, TextRenderer& text);
    // `professor`: prof.bmp, else fuss.bmp; `offer`: the formatted offer text; `now`:
    // SDL ticks at creation, where seg6:0FB4 reads TimerCount for a computer's wait.
    void open(bool professor, const std::string& offer, double cash, Uint32 now = 0);
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    Uint32 openedAt() const { return openedAt_; }
    void restartWait(Uint32 now) { openedAt_ = now; } // a window in front of it (messages) held it
    struct Result { bool done = false; int amount = 0; int sound = -1; }; // amount 0: declined
    Result handle(const SDL_Event& event);
    // The posted VK_BACK of a computer's timeout (seg6:10EE -> seg1:0176): the decline
    // of seg8:714C with its sound 7 on channel 2.
    Result timeout() const { Result r; r.done = open_; r.sound = open_ ? 7 : -1; return r; }
    void draw();

private:
    SpriteLookup borderSprites_;
    SDL_Rect windowRect() const; // CreateWindow size, centred
    SDL_Rect box() const;        // the client area inside the border (seg8:7C63)
    SDL_Texture* bitmap(const std::string& file);
    std::string text(int id) const;
    Result enter();

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    const std::map<int, std::string>& texts_;
    TextRenderer& text_;
    std::map<std::string, Texture> cache_;
    bool open_ = false, professor_ = true, asking_ = false;
    std::string offer_;
    int choice_ = 0;   // DS:8D5A: 0 Ja, 1 Nein
    int amount_ = 1000;
    double cash_ = 0;
    Uint32 openedAt_ = 0;
};

} // namespace dtvr
