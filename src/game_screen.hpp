// SPDX-License-Identifier: GPL-3.0-only
// The game window as in rally.exe (round 21): canvas with the desk background,
// the map window, the driver portrait and the player info panel. All art is
// loaded at run time from the user's installation (docs/formats.md "Game window").
#pragma once

#include "layout.hpp"
#include "play_view.hpp"
#include "sdl_assets.hpp"
#include "card_view.hpp"
#include "estates_view.hpp"
#include "save_view.hpp"
#include "setup_view.hpp"
#include "menu_view.hpp"
#include "message_view.hpp"
#include "slot_view.hpp"
#include "text.hpp"

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class GameScreen {
public:
    GameScreen(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
               const Board& board, const std::vector<Town>& towns, const Game& game,
               std::map<int, std::string> texts);
    void draw();
    // Title (seg8:03AB): window TITEL over the canvas with butt/<n>/<lang>/t.bmp
    // (DS:17FB); the menu MAIN is drawn above it.
    void setTitle(bool title) { title_ = title; }
    bool title() const { return title_; }
    // The vehicle sets of the drivers in play (after loading a game).
    void loadDriverSprites();
    MessageView& messages() { return messages_; }
    MenuView& menu() { return menu_; }
    CardView& cards() { return cards_; }
    EstatesView& estates() { return estates_; }
    SaveView& saveDialog() { return save_; }
    SetupView& setup() { return setup_; }
    const SaveSlots& saveSlots() const { return slots_; }
    const std::map<int, std::string>& texts() const { return texts_; }
    std::vector<EstateRow> estateRows() const;
    // Top-left world pixel shown in the viewport (follows the current player).
    SDL_Point viewOrigin() const;
    // Smooth driving (round 31): the drawn vehicle runs after the game position at
    // 4 pixels per 50 ms tick (seg2:1CB0: DS:19EA / 19F4 = 2, shifted by DS:34 = 1).
    void advanceVehicles(Uint32 now);
    bool vehicleMoving(int player) const;

private:
    SDL_Texture* art(const std::filesystem::path& relative); // cached, nullptr if missing
    SDL_Texture* sprite(const std::string& name);            // SBS frame by name (any loaded file)
    using SpriteSet = std::map<std::string, Texture>;
    void loadSprites(const std::filesystem::path& relative, SpriteSet& into);
    void loadSprites(const std::filesystem::path& relative) { loadSprites(relative, sprites_); }
    static SDL_Texture* find(const SpriteSet& set, const std::string& name);
    void drawMap();
    void drawPortrait();
    void drawPlayerInfo();

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    ScreenLayout layout_;
    const Board& board_;
    const std::vector<Town>& towns_;
    const Game& game_;
    std::map<int, std::string> texts_;   // language file, UTF-8
    TextRenderer text_;
    std::map<std::string, Texture> sprites_;
    SpriteSet buySprites_;   // sbs/buy.sbs of the town dialog (seg1:4C45)
    SpriteSet lsSprites_;    // sbs/ls.sbs of the save dialog (seg8:B1CE)
    std::map<std::string, Texture> art_;
    Texture world_{nullptr, SDL_DestroyTexture};
    int worldSeason_ = -1;
    SlotView slot_;
    MessageView messages_;
    MenuView menu_;
    CardView cards_;
    EstatesView estates_;
    SaveSlots slots_;
    SaveView save_;
    SetupView setup_;
    int seenSpin_ = 0;
    bool title_ = false;
    std::array<SDL_Point, kPlayerSlots> shown_{};   // drawn world pixel of each vehicle
    std::array<int, kPlayerSlots> shownField_{};    // field the drawn position belongs to
    Uint32 lastAdvance_ = 0;
    Uint32 carry_ = 0;                              // milliseconds not yet spent on a tick
};

struct GameWindowOptions {
    int canvasWidth = 0;          // 0: the largest canvas that fits the screen (seg5:1227)
    bool smokeTest = false;       // draw `frames` frames and exit
    int frames = 1;
    bool rollAtStart = false;     // spin at once (tests of the slot machine)
    std::string openMenu;         // tests: open this menu resource at the start (e.g. RIGHTBTN)
    bool openCards = false;       // tests: open the card window at the start
    bool holdRight = false;       // tests: the right arrow key is held from the start
    bool openSave = false;
    bool openStatistics = false;  // tests: open Statistik at the start
    bool openSetup = false;       // tests: open the player setup at the start
    bool title = false;           // start with the title menu MAIN (seg8:0DB0)
    int loadSlot = -1;            // load this save slot first (until the title menu exists)        // tests: open the save dialog at the start
    int openEstates = -1;         // tests: show the town dialog of this town (no stop needed)
    std::string screenshot;       // BMP of the first frame
};

// Opens the game window and runs it until it is closed; returns the exit code.
int runGameWindow(PlayView& play, const std::filesystem::path& root, const Board& board,
                  const std::vector<Town>& towns, const GameWindowOptions& options);

} // namespace dtvr
