// SPDX-License-Identifier: GPL-3.0-only
// The game window as in rally.exe (round 21): canvas with the desk background,
// the map window, the driver portrait and the player info panel. All art is
// loaded at run time from the user's installation (docs/formats.md "Game window").
#pragma once

#include "layout.hpp"
#include "map_scroll.hpp"
#include "event_scenes.hpp"
#include "mouse_control_view.hpp"
#include "film_view.hpp"
#include "vehicle_air.hpp"
#include "vehicle_anim.hpp"
#include "drago_vehicle.hpp"
#include "presentation_clock.hpp"
#include "sight_view.hpp"
#include "play_view.hpp"
#include "sdl_assets.hpp"
#include "season_wipe.hpp"
#include "card_view.hpp"
#include "card_shop_view.hpp"
#include "select_player_view.hpp"
#include "flight_view.hpp"
#include "nails_view.hpp"
#include "invest_view.hpp"
#include "risiko_view.hpp"
#include "estates_view.hpp"
#include "finish_view.hpp"
#include "victory_view.hpp"
#include "balance_view.hpp"
#include "stage_view.hpp"
#include "subtitle_view.hpp"
#include "intro_view.hpp"
#include "ambient.hpp"
#include "save_view.hpp"
#include "stat_view.hpp"
#include "options_view.hpp"
#include "overview_view.hpp"
#include "setup_view.hpp"
#include "stages_view.hpp"
#include "window_border.hpp"
#include "menu_view.hpp"
#include "message_view.hpp"
#include "slot_view.hpp"
#include "sound.hpp"
#include "text.hpp"

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

