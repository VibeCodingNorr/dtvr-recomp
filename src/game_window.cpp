// SPDX-License-Identifier: GPL-3.0-only
// The game window's flow (round 48, split from game_screen.cpp; a class since round
// 127, see game_window.hpp): set-up, title, menu commands and the loop.
#include "game_window.hpp"

#include "board.hpp"
#include "music_menu.hpp"
#include "ne_strings.hpp"
#include "panel.hpp"

#include <ctime>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace dtvr {

int runGameWindow(PlayView& play, const fs::path& root, char languageLetter, const Board& board, const std::vector<Town>& towns,
                  const GameWindowOptions& options) {
    SDL_DisplayMode display{};
    if (SDL_GetDesktopDisplayMode(0, &display) != 0) display.w = 1024, display.h = 768;
    const Settings displaySettings = loadSettings(defaultSettingsFile());
    // The canvas keeps the original size; SDL scales it into the window or the
    // desktop (logical size, black borders) and maps mouse coordinates back. The
    // optional widescreen extension widens it to the display's aspect.
    const bool widescreen = options.widescreen < 0 ? displaySettings.widescreen : options.widescreen != 0;
    const ScreenLayout baseLayout = layoutForScreen(editionProfile(options.edition), display.w, display.h, options.canvasWidth);
    const ScreenLayout wideLayout = widenLayout(baseLayout, widescreenExtra(baseLayout, options.aspectW ? options.aspectW : display.w,
                                                                            options.aspectH ? options.aspectH : display.h));
    // One layout shared by every view (they keep references): the widescreen switch of
    // the recomp menu assigns it while the game runs.
    ScreenLayout layout = widescreen ? wideLayout : baseLayout;
    const bool startFullscreen = options.fullscreen || (displaySettings.fullscreen && !options.smokeTest);
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("DTVR Recomp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, layout.width,
                         layout.height, SDL_WINDOW_RESIZABLE | (startFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0)),
        SDL_DestroyWindow);
    if (!window) throw std::runtime_error(SDL_GetError());
    if (options.windowW > 0 && options.windowH > 0) SDL_SetWindowSize(window.get(), options.windowW, options.windowH);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
        SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE), SDL_DestroyRenderer);
    if (!renderer) throw std::runtime_error(SDL_GetError());
    if (SDL_RenderSetLogicalSize(renderer.get(), layout.width, layout.height) != 0
        || SDL_RenderSetIntegerScale(renderer.get(), displaySettings.integerScale && layout.extra() == 0 ? SDL_TRUE : SDL_FALSE) != 0)
        throw std::runtime_error(SDL_GetError());
    BusyCursor busyCursor; // seg5:001B: the busy cursor until seg5:0207 (GameWindow::start)
    // seg5:0151 -> seg6:0A4D("BB.bmp", 0): the Blue Byte logo over the canvas while the
    // rest loads (language file, art, sounds, the intro's files).
    LogoView logo(renderer.get(), root, layout);
    if (logo.show()) logo.present();
    ControllerInput controller; // joystick 1 of the original (seg2:8A30), any SDL game controller
    controller.setMap(displaySettings.pad); // the Steuerung page's remapping (updatePad keeps it current)
    std::map<int, std::string> texts; // language file (texts in UTF-8)
    std::vector<uint8_t> language;    // also holds the menu resources
    if (const fs::path file = findCaseInsensitive(root, languageLibraryPath(languageLetter)); !file.empty()) language = readFile(file);
    if (!language.empty())
        for (const auto& [id, raw] : readNeStrings(language)) texts.emplace(id, ansiToUtf8(raw));
    // seg5:1BCF: LoadString(language library, 10) is the main window's title in CreateWindow
    // (seg5:1C06): "D.T.V.R." in rally_d.dat, "Dr. Drago's Madcap Chase" in rally_e.dat.
    if (const auto title = texts.find(10); title != texts.end()) SDL_SetWindowTitle(window.get(), title->second.c_str());
    if (options.loadSlot >= 0) { // before the screen: the drivers' sprites depend on the players
        std::string error;
        const SaveSlots slots(saveDirectory(options.edition), options.edition);
        if (!slots.load(options.loadSlot, play.game(), error))
            throw std::runtime_error("Spielstand " + std::to_string(options.loadSlot) + ": " + error);
        std::cout << "Geladen: " << slots.file(options.loadSlot).string() << "\n";
    }
    GameScreen screen(renderer.get(), root, layout, board, towns, play.game(), std::move(texts), languageLetter);
    // The Flugzeug rows 5602 + i as towns (seg3:32CE: _fstricmp on the names), for the
    // extension's computers; the same mapping as the booking below.
    play.game().setAirports(flightAirports(screen.texts(), towns));
    // Tschüss / Flugzeug avoid the map view of the original canvas (seg2:46C8), not
    // the widescreen extension's wider map.
    play.game().setMapCanvas(baseLayout.baseWidth);
    // The command-line game (not a loaded one) takes the extension from the settings.
    if (options.loadSlot < 0)
        play.game().setComputerExtraCards(screen.settings().computerExtraCards || options.computerExtraCards);
    std::vector<std::string> musicIssues;
    auto musicPacks = discoverMusicPacks(root.parent_path() / "music/packs", musicIssues);
    for (const auto& issue : musicIssues) std::cerr << "Klangset: " << issue << "\n";
    const fs::path musicConfig = defaultSettingsFile().parent_path() / "music.ini";
    MusicPlayer music(std::move(musicPacks), loadMusicPreferences(musicConfig));
    music.setEnabled(screen.settings().value[Settings::Midi] != 0);
    music.setVolume(100); // like the MIDI device at full volume (seg5:2194); [Audio] MasterVolume scales all
    music.interrupt(options.title ? "i" : "a1");
    DisplayControl displayControl(window.get(), renderer.get(), layout);
    ScriptedInput scripted(options.pressKeys); // --press (smoke tests)
    RecompMenuView recompMenu(renderer.get(), root, layout, screen.textRenderer(), DTVR_VERSION); // Esc (recomp)
    // Declare the audio device after the decoder so it closes before the decoder.
    SoundPlayer sound(root); // WaveMix (seg1:2C9B)
    sound.setMusic(&music.stream());
    screen.setSound(&sound);
    ScreenKeyboardView keyboard(renderer.get(), root, layout, screen.textRenderer()); // names by pad (recomp)
    keyboard.setSprites(screen.spriteLookup());
    GameWindow flow({play, towns, options, window.get(), renderer.get(), layout, baseLayout, wideLayout, language, screen,
                     music, musicIssues, musicConfig, sound, controller, displayControl, scripted, recompMenu, logo, busyCursor,
                     keyboard});
    return flow.run();
}

