// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "ai.hpp"
#include "board.hpp"
#include "cards.hpp"
#include "drago.hpp"
#include "economy.hpp"
#include "events.hpp"
#include "obstacles.hpp"
#include "rules.hpp"

#include <array>
#include <string>
#include <vector>

namespace dtvr {

// SDL-independent hotseat game assembled from the rules recovered from rally.exe.
// Simplification: vehicles move field to field along the graph instead of the
// original pixel driving (each graph link is a drivable road, see docs/formats.md).
// Status of each card and of the computer players: README / docs/formats.md.
// Players start with 30,000 (seg2:033A).
// Assumptions (not yet traced): cards are played before spinning, and the start
// order of random calls is shuffle, deal, destination.
class Game {
    friend class SaveCodec; // src/save.cpp reads and restores the complete state
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
    // Yellow field with a full hand (seg3:1057): discard a card, then the card follows.
    bool yellowCardPending() const { return yellowPending_; }
    bool discardForCard(int handSlot);
    // Play hand slot 0..7 before spinning (game_cards.cpp). `target` depends on the
    // card: the target player for Fiskus/Schulden/Zucker/Blockieren/Schlafen (only
    // needed with three or more players, seg2:51EA), the direction 1..4 for Nägel,
    // the town index for Flugzeug; ignored otherwise.
    bool playCard(int handSlot, int target = -1);
    // Professor / football club offer (seg8:6FBF): invest a multiple of 1,000 up to
    // the cash, or decline. Spinning or playing a card declines.
    EventKind investmentOffer() const { return offer_; }
    bool invest(int amount);
    void declineInvestment();
    int fieldOfTown(int town) const;
    // Computer players (game_ai.cpp). Slots 0..humans-1 start as humans (status 1);
    // setComputer switches a slot to status 2. runComputerTurn plays the current
    // turn when it belongs to a computer or the player is under Steuerlos.
    bool setComputer(int player, bool computer);
    bool isComputer(int player) const;
    bool computerControls() const;
    bool runComputerTurn();
    // Card field (kind 5) after arriving: buy one offered card type, sell a hand card.
    bool atCardField() const;
    int handCount(int player) const; // seg2:3C00
    std::vector<CardOffer> cardOffers() const;
    bool buyCard(int type);
    bool sellCard(int handSlot);
    // DS:3FC6 town dialog mode set by property cards 1-8 (card type + 1), 0xFF = none.
    int propertyMode() const { return propertyMode_; }
    // Visible map area in cells (seg2:46C8 uses the window); Tschüss avoids it.
    // Default: 40 x 30 cells centred on the target (assumption, see docs).
    static CellRect viewAround(const Field& f) { return {f.x - 20, f.y - 15, f.x + 20, f.y + 15}; }
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
    const ObstacleTable& obstacles() const { return obstacles_; }
    // Random events on/off (default on). Used for scenario tests and as a game
    // variant; the original always runs them.
    void setRandomEvents(bool on) { randomEvents_ = on; }

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
    // game_drago.cpp
    void dragoAct(int player);   // seg2:B58C after field actions and sit-outs
    void placeDragoForStage();   // seg2:14B1: farthest player gets Dr. Drago
    bool playWeihnachten();      // seg3:1156
    void playHauAb();            // seg3:3EFB
    void playPunch();            // seg3:2D17 (game_cards.cpp)
    void playTschuess(int target); // seg3:28ED
    void discard(int player, int handSlot); // seg2:3C62: back to the deck
    void giveYellowCard();                  // seg2:1127: draw, text random(13), Dr. Drago
    // game_events.cpp
    void eventTick(int player);  // seg8:1624 after every player change (player = new DS:94E2)
    void applyEvent(EventKind kind);
    void placeObstacle(uint16_t type, int durationFactor);   // rain / snow (seg8:5F43)
    void damageTown(int town, int protectionCard);          // seg8:4F1B
    int findCard(int player, int type) const;               // seg3:0D26, slot or -1
    uint16_t routeMask() const { return eastOpen_ ? 0 : 0x4000; }
    AiState aiState(bool steerless) const;

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
    EventPlaces places_;
    ObstacleTable obstacles_;
    bool randomEvents_ = true;
    EventKind offer_ = EventKind::None;     // pending professor / football offer
    int eventPlayer_ = 0;                   // DS:94E2 while the event trigger runs
    bool doubleNext_ = false;               // DS:2EB0 (Verdoppeln)
    int propertyMode_ = 0xFF;               // DS:3FC6
    bool aiActing_ = false;                 // inside runComputerTurn
    bool yellowPending_ = false;            // yellow field waits for a discard
    std::array<int, 8> lastDirection_{2, 2, 2, 2, 2, 2, 2, 2}; // vehicle facing (frame byte +2)
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
