// SPDX-License-Identifier: GPL-3.0-only
// The game window's flow around GameScreen (round 48, a class since round 127):
// runGameWindow (game_window.cpp) sets up window, renderer, screen and audio and
// runs a GameWindow over them. Its parts by responsibility:
//   game_window.cpp          menu commands (WM_COMMAND), title, game start, quit
//   game_window_frame.cpp    what happens every frame (notices, films, scenes, automatics)
//   game_window_input.cpp    input of overlays, menus, scenes, map and keys
//   game_window_dialogs.cpp  input of the game's dialogs (cards, towns, flight, Risiko ...)
#pragma once

#include "busy_cursor.hpp"
#include "controller.hpp"
#include "display_control.hpp"
#include "game_screen.hpp"
#include "logo_view.hpp"
#include "music_player.hpp"
#include "osk_view.hpp"
#include "recomp_menu_view.hpp"
#include "scripted_input.hpp"
#include "settings.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace dtvr {

// The resources runGameWindow owns; GameWindow keeps references to them.
struct GameWindowResources {
    PlayView& play;
    const std::vector<Town>& towns;
    const GameWindowOptions& options;
    SDL_Window* window;
    SDL_Renderer* renderer;
    // One layout shared by every view (they keep references): the widescreen switch of
    // the recomp menu assigns `layout` from the other two while the game runs.
    ScreenLayout& layout;
    const ScreenLayout& baseLayout;
    const ScreenLayout& wideLayout;
    const std::vector<uint8_t>& language; // the language file, also its menu resources
    GameScreen& screen;
    MusicPlayer& music;
    const std::vector<std::string>& musicIssues;
    const std::filesystem::path& musicConfig;
    SoundPlayer& sound;
    ControllerInput& controller;
    DisplayControl& displayControl;
    ScriptedInput& scripted;
    RecompMenuView& recompMenu;
    LogoView& logo; // the Blue Byte logo while loading (seg6:0A4D)
    BusyCursor& busyCursor; // the loading cursor, released before the intro / title (seg5:0207)
    ScreenKeyboardView& keyboard; // the on-screen keyboard for names (recomp, pad)
};

class GameWindow {
public:
    explicit GameWindow(const GameWindowResources& resources);
    GameWindow(const GameWindow&) = delete;
    GameWindow& operator=(const GameWindow&) = delete;
    // The states the command line asks for, then the loop; returns the exit code.
    int run();

private:
    // game_window.cpp
    void start();
    // The logo's minimum time (recomp, kLogoMinimumMs) or, with `hold` (--logo), until a
    // key or left click; false when the window was closed.
    bool waitLogo(bool hold);
    bool swallowLogoSkip(const SDL_Event& event); // the rest of the key / click that ended the logo
    Settings menuSettings();                 // the layout in use (--[no-]widescreen may override the setting)
    void applySettings(const Settings& s);   // saved; the OPTION window shows the same smoothing
    void report();
    void openMenu(const std::string& name);
    std::array<int, 4> turnState();
    bool turnMenuDue();
    void openTitle();
    void startGame(bool fresh = false);
    void askQuit(bool reopenMain);
    void foreignSaveNotice(const std::string& edition); // recomp: the foreign-save message
    void command(int id);
    // game_window_frame.cpp
    void updateFrame(int frame);
    bool gameWindowOpen(); // a window of rally.exe other than its menus (part of modal_)
    // The INTRO window has gone (its own end or a skip): the title follows at once.
    void finishIntro();
    // game_window_statement.cpp: the monthly statement STAT (seg1:972F)
    void updateStatement();
    bool handleStatementEvent(const SDL_Event& event); // true when STAT took the event
    // game_window_input.cpp
    void handleEvent(const SDL_Event& event);
    void handleRecompMenu(const SDL_Event& event);
    void desktopClose(); // SDL_QUIT: through the quit question (recomp)
    void handleQuestion(const SDL_Event& event); // Yes / No message (texts 600, 700)
    void handleKey(const SDL_Event& event);
    void mainWindowReturn(); // seg4:01EA -> seg2:0BC3 (keyboard, pad, MCTRL)
    // game_window_dialogs.cpp: true when a game dialog took the event
    bool handleGameDialog(const SDL_Event& event);
    // The investment offer (seg8:6FBF): opening for a loaded pending offer and the
    // computer's timeout (seg6:0FB4) each frame; finishInvest applies an answer.
    void updateInvest();
    void finishInvest(const InvestView::Result& r);
    bool handleCardWindow(const SDL_Event& event);
    bool handleEstates(const SDL_Event& event);
    void openEstateCards(); // the town dialog's Karte (also the pad's Y)
    // game_window_pad.cpp: the game controllers' tick and the recomp pad buttons
    void updatePad();
    PadContext padContext();
    // The on-screen keyboard over PLAYER (recomp): open on the focused name field, input
    // while it is open (true: taken), close with or without the name.
    void openKeyboard();
    bool handleKeyboard(const SDL_Event& event);
    void closeKeyboard(ScreenKeyboardView::Result result);

