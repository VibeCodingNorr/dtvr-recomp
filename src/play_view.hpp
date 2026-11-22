// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "game.hpp"

#include <SDL.h>

#include <memory>
#include <ostream>
#include <string>

namespace dtvr {

// SDL presentation and keyboard input for the hotseat game (--play). The game
// rules stay in Game; this class only maps keys to commands and draws state.
class PlayView {
public:
    PlayView(const Board& board, const std::vector<Town>& towns, std::unique_ptr<WorldMap> map,
             std::unique_ptr<TileProperties> props, const std::array<uint8_t, kPlayerSlots>& slots, int rounds,
             uint16_t seed);

    Game& game() { return *game_; }
    std::string title() const;
    // Messages of the last command, the town's properties after arriving and the hand.
    void printLog(std::ostream& out) const;
    // Returns true when the key belongs to the game (W, Enter, T, I, N, B, PgUp/PgDn,
    // digits, arrows).
    bool handleKey(SDL_Keycode key, Uint16 mod);
    // Timed game steps (Risiko light) and computer turns; true when the state changed.
    // `stepwise`: one stage of a computer turn per call (the game window drives the
    // computer's car field by field), otherwise the whole turn at once.
    bool update(Uint32 now, bool stepwise = false);
    // Origin, destination, obstacles, Dr. Drago and the players on the world view.
    void draw(SDL_Renderer* renderer, float x, float y, float zoom) const;

private:
    const Board& board_;
    const std::vector<Town>& towns_;
    std::unique_ptr<WorldMap> map_;           // kept alive for the game (barriers, robbers)
    std::unique_ptr<TileProperties> props_;
    std::unique_ptr<Game> game_;
    int cardTarget_ = -1;                     // T: target player with three or more players
    int pendingNails_ = -1;                   // hand slot waiting for a direction key
    int offerCursor_ = 0;                     // card field: selected offer (PgUp / PgDn)
    Uint32 nextTick_ = 0;                     // Risiko: time of the next light change
};

} // namespace dtvr
