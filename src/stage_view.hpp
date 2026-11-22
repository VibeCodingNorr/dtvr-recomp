// SPDX-License-Identifier: GPL-3.0-only
// The destination announcement of rally.exe (seg7:372F(mode); round 49): the show
// master on butt/club_map.bmp pulls the lever, a light (ziel\licht.sbs) jumps over
// the destination towns and stops on the new destination; voices of
// butt/<lang>/voice/zielsuch. Evidence and simplifications in docs/formats.md
// "Destination announcement".
#pragma once

#include "board.hpp"
#include "sdl_assets.hpp"
#include "sound.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace dtvr {

class StageView {
public:
    StageView(SDL_Renderer* renderer, const std::filesystem::path& root, int canvasWidth, int canvasHeight,
              const Board& board, const std::vector<Town>& towns);
    // mode 2 game start, 1 new stage, 3 last stage; variant 0-4 of Part01-04.
    bool start(int mode, int variant, int destination, SoundPlayer* sound, Uint32 now);
    bool active() const { return active_; }
    void stop() { active_ = false; }
    void update(Uint32 now);
    void draw(Uint32 now);

private:
    enum class Stage { Intro, Pull, Search, Final };
    void enter(Stage stage, Uint32 now);
    void voice(const std::string& name);
    bool voiceDone(Uint32 now, Uint32 minimum) const;
    void jump();
    int passes(Uint32 now) const;

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    int canvasWidth_, canvasHeight_;
    const Board& board_;
    const std::vector<Town>& towns_;
    Texture background_{nullptr, SDL_DestroyTexture};
    std::map<std::string, Texture> frames_;
    std::vector<int> candidates_; // kind-6 fields (seg5:1D70)
    SoundPlayer* sound_ = nullptr;
    int mode_ = 1, variant_ = 0, destination_ = -1;
    int light_ = -1;               // field under the light
    int jumps_ = 0, wait_ = 0;     // DS [bp-6] / [bp-8] of seg7:3D35
    int lastPass_ = 0;
    std::vector<std::string> queue_; // voices of the final stage
    unsigned seed_ = 1;
    bool active_ = false;
    Stage stage_ = Stage::Intro;
    Uint32 stageStart_ = 0;
};

} // namespace dtvr
