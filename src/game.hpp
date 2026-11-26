// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "ai.hpp"
#include "board.hpp"
#include "cards.hpp"
#include "drago.hpp"
#include "economy.hpp"
#include "edition.hpp"
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
    // Order of emission (presentation): a sound waits until the messages before it
    // have been closed, as rally.exe's modal message windows (seg6:51EA) hold its code.
    int seq = 0;
};
// A sound of seg1:2C9B(channel, sound, loops): sound = index into the table of
// snd/fx files (seg1:2E90 ff., docs/formats.md "Sounds"); cosmetic, no rule effect.
// `afterNotice`: the sequence number the next notice would get when the sound was
// emitted; it plays once every notice with a smaller number has been closed.
struct SoundCue { int channel = 0; int sound = 0; int loops = 0; int afterNotice = 0; };
// A card film of the FLIC window (seg7:62AA): butt/flc/karte<film>.flc played `loops`
// times with its sound butt/snd/<sound>.wav on channel 7; cosmetic, no rule effect.
struct FilmCue { int film = 0; int loops = 1; const char* sound = ""; };

class Game {
    friend class SaveCodec; // src/save.cpp reads and restores the complete state
public:
    // Balance (round 144): the year balance's show is running inside the player change;
    // Statement (round R1): the monthly statement STAT is open. Neither is saved (rally.exe's
    // windows are modal), so the save format keeps 0..Risiko.
    enum class Phase { Roll, Move, Arrived, GameOver, Risiko, Balance, Statement };

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
    // The same, continuing a running generator: rally.exe seeds rand() once at start
    // (seg1:02A9 in seg5:0029) and a new game draws on from there (round 143).
    Game(const Board& board, const std::vector<Town>& towns, const std::array<uint8_t, kPlayerSlots>& slots,
         int rounds, MsRandom random, int defaultCards = 1, const WorldMap* map = nullptr,
         const TileProperties* props = nullptr, bool soloOpponent = true);