GameWindow::GameWindow(const GameWindowResources& r)
    : play_(r.play), towns_(r.towns), options_(r.options), window_(r.window), renderer_(r.renderer), layout_(r.layout),
      baseLayout_(r.baseLayout), wideLayout_(r.wideLayout), language_(r.language), screen_(r.screen), music_(r.music),
      musicIssues_(r.musicIssues), musicConfig_(r.musicConfig), sound_(r.sound), controller_(r.controller),
      displayControl_(r.displayControl), scripted_(r.scripted), recompMenu_(r.recompMenu), logo_(r.logo), busyCursor_(r.busyCursor),
      keyboard_(r.keyboard), menu_(r.screen.menu()),
      cards_(r.screen.cards()), musicEnabled_(r.screen.settings().value[Settings::Midi]),
      finishSeen_(r.play.game().finishSerial()), balanceSeen_(r.play.game().balanceSerial()),
      announceSeen_(r.play.game().announceSerial()), risikoPressSeen_(r.play.game().risikoPressSerial()),
      statementSeen_(r.play.game().statementSerial()) {
    play_.game().setSpinPresentation(true); // moves start after the slot window (seg2:6426)
    play_.game().setArrivalConfirm(true);   // a human accepts the field with Return (seg2:0BC3)
    play_.game().setBalanceWindow(true);    // the year balance's draws follow the show (seg8:8E38 / 9605)
    play_.game().setStatementWindow(true);  // the player change waits for STAT (seg2:07E1, 12A3)
    play_.game().setInvestmentWindow(true); // a computer's offer waits for the human (seg8:6FBF)
}

