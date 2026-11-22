// SPDX-License-Identifier: GPL-3.0-only
// The game window's flow (round 48, split from game_screen.cpp): title, menus,
// dialogs, the scenes and the game loop around GameScreen.
#include "game_screen.hpp"

#include "board.hpp"
#include "controller.hpp"
#include "panel.hpp"
#include "settings.hpp"
#include "music_menu.hpp"
#include "ne_strings.hpp"

#include <array>
#include <cctype>
#include <ctime>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

// Names compared without regard to ASCII case (seg10:22CA folds A-Z only).
bool sameName(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
}

} // namespace

int runGameWindow(PlayView& play, const fs::path& root, const Board& board, const std::vector<Town>& towns,
                  const GameWindowOptions& options) {
    SDL_DisplayMode display{};
    if (SDL_GetDesktopDisplayMode(0, &display) != 0) display.w = 1024, display.h = 768;
    const ScreenLayout layout = layoutForScreen(display.w, display.h, options.canvasWidth);
    // The canvas keeps the original size; SDL scales it into the window or the
    // desktop (logical size, black borders) and maps mouse coordinates back.
    const Settings displaySettings = loadSettings(defaultSettingsFile());
    const bool startFullscreen = options.fullscreen || (displaySettings.fullscreen && !options.smokeTest);
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("Die total verrueckte Rallye", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, layout.width,
                         layout.height, SDL_WINDOW_RESIZABLE | (startFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0)),
        SDL_DestroyWindow);
    if (!window) throw std::runtime_error(SDL_GetError());
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
        SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE), SDL_DestroyRenderer);
    if (!renderer) throw std::runtime_error(SDL_GetError());
    if (SDL_RenderSetLogicalSize(renderer.get(), layout.width, layout.height) != 0
        || SDL_RenderSetIntegerScale(renderer.get(), displaySettings.integerScale ? SDL_TRUE : SDL_FALSE) != 0)
        throw std::runtime_error(SDL_GetError());
    ControllerInput controller; // joystick 1 of the original (seg2:8A30), any SDL game controller
    std::map<int, std::string> texts; // language file (texts in UTF-8)
    std::vector<uint8_t> language;    // also holds the menu resources
    if (const fs::path file = findCaseInsensitive(root, "dat/d/rally_d.dat"); !file.empty()) language = readFile(file);
    if (!language.empty())
        for (const auto& [id, raw] : readNeStrings(language)) texts.emplace(id, ansiToUtf8(raw));
    if (options.loadSlot >= 0) { // before the screen: the drivers' sprites depend on the players
        std::string error;
        const SaveSlots slots(defaultSaveDirectory());
        if (!slots.load(options.loadSlot, play.game(), error))
            throw std::runtime_error("Spielstand " + std::to_string(options.loadSlot) + ": " + error);
        std::cout << "Geladen: " << slots.file(options.loadSlot).string() << "\n";
    }
    GameScreen screen(renderer.get(), root, layout, board, towns, play.game(), std::move(texts));
    std::vector<std::string> musicIssues;
    auto musicPacks = discoverMusicPacks(root.parent_path() / "music/packs", musicIssues);
    for (const auto& issue : musicIssues) std::cerr << "Klangset: " << issue << "\n";
    const fs::path musicConfig = defaultSettingsFile().parent_path() / "music.ini";
    MusicPlayer music(std::move(musicPacks), loadMusicPreferences(musicConfig));
    int musicEnabled = screen.settings().value[Settings::Midi];
    music.setEnabled(musicEnabled != 0);
    music.interrupt(options.title ? "i" : "a1");
    MusicMenu musicMenu(renderer.get(), layout.width, layout.height, music, musicConfig, std::move(musicIssues));
    // Declare the audio device after the decoder so it closes before the decoder.
    SoundPlayer sound(root); // WaveMix (seg1:2C9B)
    sound.setMusic(&music.stream());
    screen.setSound(&sound);
    auto report = [&] {
        play.printLog(std::cout);
        SDL_SetWindowTitle(window.get(), play.title().c_str());
    };
    MenuView& menu = screen.menu();
    auto openMenu = [&](const std::string& name) {
        std::vector<NeMenuItem> items;
        try {
            if (!language.empty()) items = readNeMenu(language, name);
        } catch (const std::exception& e) {
            std::cerr << "Menue " << name << ": " << e.what() << "\n";
        }
        if (items.empty()) menu.close();
        else menu.open(name, std::move(items));
    };
    // The turn menu ONFIELD opens for a human player before the spin (seg2:0864 ->
    // seg2:068F) while no other window is open.
    auto turnState = [&] {
        const Game& g = play.game();
        const int p = g.currentPlayer();
        return std::array<int, 4>{p, int(g.phase()), g.spinSerial(), p >= 0 ? g.handCount(p) : 0};
    };
    CardView& cards = screen.cards();
    std::array<int, 4> cardWindowState{-2, 0, 0, 0}; // turn state when the card window opened
    auto turnMenuDue = [&] {
        const Game& g = play.game();
        return g.phase() == Game::Phase::Roll && g.currentPlayer() >= 0 && !g.isComputer(g.currentPlayer());
    };
    bool running = true;
    int autosaved = -1; // turn serial of the last autosave
    std::vector<SoundCue> pendingSounds;
    std::vector<FilmCue> pendingFilms;
    int finishSeen = play.game().finishSerial();
    bool victoryShown = false;
    int balanceSeen = play.game().balanceSerial();
    int announceSeen = play.game().announceSerial();
    int shopSerial = -1;
    int targetCardSlot = -1; // hand slot waiting for the target choice
    int flightCardSlot = -1; // hand slot of the Flugzeug card being booked
    int nailsCardSlot = -1;  // hand slot of the Nägel card waiting for a direction
    Uint32 messageShown = 0; // when the open message appeared (games of computers)
    auto openTitle = [&] { // seg8:03AB / 0DB0: TITEL and the menu MAIN
        screen.setTitle(true);
        openMenu("MAIN");
        // seg8:0DE7: "Spiel fortsetzen" (position 1) is grayed without an autosave (seg6:156D).
        if (menu.isOpen() && !screen.saveSlots().hasAuto()) {
            std::vector<NeMenuItem> items;
            try { items = readNeMenu(language, "MAIN"); } catch (const std::exception&) {}
            if (items.size() > 1) items[1].flags |= 0x01;
            menu.open("MAIN", std::move(items));
        }
    };
    // `fresh`: a new game announces its first destination (seg4:0E92 -> seg7:372F(2)).
    auto startGame = [&](bool fresh = false) {
        if (fresh) screen.startAnnouncement(2, 3, SDL_GetTicks());
        autosaved = -1; // the first turn of a started or loaded game is saved too
        screen.setTitle(false);
        music.interrupt("a1"); // seg4:0E8A, starting/loading a game
        screen.loadDriverSprites();
        report();
    };
    // seg4:0EDF: text 700 with Yes / No (buttons 4); Yes ends the program (DS:88 = 1,
    // WM_CLOSE). From MAIN1 (1064) a No opens MAIN1 again.
    bool quitReopensMain = false;
    int sceneSerial = -1; // message serial whose event scene has been started
    auto askQuit = [&](bool reopenMain) {
        Notice q{700, {}};
        q.buttons = 4;
        screen.messages().push(q);
        quitReopensMain = reopenMain;
        menu.close();
    };
    auto command = [&](int id) { // WM_COMMAND of the main window (seg4:0B76)
        menu.close();
        switch (id) {
        case 1020: {                                             // Neues Spiel: seg4:0DA9(0) -> PLAYER
            std::array<uint8_t, kPlayerSlots> status{};
            std::array<std::string, kPlayerSlots> names;
            for (int p = 0; p < kPlayerSlots; ++p) {
                status[size_t(p)] = play.game().players()[size_t(p)].status;
                names[size_t(p)] = play.game().playerName(p);
            }
            screen.setup().open(status, names);
            break;
        }
        case 1021: screen.saveDialog().open(false); break;       // Spiel laden: seg8:B157(0)
        case 1022: {                                             // Spiel fortsetzen: seg8:A8DF(0), as.dat
            std::string error;
            if (screen.saveSlots().loadAuto(play.game(), error)) startGame();
            else { std::cerr << "Fortsetzen: " << error << "\n"; openTitle(); }
            break;
        }
        case 1025: askQuit(false); break;                        // Rallye beenden: seg4:055E -> 0EDF
        case 1023: case 1063: screen.options().open(screen.settings()); break; // Optionen: seg1:ACE7
        case 1026: screen.showPicture("Credits.bmp"); break;    // Credits: seg4:070C -> seg6:0A4D
        case 1024: {                                             // Demo: seg4:05A8 -> seg4:0DA9(1)
            // seg2:01AF: slots 0, 2, 4, 6 are computers (zoom 1, 0, 1, 0), no setup dialog.
            std::array<uint8_t, kPlayerSlots> status{2, 0, 2, 0, 2, 0, 2, 0};
            std::array<std::string, kPlayerSlots> names;
            for (int p = 0; p < kPlayerSlots; ++p) names[size_t(p)] = play.game().playerName(p);
            std::string error;
            const uint16_t seed = uint16_t(SDL_GetTicks() ^ uint32_t(std::time(nullptr)));
            if (!play.newGame(status, names, seed, error)) { std::cerr << "Demo: " << error << "\n"; openTitle(); break; }
            for (int p = 0; p < kPlayerSlots; ++p) screen.setZoom(p, p == 0 || p == 4);
            startGame(true);
            break;
        }
        case 1030: if (play.game().roll()) report(); break;     // Würfeln: seg2:6426(1, 5, 1)
        case 1031:                                               // Karte einsetzen
            if (!play.game().openCardWindow()) { openMenu("ONFIELD"); break; } // blocked: menu stays
            if (cards.open(play.game(), 2)) cardWindowState = turnState(); // seg3:0887(2)
            break;
        case 1033: openMenu("MAIN1"); break;                     // Hauptmenü
        case 1064: askQuit(true); break;                         // Rallye beenden (seg4:073F -> 0EDF)
        case 1062: screen.saveDialog().open(true); break;        // Spiel speichern: seg8:B157(1)
        case 1032: case 1040: screen.toggleZoom(); break;       // Zoom: seg6:2E54(3), then centred
        case 1034: case 1041: screen.overview().open(); break;  // Übersicht: seg1:A240
        case 1035: case 1043: screen.startSurroundings(); break; // Umfeld: seg6:3C94
        case 1036: case 1042:                                    // Statistik: seg1:4DE5
            screen.estates().openStatistics(play.game().currentPlayer());
            break;
        default: break; // Zoom, Übersicht, Umfeld, Statistik, Neues Spiel, Speichern ...: not built yet
        }
    };
    if (options.title) openTitle();
    if (options.openSetup) { screen.setTitle(true); menu.close(); command(1020); }
    if (!options.openMenu.empty()) openMenu(options.openMenu);
    if (options.openSave) screen.saveDialog().open(true);
    if (options.openOverview) screen.overview().open();
    if (options.openSurroundings) screen.startSurroundings();
    if (options.zoom) screen.toggleZoom();
    if (options.openOptions) screen.options().open(screen.settings());
    if (options.demo) { menu.close(); command(1024); }
    if (options.victory) { music.interrupt("s"); screen.startVictory(SDL_GetTicks()); }
    if (options.balance) screen.startBalance(SDL_GetTicks());
    if (options.announce) screen.startAnnouncement(2, 3, SDL_GetTicks());
    if (options.cardShop) screen.cardShop().open(play.game());
    if (options.nails) screen.nails().open();
    if (options.invest) screen.invest().open(true, "Vera, ein verrückter Professor fragt Dich, ob Du in seine Forschungen investieren willst.", 50000);
    if (options.flight) screen.flight().open(play.game().eastOpen(), play.game().remainingStages() == 0);
    if (options.selectPlayer) {
        std::vector<int> others;
        std::array<uint8_t, kPlayerSlots> status{};
        for (int p = 0; p < kPlayerSlots; ++p) {
            status[size_t(p)] = play.game().players()[size_t(p)].status;
            if (status[size_t(p)] && p != play.game().currentPlayer()) others.push_back(p);
        }
        screen.selectPlayer().open(others, status);
    }
    if (options.finish) screen.startFinish(play.game().destination(), play.game().currentPlayer(), SDL_GetTicks());
    if (options.openStatistics) screen.estates().openStatistics(play.game().currentPlayer());
    if (options.openCards && cards.open(play.game(), 2)) cardWindowState = turnState();
    if (options.rollAtStart && play.game().roll()) report();
    report();
    SDL_Keycode heldArrow = options.holdRight ? SDLK_RIGHT : SDLK_UNKNOWN; // seg4:01F2: DS:A77A held
    auto isArrow = [](SDL_Keycode k) { return k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT || k == SDLK_RIGHT; };
    for (int frame = 1; running; ++frame) {
        if (screen.settings().value[Settings::Midi] != musicEnabled) {
            musicEnabled = screen.settings().value[Settings::Midi];
            music.setEnabled(musicEnabled != 0);
        }
        if (!screen.title()) music.turn(play.game().eventClock().turns);
        music.tick();
        for (const auto& cue : play.game().takeMusic()) if (!screen.title()) music.interrupt(cue);
        screen.advanceVehicles(SDL_GetTicks());
        const bool moving = screen.vehicleMoving(play.game().currentPlayer());
        if (!screen.title())
            for (Notice& n : play.game().takeNotices()) {
                if (const auto cue = musicForNotice(n.text); !cue.empty()) music.interrupt(cue);
                // seg8:6FBF: the offer text (5321 + random(2) / 5401 + random(2)) belongs
                // to the investment window, not to the message window.
                const bool offerText = n.text == 5321 || n.text == 5322 || n.text == 5401 || n.text == 5402;
                if (offerText && play.game().investmentOffer() != EventKind::None) {
                    const auto t = screen.texts().find(n.text);
                    const std::string offer = t == screen.texts().end() ? std::string() : formatNotice(t->second, n, play.game());
                    const Game& g = play.game();
                    screen.invest().open(g.investmentOffer() == EventKind::Professor, offer, g.cash(g.currentPlayer()));
                    continue;
                }
                screen.messages().push(std::move(n));
            }
        { // event places: the view zooms in on the place while its message is open
            const Notice* front = screen.messages().front();
            if (front && front->focusX >= 0) {
                screen.eventFocus(front->focusX, front->focusY);
                // seg8 handlers: the animation plays before the message (once per message).
                if (screen.messages().frontSerial() != sceneSerial) {
                    sceneSerial = screen.messages().frontSerial();
                    if (const EventScene* scene = eventSceneForNotice(front->text))
                        screen.startEventScene(*scene, front->focusX, front->focusY, front->variant, SDL_GetTicks());
                }
            } else {
                screen.eventFocus(-1, -1);
            }
        }
        // The original plays a field's sounds when the vehicle is on it: cues wait while the
        // drawn car still drives there.
        for (const SoundCue& c : play.game().takeSounds())
            if (!screen.title()) pendingSounds.push_back(c);
        // Card films (seg7:62AA): one after the other, modal, sound on channel 7.
        for (const FilmCue& f : play.game().takeFilms())
            if (!screen.title()) pendingFilms.push_back(f);
        if (screen.film().active()) {
            screen.film().update(SDL_GetTicks());
            if (!screen.film().active()) sound.stop(7); // seg1:317D
        }
        if (!screen.film().active() && !pendingFilms.empty() && !moving && !screen.messages().open()) {
            const FilmCue f = pendingFilms.front();
            pendingFilms.erase(pendingFilms.begin());
            if (screen.film().start(f.film, f.loops, 10, 10, SDL_GetTicks())) { // (10, 10) of the canvas
                const fs::path wav = findCaseInsensitive(screen.root(), fs::path("butt") / "snd" / f.sound);
                if (!wav.empty()) sound.playFile(7, wav); // seg1:2D40(7, …)
            }
        }
        if (!moving) {
            for (const SoundCue& c : pendingSounds) sound.play(c.channel, c.sound, c.loops);
            pendingSounds.clear();
        }
        // Message window and menu are modal (seg6:0FB4, SetCapture in seg5:578C).
        // A played card changes the turn state and closes the card window.
        if (cards.isOpen() && turnState() != cardWindowState) cards.close();
        // seg3:1057: after text 403 the card window opens in discard mode (seg3:0887(0)) and
        // stays until a card is dropped.
        if (!screen.title() && !moving && play.game().yellowCardPending() && !screen.messages().open() && !cards.isOpen()
            && !play.game().isComputer(play.game().currentPlayer()) && cards.open(play.game(), -1))
            cardWindowState = turnState();
        // The town dialog belongs to a human player's stop in a town (seg2:141A -> seg1:4BDD).
        EstatesView& estates = screen.estates();
        {
            const Game& g = play.game();
            const int p = g.currentPlayer();
            const int town = p >= 0 && g.phase() == Game::Phase::Arrived && !g.isComputer(p)
                ? g.townAt(g.playerField(p)) : -1;
            if (estates.statistics()) {
                // Statistik stays until it is closed.
            } else if (options.openEstates >= 0) {
                if (!estates.isOpen()) estates.open(options.openEstates);
            } else if (town < 0) estates.close();
            else if (moving) {} // the dialog opens when the car has arrived
            else if (!estates.isOpen() || estates.town() != town) estates.open(town);
        }
        SaveView& saveDialog = screen.saveDialog();
        if (screen.title()) { // nothing of the game runs behind the title
            if (!menu.isOpen() && !saveDialog.isOpen() && !screen.setup().isOpen() && !screen.options().isOpen()
                && !screen.pictureShown() && !screen.messages().open())
                openTitle();
        } else if (play.game().humansLeft() && play.game().turnSerial() != autosaved
                   && !play.game().computerTurnRunning()) { // seg2:0837 -> seg8:A870 at each turn start
            std::string error;
            if (!screen.saveSlots().saveAuto(play.game(), error)) std::cerr << "Autosave: " << error << "\n";
            autosaved = play.game().turnSerial();
        }
        // Without a human nobody answers the message window: it closes after a while
        // (assumption: 2 s times the pause factor).
        if (!screen.title() && !play.game().humansLeft() && screen.messages().open()) {
            if (messageShown == 0) messageShown = SDL_GetTicks();
            else if (SDL_GetTicks() - messageShown > Uint32(2000 * screen.settings().pauseFactor())) {
                screen.messages().dismiss();
                messageShown = 0;
            }
        } else {
            messageShown = 0;
        }
        // The card shop of a human player's stop on a card field (seg2:15AE -> seg3:4437).
        {
            const Game& g = play.game();
            const bool shopDue = g.phase() == Game::Phase::Arrived && g.atCardField() && g.currentPlayer() >= 0
                                 && !g.isComputer(g.currentPlayer());
            if (!shopDue && !options.cardShop) screen.cardShop().close();
            else if (shopDue && !moving && !screen.cardShop().isOpen() && shopSerial != g.turnSerial()) {
                shopSerial = g.turnSerial(); // once per stop
                screen.cardShop().open(g);
            }
        }
        // The finish sequence (seg2:131D / 11DF -> seg7:2628) when the car has arrived.
        screen.finish().update(SDL_GetTicks());
        // The year balance (seg2:07C5 -> seg8:88C6) at the year change.
        screen.balance().update(SDL_GetTicks());
        // The next destination (seg2:1486 / 1453 -> seg7:372F(1 / 3)) after the finish.
        screen.announcement().update(SDL_GetTicks());
        if (!screen.title() && play.game().announceSerial() != announceSeen && !moving
            && !screen.messages().open() && !screen.finish().active()) {
            announceSeen = play.game().announceSerial();
            screen.startAnnouncement(play.game().announceMode(), play.game().announceVariant(), SDL_GetTicks());
        }
        if (!screen.title() && play.game().balanceSerial() != balanceSeen && !moving && !screen.messages().open()
            && !screen.finish().active()) {
            balanceSeen = play.game().balanceSerial();
            screen.startBalance(SDL_GetTicks());
        }
        // After the Master-Rallye: the victory (seg7:71C4), then the title (WM_COMMAND 0x424).
        screen.victory().update(SDL_GetTicks());
        if (!screen.title() && play.game().phase() == Game::Phase::GameOver && !screen.finish().active()
            && !screen.messages().open()) {
            if (!victoryShown) { music.interrupt("s"); victoryShown = screen.startVictory(SDL_GetTicks()) || true; }
            else if (!screen.victory().active()) { victoryShown = false; openTitle(); }
        }
        if (!screen.title() && play.game().finishSerial() != finishSeen && !moving) {
            finishSeen = play.game().finishSerial();
            music.interrupt(play.game().finishMusic());
            screen.startFinish(play.game().finishField(), play.game().finishPlayer(), SDL_GetTicks());
        }
        const bool modal = musicMenu.open() || screen.messages().open() || menu.isOpen() || cards.isOpen() || saveDialog.isOpen()
                           || estates.statistics() || screen.title() || screen.overview().isOpen()
                           || screen.surroundings() || screen.options().isOpen() || screen.finish().active()
                           || screen.victory().active() || screen.balance().active()
                           || screen.announcement().active() || screen.cardShop().isOpen()
                           || screen.selectPlayer().isOpen() || screen.flight().isOpen()
                           || screen.nails().isOpen() || screen.invest().isOpen() || screen.film().active();
        if (!modal && !moving && play.update(SDL_GetTicks(), true)) report();
        if (!modal && !moving && turnMenuDue() && !options.rollAtStart) openMenu("ONFIELD");
        // A held arrow drives on field by field once the car has reached its field.
        if (!modal && !moving && heldArrow != SDLK_UNKNOWN && play.game().phase() == Game::Phase::Move
            && !play.game().isComputer(play.game().currentPlayer()) && play.handleKey(heldArrow, 0))
            report();
        // seg2:8A30 every 80 ms while [Setup] Joystick is on: keys for the focused window.
        if (!options.smokeTest) controller.update(SDL_GetTicks(), screen.settings().value[Settings::Joystick] != 0);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            controller.handleEvent(event);
            if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) // keep the canvas centred and scaled
                    SDL_RenderSetLogicalSize(renderer.get(), layout.width, layout.height);
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) music.setFocused(false);
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) music.setFocused(true);
            }
            if (event.type == SDL_KEYDOWN && !event.key.repeat
                && (event.key.keysym.sym == SDLK_F11
                    || (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT)))) {
                Settings s = screen.settings(); // window / fullscreen (recomb, [Display])
                s.fullscreen = !(SDL_GetWindowFlags(window.get()) & SDL_WINDOW_FULLSCREEN_DESKTOP);
                if (SDL_SetWindowFullscreen(window.get(), s.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) == 0) {
                    SDL_RenderSetLogicalSize(renderer.get(), layout.width, layout.height); // new output size
                    screen.applySettings(s);
                }
                else
                    std::cerr << "Vollbild: " << SDL_GetError() << "\n";
                heldArrow = SDLK_UNKNOWN;
                continue;
            }
            if (screen.film().active() && (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN))
                continue; // the FLIC window runs modally (seg6:0FB4)
            if (screen.mouseControl().isOpen()) { // MCTRL (seg4:103B): the cursor stays in the cross
                bool closed = false;
                const SDL_Keycode key = screen.mouseControl().handle(event, closed);
                if (key != SDLK_UNKNOWN && play.handleKey(key, 0)) report(); // posted to the main window
                if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN
                    || event.type == SDL_MOUSEBUTTONUP || closed)
                    continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F8) {
                if (!event.key.repeat) musicMenu.toggle();
                heldArrow = SDLK_UNKNOWN;
                continue;
            }
            if (musicMenu.open()) {
                if (musicMenu.handle(event)) {
                    Settings settings = screen.settings();
                    settings.value[Settings::Midi] = music.enabled();
                    screen.applySettings(settings);
                    musicEnabled = settings.value[Settings::Midi];
                }
                continue;
            }
            if (event.type == SDL_KEYUP && event.key.keysym.sym == heldArrow) heldArrow = SDLK_UNKNOWN;
            const bool click = event.type == SDL_MOUSEBUTTONDOWN;
            if (screen.messages().open() && screen.eventSceneActive(SDL_GetTicks())) continue; // animation first
            if (screen.messages().question()) { // seg2:2404: obstacle question (text 600)
                if (const int answer = screen.messages().handle(event); answer) {
                    const int text = screen.messages().frontText();
                    screen.messages().dismiss();
                    sound.play(0, 2); // seg6:5627: Plopp02 when the window closes
                    if (text == 700) { // seg4:0F20
                        if (answer == 6) running = false;
                        else if (quitReopensMain) openMenu("MAIN1");
                    } else if (play.game().answerObstacle(answer == 6)) {
                        report();
                    }
                }
                continue;
            }
            if (screen.messages().open()) {
                if (event.type == SDL_KEYDOWN || click) screen.messages().dismiss();
                continue;
            }
            if (menu.isOpen()) {
                const int id = menu.handle(event);
                if (const int s = menu.takeSound(); s >= 0) sound.play(2, s);
                if (id) command(id);
                continue;
            }
            const bool endDemo = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                                 || (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT);
            if (!screen.title() && !play.game().humansLeft() && endDemo) { // seg1:021D: a game of computers ends
                play.game().requestDemoEnd();
                screen.messages().clear();
                menu.close();
                openTitle();
                continue;
            }
            if (screen.announcement().active()) { // assumption: a key or click ends it early
                if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) screen.announcement().stop();
                continue;
            }
            if (screen.balance().active()) { // a click lets the show master go on (seg8:8D5C)
                if (event.type == SDL_MOUSEBUTTONDOWN || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN))
                    screen.balance().click();
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                    screen.balance().stop(); // assumption
                continue;
            }
            if (screen.victory().active()) { // Enter closes it (seg7:7D57 sends VK_RETURN after 5000 passes)
                if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) screen.victory().stop();
                continue;
            }
            if (screen.finish().active()) { // assumption: a key or click cuts the sequence short
                if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) screen.finish().stop();
                continue;
            }
            if (screen.pictureShown()) { // a key or a click closes it (assumption)
                if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) {
                    screen.closePicture();
                    if (screen.title()) openTitle();
                }
                continue;
            }
            if (screen.options().isOpen()) {
                const OptionsView::Result r = screen.options().handle(event);
                if (const int fx = screen.options().takeSound(); fx >= 0) sound.play(2, fx);
                if (r == OptionsView::Result::Accept) screen.applySettings(screen.options().settings());
                if (r != OptionsView::Result::None) {
                    screen.options().close();
                    if (screen.title()) openTitle();
                }
                continue;
            }
            if (screen.overview().isOpen()) { screen.overview().handle(event); continue; }
            if (screen.handleSurroundings(event)) continue;
            if (screen.setup().isOpen()) { // seg1:0A60
                const SetupView::Result r = screen.setup().handle(event);
                if (const int fx = screen.setup().takeSound(); fx >= 0) sound.play(2, fx);
                if (r == SetupView::Result::Start) {
                    std::string error;
                    const uint16_t seed = uint16_t(SDL_GetTicks() ^ uint32_t(std::time(nullptr))); // seg1:02A9: time
                    const auto status = screen.setup().status();
                    const auto names = screen.setup().names();
                    screen.setup().close();
                    if (play.newGame(status, names, seed, error)) startGame(true);
                    else { std::cerr << "Neues Spiel: " << error << "\n"; openTitle(); }
                } else if (r == SetupView::Result::Cancel) {
                    screen.setup().close();
                    openTitle();
                }
                continue;
            }
            if (saveDialog.isOpen()) { // seg8:B2F0: modal until Ok or Nein
                const SaveView::Result r = saveDialog.handle(event);
                if (r == SaveView::Result::Confirm && !saveDialog.saving()) { // load (title)
                    std::string error;
                    saveDialog.close();
                    if (screen.saveSlots().load(saveDialog.slot(), play.game(), error)) startGame();
                    else { // seg8:B3F9: Zaehler2
                        sound.play(2, 7);
                        std::cerr << "Laden fehlgeschlagen: " << error << "\n";
                        openTitle();
                    }
                    continue;
                }
                if (r == SaveView::Result::Close && screen.title()) { saveDialog.close(); openTitle(); continue; }
                if (r == SaveView::Result::Confirm) {
                    std::string error;
                    const int slot = saveDialog.slot();
                    const bool saved = screen.saveSlots().save(slot, play.game(), saveDescription(play.game(), screen.texts()), error);
                    if (!saved) sound.play(6, 17); // seg8:B4DC: Fate01 when saving fails
                    if (saved)
                        std::cout << "Gespeichert: " << screen.saveSlots().file(slot).string() << "\n";
                    else
                        std::cerr << "Speichern fehlgeschlagen: " << error << "\n";
                }
                if (r != SaveView::Result::None) saveDialog.close();
                continue;
            }
            // Risiko (seg7:6BF0): any key but Escape or the CHANCE area presses, Escape or
            // the STOP area stops; computers press inside the tick.
            if (play.game().phase() == Game::Phase::Risiko && !play.game().isComputer(play.game().currentPlayer())
                && !screen.messages().open() && !menu.isOpen() && !saveDialog.isOpen()) {
                int button = 0;
                if (click && event.button.button == SDL_BUTTON_LEFT)
                    button = screen.risiko().buttonAt(event.button.x, event.button.y);
                else if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.sym != SDLK_F8)
                    button = event.key.keysym.sym == SDLK_ESCAPE ? 2 : 1;
                if (button) {
                    screen.risiko().press(button, SDL_GetTicks());
                    if (button == 1 ? play.game().risikoPress() : play.game().risikoStop()) report();
                }
                if (button || event.type == SDL_KEYDOWN || click) continue;
            }
            if (screen.invest().isOpen() && !screen.messages().open()) { // seg8:7098
                const InvestView::Result r = screen.invest().handle(event);
                if (r.sound >= 0) sound.play(0, r.sound);
                if (r.done) {
                    screen.invest().close();
                    if (r.amount > 0 && play.game().invest(r.amount)) report();
                    else { play.game().declineInvestment(); report(); }
                }
                continue;
            }
            if (screen.nails().isOpen()) { // seg3:1E5E
                const int d = screen.nails().handle(event);
                if (d != 0) screen.nails().close();
                if (d > 0 && play.game().playCard(nailsCardSlot, d)) report();
                continue;
            }
            if (screen.flight().isOpen()) { // seg3:3384
                const FlightView::Result r = screen.flight().handle(event);
                if (r.closed) screen.messages().push(Notice{8300, {}}); // Bodennebel over Brussels
                if (r.cancel) screen.flight().close();
                if (!r.town.empty()) {
                    screen.flight().close();
                    // seg3:32CE: the town whose name equals the booked one (_fstricmp).
                    int town = -1;
                    for (size_t t = 0; t < towns.size() && town < 0; ++t)
                        if (sameName(oemToUtf8(towns[t].name), r.town)) town = int(t);
                    if (town >= 0 && play.game().playCard(flightCardSlot, town)) report();
                }
                continue;
            }
            if (screen.selectPlayer().isOpen()) { // SELPLAYER (seg2:5386)
                const SbButtonGroup::Input input = screen.selectPlayer().handle(event);
                if (input.sound >= 0) sound.play(2, input.sound);
                if (input.id >= 0) {
                    screen.selectPlayer().close();
                    if (play.game().playCard(targetCardSlot, input.id)) report();
                } else if (input.cancel) {
                    screen.selectPlayer().close();
                }
                continue;
            }
            if (cards.isOpen()) { // the card buttons; Escape or the right button closes the window
                // SBBUTTON (seg1:0E78): a card plays when the button is released over it,
                // or by space / Return on the focused card; arrows move the focus.
                CardView::Input input;
                if (click && event.button.button == SDL_BUTTON_LEFT)
                    input = cards.mouseDown(event.button.x, event.button.y);
                else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
                    input = cards.mouseUp(event.button.x, event.button.y);
                else if (event.type == SDL_KEYDOWN)
                    input = cards.key(event.key.keysym.sym, event.key.keysym.mod);
                if (input.sound >= 0) sound.play(2, input.sound);
                if (input.cancel && !play.game().yellowCardPending()) { cards.close(); continue; } // no cancel in discard mode
                if (input.id >= 0) {
                    const int slot = input.id;
                    // seg2:51EA: a card with a target asks for the player when more than one is
                    // possible (excluded: the player; Tschüss excludes nobody, argument 0xFF).
                    const Game& g = play.game();
                    if (g.yellowCardPending()) { // seg3:1057: discard mode, the clicked card is dropped
                        if (play.game().discardForCard(slot)) report();
                        cards.close();
                        continue;
                    }
                    const CardKind kind = cardKind(g.deck().typeOfSlot(g.hand(g.currentPlayer())[size_t(slot)]));
                    if (cardNeedsTarget(kind) || kind == CardKind::Tschuess) {
                        std::vector<int> candidates;
                        std::array<uint8_t, kPlayerSlots> status{};
                        for (int p = 0; p < kPlayerSlots; ++p) {
                            status[size_t(p)] = g.players()[size_t(p)].status;
                            if (status[size_t(p)] && (kind == CardKind::Tschuess || p != g.currentPlayer()))
                                candidates.push_back(p);
                        }
                        if (candidates.size() > 1) {
                            targetCardSlot = slot;
                            screen.selectPlayer().open(candidates, status);
                            continue;
                        }
                        if (candidates.size() == 1 && play.game().playCard(slot, candidates.front())) report();
                        continue;
                    }
                    if (kind == CardKind::Naegel && g.nailsInWater()) { // seg3:1CCF: into the water, no window
                        if (play.game().playCard(slot, 0)) report();
                        continue;
                    }
                    if (kind == CardKind::Naegel) { // seg3:1C96: the direction window
                        nailsCardSlot = slot;
                        screen.nails().open();
                        continue;
                    }
                    if (kind == CardKind::Flugzeug) { // seg3:31D5: the booking window
                        flightCardSlot = slot;
                        screen.flight().open(g.eastOpen(), g.remainingStages() == 0);
                        continue;
                    }
                    if (play.handleKey(SDL_Keycode(SDLK_1 + slot), 0)) report();
                    continue;
                }
                const bool mustDiscard = play.game().yellowCardPending(); // no cancel in discard mode
                if (!mustDiscard && click && event.button.button == SDL_BUTTON_RIGHT) { cards.close(); continue; }
                if (event.type == SDL_KEYDOWN || click) continue; // the window keeps the keyboard
            }
            if (screen.cardShop().isOpen()) { // seg3:4640
                const auto action = screen.cardShop().handle(event, play.game());
                using Kind = CardShopView::Action::Kind;
                Game& g = play.game();
                if (action.sound >= 0) sound.play(2, action.sound);
                if (action.kind == Kind::Buy || action.kind == Kind::Sell) {
                    const bool done = action.kind == Kind::Buy ? g.buyCard(action.value) : g.sellCard(action.value);
                    sound.play(2, done ? 2 : 7); // seg3:4782 / 4850: Plopp02 or Zaehler2
                    if (done) report();
                } else if (action.kind == Kind::Close) { // Beenden: the stop ends the turn
                    screen.cardShop().close();
                    g.endTurn();
                    report();
                }
                continue;
            }
            if (estates.isOpen() && !cards.isOpen()) {
                const auto action = estates.handle(event, screen.estateRows());
                using Kind = EstatesView::Action::Kind;
                Game& g = play.game();
                if (action.kind == Kind::Row) {
                    const bool done = estates.tab() == EstatesTab::Sell ? g.sellProperty(action.entry)
                                                                        : g.buyProperty(action.entry);
                    sound.play(2, done ? 2 : 7); // seg1:5598 / 55D0 (5633 / 566B): Plopp02 or Zaehler2
                    if (done) report();
                } else if (action.kind == Kind::Tab) {
                    sound.play(2, 2); // seg1:5436 / 546D: Plopp02
                } else if (action.kind == Kind::Cards) { // the card window for property cards
                    sound.play(2, 2); // seg1:54B2
                    if (g.openCardWindow() && cards.open(g, 1)) cardWindowState = turnState();
                } else if (action.kind == Kind::Close && estates.statistics()) {
                    estates.close();
                } else if (action.kind == Kind::Close) { // leaving the town ends the turn
                    estates.close();
                    g.endTurn();
                    report();
                }
                if (event.type != SDL_KEYDOWN || action.kind != Kind::None) continue;
                const SDL_Keycode k = event.key.keysym.sym;
                if (k == SDLK_UP || k == SDLK_DOWN || k == SDLK_RETURN || k == SDLK_KP_ENTER) continue;
            }
            if (click && event.button.button == SDL_BUTTON_LEFT && !modal && !moving && !screen.title()) {
                // seg6:1798 -> seg4:0F52: a click on the map opens the steering cross there.
                const Game& g = play.game();
                const ScreenRect& map = screen.layout().map;
                const bool onMap = event.button.x >= map.x && event.button.x < map.x + map.w && event.button.y >= map.y
                                   && event.button.y < map.y + map.h;
                if (onMap && !screen.surroundings() && g.currentPlayer() >= 0 && !g.isComputer(g.currentPlayer())
                    && (g.phase() == Game::Phase::Move || g.phase() == Game::Phase::Arrived))
                    screen.mouseControl().open(event.button.x, event.button.y);
                continue;
            }
            if (event.type != SDL_KEYDOWN) continue;
            const SDL_Keycode key = event.key.keysym.sym;
            if (isArrow(key) && play.game().phase() == Game::Phase::Move) { heldArrow = key; continue; }
            if (key == SDLK_INSERT || key == SDLK_DELETE) { screen.toggleZoom(); continue; } // seg4:0212
            if (key >= SDLK_1 && key <= SDLK_4) { // seg4:018B -> seg6:0DA0(key - 1, 1): pause and memo
                Settings s = screen.settings();
                s.pause = int(key - SDLK_1);
                screen.applySettings(s);
                sound.play(0, 35 + s.pause); // MEMO01-04 (seg6:0DFD / 0DCA / 0DDB / 0DEC)
                continue;
            }
            if (key == SDLK_q && !screen.title()) { askQuit(false); continue; } // seg4:0259: Q -> seg4:0EDF
            if (key == SDLK_SPACE && play.game().phase() != Game::Phase::Risiko && !screen.title()) {
                screen.centreOnPlayer(); // seg4:01B2 -> seg2:3216, 05A8
                continue;
            }
            const bool risiko = play.game().phase() == Game::Phase::Risiko;
            if ((key == SDLK_ESCAPE || key == SDLK_BACKSPACE) && !risiko) openMenu("RIGHTBTN"); // seg4:01D1
            else if (play.handleKey(key == SDLK_ESCAPE ? SDLK_BACKSPACE : key, event.key.keysym.mod)) report();
        }
        SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
        SDL_RenderClear(renderer.get());
        screen.draw();
        musicMenu.draw(screen.textRenderer(), screen.options().isOpen());
        const bool last = options.smokeTest && frame >= options.frames;
        if (!options.screenshot.empty() && (last || !options.smokeTest)) {
            int outW = layout.width, outH = layout.height; // the scaled output, borders included
            SDL_GetRendererOutputSize(renderer.get(), &outW, &outH);
            Surface shot(SDL_CreateRGBSurfaceWithFormat(0, outW, outH, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
            // ReadPixels(NULL) reads the viewport; without the logical size it is the whole output.
            SDL_RenderSetLogicalSize(renderer.get(), 0, 0);
            SDL_RenderSetViewport(renderer.get(), nullptr);
            const bool read = shot && SDL_RenderReadPixels(renderer.get(), nullptr, SDL_PIXELFORMAT_ARGB8888,
                                                           shot->pixels, shot->pitch) == 0;
            SDL_RenderSetLogicalSize(renderer.get(), layout.width, layout.height);
            if (!read || SDL_SaveBMP(shot.get(), options.screenshot.c_str()) != 0) throw std::runtime_error(SDL_GetError());
        }
        SDL_RenderPresent(renderer.get());
        if (last) break;
        SDL_Delay(16);
    }
    return 0;
}

} // namespace dtvr
