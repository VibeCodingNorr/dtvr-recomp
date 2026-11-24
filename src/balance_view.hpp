// SPDX-License-Identifier: GPL-3.0-only
// The year balance BILANZ of rally.exe (seg8:88C6; round 47): butt/bil_leer.bmp with
// the show master (intro\redet.sbs, ziel\hebel.sbs), his foot and the lever; he
// opens with Bilanz_<A..D>, then per player announces Next_<variant>_<a..h>, pulls
// the lever (Hebel01) and says pos_ / neg_<letter> (voices of butt/<lang>/voice/bilanz;
// the players' part's passes in balance_players.cpp).
// Evidence and simplifications in docs/formats.md "Year balance".
#pragma once

#include "window_border.hpp"

#include "balance_board.hpp"
#include "balance_closing.hpp"
#include "balance_players.hpp"
#include "game.hpp"
#include "sdl_assets.hpp"
#include "sound.hpp"
#include "subtitle_view.hpp"
#include "text.hpp"

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
        std::function<void(size_t)> spoken; // pos_ / neg_ of entry i spoken (seg8:913D)
        std::function<int()> close;
    };
    bool start(int intro, const std::vector<Game::BalanceEntry>& entries, int closing, bool humans, SoundPlayer* sound,
               Uint32 now, Commands commands = {});
    bool active() const { return active_; }
    // The board's contents (seg8:9AB8, round 149): the sheets of the eight slots and the
    // worth graph, from the game state at the year balance; text through `text`.
    void setBoard(std::array<BalanceSheet, kPlayerSlots> sheets, BalanceGraph graph) {
        sheets_ = std::move(sheets);
        graph_ = std::move(graph);
    }
    void setText(TextRenderer* text) { text_ = text; }
    void setSubtitles(SubtitleView* subtitles) { subtitles_ = subtitles; }
    // seg8:8BDB: with subtitles on, seg6:160B(hwnd, 0, -22) lifts the centred window.
    int lift() const { return subtitles_ && subtitles_->enabled() ? 22 : 0; }
    void stop() { active_ = false; }
    // BILANZ (seg8:A829): Enter, Space or a left click set DS:8D5C: during the opening
    // voice it stops (seg8:8D17), later the show master goes on, in the closing
    // part the show ends (seg8:986C).
    void click();
    // Escape or a right click clear DS:A6F2 (seg8:99B4 / 99DC): the players' part jumps to
    // the closing part at its end test (seg8:9253), the closing part ends (seg8:984E). On
    // that jump DS:8D5C is not cleared (only seg8:8E1A does), so a pending click ends the
    // closing part after its first pass (seg8:986C).
    void escape(Uint32 now);
    void update(Uint32 now);
    void draw(Uint32 now);

private:
    SpriteLookup borderSprites_;
    enum class Stage { Players, Closing };
    class PlayersIo; // BalancePlayers::Io on this window's sound and sync.dat
    bool closingPart() const { return stage_ == Stage::Closing; }
    void enterClosing(); // seg8:925C
    void finish();       // seg8:9875 ff.: seg1:3C3B resets the wave output, the window goes
    void voice(const std::string& name);
    // One pass at least every kSceneStepMs (seg6:0CDF(20) at seg8:91F0 / 97EB).
    void runPasses(Uint32 now);
    // WM_PAINT's board (seg8:9B93 ff.) at the bitmap origin (ox, oy).
    void drawBoard(int ox, int oy);
    void drawSheet(const BalanceSheet& sheet, int ox, int oy);
    void drawGraph(int ox, int oy);
    SDL_Color paletteColour(int index) const;
    SDL_Color nearestColour(BalanceGraph::Rgb rgb) const; // PALETTERGB

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    char language_; // the lan.inf letter DS:A0F0 (seg5:028C)
    const int& canvasWidth_; // the shared layout's size (live widescreen switch)
    const int& canvasHeight_;
    Texture background_{nullptr, SDL_DestroyTexture};
    std::map<std::string, Texture> frames_;
    std::map<std::string, Texture> legend_;  // bilanz\bilanz.sbs (DS:7C9E), drawn opaque
    std::vector<SDL_Color> palette_;         // bil_leer.bmp's (the game palette DS:9CD2)
    std::array<BalanceSheet, kPlayerSlots> sheets_;
    BalanceGraph graph_;
    TextRenderer* text_ = nullptr;
    static constexpr int kBoardGraph = 0xA;
    int board_ = -1; // DS:8D5E: -1 none (seg8:88F8), a slot, or kBoardGraph (seg8:954C)
    Commands commands_;
    SoundPlayer* sound_ = nullptr;
    SubtitleView* subtitles_ = nullptr;
    int intro_ = 0, closing_ = 4;
    bool humans_ = true, active_ = false;
    bool endAfterPass_ = false; // DS:8D5C still set when Escape jumped to the closing part
    BalancePlayers players_;
    BalanceClosing closingPart_;
    // The voice that seg1:3E64 / 4071 look at in the players' part: its sync.dat
    // record (nullptr when none or stopped) and the pass in which it started.
    const VoiceSyncRecord* voiceSync_ = nullptr;
    int pass_ = 0, voicePass_ = 0;
    Uint32 nextPass_ = 0;
    std::vector<VoiceSyncRecord> sync_; // dat/<c>/sync.dat
    Stage stage_ = Stage::Players;
    Uint32 started_ = 0;
};

} // namespace dtvr