void GameWindow::start() {
    busyCursor_.release(); // seg5:0207: the cursor from before, then the logo goes (seg5:020C)
    introShown_ = options_.title && (options_.intro || (!options_.noIntro && screen_.settings().showIntro != 0))
                      && screen_.startIntro(play_.game().sessionRandom(), SDL_GetTicks());
    // The intro removes the logo once its files are loaded (seg7:044D), the set-up
    // before the title (seg5:020C); both -> seg6:0B37.
    logo_.hide();
    if (options_.title && !introShown_) openTitle();
    if (options_.openSetup) { screen_.setTitle(true); menu_.close(); command(1020); }
    if (options_.openStages) { screen_.setTitle(true); menu_.close(); screen_.stages().open(screen_.settings().rounds); }
    if (!options_.openMenu.empty()) openMenu(options_.openMenu);
    if (options_.openSave) screen_.saveDialog().open(true);
    if (options_.openOverview) screen_.overview().open();
    if (options_.openSurroundings) screen_.startSurroundings();
    if (options_.zoom) screen_.toggleZoom();
    if (options_.openOptions) screen_.options().open(screen_.settings());
    if (options_.recompMenuPage >= 0) {
        recompMenu_.menu().open(menuSettings(), true);
        recompMenu_.menu().focusPage(options_.recompMenuPage);
        recompMenu_.menu().activate(); // into the page's entries
    }
    if (options_.demo) { menu_.close(); command(1024); }
    if (options_.victory) { music_.interrupt("s"); screen_.startVictory(SDL_GetTicks()); }
    if (options_.balance) screen_.startBalance(SDL_GetTicks());
    if (options_.statement) screen_.statement().open(statSheet(play_.game()), SDL_GetTicks());
    if (options_.announce) screen_.startAnnouncement(2, 3, SDL_GetTicks());
    if (options_.cardShop) screen_.cardShop().open(play_.game());
    if (options_.nails) screen_.nails().open(play_.game().nailsDirections());
    if (options_.invest) screen_.invest().open(true, "Vera, ein verrückter Professor fragt Dich, ob Du in seine Forschungen investieren willst.", 50000,
                                                SDL_GetTicks()); // the computer's wait starts now (seg6:0FB4)
    if (options_.flight) screen_.flight().open(play_.game().eastOpen(), play_.game().remainingStages() == 0);
    if (options_.selectPlayer) {
        std::vector<int> others;
        std::array<uint8_t, kPlayerSlots> status{};
        for (int p = 0; p < kPlayerSlots; ++p) {
            status[size_t(p)] = play_.game().players()[size_t(p)].status;
            if (status[size_t(p)] && p != play_.game().currentPlayer()) others.push_back(p);
        }
        screen_.selectPlayer().open(others, status);
    }
    if (options_.finish) screen_.startFinish(play_.game().destination(), play_.game().currentPlayer(), SDL_GetTicks());
    if (options_.openStatistics) screen_.estates().openStatistics(play_.game().currentPlayer());
    if (options_.openCards && cards_.open(play_.game(), 2)) cardWindowState_ = turnState();
    if (options_.rollAtStart && play_.game().roll()) report();
    if (options_.drago >= 0) play_.game().startDrago(options_.drago); // --drago FORM
    if (!options_.event.empty()) { // --event: the name as eventName() gives it
        for (int k = int(EventKind::OpenEast); k <= int(EventKind::FootballClub); ++k)
            if (eventName(EventKind(k)) == options_.event) play_.game().triggerEvent(EventKind(k));
    }
    report();
    heldArrow_ = options_.holdRight ? SDLK_RIGHT : SDLK_UNKNOWN; // seg4:01F2: DS:A77A held
}

