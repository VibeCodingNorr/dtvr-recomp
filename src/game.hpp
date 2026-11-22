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
// A message of the original: text number of the language file and its %Fs
// arguments in order (a driver, an amount formatted by seg6:137F, or a name).
struct NoticeArg {
    enum class Kind { Player, Money, Text } kind = Kind::Text;
    int player = -1;
    double money = 0;
    std::string text; // UTF-8
};
struct Notice {
    int text = 0;
    std::vector<NoticeArg> args;
    // seg6:51EA(picture, text, buttons): 0 none, 4 Yes / No (MB_YESNO, answers 6 / 7);
    // `cardPicture` >= 0 replaces the showmaster with that card type's picture.
    int buttons = 0;
    int cardPicture = -1;
    // Event place (map cell) the view zooms in on and centres while the message is
    // open (seg6:2E54(2), seg6:3064); -1: none. Cosmetic only.
    int focusX = -1, focusY = -1;
    int variant = 0; // the scene start corner random(3) of seg8:517B (rain, snow, flood),
                     // Nessie's place index DS:680C
    // The picture seg6:51EA shows instead of the showmaster: Dr. Drago bmp/<n>/qp.bmp
    // (seg2:BAB5 ff.), butt/prof.bmp / fuss.bmp for investment results (seg8:65C0 /
    // 6428); `cardPicture` takes precedence.
    enum class Picture : uint8_t { Showmaster, Drago, Professor, Football } picture = Picture::Showmaster;
};
// A sound of seg1:2C9B(channel, sound, loops): sound = index into the table of
// snd/fx files (seg1:2E90 ff., docs/formats.md "Sounds"); cosmetic, no rule effect.
struct SoundCue { int channel = 0; int sound = 0; int loops = 0; };
// A card film of the FLIC window (seg7:62AA): butt/flc/karte<film>.flc played `loops`
// times with its sound butt/snd/<sound>.wav on channel 7; cosmetic, no rule effect.
struct FilmCue { int film = 0; int loops = 1; const char* sound = ""; };

class Game {
    friend class SaveCodec; // src/save.cpp reads and restores the complete state
public:
    enum class Phase { Roll, Move, Arrived, GameOver, Risiko };

    // `map` and `props` are optional; without them border barriers and the sea
    // route test of the robbers are not applied. A single human gets a computer
    // opponent as in the original (seg2:021F); `soloOpponent = false` is a variant
    // without it.
    Game(const Board& board, const std::vector<Town>& towns, int humans, int rounds, uint16_t seed,
         int defaultCards = 1, const WorldMap* map = nullptr, const TileProperties* props = nullptr,
         bool soloOpponent = true);
    // Slot setup as in [Player] P0..P7 of rally.ini (0 empty, 1 human, 2 computer;
    // the original's defaults are P0 = 1, P1 = 2). Without humans it is a demo game.
    Game(const Board& board, const std::vector<Town>& towns, const std::array<uint8_t, kPlayerSlots>& slots,
         int rounds, uint16_t seed, int defaultCards = 1, const WorldMap* map = nullptr,
         const TileProperties* props = nullptr, bool soloOpponent = true);

