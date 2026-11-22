// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "board.hpp"
#include "rules.hpp"

#include <string>
#include <vector>

namespace dtvr {

// SDL-independent hotseat game assembled from the rules recovered from rally.exe.
// Simplification: vehicles move field to field along the graph instead of the
// original pixel driving (each graph link is a drivable road, see docs/formats.md).
// Event cards, card fields, the town dialog and the opening capital are not
// reconstructed yet; money is tracked as the change since the start.
class Game {
public:
    enum class Phase { Roll, Move, Arrived, GameOver };

    Game(const Board& board, const std::vector<Town>& towns, int humans, int rounds, uint16_t seed);

    // Commands; each returns false when it is not allowed in the current state.
    bool roll();                 // slot machine, 1 reel 1..6 (WM_USER+6)
    bool step(Direction d);      // one field; reversing undoes the last step
    bool endTurn();              // after arrival: next player, month bookkeeping

    Phase phase() const { return phase_; }
    int currentPlayer() const { return current_; }
    int playerField(int player) const { return players_[size_t(player)].field; }
    double moneyChange(int player) const { return money_[size_t(player)]; }
    const std::vector<PlayerSlot>& players() const { return players_; }
    int origin() const { return origin_; }
    int destination() const { return destination_; }
    int remainingStages() const { return remaining_; }
    int allowance() const { return counter_.allowance(); }
    int stepsTaken() const { return counter_.taken(); }
    const Calendar& calendar() const { return calendar_; }
    // Messages produced by the last command (German, for the viewer log).
    const std::vector<std::string>& messages() const { return messages_; }
    std::string townName(int field) const;

private:
    void arrive();

    const Board& board_;
    const std::vector<Town>& towns_;
    MsRandom rng_;
    std::vector<PlayerSlot> players_;
    std::vector<double> money_;
    Calendar calendar_;
    MoveCounter counter_{0};
    Phase phase_ = Phase::Roll;
    int current_ = 0;
    int origin_ = -1, destination_ = -1;
    int remaining_ = 0;
    std::vector<std::string> messages_;
};

} // namespace dtvr