bool GameWindow::swallowLogoSkip(const SDL_Event& event) {
    if (logoSkipKey_ != SDLK_UNKNOWN && (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
        && event.key.keysym.sym == logoSkipKey_) {
        if (event.type == SDL_KEYUP) logoSkipKey_ = SDLK_UNKNOWN;
        return true;
    }
    if (logoSkipButton_ && event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        logoSkipButton_ = false;
        return true;
    }
    return false;
}

bool GameWindow::waitLogo(bool hold) {
    if (!logo_.shown()) return true;
    if (options_.smokeTest && !hold) return true; // smoke runs keep their frame counts
    for (int frame = 1;; ++frame) {
        const Uint32 now = SDL_GetTicks();
        // Recomp addition: rally.exe has no timer, the logo stays for its loading time.
        if (!hold && now - logo_.shownAt() >= kLogoMinimumMs) return true;
        if (!options_.smokeTest) // keys only; the recomp pad actions wait for the game window
            controller_.update(now, screen_.settings().value[Settings::Joystick] != 0,
                               (SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS) != 0, PadContext{});
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { running_ = false; return false; }
            controller_.handleEvent(event);
            if (swallowLogoSkip(event)) continue;
            if (event.type == SDL_WINDOWEVENT) { // WM_ACTIVATEAPP as in handleEvent
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) { music_.setFocused(false); sound_.setFocused(false); }
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) { music_.setFocused(true); sound_.setFocused(true); }
            }
            Settings s = screen_.settings(); // F11 / Alt+Enter, F12 (recomp, [Display])
            bool changed = false;
            if (displayControl_.handle(event, s, changed)) {
                if (changed) applySettings(s);
                continue;
            }
            // PICT seg6:0B66: WM_KEYDOWN and WM_LBUTTONDOWN destroy the window (controller
            // buttons arrive as Return / Escape). The rest of that input is dropped.
            // Shift / Ctrl / Alt / GUI alone do not (Alt+Enter is the fullscreen key; Alt is
            // WM_SYSKEYDOWN in rally.exe, which PICT does not handle).
            if (event.type == SDL_KEYDOWN && !isModifierKey(event.key.keysym.sym)) {
                logoSkipKey_ = event.key.keysym.sym;
                return true;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                logoSkipButton_ = true;
                return true;
            }
        }
        displayControl_.beginFrame();
        logo_.draw();
        displayControl_.endFrame();
        const bool last = options_.smokeTest && frame >= options_.frames;
        if (!options_.screenshot.empty() && last) displayControl_.screenshot(options_.screenshot);
        SDL_RenderPresent(renderer_);
        if (last) { running_ = false; return false; }
        SDL_Delay(16);
    }
}

int GameWindow::run() {
    if (!waitLogo(options_.logo)) return 0;
    start();
    for (int frame = 1; running_; ++frame) {
        updateFrame(frame);
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (!swallowLogoSkip(event) && !handleStatementEvent(event)) handleEvent(event); // STAT first: it is modal
        displayControl_.beginFrame();
        screen_.draw();
        if (!quitAsked_ && screen_.setup().isOpen()) keyboard_.draw(); // the quit question takes the input first
        if (recompMenu_.isOpen()) recompMenu_.menu().setMusic(musicMenuState(music_, musicIssues_));
        recompMenu_.draw();
        displayControl_.endFrame();
        const bool last = options_.smokeTest && frame >= options_.frames;
        if (!options_.screenshot.empty() && (last || !options_.smokeTest)) displayControl_.screenshot(options_.screenshot);
        SDL_RenderPresent(renderer_);
        if (last) break;
        SDL_Delay(16);
    }
    return 0;
}

Settings GameWindow::menuSettings() { // the layout in use (--[no-]widescreen may override the setting)
    Settings s = screen_.settings();
    s.widescreen = layout_.width != baseLayout_.width;
    return s;
}

void GameWindow::applySettings(const Settings& s) { // saved; the OPTION window shows the same smoothing
    screen_.applySettings(s);
    screen_.options().setTextSmoothing(s.textSmoothing);
}

void GameWindow::report() {
    play_.printLog(std::cout); // the window keeps the original title (text 10)
}

void GameWindow::openMenu(const std::string& name) {
    std::vector<NeMenuItem> items;
    try {
        if (!language_.empty()) items = readNeMenu(language_, name);
    } catch (const std::exception& e) {
        std::cerr << "Menue " << name << ": " << e.what() << "\n";
    }
    if (items.empty()) menu_.close();
    else menu_.open(name, std::move(items));
}