    PlayView& play_;
    const std::vector<Town>& towns_;
    const GameWindowOptions& options_;
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    ScreenLayout& layout_;
    const ScreenLayout& baseLayout_;
    const ScreenLayout& wideLayout_;
    const std::vector<uint8_t>& language_;
    GameScreen& screen_;
    MusicPlayer& music_;
    const std::vector<std::string>& musicIssues_;
    const std::filesystem::path& musicConfig_;
    SoundPlayer& sound_;
    ControllerInput& controller_;
    DisplayControl& displayControl_;
    ScriptedInput& scripted_;
    RecompMenuView& recompMenu_;
    LogoView& logo_;
    BusyCursor& busyCursor_;
    ScreenKeyboardView& keyboard_;
    MenuView& menu_;
    CardView& cards_;

    int musicEnabled_;
    std::array<int, 4> cardWindowState_{-2, 0, 0, 0}; // turn state when the card window opened
    bool running_ = true;
    int autosaved_ = -1; // turn serial of the last autosave
    std::vector<SoundCue> pendingSounds_;
    int noticesTaken_ = 0; // one past the highest notice sequence taken from the game
    int eventMusicNotice_ = -1; // the notice whose closing ends the event music, -1 none
    std::vector<FilmCue> pendingFilms_;
    int finishSeen_, balanceSeen_, announceSeen_, risikoPressSeen_; // serials already shown
    bool victoryShown_ = false;
    bool balanceShown_ = false; // a year balance's show was started for Game::Phase::Balance
    int statementSeen_;         // Game::statementSerial already shown
    bool statementShown_ = false; // STAT was opened for Game::Phase::Statement
    Uint32 forcedCloseAt_ = 0;  // seg1:589C: when the settled forced sale posts its two VK_BACK
    int shopSerial_ = -1;
    int targetCardSlot_ = -1; // hand slot waiting for the target choice (-2: Mitspieler)
    int flightCardSlot_ = -1; // hand slot of the Flugzeug card being booked
    int nailsCardSlot_ = -1;  // hand slot of the Nägel card waiting for a direction
    Uint32 messageShown_ = 0; // when the open message appeared (games of computers)
    Uint32 timeLimitTick_ = 0; // the last second of the time limit timer
    // seg4:0EDF: text 700 with Yes / No (buttons 4); Yes ends the program (DS:88 = 1,
    // WM_CLOSE). From MAIN1 (1064) a No opens MAIN1 again.
    bool quitReopensMain_ = false;
    bool quitAsked_ = false; // text 700 was pushed and not answered yet
    // The recomp menu hid for the quit question of its Pause page and opens again where
    // it was when that question is answered No (CommandPlan::returnAfter).
    bool recompReturn_ = false;
    int sceneSerial_ = -1; // message serial whose event scene has been started
    // A new game between PLAYER and TEMP (seg4:0DA9): the setup's slots and names.
    std::array<uint8_t, kPlayerSlots> pendingStatus_{};
    std::array<std::string, kPlayerSlots> pendingNames_;
    bool cardsFromEstates_ = false; // the card window was opened by the town dialog's Karte (mask 1)
    bool introShown_ = false;
    SDL_Keycode logoSkipKey_ = SDLK_UNKNOWN; // ended the logo; its repeats and release are dropped
    bool logoSkipButton_ = false;            // a left click ended the logo; its release is dropped
    SDL_Keycode heldArrow_ = SDLK_UNKNOWN; // seg4:01F2: DS:A77A held
    bool modal_ = false, moving_ = false;  // computed each frame, used by the input
};

} // namespace dtvr