    // Commands; each returns false when it is not allowed in the current state.
    bool roll();                 // slot machine, 1 reel 1..6 (WM_USER+6)
    // Developer option --event (tests of the presentation): applies an event now, as
    // the event trigger would; the game's rules are those of the event itself.
    void triggerEvent(EventKind kind) { applyEvent(kind); }
    // The program's one generator (DS:838E, seeded once at seg5:0029): the game's draws,
    // and before a game the intro's wipe (seg7, round 143) draw from it.
    MsRandom& sessionRandom() { return rng_; }
    // --drago FORM (tests): Dr. Drago joins as at a stage start, in form 0-2.
    void startDrago(int form) { placeDragoForStage(destination_); drago_.form = form < 0 ? 0 : form > 2 ? 2 : form; }
    // seg2:69AD: a human may close the slot window of Würfeln (seg2:6426 flag DS:9AD8)
    // with Escape / Backspace / the right button before the reels stop: the roll is
    // taken back. In rally.exe the reel value is drawn when the lever ends (seg2:7671
    // -> 776A), so `reelsDrawn` keeps that one draw of the random generator. Possible
    // while the roll is the last command and left the player moving without messages
    // (round 129: a shut-in roll is not taken back, see docs/formats.md).
    bool rollCancellable() const { return !rollUndo_.empty() && spinPending(); }
    bool cancelRoll(bool reelsDrawn);
    // seg2:6426 returns only when the slot window has closed (DS:A780 = 0); then a
    // human with sum 0 (cancelled) gets no move (seg2:66A6), otherwise seg2:66B9 ->
    // seg2:61B7 starts the move (reach, shut-in notices). With a window
    // (setSpinPresentation) a spin of Würfeln, a reel card or a status card waits in
    // spinPending() until finishSpin(); without one the move starts at once (round 131).
    void setSpinPresentation(bool on) { spinPresentation_ = on; }
    // seg2:0BC3 (with the arrival test seg2:0D38) has one caller, the main window's
    // Return (seg4:01EA): a human's vehicle stops on its last field and the field's
    // action runs only when Return accepts it — until then the move can still be
    // turned back. With setArrivalConfirm (the game window) step() leaves the arrival to
    // pressReturn(); computers and windowless play arrive at once (round 137), with the
    // same Return effects, as the computer (seg5:6E12) and Flugzeug (seg3:3361) post
    // VK_RETURN to the main window too.
    void setArrivalConfirm(bool on) { arrivalConfirm_ = on; }
    // seg8:6FBF has no computer branch: on a computer's turn the modal loop seg6:0FB4
    // posts 0x7EF (wParam 8) to the offer window after the message window's wait,
    // seg1:0176 turns it into WM_KEYDOWN VK_BACK and seg8:73B9 -> 714C declines (no
    // random draw); seg1:00D2 drops a human's keys and buttons meanwhile. With
    // setInvestmentWindow (the game window) a computer's turn waits until the window has
    // declined; without a window the computer declines at once (round ai-invest).
    void setInvestmentWindow(bool on) { investmentWindow_ = on; }
    bool arrivalDue() const;     // the move is complete, Return would run the field
    // The main window's Return, seg4:01EA -> seg2:0BC3: it never ends a turn or rolls.
    // In the turn's Roll and Move phases it ends Verdoppeln (seg2:0CE8) and the TimeOut
    // timer (seg2:0CEE); then, with the vehicle on the cell grid (`aligned`, seg2:2C0F),
    // it runs a due arrival. Returns true when the field's action ran. Other phases are
    // inside seg2:0BC3 already (DS:1A2A) or belong to another window (Risiko).
    bool pressReturn(bool aligned = true);
    bool spinPending() const { return pendingAllowance_ >= 0; }
    bool finishSpin();
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
    // seg3:1CCF: the current vehicle flies (record +0x2D = 2), which is the case on a
    // sea field (cell bits field + water); Nägel then fall into the water without
    // asking for a direction.
    bool nailsInWater() const;
    // seg3:225A ff. (WM_CREATE of the Nägel window): direction 0 up, 1 right, 2 down,
    // 3 left is available when the player's field links there and the cell two
    // steps away has no sea-route bit 0x04. Without any, CreateWindow fails and the
    // card takes the water path (seg3:1E02: sound 0x12, text 5500).
    std::array<bool, 4> nailsDirections() const;
    // Recomp extension (not in rally.exe, docs/formats.md "Computer card play"):
    // computers also play the cards whose original score never wins. Part of the
    // game state (saved), off by default.
    void setComputerExtraCards(bool on) { computerExtraCards_ = on; }
    // The towns of the Flugzeug rows (texts 5602 + i, matched by name like seg3:32CE),
    // for the extension's computers; installation data, not game state.
    void setAirports(const std::array<int, 8>& towns) { airports_ = towns; }
    // The edition of the original data (src/edition.hpp), set once by the program: saves
    // record it and refuse another edition's saves (docs/save-format.md). Not changed
    // by loading.
    void setEdition(Edition edition) { edition_ = edition; }
    Edition edition() const { return edition_; }
    // [Setup] TimeOut (DS:B5D6, from the settings): after an "n Felder" card
    // seg2:3ABD gives (n + 2) * 5 seconds; seg2:3B0E then fines the driver every
    // second (500 for 10 s, 750 for 10 s, then 1000) until the turn ends.
    void setTimeLimit(bool on) { timeLimitOn_ = on; }
    // One second of the timer (seg2:3B0E); the presentation calls it every 1000 ms
    // while the game window is active (DS:94E6). Returns the sound for channel 6 to
    // play at once (0x22 time up, 0x15 a fine), -1 none.
    int timeLimitSecond();
    // seg1:4302: the value of the player's own entries that are not rented out and
    // not of category 3 (entry +0x1E).
    double sellableValue(int player) const;
    // seg1:4E39 (seg2:1618, before nextPlayer): endTurn with negative cash and a
    // sellable value opens the forced sale instead of ending the turn; it stays
    // until the cash is no longer negative or nothing is left (DS:3FB8 = 3, seg1:5857).
    bool forcedSale() const { return forcedSale_; }
    bool forcedSaleLocked() const;
    // seg1:57A4 -> seg1:8327: one own, not rented entry at value x 0.7 (DS:868E).
    bool forcedSell(int town, int entry);
    bool timeLimitRunning() const { return timeLimitActive_; }
    bool computerExtraCards() const { return computerExtraCards_; }
    // europe.pit properties of a map cell (seg2:3260), 0 without map data.
    uint16_t cellProperties(int x, int y) const;
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
        for (Notice& n : other.takeNotices()) pushNotice(std::move(n));
    }
    std::vector<SoundCue> takeSounds() { std::vector<SoundCue> s; s.swap(sounds_); return s; }
    std::vector<FilmCue> takeFilms() { std::vector<FilmCue> f; f.swap(films_); return f; }
    std::vector<std::string> takeMusic() { std::vector<std::string> cues; cues.swap(music_); return cues; }
    std::vector<Notice> takeNotices() { std::vector<Notice> n; n.swap(notices_); return n; }
    bool hasNotices() const { return !notices_.empty() || !films_.empty(); }
    // Vehicle facing of a slot (1 up, 2 right, 3 down, 4 left; frame byte +2).
    int facing(int player) const { return lastDirection_[size_t(player & 7)]; }
    // Counts the jumps of a slot's vehicle (seg2:04AD and the Master-Rallye start
    // seg2:5CDE set the pixel position directly): the map shows the vehicle there at
    // once instead of driving it. Presentation only, not saved.
    int positionJumps(int player) const { return jumps_[size_t(player & 7)]; }
    uint16_t routeMaskForDisplay() const { return routeMask(); } // DS:A10F for panels
    // DS:22 outside the stage change: distances from the destination DS:A10D with the
    // mask DS:A10F, 0xFFFF unreached (seg2:43A8; rules.hpp distanceTable).
    std::vector<uint16_t> distanceTable() const;
    // Card Risiko (seg7:674D, game_risiko.cpp): after a one-reel spin the light
    // blinks at levels 2..4. Pressing while lit multiplies the steps by L/(L-1),
    // pressing while dark loses them all, stopping keeps them. The presentation
    // calls risikoTick every risikoTickMs(); computers press inside the tick.
    struct RisikoState { int level = 0; int light = 0; int ticks = 0; int steps = 0; };
    const RisikoState& risiko() const { return risiko_; }
    bool risikoTick();
    bool risikoPress();
    // Every CHANCE press (human or computer) with the light it met (presentation).
    int risikoPressSerial() const { return risikoPresses_; }
    int risikoPressLight() const { return risikoPressLight_; }
    bool risikoStop();
    int risikoTickMs() const;
    // Computer players (game_ai.cpp). Slots 0..humans-1 start as humans (status 1);
    // setComputer switches a slot to status 2. runComputerTurn plays the current
    // turn when it belongs to a computer or the player is under Steuerlos.
    bool setComputer(int player, bool computer);
    // Mitspieler (menu 1066, seg4:0804 -> seg2:5E93): toggles the slot between human
    // and computer. Only possible from MAIN1, which opens from the turn menu ONFIELD
    // (seg4:09DE), i.e. at the start of a turn before the spin: false otherwise. For
    // the current player the computer's state machine restarts (seg5:6CE8, 6BF2):
    // a computer then plays the turn from its card choice (state 1).
    bool switchDriver(int player);
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
    // Counts turn starts (seg2:0407) since construction or loading (not saved; for the
    // window's autosave, seg2:0837, and its centring on the new player, seg2:3216). It
    // changes when the new player is current, after STAT or BILANZ held the change;
    // resumeLoaded counts as one.
    int turnSerial() const { return turnSerial_; }
    // What rally.exe does after it has read a saved game (game_load.cpp; user decision
    // 2026-11-25: loading behaves like the original loader instead of restoring every
    // value). Call after SaveCodec::decode with Random::Session. The loader seg8:AC9A
    // (seg8:AF67-B06A): the East again (seg8:69F0(1), DS:A113 anew), every slot in play
    // on its field with the start facing seg2:5067, Dr. Drago placed anew at his holder
    // (seg2:9C97: random(3) for the countdown). Then the title's start of a loaded game
    // (seg4:069B / 061D): the season seg5:186A(month, 1) and, at a turn start (phase
    // Roll; rally.exe saves only there), the turn start seg2:0407 of the current player
    // (computer state reset, hand order seg2:3CF1, Dr. Drago's countdown seg2:B3A4).
    // DS:16 (demo end) is cleared before loading (seg4:066F), so a loaded game has none.
    void resumeLoaded();
    // Driver names of the slots (texts 120-127 of the language file, UTF-8); without
    // them messages say "Spieler N".
    void setPlayerNames(const std::array<std::string, kPlayerSlots>& names) { names_ = names; }
    // seg2:6332 (new game, seg4:0E71): the name codes of slot 7 (Armino Gesserti),
    // compared case-sensitively: "Armin Gessert" sets its cash to 500,000 (DS:861E);
    // "130663" sets 1,000,000 (DS:8626), draws a card (seg2:4327) for every occupied
    // hand slot while walking the eight slots and renames it "Armin Gessert".
    void applyNameCodes();
    const std::array<std::string, kPlayerSlots>& playerNames() const { return names_; }
    std::string playerName(int player) const;
    bool requestDemoEnd();
    bool demoEnding() const { return demoEnd_; } // DS:16
    // DS:14: 1 only for the title entry Demo (seg4:05BB -> seg4:0DA9(1), seg4:05CF); every
    // other start clears it (seg4:0591 -> 0DA9(0), seg4:05F0 / 0675 / 0767). Not saved.
    void setDemo(bool on) { demo_ = on; }
    bool demo() const { return demo_; }
    // seg1:01D7, the input seg1:00D2 drops while the current player is a computer:
    // WM_KEYDOWN VK_ESCAPE or WM_RBUTTONDOWN. Without humans in a demo it is the end of
    // the demo (seg2:5C5A / 5CAD); otherwise, outside a demo and while no Steuerlos holds
    // a status in DS:10, the current computer becomes a human (seg1:0253-027C).
    enum class EscapeAction { None, DemoEnd, TakeOver };
    EscapeAction computerEscapeAction() const;
    int escapePlayer() const; // DS:94E2 as seg1:01D7 sees it (STAT: its player)
    // seg1:0253-027C: sound 0x10 on channel 0 (played by the window at once), the status
    // byte +0 = 1, DS:A77A = 0 (the window's held arrow), seg5:6CE8 (the computer's state
    // machine cleared) and seg2:2D8D (the portrait repainted). No random draw, no message;
    // the turn goes on where it is for the new human. False when not allowed.
    bool takeOverComputer();
    // Card field (kind 5) after arriving: buy one offered card type, sell a hand card.
    bool atCardField() const;
    int handCount(int player) const; // seg2:3C00
    std::vector<CardOffer> cardOffers() const;
    bool buyCard(int type);
    bool sellCard(int handSlot);
    // DS:3FC6 town dialog mode set by property cards 1-8 (card type + 1), 0xFF = none.
    int propertyMode() const { return propertyMode_; }
    // The original canvas width (640, 800 or 1024, seg5:1227) whose map window
    // decides the area Tschüss and Flugzeug avoid (seg2:46C8, see carryView). A
    // session setting like the resolution in rally.exe: not saved, kept by newGame
    // and loading; the widescreen extension does not change it. Default 1024.
    bool setMapCanvas(int canvasWidth);
    int mapCanvas() const { return mapCanvas_; }
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
    // Counts seg2:9C97 placements (stage start, Konferenz): the map puts Dr. Drago's
    // own vehicle beside the holder anew (presentation only, not saved).
    int dragoPlacements() const { return dragoPlacements_; }
    // Counts the season changes of the event trigger (seg8:1624 -> seg5:186A, not saved):
    // the window runs the map wipe seg8:0E20 there (src/season_wipe.hpp).
    int seasonChanges() const { return seasonChanges_; }
    // [Setup] Colorcycle ("Wellen und Jahreszeiten", DS:B5DC) as a rule input (saved,
    // default on): seg5:186A reads it at every call. On, the tile directory follows the
    // month (seg5:1956-1986) and the event trigger seg8:1624 skips the event at a new
    // season; off, the directory is always 1 (seg5:1929-1953), so the trigger never skips.
    // Call when the setting changes and at a game start or load: it is what OPTION's
    // close does (seg1:B539 -> seg5:186A(DS:A100, 0), toggle at seg1:B43C), a tile reload
    // to the directory of the setting without the wipe, an event or a random draw.
    void setColorcycle(bool on);
    bool colorcycle() const { return colorcycle_; }
    // DS:2CA, the tile directory map/0..3 loaded by seg5:186A (the map shows it).
    int loadedSeason() const { return loadedSeason_; }
    // A form change (seg2:B3A4 -> 9804): its count and the start side random(3) that
    // seg2:454E draws for the search of the start field outside the view.
    int dragoFormChanges() const { return dragoFormChanges_; }
    int dragoStartSide() const { return dragoStartSide_; }
    // Hau ab (seg2:B4E3): its count and the old holder's field, where the route starts.
    int dragoHandOvers() const { return dragoHandOvers_; }
    int dragoHandOverFrom() const { return dragoHandOverFrom_; }
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
    int balanceClosing() const { return balanceClosing_; } // 4 or 5: pos_e / pos_f (seg8:9616)
    // The show's commands (round 144, docs/formats.md "Year balance"). rally.exe draws
    // rand() % 4 when the show master goes on to the next player (seg8:8E38) and
    // rand() % 2 at the closing lever (seg8:9605); ending the show earlier (Escape,
    // Enter, a click) skips them. With setBalanceWindow the player change waits in
    // Phase::Balance until balanceFinish; without a window it draws them all at once.
    void setBalanceWindow(bool on) { balanceWindow_ = on; }
    bool balanceAnnounce();   // the next entry's variant; false when all are announced
    // The show master has spoken the next announced entry's pos_ / neg_ (seg8:9075..9153):
    // only then the letter [bp-0x7E] advances (seg8:913D), which the closing lever's
    // rule seg8:95E5..9613 reads. False when no announced entry is left to speak.
    bool balanceSpeak();
    bool balanceClose();      // the closing letter; false when already drawn
    bool balanceFinish();     // the show has ended: the player change goes on
    size_t balanceAnnounced() const { return balanceAnnounced_; }
    size_t balanceSpoken() const { return balanceSpoken_; }
    // The statement STAT (seg1:972F, round R1): seg2:07E1 opens it after the settlement of
    // months 0..10 (not after December's, whose year end shows the balance) with the new
    // player, seg2:12A3 after the Master-Rallye reward's message, before the victory. It is
    // modal and draws nothing. With setStatementWindow the player change waits in
    // Phase::Statement until statementFinish (then seg1:8CBA clears +0x52 / +0x4A); after the
    // Master-Rallye the game is over once it has closed. Without a window nothing waits.
    void setStatementWindow(bool on) { statementWindow_ = on; }
    bool statementFinish();
    int statementSerial() const { return statementSerial_; } // statements opened (not saved)
    int statementPlayer() const { return statementPlayer_; } // DS:94E2 while it is open
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
    void returnEffects();        // seg2:0CE2-0D35, before the arrival test
    void arrive();
    void advanceTurn();          // nextPlayer plus month/year bookkeeping
    std::vector<int> activePlayers() const;
    // game_drago.cpp
    void dragoAct(int player);   // seg2:B58C after field actions and sit-outs
    void placeDragoForStage(int reached); // seg2:14B1: farthest from `reached` gets Dr. Drago
    // seg2:04AD(player, field): the player is put on the field, the start facing is set
    // (seg2:5067) and Dr. Drago is placed anew when the player carries him (seg2:058C / 0597).
    void setPosition(int player, int field);
    // seg2:5CDE + seg2:5FCA / 9C97 before the last stage (round R3a).
    void startMasterRallye();
    bool playWeihnachten();      // seg3:1156
    void playHauAb();            // seg3:3EFB
    void playPunch();            // seg3:2D17 (game_cards.cpp)
    void playTschuess(int target); // seg3:28ED
    int carryAway(int player);     // seg3:28ED: the player is set down out of view
    void discard(int player, int handSlot); // seg2:3C62: back to the deck
    void giveYellowCard();                  // seg2:1127: draw, text random(13), Dr. Drago
    int spin(int reels, int maximum);       // seg2:6426 / 776A: sum of the reels
    void pushNotice(Notice n) { n.seq = nextNotice_++; notices_.push_back(std::move(n)); }
    void notice(int text, std::vector<NoticeArg> args = {}) { pushNotice({text, std::move(args)}); }
    void sound(int channel, int index, int loops = 0) { sounds_.push_back({channel, index, loops, nextNotice_}); }
    // seg2:2676: the moving vehicle reaches `field`; the first other driver in play
    // standing there draws random(1), on 0 the horn Hupe of the mover sounds.
    void honk(int field);
    static NoticeArg playerArg(int p) { NoticeArg a; a.kind = NoticeArg::Kind::Player; a.player = p; return a; }
    static NoticeArg moneyArg(double m) { NoticeArg a; a.kind = NoticeArg::Kind::Money; a.money = m; return a; }
    NoticeArg townArg(int town) const;
    // A notice whose message centres the zoomed view on a map cell (event places).
    void eventNotice(int text, std::vector<NoticeArg> args, int cellX, int cellY, int variant = 0) {
        Notice n{text, std::move(args)};
        n.focusX = cellX;
        n.focusY = cellY;
        n.variant = variant;
        pushNotice(std::move(n));
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
    int tileDirectory() const;   // seg5:186A: table[month % 12] with Colorcycle, else 1
    void applyEvent(EventKind kind);
    void placeObstacle(uint16_t type, int durationFactor);   // rain / snow (seg8:5F43)
    void damageTown(int town, int protectionCard);          // seg8:4F1B
    int findCard(int player, int type) const;               // seg3:0D26, slot or -1
    uint16_t routeMask() const { return eastOpen_ ? 0 : 0x4000; }
    AiState aiState(bool steerless) const;
    void planComputerRoute(); // state 5 for the move just started (game_ai.cpp)

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
    bool aiRoutePlanned_ = false;           // state 5 (seg5:72C2) has run for this move
    bool yellowPending_ = false;            // yellow field waits for a discard
    ObstacleQuestion question_;             // transient: not saved, asked again after loading
    bool obstacleConfirmed_ = false;
    RisikoState risiko_;                    // DS:9C92 light, level, tick counter, DS:A778 steps
    bool demoEnd_ = false;                  // DS:16
    bool demo_ = false;                     // DS:14 (not saved)
    int mapCanvas_ = 1024;                  // canvas width of the map view (session setting, not saved)
    bool computerExtraCards_ = false;       // recomp extension: computers also play the cards rally.exe never picks
    std::array<int, 8> airports_{-1, -1, -1, -1, -1, -1, -1, -1};
    Edition edition_ = Edition::GermanRetail; // installation data, written into saves
    bool forcedSale_ = false;     // the statistics in sale mode 0x10 are open
    bool timeLimitOn_ = false;    // DS:B5D6
    bool timeLimitActive_ = false; // DS:12 (not saved: a turn-local timer)
    int timeLimitLeft_ = 0;       // DS:9470: seconds
    int timeLimitOver_ = 0;       // DS:8C02: seconds fined so far
    std::vector<int> lastReels_;            // DS:9584 (presentation only)
    std::string rollUndo_;                  // the state before a human's roll (not saved)
    int pendingAllowance_ = -1;             // a spin waiting for its window (not saved)
    bool spinPresentation_ = false;         // a slot window shows the spins
    bool arrivalConfirm_ = false;           // a human accepts the field with Return
    bool investmentWindow_ = false;         // a window answers the offers, also a computer's
    int dragoPlacements_ = 0;               // seg2:9C97 calls, see dragoPlacements()
    int dragoFormChanges_ = 0, dragoStartSide_ = 0;   // presentation only, not saved
    int dragoHandOvers_ = 0, dragoHandOverFrom_ = -1;
    void moveAfterSpin(int allowance);      // startMove now or after the window
    void spinAfterCard();                   // WM_COMMAND 0x2710 of a card (seg4:0A27)
    int spinSerial_ = 0;
    std::vector<Notice> notices_;
    int nextNotice_ = 0; // sequence of the next notice (presentation, not saved)
    std::vector<SoundCue> sounds_;
    std::vector<FilmCue> films_;
    int risikoPresses_ = 0, risikoPressLight_ = 0; // not saved: presentation only
    std::array<std::string, kPlayerSlots> names_; // presentation only, not saved
    std::array<int, 8> lastDirection_{2, 2, 2, 2, 2, 2, 2, 2}; // vehicle facing (frame byte +2)
    std::array<int, 8> jumps_{};   // positionJumps (presentation only, not saved)
    bool eastOpen_ = false;        // DS:A10F == 0
    double westValue_ = 0;         // DS:A113
    int loadedSeason_ = 0;         // DS:2CA, season of the loaded SBB set (seg5:186A)
    bool colorcycle_ = true;       // DS:B5DC, [Setup] Colorcycle
    int seasonChanges_ = 0;        // season reloads of the event trigger (not saved)
    Deck deck_;
    std::vector<Hand> hands_;
    Calendar calendar_;
    MoveCounter counter_{0};
    Phase phase_ = Phase::Roll;
    int current_ = 0;
    int origin_ = -1, destination_ = -1;
    int remaining_ = 0;
    int totalStages_ = 0;
    int balanceSerial_ = 0, balanceChoice_ = 0, balanceIntro_ = 0, balanceClosing_ = 4;
    bool balanceWindow_ = false, balanceClosed_ = false;
    size_t balanceAnnounced_ = 0, balanceSpoken_ = 0;
    TurnChange balanceTurn_;                // the suspended player change
    TurnHooks turnHooks();
    void completeTurn(const TurnChange& change);
    // seg2:0407, the turn start of the current player: the rule parts (seg5:6CE8, the
    // hand order seg2:3CF1 at seg2:0473, Dr. Drago's turn start seg2:B3A4 at seg2:049F).
    void beginTurn();
    // game_statement.cpp: a change waiting for the balance or the statement; the texts and
    // sit-outs found so far are announced before the window (their messages are modal).
    void suspendTurn(TurnChange change);
    void announceSitOut(const SitOutText& t); // seg2:096F / 0A0D / C07A
    void announceChange(const TurnChange& change);
    bool openStatement(int player, bool master);
    bool statementWindow_ = false, statementMaster_ = false;
    int statementSerial_ = 0, statementPlayer_ = -1;
    int announceSerial_ = 0, announceMode_ = 2, announceVariant_ = 3;
    std::vector<BalanceEntry> balance_;
    int finishSerial_ = 0, finishField_ = -1, finishPlayer_ = -1;
    std::string finishMusic_ = "z1"; // cosmetic; reuses the existing draw, no new RNG call
    std::vector<std::string> music_; // transient presentation commands, never saved
    std::vector<std::string> messages_;
};

} // namespace dtvr