// The turn menu ONFIELD opens for a human player before the spin (seg2:0864 ->
// seg2:068F) while no other window is open.
std::array<int, 4> GameWindow::turnState() {
    const Game& g = play_.game();
    const int p = g.currentPlayer();
    return std::array<int, 4>{p, int(g.phase()), g.spinSerial(), p >= 0 ? g.handCount(p) : 0};
}

bool GameWindow::turnMenuDue() {
    const Game& g = play_.game();
    return g.phase() == Game::Phase::Roll && g.currentPlayer() >= 0 && !g.isComputer(g.currentPlayer());
}

void GameWindow::openTitle() { // seg8:03AB / 0DB0: TITEL and the menu MAIN
    screen_.setTitle(true);
    openMenu("MAIN");
    // seg8:0DE7: "Spiel fortsetzen" (position 1) is grayed without an autosave: profile
    // Save/AS (DS:1864 / DS:1861, seg5:22C0) 0 -> seg6:156D, EnableMenuItem(MAIN, 1,
    // MF_BYPOSITION | MF_GRAYED = 0x401) at seg6:159F; else seg6:15BC enables it. Recomp
    // (round E3): an autosave of another edition counts as none (docs/save-format.md).
    if (menu_.isOpen() && !screen_.saveSlots().hasOwnAuto()) {
        std::vector<NeMenuItem> items;
        try { items = readNeMenu(language_, "MAIN"); } catch (const std::exception&) {}
        if (items.size() > 1) items[1].flags |= 0x01;
        menu_.open("MAIN", std::move(items));
    }
}

// `fresh`: a new game announces its first destination (seg4:0E92 -> seg7:372F(2)).
// Otherwise a game was loaded (SaveSlots::load / loadAuto, Game::resumeLoaded).
void GameWindow::startGame(bool fresh) {
    screen_.setTitle(false);
    screen_.reloadMap(); // europe.map afresh for every new or loaded game (R seg5:182F)
    if (fresh) {
        screen_.startAnnouncement(2, 3, SDL_GetTicks());
        autosaved_ = -1;        // the first turn of a started game is saved too
        music_.interrupt("a1"); // seg4:0E8A, starting a game
    } else {
        // seg4:069B / 061D: no autosave (seg8:A870 is called only by the player change,
        // seg2:0837) and no cue a1, only seg1:275E: the next background cue g0-g5 from
        // zero. Its DS:A111 = 6 holds the turn-based change for six player changes, as
        // the player's hold from the turn the cue starts at.
        autosaved_ = play_.game().turnSerial();
        music_.turn(play_.game().eventClock().turns);
        music_.nextBackground();
    }
    screen_.loadDriverSprites();
    report();
}

void GameWindow::askQuit(bool reopenMain) {
    Notice q{700, {}};
    q.buttons = 4;
    screen_.messages().push(q);
    quitReopensMain_ = reopenMain;
    quitAsked_ = true;
    menu_.close();
}

// Recomp (round E3): loading a save of another edition fails with a message in the
// window (docs/save-format.md "Editions"); `edition` is the save's id, empty for none.
void GameWindow::foreignSaveNotice(const std::string& edition) {
    if (edition.empty()) return;
    NoticeArg text;
    text.text = foreignSaveMessage(screen_.saveSlots().edition(), edition);
    screen_.messages().push(Notice{kRecompNoticeText, {text}});
}