class GameScreen {
public:
    GameScreen(SDL_Renderer* renderer, const std::filesystem::path& root, const ScreenLayout& layout,
               const Board& board, const std::vector<Town>& towns, const Game& game,
               std::map<int, std::string> texts, char language);
    void draw();
    // Title (seg8:03AB): window TITEL over the canvas with butt/<n>/<lang>/t.bmp
    // (DS:17FB); the menu MAIN is drawn above it.
    void setTitle(bool title) { title_ = title; }
    // Credits (1026 -> seg6:0A4D(.., "Credits.bmp", 1)): the picture butt/<n>/Credits.bmp
    // in a modal window PICT over the canvas (English: butt/<n>/<letter>/credits.bmp,
    // see creditsPicture in edition.hpp).
    void showPicture(const std::string& file) { picture_ = file; }
    char languageLetter() const { return language_; } // DS:A0F0 as stored
    bool pictureShown() const { return !picture_.empty(); }
    void closePicture() { picture_.clear(); }
    bool title() const { return title_; }
    // The vehicle sets of the drivers in play (after loading a game).
    void loadDriverSprites();
    // A new or loaded game starts from a freshly read map/europe.map (user decision
    // 2026-11-26, both editions): the US retail rally.exe reloads it there (R seg5:182F
    // from seg5:186A(.., 1) and the loader R seg8:AF67), the German one reads it once per
    // run (D seg5:1559), so tiles changed by the previous game would stay. The next
    // drawMap rebuilds the world bitmap and the ambient animations' map from the file.
    void reloadMap() {
        worldSeason_ = -1;
        seasonWipe_.stop();
        seasonChangesSeen_ = game_.seasonChanges();
    }
    MessageView& messages() { return messages_; }
    MenuView& menu() { return menu_; }
    MouseControlView& mouseControl() { return mouseControl_; }
    FilmView& film() { return film_; }
    SightView& sight() { return sight_; }
    // seg2:1DE2: with [Setup] Views the human's vehicle stops at a view point cell
    // while no direction key is held (DS:A77A = 0); a held key drives on.
    void setArrowHeld(bool held) { arrowHeld_ = held; }
    bool viewStopped() const { return viewStop_; }
    SDL_Point viewStopCell() const { return viewCell_; }
    const std::filesystem::path& root() const { return root_; }
    const ScreenLayout& layout() const { return layout_; }
    CardView& cards() { return cards_; }
    CardShopView& cardShop() { return cardShop_; }
    SelectPlayerView& selectPlayer() { return selectPlayer_; }
    FlightView& flight() { return flight_; }
    NailsView& nails() { return nails_; }
    InvestView& invest() { return invest_; }
    RisikoView& risiko() { return risiko_; }
    EstatesView& estates() { return estates_; }
    SaveView& saveDialog() { return save_; }
    StatView& statement() { return stat_; } // STAT (seg1:972F)
    SetupView& setup() { return setup_; }
    StagesView& stages() { return stages_; }
    OverviewView& overview() { return overview_; }
    FinishView& finish() { return finish_; }
    VictoryView& victory() { return victory_; }
    BalanceView& balance() { return balance_; }
    StageView& announcement() { return stage_; }
    bool startAnnouncement(int mode, int variant, Uint32 now);
    bool startBalance(Uint32 now, BalanceView::Commands commands = {});
    bool startVictory(Uint32 now);
    // seg5:0174 -> seg7:0000 before the title with [Setup] ShowIntro.
    bool startIntro(MsRandom& random, Uint32 now); // `random`: the session's generator
    IntroView& intro() { return intro_; }
    // Starts the finish sequence for a destination field and driver.
    bool startFinish(int field, int player, Uint32 now);
    OptionsView& options() { return options_; }
    const Settings& settings() const { return settings_; }
    TextRenderer& textRenderer() { return text_; }
    // Stores the options (seg1:ACE7 star) and applies them (Views: tile 0x33 on the map).
    void applySettings(const Settings& settings);
    // Sounds (round 40): the screen plays the slot machine's; may be null.
    void setSound(SoundPlayer* sound) { sound_ = sound; }
    const SaveSlots& saveSlots() const { return slots_; }
    const std::map<int, std::string>& texts() const { return texts_; }
    std::vector<EstateRow> estateRows() const;
    // Top-left world pixel shown in the viewport (follows the current player).
    SDL_Point viewOrigin() const;
    // Animation tempo (recomp, [Extensions] AnimationTempo): the presentation time of
    // event animations and the vehicles' drive; call once per frame with real time.
    // `paused` (the recomp menu) stops it.
    void tickPresentation(Uint32 realNow, bool paused);
    Uint32 presentationNow() const { return clock_.now(); }
    // Event animations (Nessie, Erdbeben ...) and the vehicles' frames: AnimationTempo
    // only, so a computer's turn under Computerzüge shows them at full length.
    Uint32 sceneNow() const { return animClock_.now(); }
    // The shared layout changed size (widescreen switch): keep the map origin valid.
    void layoutChanged();
    // Smooth driving (round 31): the drawn vehicle runs after the game position at
    // 4 pixels per 20 ms tick (seg2:1CB0: DS:19EA / 19F4 = 2, shifted by DS:34 = 1).
    // `now` is presentation time.
    void advanceVehicles(Uint32 now);
    // seg2:3216: centre the map on the current player (Space, zoom, turn start, spin).
    void centreOnPlayer();
    // Event places (seg8 handlers): zoom in (seg6:2E54(2)), centre on the cell and
    // restore zoom and origin afterwards (seg8:1E17 ff.). `focus` < 0 ends it.
    void eventFocus(int cellX, int cellY);
    // Plays an event animation at a map cell (seg8:6E0F); the message waits for it.
    void startEventScene(const EventScene& scene, int cellX, int cellY, int corner, Uint32 now);
    bool eventSceneActive(Uint32 now) const;
    // The blind wipe of a season change (seg8:1624 -> seg8:0E20 mode 0) runs; it keeps
    // the game waiting like the synchronous original (src/season_wipe.hpp). Starts it
    // when the game has changed the season since the last call.
    bool seasonWipeActive() {
        updateSeasonWipe(SDL_GetTicks());
        return seasonWipe_.active(SDL_GetTicks());
    }
    // The slot machine is on the map (sliding in, turning, or sliding out; modal in rally.exe).
    bool slotActive(Uint32 now) const { return slot_.active(now); }
    // The slot window of spin `serial` has been shown and has closed again.
    bool slotDone(int serial, Uint32 now) const { return seenSpin_ == serial && !slot_.active(now); }
    // Enter / Space in the slot window; the lever pull and the stop of the turning reels
    // play sound 2 on channel 1 (seg2:6A97, seg2:6AF7).
    bool slotKey(Uint32 now) {
        const int r = slot_.key(now);
        if ((r == 1 || r == 3) && sound_) sound_->play(1, 2);
        return r != 0;
    }
    // seg2:69AD for a human's Würfeln: the window closes at once with sound 7 on channel
    // 1 and the reels' sound on channel 3 stops. Returns false when not possible; else
    // `reelsDrawn` tells whether the lever had already set the reels turning.
    bool slotCancel(Uint32 now, bool& reelsDrawn) {
        if (!slot_.cancellable(now)) return false;
        reelsDrawn = slot_.running(now);
        slot_.close();
        reelsPending_ = reelsSounding_ = false;
        if (sound_) {
            sound_->play(1, 7);
            sound_->stop(3);
        }
        return true;
    }
    bool vehicleMoving(int player) const;
    // DS:9CFE: Dr. Drago travels (form change, Hau ab); rally.exe waits (seg2:B454 / B55E).
    bool dragoTravelling() const { return dragoVehicle_.travelling(); }
    // Umfeld (seg6:3C94, round 37): the view leaves the player; a left click in a
    // 60-pixel edge strip (seg6:1798) or an arrow key moves it by a third of the view;
    // the right button (seg6:176E), Enter, Escape or Space end it. While it is open it
    // takes every key and mouse event (focus and capture). Returns true when it did.
    void startSurroundings();
    // Zoom (seg6:2E54(3), round 38): DS:9C70 toggles between 0 and 1 and is kept per
    // player (player +0x15); at 1 the viewport shows half the world area, twice as large.
    void toggleZoom();
    void setZoom(int player, bool zoom) { if (player >= 0 && player < kPlayerSlots) zoom_[size_t(player)] = zoom; }
    int zoomFactor() const;
    bool surroundings() const { return surroundings_; }
    bool handleSurroundings(const SDL_Event& event);
    // The SBS frames by name for views outside the screen (the recomp on-screen keyboard).
    SpriteLookup spriteLookup() { return [this](const std::string& name) { return sprite(name); }; }

private:
    SDL_Texture* art(const std::filesystem::path& relative); // cached, nullptr if missing
    SDL_Texture* sprite(const std::string& name);            // SBS frame by name (any loaded file)
    // seg3:2517: the 0x60 x 0x60 world pixels from (player - 0x28) for the Nägel compass.
    SDL_Texture* compassTexture();
    Texture compass_{nullptr, SDL_DestroyTexture};
    // [Setup] Animations: the ambient map animations (seg8:83A0 from seg2:1C37), on a
    // copy of the map with their own random numbers; cells are redrawn into world_.
    void tickAmbient(bool views);
    std::vector<Tile> tiles_;
    std::unique_ptr<WorldMap> ambientMap_;
    AmbientAnimations ambient_;
    MsRandom ambientRng_{1};
    Uint32 ambientLast_ = 0;
    std::array<int, 4> ambientView_{-1, -1, -1, -1}; // origin (tiles) and size of the listed view
    using SpriteSet = std::map<std::string, Texture>;
    // `transparent`: the colour key (0 for SBS frames; seg2:9497 callers pass their own).
    void loadSprites(const std::filesystem::path& relative, SpriteSet& into, int transparent = 0);
    void loadSprites(const std::filesystem::path& relative) { loadSprites(relative, sprites_); }
    static SDL_Texture* find(const SpriteSet& set, const std::string& name);
    void drawMap();
    void drawPortrait();
    void drawHandStack(); // seg3:0EE8: the hand under the portrait while the card window is closed
    SDL_Rect portraitWindow_{}; // the PORTRAIT window of the last drawPortrait
    void drawPlayerInfo();
    void drawBackground(SDL_Texture* background); // butt/<n>/bk.bmp, stretched between its edges when wide

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    char language_; // the lan.inf letter DS:A0F0 (seg5:028C), for language paths
    const ScreenLayout& layout_; // shared; the widescreen switch changes it
    const Board& board_;
    const std::vector<Town>& towns_;
    const Game& game_;
    std::map<int, std::string> texts_;   // language file, UTF-8
    TextRenderer text_;
    std::map<std::string, Texture> sprites_;
    SpriteSet buySprites_;   // sbs/buy.sbs of the town dialog (seg1:4C45)
    SpriteSet dragoSprites_; // sbs/F_BIKE / F_CAR / F_BLIMP.sbs (seg2:9BAE, DS:3DB6 ff.)
    SpriteSet buyKeyed_;     // the same with colour key 0xFF: PFEIL1 / PFEIL2 (seg1:7540)
    SpriteSet lsSprites_;    // sbs/ls.sbs of the save dialog (seg8:B1CE)
    std::map<std::string, Texture> art_;
    Surface world_{nullptr, SDL_FreeSurface};      // 8-bit world, its palette cycles
    int worldSeason_ = -1;
    SeasonWipe seasonWipe_;    // real time: the original wipe has no timer
    int seasonChangesSeen_ = 0; // Game::seasonChanges() already wiped
    void updateSeasonWipe(Uint32 realNow);
    std::array<SDL_Color, 10> cycleBase_{};        // palette entries 0x90..0x99 as loaded
    int cyclePhase_ = 0;
    Surface viewRgb_{nullptr, SDL_FreeSurface};    // the visible world area, converted
    Texture viewTexture_{nullptr, SDL_DestroyTexture};
    void cyclePalette(bool on);
    SlotView slot_;
    MessageView messages_;
    MenuView menu_;
    MouseControlView mouseControl_;
    FilmView film_;
    SightView sight_;
    bool arrowHeld_ = false, viewStop_ = false;
    SDL_Point viewCell_{-1, -1}, viewPassed_{-1, -1};
    CardView cards_;
    CardShopView cardShop_;
    SelectPlayerView selectPlayer_;
    FlightView flight_;
    NailsView nails_;
    InvestView invest_;
    RisikoView risiko_;
    EstatesView estates_;
    SaveSlots slots_;
    SaveView save_;
    StatView stat_;
    SetupView setup_;
    StagesView stages_;
    OverviewView overview_;
    Settings settings_;
    OptionsView options_;
    FinishView finish_;
    VictoryView victory_;
    BalanceView balance_;
    StageView stage_;
    SubtitleView subtitles_; // after texts_ and text_
    IntroView intro_;
    void prepareSubtitles();
    int seenSpin_ = 0;
    SoundPlayer* sound_ = nullptr;
    bool reelsSounding_ = false;
    bool reelsPending_ = false;
    bool title_ = false;
    std::string picture_;
    bool surroundings_ = false;
    std::array<bool, kPlayerSlots> zoom_{};         // player +0x15 (not saved)
    SDL_Point surroundOrigin_{};                    // DS:956A / 9568 in tiles while in Umfeld
    std::array<SDL_Point, kPlayerSlots> shown_{};   // drawn world pixel of each vehicle
    std::array<int, kPlayerSlots> shownField_{};    // field the drawn position belongs to
    std::array<int, kPlayerSlots> shownJumps_{};    // Game::positionJumps already shown
    std::array<VehicleAir, kPlayerSlots> air_{};    // plane state +0x2D / height +0x2E
    std::array<VehicleAnimation, kPlayerSlots> anim_{}; // animation +0x20 and its frame +0x2C
    std::array<bool, kPlayerSlots> airSet_{};
    std::array<bool, kPlayerSlots> stepPending_{};  // a step began, its first tick is still due
    int labelField_ = -1; // DS:B632: the field the drive last reached, -1 after leaving one
    // Dr. Drago's own vehicle (seg2:9F61): its state, the holder and placement seen,
    // and the frame passes on the animation clock.
    DragoVehicle dragoVehicle_;
    int dragoHolderSeen_ = -1, dragoPlacementsSeen_ = -1, dragoFormChangesSeen_ = 0, dragoHandOversSeen_ = 0;
    uint32_t dragoAnimPass_ = 0;
    bool dragoAnimStarted_ = false;
    void advanceDrago(int ticks);
    void drawDrago(const SDL_Rect& target, SDL_Point origin, int z);
    Uint32 lastAdvance_ = 0;
    bool advanced_ = false;                         // lastAdvance_ is set
    PresentationClock clock_;
    // The vehicles' frame animation: AnimationTempo only, so a faster computer turn
    // drives faster without its wheels spinning faster (round 137).
    PresentationClock animClock_;
    MapViewSize viewSize() const;
    TilePoint origin_{};          // DS:956A / 9568 outside Umfeld
    bool originSet_ = false;
    int seenTurn_ = -1, seenCentreSpin_ = -1;
    struct EventFocus { bool active = false, zoomed = false; TilePoint saved{}; bool savedSet = false; int x = -1, y = -1; };
    EventFocus focus_;
    const EventScene* scene_ = nullptr;
    std::vector<SceneStep> sceneSteps_;
    size_t sceneCamera_ = 0; // steps whose recentring has been applied
    Uint32 sceneStart_ = 0;
    std::map<std::string, std::map<std::string, Texture>> sceneFrames_; // per SBS file
    Uint32 carry_ = 0;                              // milliseconds not yet spent on a tick
};

