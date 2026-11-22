// SPDX-License-Identifier: GPL-3.0-only
// The game window as in rally.exe (round 21): canvas with the desk background,
// the map window, the driver portrait and the player info panel. All art is
// loaded at run time from the user's installation (docs/formats.md "Game window").
#pragma once

#include "layout.hpp"
#include "play_view.hpp"
#include "sdl_assets.hpp"

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class GameScreen {
public:
    GameScreen(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
               const Board& board, const Game& game);
    void draw();
    // Top-left world pixel shown in the viewport (follows the current player).
    SDL_Point viewOrigin() const;

private:
    SDL_Texture* art(const std::filesystem::path& relative); // cached, nullptr if missing
    void drawMap();
    void drawPortrait();
    void drawPlayerInfo();

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    ScreenLayout layout_;
    const Board& board_;
    const Game& game_;
    std::map<std::string, Texture> art_;
    Texture world_{nullptr, SDL_DestroyTexture};
    int worldSeason_ = -1;
};

struct GameWindowOptions {
    int canvasWidth = 0;          // 0: the largest canvas that fits the screen (seg5:1227)
    bool smokeTest = false;       // draw one frame and exit
    std::string screenshot;       // BMP of the first frame
};

// Opens the game window and runs it until it is closed; returns the exit code.
int runGameWindow(PlayView& play, const std::filesystem::path& root, const Board& board,
                  const GameWindowOptions& options);

} // namespace dtvr
