// SPDX-License-Identifier: GPL-3.0-only
// The message window INFO (seg6:51EA, procedure seg6:5492, paint seg6:59C6):
// paper butt/<n>/p1.bmp, the showmaster bmp/<n>/sm.bmp at (10, 10) and the text
// of the language file with its arguments, centred on the canvas and modal
// (round 25).
#pragma once

#include "window_border.hpp"

#include "game.hpp"
#include "layout.hpp"
#include "sdl_assets.hpp"
#include "text.hpp"

#include <deque>
#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

// Recomp (round E3): a notice with this text id shows its first argument's text instead of
// a language text (the foreign-save message); the simulation never emits it.
constexpr int kRecompNoticeText = -1;

class MessageView {
public:
    void setSprites(SpriteLookup sprites) { sprites_ = std::move(sprites); } // B1-B8 of assorted.sbs
    MessageView(SDL_Renderer* renderer, const std::filesystem::path& root, char language, const ScreenLayout& layout,
                const std::map<int, std::string>& texts, TextRenderer& text);
    void push(Notice notice) { queue_.push_back(std::move(notice)); }
    bool open() const { return !queue_.empty(); }
    void dismiss() { if (!queue_.empty()) queue_.pop_front(); choice_ = 6; ++serial_; } // a click or a key closes it
    // Changes whenever the front message changes (a played event scene belongs to one).
    int frontSerial() const { return serial_; }
    // The front message asks Yes / No (buttons 4, MB_YESNO).
    bool question() const { return !queue_.empty() && queue_.front().buttons == 4; }
    int frontText() const { return queue_.empty() ? 0 : queue_.front().text; }
    const Notice* front() const { return queue_.empty() ? nullptr : &queue_.front(); }
    // seg6:5492 for a question: arrows choose, Return / space keep the choice, Escape /
    // Backspace answer No, a click on a button answers it. 6 Yes, 7 No, 0 still open.
    // The caller dismisses the message.
    int handle(const SDL_Event& event);
    void clear() { queue_.clear(); }
    void draw(const Game& game);

private:
    SDL_Texture* bitmap(const std::filesystem::path& relative, int& w, int& h);
    // box: the window (CreateWindow size); client: inside the 8-pixel border of
    // seg6:1A68 (INFO calls it at seg6:593F).
    struct Geometry { SDL_Rect box{}, client{}; SDL_Texture* picture = nullptr; int pw = 0, ph = 0; SDL_Rect yes{}, no{}; };
    Geometry geometry();
    int buttonSize() const;

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    char language_; // the lan.inf letter DS:A0F0 (seg5:028C)
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    const std::map<int, std::string>& texts_;
    TextRenderer& text_;
    std::map<std::string, Texture> cache_;
    std::deque<Notice> queue_;
    int choice_ = 6; // DS:8C38, Yes after WM_CREATE (seg6:5834)
    int serial_ = 0;
    SpriteLookup sprites_;
};

} // namespace dtvr