struct GameWindowOptions {
    Edition edition = Edition::GermanRetail; // the detected profile (src/edition.hpp)
    int canvasWidth = 0;          // 0: the largest canvas that fits the screen (seg5:1227)
    bool fullscreen = false;      // start in fullscreen (also [Display] Fullscreen)
    int widescreen = -1;          // -1: [Display] Widescreen, 0 off, 1 on (recomp extension)
    int aspectW = 0, aspectH = 0; // widescreen aspect; 0: the display's
    int recompMenuPage = -1;      // open the recomp menu on this page at start
    int windowW = 0, windowH = 0; // initial window size (tests of the scaled output)
    std::string pressKeys;        // --press: scripted keys for smoke tests
    std::string event;            // --event NAME: apply this event at the start (tests)
    int drago = -1;               // --drago FORM: Dr. Drago joins at the start (tests)
    bool smokeTest = false;       // draw `frames` frames and exit
    int frames = 1;
    bool rollAtStart = false;     // spin at once (tests of the slot machine)
    std::string openMenu;         // tests: open this menu resource at the start (e.g. RIGHTBTN)
    bool openCards = false;       // tests: open the card window at the start
    bool holdRight = false;       // tests: the right arrow key is held from the start
    bool openSave = false;
    bool openStatistics = false;  // tests: open Statistik at the start
    bool openSetup = false;       // tests: open the player setup at the start
    bool openStages = false;      // tests: open the stage count window at the start
    bool openOverview = false;    // tests: open Übersicht at the start
    bool openSurroundings = false; // tests: start Umfeld
    bool zoom = false;            // tests: zoom in for the current player
    bool openOptions = false;     // tests: open the options window
    bool demo = false;            // tests: start the demo (title entry Demo)
    bool finish = false;          // tests: show the finish sequence of the destination
    bool victory = false;         // tests: show the victory sequence of the current ranking
    bool balance = false;         // tests: show the year balance (without announcements)
    bool statement = false;       // tests: show STAT with the values of the started game
    bool announce = false;        // tests: show the destination announcement of the game start
    bool cardShop = false;        // tests: open the card shop
    bool selectPlayer = false;    // tests: open the target choice for the other players
    bool flight = false;          // tests: open the Flugzeug booking
    bool nails = false;           // tests: open the Nägel direction choice
    bool invest = false;          // tests: open the investment offer
    bool title = false;           // start with the title menu MAIN (seg8:0DB0)
    bool intro = false;           // play the intro before the title even with ShowIntro=0
    bool noIntro = false;         // skip the intro (seg7:0000) before the title
    bool logo = false;            // tests: keep the loading logo BB.bmp until a key or left click
    bool computerExtraCards = false; // --computer-cards: the recomp extension for this game
    int loadSlot = -1;            // load this save slot first (until the title menu exists)        // tests: open the save dialog at the start
    int openEstates = -1;         // tests: show the town dialog of this town (no stop needed)
    std::string screenshot;       // BMP of the first frame
};

// Opens the game window and runs it until it is closed; returns the exit code.
// `language`: the letter of dat/lan.inf (readLanguageLetter).
int runGameWindow(PlayView& play, const std::filesystem::path& root, char language, const Board& board,
                  const std::vector<Town>& towns, const GameWindowOptions& options);

} // namespace dtvr
