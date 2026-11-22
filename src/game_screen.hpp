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
    MessageView& messages() { return messages_; }
    MenuView& menu() { return menu_; }
    CardView& cards() { return cards_; }
    EstatesView& estates() { return estates_; }
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
    void loadSprites(const std::filesystem::path& relative);
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
    std::map<std::string, Texture> art_;
    Texture world_{nullptr, SDL_DestroyTexture};
    int worldSeason_ = -1;
    SlotView slot_;
    MessageView messages_;
    MenuView menu_;
    CardView cards_;
    EstatesView estates_;
    int seenSpin_ = 0;
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
    int openEstates = -1;         // tests: show the town dialog of this town (no stop needed)
    std::string screenshot;       // BMP of the first frame
};

// Opens the game window and runs it until it is closed; returns the exit code.
int runGameWindow(PlayView& play, const std::filesystem::path& root, const Board& board,
                  const std::vector<Town>& towns, const GameWindowOptions& options);

} // namespace dtvr
