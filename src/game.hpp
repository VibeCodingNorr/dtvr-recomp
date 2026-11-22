// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "board.hpp"
#include "cards.hpp"
#include "drago.hpp"
#include "economy.hpp"
#include "events.hpp"
#include "rules.hpp"

#include <string>
#include <vector>

namespace dtvr {

// SDL-independent hotseat game assembled from the rules recovered from rally.exe.
// Simplification: vehicles move field to field along the graph instead of the
// original pixel driving (each graph link is a drivable road, see docs/formats.md).
// Card fields, non-movement card effects and the town dialog's other functions
// are not reconstructed yet. Players start with 30,000 (seg2:033A).
// Assumptions (not yet traced): cards are played before spinning, and the start
// order of random calls is shuffle, deal, destination.
class Game {
public:
    enum class Phase { Roll, Move, Arrived, GameOver };

    // `map` and `props` are optional; without them border barriers and the sea
    // route test of the robbers are not applied.
    Game(const Board& board, const std::vector<Town>& towns, int humans, int rounds, uint16_t seed,
         int defaultCards = 1, const WorldMap* map = nullptr, const TileProperties* props = nullptr);

    // Commands; each returns false when it is not allowed in the current state.
    bool roll();                 // slot machine, 1 reel 1..6 (WM_USER+6)
    bool step(Direction d);      // one field; reversing undoes the last step
    bool endTurn();              // after arrival: next player, month bookkeeping
    // Play hand slot 0..7 before spinning. Movement, money (Fiskus, Konto, Schulden,
    // Zero) and status cards (Zucker, Blockieren, Schlafen) are rebuilt; `target`
    // is only needed with three or more players (seg2:51EA).
    bool playCard(int handSlot, int target = -1);
    // Sell an own entry of the town the player stopped in (seg1:832A, full value).
    bool sellProperty(int entry);
    const Hand& hand(int player) const { return hands_[size_t(player)]; }
    const Deck& deck() const { return deck_; }
    std::string cardName(int deckSlot) const;
    // Town dialog subset: buy a free entry of the town the player stopped in
    // (seg1:7DC5, normal mode: price = entry value, cash must cover it).
    bool buyProperty(int entry);
    int townAt(int field) const;                 // index into the town list, -1 if none
    const Economy& economy() const { return economy_; }
    const DragoState& drago() const { return drago_; }
    bool eastOpen() const { return eastOpen_; }
    // A link exists and is not closed by a border barrier (for planners and the UI).
    bool linkOpen(int field, Direction d) const;
    const EventClock& eventClock() const { return events_; }

    Phase phase() const { return phase_; }
    int currentPlayer() const { return current_; }
    int playerField(int player) const { return players_[size_t(player)].field; }
    double cash(int player) const { return economy_.money(player).cash; }
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
    void advanceTurn();          // nextPlayer plus month/year bookkeeping
    std::vector<int> activePlayers() const;
    int handCount(int player) const;
    // game_drago.cpp
    void dragoAct(int player);   // seg2:B58C after field actions and sit-outs
    void placeDragoForStage();   // seg2:14B1: farthest player gets Dr. Drago
    bool playWeihnachten();      // seg3:1156
    void playHauAb();            // seg3:3EFB
    // game_events.cpp
    void eventTick();            // seg8:1624 after every player change
    void applyEvent(EventKind kind);
    uint16_t routeMask() const { return eastOpen_ ? 0 : 0x4000; }

    const Board& board_;
    const std::vector<Town>& towns_;
    MsRandom rng_;
    std::vector<PlayerSlot> players_;
    Economy economy_;
    DragoState drago_;
    const WorldMap* map_ = nullptr;
    const TileProperties* props_ = nullptr;
    std::vector<bool> barriers_;
    EventClock events_;
    bool eastOpen_ = false;        // DS:A10F == 0
    double westValue_ = 0;         // DS:A113
    int loadedSeason_ = 0;         // season of the loaded SBB set (seg5:186A)
    Deck deck_;
    std::vector<Hand> hands_;
    Calendar calendar_;
    MoveCounter counter_{0};
    Phase phase_ = Phase::Roll;
    int current_ = 0;
    int origin_ = -1, destination_ = -1;
    int remaining_ = 0;
    std::vector<std::string> messages_;
};

} // namespace dtvr