void GameWindow::command(int id) { // WM_COMMAND of the main window (seg4:0B76)
    menu_.close();
    switch (id) {
    case 1020: {                                             // Neues Spiel: seg4:0DA9(0) -> PLAYER
        std::array<uint8_t, kPlayerSlots> status{};
        std::array<std::string, kPlayerSlots> names;
        for (int p = 0; p < kPlayerSlots; ++p) { // seg1:02CE ff.: [Player] P0-P7
            status[size_t(p)] = uint8_t(screen_.settings().players[size_t(p)]);
            names[size_t(p)] = play_.game().playerName(p);
        }
        screen_.setup().open(status, names);
        screen_.setup().setExtraCards(screen_.settings().computerExtraCards);
        break;
    }
    case 1021: screen_.saveDialog().open(false); break;       // Spiel laden: seg8:B157(0)
    case 1022: {                                             // Spiel fortsetzen: seg8:A8DF(0), as.dat
        std::string error;
        if (screen_.saveSlots().loadAuto(play_.game(), error)) startGame();
        else {
            std::cerr << "Fortsetzen: " << error << "\n";
            openTitle();
            foreignSaveNotice(screen_.saveSlots().foreignAuto());
        }
        break;
    }
    case 1025: askQuit(false); break;                        // Rallye beenden: seg4:0733 -> 073F (0EDF)
    case 1023: case 1063: screen_.options().open(screen_.settings()); break; // Optionen: seg1:ACE7
    case 1026: screen_.showPicture(creditsPicture(layout_.edition, screen_.languageLetter())); break; // seg4:070C -> seg6:0A4D
    case 1024: {                                             // Demo: seg4:05A8 -> seg4:0DA9(1)
        // seg2:01AF: slots 0, 2, 4, 6 are computers (zoom 1, 0, 1, 0), no setup dialog.
        std::array<uint8_t, kPlayerSlots> status{2, 0, 2, 0, 2, 0, 2, 0};
        std::array<std::string, kPlayerSlots> names;
        for (int p = 0; p < kPlayerSlots; ++p) names[size_t(p)] = play_.game().playerName(p);
        std::string error;
        // The demo continues the session's generator as well (seg4:0DA9(1)).
        if (!play_.newGame(status, names, error)) { std::cerr << "Demo: " << error << "\n"; openTitle(); break; }
        play_.game().setComputerExtraCards(screen_.settings().computerExtraCards);
        play_.game().setDemo(true); // seg4:05CF: DS:14 = 1
        for (int p = 0; p < kPlayerSlots; ++p) screen_.setZoom(p, p == 0 || p == 4);
        startGame(true);
        break;
    }
    case 1030: if (play_.game().roll()) report(); break;     // Würfeln: seg2:6426(1, 5, 1)
    case 1031:                                               // Karte einsetzen
        if (!play_.game().openCardWindow()) { openMenu("ONFIELD"); break; } // blocked: menu stays
        if (cards_.open(play_.game(), 2)) cardWindowState_ = turnState(); // seg3:0887(2)
        break;
    case 1033: openMenu("MAIN1"); break;                     // Hauptmenü
    case 1064: askQuit(true); break;                         // Rallye beenden: seg4:082B -> 0837 (0EDF), No: MAIN1 (0851)
    case 1062: screen_.saveDialog().open(true); break;        // Spiel speichern: seg8:B157(1)
    case 1060: openTitle(); break;                           // Neues Spiel: seg4:075B, back to the title
    case 1065: break;                                        // Weiter spielen: seg4:0753, ONFIELD again
    case 1066: {                                             // Mitspieler: seg4:0804 -> seg2:51EA(0xFF)
        const Game& g = play_.game();
        std::vector<int> all;
        std::array<uint8_t, kPlayerSlots> status{};
        for (int p = 0; p < kPlayerSlots; ++p) {
            status[size_t(p)] = g.players()[size_t(p)].status;
            if (status[size_t(p)]) all.push_back(p);
        }
        targetCardSlot_ = -2;
        screen_.selectPlayer().open(all, status);
        break;
    }
    case 1032: case 1040: screen_.toggleZoom(); break;       // Zoom: seg6:2E54(3), then centred
    case 1034: case 1041: screen_.overview().open(); break;  // Übersicht: seg1:A240
    case 1035: case 1043: screen_.startSurroundings(); break; // Umfeld: seg6:3C94
    case 1036: case 1042:                                    // Statistik: seg1:4DE5
        screen_.estates().openStatistics(play_.game().currentPlayer());
        break;
    default: break; // Zoom, Übersicht, Umfeld, Statistik, Neues Spiel, Speichern ...: not built yet
    }
}

// seg5:0174: [Setup] ShowIntro plays the intro (seg7:0000) before the title.
} // namespace dtvr
