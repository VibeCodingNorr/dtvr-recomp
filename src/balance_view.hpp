// SPDX-License-Identifier: GPL-3.0-only
// The year balance BILANZ of rally.exe (seg8:88C6; round 47): butt/bil_leer.bmp with
// the show master (intro\redet.sbs, ziel\hebel.sbs), his foot and the lever; he
// opens with Bilanz_<A..D>, then per player pulls the lever and announces
// Next_<variant>_<a..h> and pos_ / neg_<letter> (voices of butt/<lang>/voice/bilanz).
// Evidence and simplifications in docs/formats.md "Year balance".
#pragma once

#include "game.hpp"
#include "sdl_assets.hpp"
#include "sound.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <vector>

namespace dtvr {

class BalanceView {
public:
    BalanceView(SDL_Renderer* renderer, const std::filesystem::path& root, int canvasWidth, int canvasHeight);
    bool start(int intro, const std::vector<Game::BalanceEntry>& entries, bool humans, SoundPlayer* sound, Uint32 now);
    bool active() const { return active_; }
    void stop() { active_ = false; }
    // A click (seg8:8D5C) lets the show master go on to the next player.
    void click() { clicked_ = true; }
    void update(Uint32 now);
    void draw(Uint32 now);

private:
    enum class Stage { Intro, Wait, Pull, Next, Result };
    void enter(Stage stage, Uint32 now);
    void voice(const std::string& name);
    bool voiceDone(Uint32 now, Uint32 minimum) const;

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    int canvasWidth_, canvasHeight_;
    Texture background_{nullptr, SDL_DestroyTexture};
    std::map<std::string, Texture> frames_;
    std::vector<Game::BalanceEntry> entries_;
    SoundPlayer* sound_ = nullptr;
    int intro_ = 0;
    size_t index_ = 0;
    bool humans_ = true, clicked_ = false, active_ = false;
    Stage stage_ = Stage::Intro;
    Uint32 started_ = 0, stageStart_ = 0;
};

} // namespace dtvr
