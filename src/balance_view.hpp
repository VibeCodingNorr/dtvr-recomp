// SPDX-License-Identifier: GPL-3.0-only
// The year balance BILANZ of rally.exe (seg8:88C6; round 47): butt/bil_leer.bmp with
// the show master (intro\redet.sbs, ziel\hebel.sbs), his foot and the lever; he
// opens with Bilanz_<A..D>, then per player announces Next_<variant>_<a..h>, pulls
// the lever (Hebel01) and says pos_ / neg_<letter> (voices of butt/<lang>/voice/bilanz;
// the players' part's passes in balance_players.cpp).
// Evidence and simplifications in docs/formats.md "Year balance".
#pragma once

#include "window_border.hpp"

#include "balance_players.hpp"
#include "game.hpp"
#include "sdl_assets.hpp"
#include "sound.hpp"
#include "subtitle_view.hpp"

#include <SDL.h>

#include <filesystem>
#include <functional>
#include <map>
#include <vector>

#include "voice_sync.hpp"

namespace dtvr {

class BalanceView {
public:
    // B1-B8 of assorted.sbs for the 8-pixel border of seg6:1A68 (round 112).
    void setSprites(SpriteLookup sprites) { borderSprites_ = std::move(sprites); }
    BalanceView(SDL_Renderer* renderer, const std::filesystem::path& root, char language, const int& canvasWidth, const int& canvasHeight);
    // The game's commands at the points where rally.exe draws (round 144): `announce(i)`
    // when the show master goes on to player i (seg8:8E38) returns its variant,
    // `close()` at the closing lever (seg8:9605) returns the closing letter. Without
    // them the entries' and `closing`'s values are used (--balance).
    struct Commands {
        std::function<int(size_t)> announce;
        std::function<int()> close;
    };
    bool start(int intro, const std::vector<Game::BalanceEntry>& entries, int closing, bool humans, SoundPlayer* sound,
               Uint32 now, Commands commands = {});
    bool active() const { return active_; }
    void setSubtitles(SubtitleView* subtitles) { subtitles_ = subtitles; }
    // seg8:8BDB: with subtitles on, seg6:160B(hwnd, 0, -22) lifts the centred window.
    int lift() const { return subtitles_ && subtitles_->enabled() ? 22 : 0; }
    void stop() { active_ = false; }
    // BILANZ (seg8:A829): Enter, Space or a left click set DS:8D5C: during the opening
    // voice it stops (seg8:8D17), later the show master goes on, in the closing
    // part the show ends (seg8:986C).
    void click();
    // Escape or a right click clear DS:A6F2: the players' part jumps to the closing
    // part (seg8:9253), the closing part ends (seg8:984E). On that jump DS:8D5C is not
    // cleared (only seg8:8E1A does), so a pending click ends the closing part after its
    // first pass (seg8:986C).
    void escape(Uint32 now);
    void update(Uint32 now);
    void draw(Uint32 now);

private:
    SpriteLookup borderSprites_;
    enum class Stage { Players, Closing, ClosingPull, ClosingResult, ClosingWait };
    class PlayersIo; // BalancePlayers::Io on this window's sound and sync.dat
    bool closingPart() const { return stage_ >= Stage::Closing; }
    void enter(Stage stage, Uint32 now);
    void voice(const std::string& name);
    bool voiceDone(Uint32 now, Uint32 minimum) const;
    // Players' part: one pass at least every kSceneStepMs (seg6:0CDF(20), seg8:91F0).
    void runPlayers(Uint32 now);

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    char language_; // the lan.inf letter DS:A0F0 (seg5:028C)
    const int& canvasWidth_; // the shared layout's size (live widescreen switch)
    const int& canvasHeight_;
    Texture background_{nullptr, SDL_DestroyTexture};
    std::map<std::string, Texture> frames_;
    Commands commands_;
    SoundPlayer* sound_ = nullptr;
    SubtitleView* subtitles_ = nullptr;
    int intro_ = 0, closing_ = 4;
    bool humans_ = true, active_ = false;
    BalancePlayers players_;
    // The voice that seg1:3E64 / 4071 look at in the players' part: its sync.dat
    // record (nullptr when none or stopped) and the pass in which it started.
    const VoiceSyncRecord* voiceSync_ = nullptr;
    int pass_ = 0, voicePass_ = 0;
    Uint32 nextPass_ = 0;
    bool hebelPlayed_ = false; // closing part: Hebel01 started at pass 35 (seg8:94E7)
    // Closing part before the lever: record 0 = DS:4938 (seg8:92A7), held on frame 0 with
    // countdown 2 in every pass in which Next_a_a talks (seg1:4071) or has ended
    // (seg8:9408-9436). The last such pass (-1: none yet) and the passes processed.
    std::vector<VoiceSyncRecord> sync_;
    const VoiceSyncRecord* nextSync_ = nullptr;
    int holdPass_ = -1, passesSeen_ = 0;
    const char* closingHoldFrame(int pass) const;
    Stage stage_ = Stage::Players;
    Uint32 started_ = 0, stageStart_ = 0;
};

} // namespace dtvr