    // Commands; each returns false when it is not allowed in the current state.
    bool roll();                 // slot machine, 1 reel 1..6 (WM_USER+6)
    bool step(Direction d);      // one field; reversing undoes the last step
    bool endTurn();              // after arrival: next player, month bookkeeping
    // Yellow field with a full hand (seg3:1057): discard a card, then the card follows.
    bool yellowCardPending() const { return yellowPending_; }
    // Obstacle with a clearing card in a human hand (seg2:2404): the message window
    // asks with text 600 and the card picture; Yes uses the card and drives on.
    struct ObstacleQuestion { int card = -1; Direction direction = Direction::None; };
    const ObstacleQuestion& obstacleQuestion() const { return question_; }
    bool answerObstacle(bool yes);
    bool discardForCard(int handSlot);
    // Play hand slot 0..7 before spinning (game_cards.cpp). `target` depends on the
    // card: the target player for Fiskus/Schulden/Zucker/Blockieren/Schlafen (only
    // needed with three or more players, seg2:51EA), the direction 1..4 for Nägel,
    // the town index for Flugzeug; ignored otherwise.
    bool playCard(int handSlot, int target = -1);
    // seg3:1CCF: the current vehicle is a boat (record +0x2D = 2, on a seaway); Nägel
    // then fall into the water without asking for a direction.
    bool nailsInWater() const;
    // "Karte einsetzen" (seg4:0930 -> seg2:0B08): false with text 1020 + random(2)
    // when the current player's cards are blocked.
    bool openCardWindow();
    // Professor / football club offer (seg8:6FBF): invest a multiple of 1,000 up to
    // the cash, or decline. Spinning or playing a card declines.
    EventKind investmentOffer() const { return offer_; }
    bool invest(int amount);
    void declineInvestment();
    int fieldOfTown(int town) const;
    // Presentation: reel values of the last slot machine spin and a counter that
    // changes with every spin (not part of the saved state).
    const std::vector<int>& lastReels() const { return lastReels_; }
    int spinSerial() const { return spinSerial_; }
    // Messages for the presentation, in the order the original shows them; taking
    // them clears the queue (not part of the saved state).
    // Moves the notices of another game (a new game handed over through the save codec).
    void adoptNotices(Game& other) {
        for (Notice& n : other.takeNotices()) notices_.push_back(std::move(n));
    }
    std::vector<SoundCue> takeSounds() { std::vector<SoundCue> s; s.swap(sounds_); return s; }
    std::vector<FilmCue> takeFilms() { std::vector<FilmCue> f; f.swap(films_); return f; }
    std::vector<std::string> takeMusic() { std::vector<std::string> cues; cues.swap(music_); return cues; }
    std::vector<Notice> takeNotices() { std::vector<Notice> n; n.swap(notices_); return n; }
    // Vehicle facing of a slot (1 up, 2 right, 3 down, 4 left; frame byte +2).
    int facing(int player) const { return lastDirection_[size_t(player & 7)]; }
    uint16_t routeMaskForDisplay() const { return routeMask(); } // DS:A10F for panels
    // Card Risiko (seg7:674D, game_risiko.cpp): after a one-reel spin the light
    // blinks at levels 2..4. Pressing while lit multiplies the steps by L/(L-1),
    // pressing while dark loses them all, stopping keeps them. The presentation
    // calls risikoTick every risikoTickMs(); computers press inside the tick.
    struct RisikoState { int level = 0; int light = 0; int ticks = 0; int steps = 0; };
    const RisikoState& risiko() const { return risiko_; }
    bool risikoTick();
    bool risikoPress();
    bool risikoStop();
    int risikoTickMs() const;
    // Computer players (game_ai.cpp). Slots 0..humans-1 start as humans (status 1);
    // setComputer switches a slot to status 2. runComputerTurn plays the current
    // turn when it belongs to a computer or the player is under Steuerlos.
    bool setComputer(int player, bool computer);
    bool isComputer(int player) const;
    bool computerControls() const;
    bool runComputerTurn();
    // One stage of a computer turn (round 32): card / spin and route, then one field
    // step per call, then the stop's actions and the end of the turn. The same draws
    // in the same order as runComputerTurn, which runs these stages at once.
    bool computerAdvance();
    bool computerTurnRunning() const { return aiTurn_; }
    // seg2:5C5A: whether any slot is human. A game of computers runs until
    // requestDemoEnd (Escape / right click, seg1:0211 -> seg2:5CAD sets DS:16 = 1);
    // it ends at the next player change.
    bool humansLeft() const;
    // Counts turn changes since construction or loading (not saved; for the window's
    // autosave at each turn start, seg2:0837).
    int turnSerial() const { return turnSerial_; }
    // Driver names of the slots (texts 120-127 of the language file, UTF-8); without
    // them messages say "Spieler N".
    void setPlayerNames(const std::array<std::string, kPlayerSlots>& names) { names_ = names; }
    std::string playerName(int player) const;
    bool requestDemoEnd();
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
    const std::vector<Town>& towns() const { return towns_; }
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
    int totalStages() const { return totalStages_; } // DS:A106
    // Counts the year balances shown (BILANZ, not saved) and the choice rand() % 7 of
    // the last one (seg8:88DF), for the window.
    int balanceSerial() const { return balanceSerial_; }
    // Reached destinations (the finish sequence seg7:2628; not saved): count, field, driver.
    int finishSerial() const { return finishSerial_; }
    const std::string& finishMusic() const { return finishMusic_; }
    // seg1:8EE1: the slots in play by cash + property value (+0x42 + +0x5A), highest
    // first; equal values keep slot order (bubble sort with adjacent swaps). With
    // one player in play just that player (seg2:1A41).
    std::vector<int> ranking() const;
    int finishField() const { return finishField_; }
    int finishPlayer() const { return finishPlayer_; }
    int balanceChoice() const { return balanceChoice_; }
    // The year balance's announcements (seg8:88C6, computed when it starts): the
    // opening voice Bilanz_<A + intro>, then per player in play Next_<A + variant>_<A + i>
    // and pos_ / neg_<A + letter>. Not saved.
    struct BalanceEntry { int player = -1; int variant = 0; bool positive = true; int letter = 0; };
    int balanceIntro() const { return balanceIntro_; }
    // The destination announcement seg7:372F(mode) (not saved): 2 at the game start,
    // 1 for a new stage, 3 before the last stage; variant 'a' + it of the voices
    // Part01-04 (mode 1: the rand() % 5 of seg7:3B4E, otherwise 3).
    int announceSerial() const { return announceSerial_; }
    int announceMode() const { return announceMode_; }
    int announceVariant() const { return announceVariant_; }
    const std::vector<BalanceEntry>& balanceEntries() const { return balance_; }
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
    int spin(int reels, int maximum);       // seg2:6426 / 776A: sum of the reels
    void notice(int text, std::vector<NoticeArg> args = {}) { notices_.push_back({text, std::move(args)}); }
    void sound(int channel, int index, int loops = 0) { sounds_.push_back({channel, index, loops}); }
    static NoticeArg playerArg(int p) { NoticeArg a; a.kind = NoticeArg::Kind::Player; a.player = p; return a; }
    static NoticeArg moneyArg(double m) { NoticeArg a; a.kind = NoticeArg::Kind::Money; a.money = m; return a; }
    NoticeArg townArg(int town) const;
    // A notice whose message centres the zoomed view on a map cell (event places).
    void eventNotice(int text, std::vector<NoticeArg> args, int cellX, int cellY, int variant = 0) {
        Notice n{text, std::move(args)};
        n.focusX = cellX;
        n.focusY = cellY;
        n.variant = variant;
        notices_.push_back(std::move(n));
    }
    double propertyPrice(const Economy::PropertyRef& p) const;
    void startRisiko();                     // game_risiko.cpp
    // seg2:61B7: the move starts with `allowance` fields unless the player is shut
    // in (seg5:99B3); false when the turn ended instead.
    bool startMove(int allowance);
    void startRisikoLevel(int level);
    void finishRisiko();
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
    int turnSerial_ = 0;
    bool aiTurn_ = false;                   // a computer turn is between stages (not saved)
    int aiPlayer_ = -1;
    bool aiSteerless_ = false;
    std::vector<int> aiRoute_;              // fields still to drive
    bool yellowPending_ = false;            // yellow field waits for a discard
    ObstacleQuestion question_;             // transient: not saved, asked again after loading
    bool obstacleConfirmed_ = false;
    RisikoState risiko_;                    // DS:9C92 light, level, tick counter, DS:A778 steps
    bool demoEnd_ = false;                  // DS:16
    std::vector<int> lastReels_;            // DS:9584 (presentation only)
    int spinSerial_ = 0;
    std::vector<Notice> notices_;
    std::vector<SoundCue> sounds_;
    std::vector<FilmCue> films_;
    std::array<std::string, kPlayerSlots> names_; // presentation only, not saved
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
    int totalStages_ = 0;
    int balanceSerial_ = 0, balanceChoice_ = 0, balanceIntro_ = 0;
    int announceSerial_ = 0, announceMode_ = 2, announceVariant_ = 3;
    std::vector<BalanceEntry> balance_;
    int finishSerial_ = 0, finishField_ = -1, finishPlayer_ = -1;
    std::string finishMusic_ = "z1"; // cosmetic; reuses the existing draw, no new RNG call
    std::vector<std::string> music_; // transient presentation commands, never saved
    std::vector<std::string> messages_;
};

} // namespace dtvr
