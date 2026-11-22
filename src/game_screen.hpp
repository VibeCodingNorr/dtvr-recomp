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
#include "sight_view.hpp"
#include "play_view.hpp"
#include "sdl_assets.hpp"
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
#include "save_view.hpp"
#include "options_view.hpp"
#include "overview_view.hpp"
#include "setup_view.hpp"
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
               std::map<int, std::string> texts);
    void draw();
    // Title (seg8:03AB): window TITEL over the canvas with butt/<n>/<lang>/t.bmp
    // (DS:17FB); the menu MAIN is drawn above it.
    void setTitle(bool title) { title_ = title; }
    // Credits (1026 -> seg6:0A4D(.., "Credits.bmp", 1)): the picture butt/<n>/Credits.bmp
    // in a modal window PICT over the canvas.
    void showPicture(const std::string& file) { picture_ = file; }
    bool pictureShown() const { return !picture_.empty(); }
    void closePicture() { picture_.clear(); }
    bool title() const { return title_; }
    // The vehicle sets of the drivers in play (after loading a game).
    void loadDriverSprites();
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
    SetupView& setup() { return setup_; }
    OverviewView& overview() { return overview_; }
    FinishView& finish() { return finish_; }
    VictoryView& victory() { return victory_; }
    BalanceView& balance() { return balance_; }
    StageView& announcement() { return stage_; }
    bool startAnnouncement(int mode, int variant, Uint32 now);
    bool startBalance(Uint32 now);
    bool startVictory(Uint32 now);
    // seg5:0174 -> seg7:0000 before the title with [Setup] ShowIntro.
    bool startIntro(Uint32 now);
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
    // Smooth driving (round 31): the drawn vehicle runs after the game position at
    // 4 pixels per 50 ms tick (seg2:1CB0: DS:19EA / 19F4 = 2, shifted by DS:34 = 1).
    void advanceVehicles(Uint32 now);
    // seg2:3216: centre the map on the current player (Space, zoom, turn start, spin).
    void centreOnPlayer();
    // Event places (seg8 handlers): zoom in (seg6:2E54(2)), centre on the cell and
    // restore zoom and origin afterwards (seg8:1E17 ff.). `focus` < 0 ends it.
    void eventFocus(int cellX, int cellY);
    // Plays an event animation at a map cell (seg8:6E0F); the message waits for it.
    void startEventScene(const EventScene& scene, int cellX, int cellY, int corner, Uint32 now);
    bool eventSceneActive(Uint32 now) const;
    bool vehicleMoving(int player) const;
    // Umfeld (seg6:3C94, round 37): the view leaves the player; the mouse in a 60-pixel
    // edge strip or an arrow key moves it by a third of the view; a left click,
    // Enter, Escape or Space ends it. Returns true when the event belonged to it.
    void startSurroundings();
    // Zoom (seg6:2E54(3), round 38): DS:9C70 toggles between 0 and 1 and is kept per
    // player (player +0x15); at 1 the viewport shows half the world area, twice as large.
    void toggleZoom();
    void setZoom(int player, bool zoom) { if (player >= 0 && player < kPlayerSlots) zoom_[size_t(player)] = zoom; }
    int zoomFactor() const;
    bool surroundings() const { return surroundings_; }
    bool handleSurroundings(const SDL_Event& event);

private:
    SDL_Texture* art(const std::filesystem::path& relative); // cached, nullptr if missing
    SDL_Texture* sprite(const std::string& name);            // SBS frame by name (any loaded file)
    // seg3:2517: the 0x60 x 0x60 world pixels from (player - 0x28) for the Nägel compass.
    SDL_Texture* compassTexture();
    Texture compass_{nullptr, SDL_DestroyTexture};
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
    Surface world_{nullptr, SDL_FreeSurface};      // 8-bit world, its palette cycles
    int worldSeason_ = -1;
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
    SetupView setup_;
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
    bool title_ = false;
    std::string picture_;
    bool surroundings_ = false;
    std::array<bool, kPlayerSlots> zoom_{};         // player +0x15 (not saved)
    SDL_Point surroundOrigin_{};                    // DS:956A / 9568 in tiles while in Umfeld
    std::array<SDL_Point, kPlayerSlots> shown_{};   // drawn world pixel of each vehicle
    std::array<int, kPlayerSlots> shownField_{};    // field the drawn position belongs to
    std::array<VehicleAir, kPlayerSlots> air_{};    // plane state +0x2D / height +0x2E
    std::array<bool, kPlayerSlots> airSet_{};
    std::array<bool, kPlayerSlots> stepPending_{};  // a step began, its first tick is still due
    Uint32 lastAdvance_ = 0;
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
    int canvasWidth = 0;          // 0: the largest canvas that fits the screen (seg5:1227)
    bool fullscreen = false;      // start in fullscreen (also [Display] Fullscreen)
    bool smokeTest = false;       // draw `frames` frames and exit
    int frames = 1;
    bool rollAtStart = false;     // spin at once (tests of the slot machine)
    std::string openMenu;         // tests: open this menu resource at the start (e.g. RIGHTBTN)
    bool openCards = false;       // tests: open the card window at the start
    bool holdRight = false;       // tests: the right arrow key is held from the start
    bool openSave = false;
    bool openStatistics = false;  // tests: open Statistik at the start
    bool openSetup = false;       // tests: open the player setup at the start
    bool openOverview = false;    // tests: open Übersicht at the start
    bool openSurroundings = false; // tests: start Umfeld
    bool zoom = false;            // tests: zoom in for the current player
    bool openOptions = false;     // tests: open the options window
    bool demo = false;            // tests: start the demo (title entry Demo)
    bool finish = false;          // tests: show the finish sequence of the destination
    bool victory = false;         // tests: show the victory sequence of the current ranking
    bool balance = false;         // tests: show the year balance (without announcements)
    bool announce = false;        // tests: show the destination announcement of the game start
    bool cardShop = false;        // tests: open the card shop
    bool selectPlayer = false;    // tests: open the target choice for the other players
    bool flight = false;          // tests: open the Flugzeug booking
    bool nails = false;           // tests: open the Nägel direction choice
    bool invest = false;          // tests: open the investment offer
    bool title = false;           // start with the title menu MAIN (seg8:0DB0)
    bool intro = false;           // play the intro before the title even with ShowIntro=0
    bool noIntro = false;         // skip the intro (seg7:0000) before the title
    int loadSlot = -1;            // load this save slot first (until the title menu exists)        // tests: open the save dialog at the start
    int openEstates = -1;         // tests: show the town dialog of this town (no stop needed)
    std::string screenshot;       // BMP of the first frame
};

// Opens the game window and runs it until it is closed; returns the exit code.
int runGameWindow(PlayView& play, const std::filesystem::path& root, const Board& board,
                  const std::vector<Town>& towns, const GameWindowOptions& options);

} // namespace dtvr
