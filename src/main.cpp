// SPDX-License-Identifier: GPL-3.0-only
#include "board.hpp"
#include "game_screen.hpp"
#include "ne_strings.hpp"
#include "play_view.hpp"
#include "rules.hpp"
#include "sbs.hpp"
#include "sdl_assets.hpp"
#include "worldmap.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <memory>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using dtvr::Surface;
using dtvr::Texture;
using dtvr::loadBmp;
using dtvr::readFile;

static fs::path programDirectory(fs::path root) {
    if (fs::is_regular_file(root / "program/butt/1/map.bmp")) root /= "program";
    if (!fs::is_regular_file(root / "butt/1/map.bmp"))
        throw std::runtime_error("Kein Spielverzeichnis: " + root.string());
    return root;
}

enum class View { Overview, Tiles, Sprites, World };

static Surface loadTiles(const fs::path& path) {
    auto image = loadBmp(path);
    // SBB files are BMP strips. Present their 16x16 blocks in a grid;
    // this is an inspection layout, not a reconstructed world map.
    if (image->w != 16 || image->h % 16 != 0)
        throw std::runtime_error("Unerwartete SBB-Abmessungen: " + path.string());
    const int count = image->h / 16;
    Surface grid(SDL_CreateRGBSurfaceWithFormat(0, 16 * 16,
                 ((count + 15) / 16) * 16, 32, SDL_PIXELFORMAT_RGBA32), SDL_FreeSurface);
    if (!grid) throw std::runtime_error(SDL_GetError());
    for (int i = 0; i < count; ++i) {
        SDL_Rect source{0, i * 16, 16, 16};
        SDL_Rect target{(i % 16) * 16, (i / 16) * 16, 16, 16};
        if (SDL_BlitSurface(image.get(), &source, grid.get(), &target) != 0)
            throw std::runtime_error(SDL_GetError());
    }
    return grid;
}

// Lays out all SBS frames in rows. The palette comes from the overview BMP and
// index 0 is drawn as a checkerboard; both are working hypotheses, see docs/formats.md.
static Surface loadSprites(const fs::path& path, const SDL_Palette& palette) {
    const auto file = dtvr::parseSbs(readFile(path));
    constexpr int gap = 4, maxRow = 640;
    int width = gap, height = gap, rowW = gap, rowH = 0;
    std::vector<SDL_Point> at;
    for (const auto& f : file.frames) {
        if (rowW > gap && rowW + f.width + gap > maxRow) { height += rowH + gap; rowW = gap; rowH = 0; }
        at.push_back({rowW, height});
        rowW += f.width + gap; rowH = std::max(rowH, int(f.height));
        width = std::max(width, rowW);
    }
    height += rowH + gap;
    Surface sheet(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
    if (!sheet) throw std::runtime_error(SDL_GetError());
    SDL_FillRect(sheet.get(), nullptr, 0xff202530);
    for (size_t i = 0; i < file.frames.size(); ++i) {
        const auto& f = file.frames[i];
        for (int y = 0; y < f.height; ++y) {
            auto* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(sheet->pixels)
                        + (at[i].y + y) * sheet->pitch) + at[i].x;
            for (int x = 0; x < f.width; ++x) {
                const uint8_t index = f.pixels[size_t(y) * f.width + x];
                if (index == 0) { row[x] = ((x / 4 + y / 4) & 1) ? 0xff505050 : 0xff707070; continue; }
                const SDL_Color c = index < palette.ncolors ? palette.colors[index] : SDL_Color{255, 0, 255, 255};
                row[x] = 0xff000000u | uint32_t(c.r) << 16 | uint32_t(c.g) << 8 | c.b;
            }
        }
    }
    std::cout << path.filename().string() << ": " << file.frames.size() << " Bilder\n";
    return sheet;
}

// Composed world map (src/sdl_assets.cpp) with the viewer's log line.
static Surface loadWorld(const fs::path& root, int month, bool views, fs::path& shown) {
    auto world = dtvr::loadWorld(root, month, views);
    shown = root / "map/europe.map";
    std::cout << "Weltkarte: Monat " << month % 12 << ", map/" << dtvr::seasonForMonth(month) << ", Views "
              << (views ? "an" : "aus") << "\n";
    return world;
}

static std::vector<fs::path> findFiles(const fs::path& dir, const std::string& extension) {
    std::vector<fs::path> result;
    if (fs::is_directory(dir))
        for (const auto& entry : fs::recursive_directory_iterator(dir))
            if (entry.is_regular_file() && entry.path().extension() == extension)
                result.push_back(entry.path());
    std::sort(result.begin(), result.end());
    return result;
}

// The game from the recovered rules with slots and driver names (viewer and game window).
static std::unique_ptr<dtvr::PlayView> makePlayView(const fs::path& root, const dtvr::Board& board,
                                                    const std::vector<dtvr::Town>& towns, int humans, int computers,
                                                    int rounds, unsigned seed) {
    if (board.fields.empty()) throw std::runtime_error("Spielbrett fehlt");
    std::unique_ptr<dtvr::PlayView> playView;
    // Slots as [Player] P0..P7: humans first, then computers (status 2).
    if (humans < 0 || computers < 0 || humans + computers < 1 || humans + computers > dtvr::kPlayerSlots)
        throw std::runtime_error("--players und --computers: zusammen 1 bis 8");
    std::array<uint8_t, dtvr::kPlayerSlots> slots{};
    for (int p = 0; p < humans + computers; ++p) slots[size_t(p)] = p < humans ? 1 : 2;
    playView = std::make_unique<dtvr::PlayView>(
        board, towns, std::make_unique<dtvr::WorldMap>(readFile(root / "map/europe.map")),
        std::make_unique<dtvr::TileProperties>(readFile(root / "map/europe.pit")), slots, rounds,
        uint16_t(seed));
    // Driver names from the language file (texts 120-127), when present.
    const auto language = root / "dat/d/rally_d.dat";
    if (std::filesystem::exists(language)) {
        const auto texts = dtvr::readNeStrings(readFile(language));
        std::array<std::string, dtvr::kPlayerSlots> names;
        for (int p = 0; p < dtvr::kPlayerSlots; ++p)
            if (auto it = texts.find(dtvr::kDriverNameText + p); it != texts.end())
                names[size_t(p)] = dtvr::ansiToUtf8(it->second);
        playView->game().setPlayerNames(names);
    }
    return playView;
}

static int run(int argc, char** argv) {
    fs::path root, screenshot;
    std::string menuName; // --game: open this menu resource at the start (screenshots)
    bool openCards = false; // --game: open the card window at the start (screenshots)
    bool holdRight = false; // --game: hold the right arrow key (tests of driving)
    bool openSave = false;  // --game: open the save dialog (screenshots)
    bool openStatistics = false; // --game: open Statistik (screenshots)
    int loadSlot = -1;           // --game: load save slot 0-8
    bool noTitle = false;        // --game: start the game without the title menu
    bool intro = false, noIntro = false; // --game: force or skip the intro before the title
    bool computerCards = false;          // --game: computers also play the cards rally.exe never picks
    bool openSetup = false;      // --game: open the player setup (screenshots)
    bool openStages = false;     // --game: open the stage count window (screenshots)
    bool openOverview = false;   // --game: open Übersicht (screenshots)
    bool openSurroundings = false; // --game: start Umfeld (tests)
    bool zoomIn = false;           // --game: zoom in (screenshots)
    bool openOptions = false;      // --game: open the options window (screenshots)
    bool demo = false;             // --game: start the demo
    bool finish = false;           // --game: show the finish sequence (screenshots)
    bool victory = false;          // --game: show the victory sequence (screenshots)
    bool balance = false;          // --game: show the year balance (screenshots)
    bool announce = false;         // --game: show the destination announcement (screenshots)
    bool cardShop = false;         // --game: open the card shop (screenshots)
    bool selectPlayer = false;     // --game: open the target choice (screenshots)
    bool flight = false;           // --game: open the Flugzeug booking (screenshots)
    bool nails = false;            // --game: open the Nägel direction choice (screenshots)
    bool invest = false;           // --game: open the investment offer (screenshots)
    bool fullscreen = false;       // --game: start in fullscreen (F11 / Alt+Enter toggle)
    int openEstates = -1;   // --game: show the town dialog of this town (screenshots)
    bool smoke = false, views = true, graph = false, rollAtStart = false, play = false, gameWindow = false;
    int humans = 2, computers = 0, rounds = 99, resolution = 0, frames = 1;
    int month = 2, roll = 0;
    unsigned seed = 1;
    View view = View::Overview;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--version") {
            std::cout << "DTVR Recomb " << DTVR_VERSION << '\n';
            return 0;
        } else if (arg == "--help") {
            std::cout << "dtvr-recomb --data PFAD [--tiles|--sprites|--world [--month 0-11] [--no-views] [--graph] [--roll] [--seed N]]\n"
                         "            [--play|--game [--resolution 640|800|1024] [--no-title] [--intro|--no-intro] [--computer-cards] [--load 0-8] [--menu NAME] [--cards] [--estates STADT] [--players 0-8] [--computers 0-8] [--rounds N] [--seed N]]\n"
                         "            [--smoke-test] [--screenshot DATEI.bmp]\n"
                         "PFAD: CD-Wurzel oder program-Verzeichnis.\n"
                         "M: Uebersicht/Bausteine/Sprites/Weltkarte | Links/Rechts: Datei bzw. Monat\n"
                         "V: Baustein 0x33 (Views) | G: Spielfelder und Staedte | Mausrad: Zoom\n"
                         "W: Wuerfeln (Automat, 1 Walze) | Rechtsklick: Startfeld waehlen\n"
                         "--play: W wuerfeln, 1-8 Handkarte spielen, Pfeiltasten ein Feld fahren, Enter Zug beenden\n"
                         "        in einer Stadt nach der Ankunft: 1-9, 0 Grundstueck kaufen, mit Umschalt verkaufen\n"
                         "        T: Zielspieler fuer Karten (ab 3 Spielern)\n"
                         "Ziehen mit linker Maustaste: Verschieben | R: Einpassen | Esc: Ende\n";
            return 0;
        } else if ((arg == "--data" || arg == "--screenshot") && i + 1 < argc) {
            (arg == "--data" ? root : screenshot) = argv[++i];
        } else if (arg == "--smoke-test") smoke = true;
        else if (arg == "--tiles") view = View::Tiles;
        else if (arg == "--sprites") view = View::Sprites;
        else if (arg == "--world") view = View::World;
        else if (arg == "--no-views") views = false;
        else if (arg == "--graph") graph = true;
        else if (arg == "--roll") rollAtStart = true;
        else if (arg == "--play") { play = true; view = View::World; }
        else if (arg == "--game") { gameWindow = true; view = View::World; }
        else if (arg == "--resolution" && i + 1 < argc) resolution = std::stoi(argv[++i]);
        else if (arg == "--frames" && i + 1 < argc) frames = std::stoi(argv[++i]);
        else if (arg == "--menu" && i + 1 < argc) menuName = argv[++i];
        else if (arg == "--cards") openCards = true;
        else if (arg == "--hold-right") holdRight = true;
        else if (arg == "--save-dialog") openSave = true;
        else if (arg == "--statistics") openStatistics = true;
        else if (arg == "--load" && i + 1 < argc) loadSlot = std::stoi(argv[++i]);
        else if (arg == "--no-title") noTitle = true;
        else if (arg == "--intro") intro = true;
        else if (arg == "--no-intro") noIntro = true;
        else if (arg == "--computer-cards") computerCards = true;
        else if (arg == "--setup") openSetup = true;
        else if (arg == "--stages") openStages = true;
        else if (arg == "--overview") openOverview = true;
        else if (arg == "--surroundings") openSurroundings = true;
        else if (arg == "--zoom") zoomIn = true;
        else if (arg == "--options") openOptions = true;
        else if (arg == "--demo") demo = true;
        else if (arg == "--finish") finish = true;
        else if (arg == "--victory") victory = true;
        else if (arg == "--balance") balance = true;
        else if (arg == "--announce") announce = true;
        else if (arg == "--card-shop") cardShop = true;
        else if (arg == "--select-player") selectPlayer = true;
        else if (arg == "--flight") flight = true;
        else if (arg == "--nails") nails = true;
        else if (arg == "--invest") invest = true;
        else if (arg == "--fullscreen") fullscreen = true;
        else if (arg == "--estates" && i + 1 < argc) openEstates = std::stoi(argv[++i]);
        else if (arg == "--players" && i + 1 < argc) humans = std::stoi(argv[++i]);
        else if (arg == "--rounds" && i + 1 < argc) rounds = std::stoi(argv[++i]);
        else if (arg == "--computers" && i + 1 < argc) computers = std::stoi(argv[++i]);
        else if (arg == "--seed" && i + 1 < argc) seed = unsigned(std::stoul(argv[++i]));
        else if (arg == "--month" && i + 1 < argc) month = std::stoi(argv[++i]);
        else throw std::runtime_error("Unbekanntes oder unvollstaendiges Argument: " + arg);
    }
    if (root.empty()) throw std::runtime_error("Datenpfad fehlt. Aufruf: dtvr-recomb --data PFAD");
    root = programDirectory(root);
    const auto tiles = findFiles(root / "map", ".sbb");
    const auto sprites = findFiles(root, ".sbs");
    if (view == View::Tiles && tiles.empty()) throw std::runtime_error("Keine SBB-Dateien gefunden");
    if (view == View::Sprites && sprites.empty()) throw std::runtime_error("Keine SBS-Dateien gefunden");
    const auto overview = loadBmp(root / "butt/1/map.bmp");
    // Board data for the world-view overlay (loader seg5:14C0); optional elsewhere.
    dtvr::Board board;
    std::vector<dtvr::Town> towns;
    int startField = 0;
    std::vector<int> targets;
    if (view == View::World || fs::is_regular_file(root / "map/europe.inf")) {
        const auto info = dtvr::parseBoardInfo(readFile(root / "map/europe.inf"));
        board = dtvr::parseBoard(readFile(root / "map/europe.str"), info.fields);
        fs::path townFile;
        for (const auto& candidate : findFiles(root / "map", ".tow")) townFile = candidate;
        if (!townFile.empty()) towns = dtvr::parseTowns(readFile(townFile), info.towns);
        if (!towns.empty())
            for (size_t i = 0; i < board.fields.size(); ++i)
                if (board.fields[i].x == towns[0].x && board.fields[i].y == towns[0].y) startField = int(i);
        std::cout << "Spielbrett: " << board.fields.size() << " Felder, " << board.lists.size()
                  << " Feldlisten, " << towns.size() << " Staedte\n";
    }
    if (!overview->format->palette) throw std::runtime_error("map.bmp hat keine Palette");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) throw std::runtime_error(SDL_GetError());
    if (gameWindow) { // the game window as in rally.exe (src/game_screen.cpp)
        auto game = makePlayView(root, board, towns, humans, computers, rounds, seed);
        dtvr::GameWindowOptions options;
        options.canvasWidth = resolution;
        options.smokeTest = smoke;
        options.frames = frames;
        options.rollAtStart = rollAtStart;
        options.openMenu = menuName;
        options.openCards = openCards;
        options.holdRight = holdRight;
        options.openSave = openSave;
        options.openStatistics = openStatistics;
        options.loadSlot = loadSlot;
        options.openSetup = openSetup;
        options.openStages = openStages;
        options.openOverview = openOverview;
        options.openSurroundings = openSurroundings;
        options.zoom = zoomIn;
        options.openOptions = openOptions;
        options.demo = demo;
        options.finish = finish;
        options.victory = victory;
        options.balance = balance;
        options.announce = announce;
        options.cardShop = cardShop;
        options.selectPlayer = selectPlayer;
        options.flight = flight;
        options.nails = nails;
        options.invest = invest;
        options.fullscreen = fullscreen;
        options.intro = intro;
        options.noIntro = noIntro;
        options.computerExtraCards = computerCards;
        // The title menu comes first unless a test option asks for a specific state.
        options.title = !noTitle && loadSlot < 0 && menuName.empty() && !openCards && !holdRight && !openSave
                        && !openStatistics && openEstates < 0 && !rollAtStart && !openOverview && !openSetup && !openStages
                        && !openSurroundings && !zoomIn && !openOptions && !finish && !victory && !balance && !announce && !cardShop && !selectPlayer && !flight && !nails && !invest;
        options.openEstates = openEstates;
        options.screenshot = screenshot.string();
        const int code = dtvr::runGameWindow(*game, root, board, towns, options);
        SDL_Quit();
        return code;
    }
    struct Quit { ~Quit() { SDL_Quit(); } } quit;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("DTVR Recomb", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1000, 800, SDL_WINDOW_RESIZABLE | (smoke ? SDL_WINDOW_HIDDEN : 0)),
        SDL_DestroyWindow);
    if (!window) throw std::runtime_error(SDL_GetError());
    SDL_SetWindowMinimumSize(window.get(), 320, 240);
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
        SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE), SDL_DestroyRenderer);
    if (!renderer) throw std::runtime_error(SDL_GetError());
    Texture texture(nullptr, SDL_DestroyTexture);
    int width = 0, height = 0;
    size_t tileIndex = 0, spriteIndex = 0;
    float zoom = 1, x = 0, y = 0;
    auto fit = [&] {
        int w, h;
        SDL_GetRendererOutputSize(renderer.get(), &w, &h);
        zoom = std::min(float(w - 32) / width, float(h - 32) / height);
        x = (w - width * zoom) / 2; y = (h - height * zoom) / 2;
    };
    auto load = [&] {
        fs::path path = root / "butt/1/map.bmp";
        Surface surface(nullptr, SDL_FreeSurface);
        if (view == View::Tiles) surface = loadTiles(path = tiles[tileIndex]);
        else if (view == View::Sprites) surface = loadSprites(path = sprites[spriteIndex], *overview->format->palette);
        else if (view == View::World) surface = loadWorld(root, month, views, path);
        else surface = loadBmp(path);
        width = surface->w; height = surface->h;
        texture.reset(SDL_CreateTextureFromSurface(renderer.get(), surface.get()));
        if (!texture) throw std::runtime_error(SDL_GetError());
        const auto title = "DTVR Recomb | " + fs::relative(path, root).string()
            + " | M: Ansicht | Pfeile: Datei | Rad: Zoom | Ziehen | R: Einpassen";
        SDL_SetWindowTitle(window.get(), title.c_str());
        std::cout << "Geladen: " << path << " (" << width << "x" << height << ")\n";
        fit();
    };
    auto step = [&](int delta) {
        if (view == View::World) { month = (month + 12 + delta) % 12; load(); return; }
        auto& files = view == View::Tiles ? tiles : sprites;
        auto& index = view == View::Tiles ? tileIndex : spriteIndex;
        if (view == View::Overview || files.empty()) return;
        index = (index + files.size() + delta) % files.size();
        load();
    };
    dtvr::MsRandom rng{uint16_t(seed)};
    auto rollDie = [&] {
        // WM_USER+6 path: 1 reel, maximum 5 (seg4:0919 -> seg2:6426, seg2:776A).
        const auto reels = dtvr::spinReels(rng, 1, 5);
        roll = 0;
        for (int v : reels) roll += v;
        targets = dtvr::fieldsAfterSteps(board, startField, roll);
        std::cout << "Wurf " << roll << " ab Feld " << startField << ": " << targets.size()
                  << " moegliche Zielfelder\n";
    };
    if (rollAtStart && !play && view == View::World && !board.fields.empty()) rollDie();
    // Hotseat game from the recovered rules (src/game.cpp), shown by src/play_view.cpp.
    std::unique_ptr<dtvr::PlayView> playView;
    auto report = [&] {
        if (!playView) return;
        playView->printLog(std::cout);
        SDL_SetWindowTitle(window.get(), playView->title().c_str());
    };
    if (play) {
        if (board.fields.empty()) throw std::runtime_error("Spielbrett fehlt");
        playView = makePlayView(root, board, towns, humans, computers, rounds, seed);
        graph = true;
    }
    load();
    report();
    if (playView && rollAtStart) { playView->game().roll(); report(); }
    bool running = true;
    while (running) {
        SDL_Event event;
        if (playView && playView->update(SDL_GetTicks())) report();
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) fit();
            if (event.type == SDL_KEYDOWN && playView
                && playView->handleKey(event.key.keysym.sym, event.key.keysym.mod)) {
                report();
                continue;
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE: running = false; break;
                case SDLK_r: fit(); break;
                case SDLK_m:
                    do view = View((int(view) + 1) % 4);
                    while ((view == View::Tiles && tiles.empty()) || (view == View::Sprites && sprites.empty()));
                    load(); break;
                case SDLK_v: if (view == View::World) { views = !views; load(); } break;
                case SDLK_g: graph = !graph; break;
                case SDLK_w:
                    if (view == View::World && !board.fields.empty()) rollDie();
                    break;
                case SDLK_RIGHT: step(1); break;
                case SDLK_LEFT: step(-1); break;
                default: break;
                }
            }
            if (event.type == SDL_MOUSEMOTION && (event.motion.state & SDL_BUTTON_LMASK)) {
                x += event.motion.xrel; y += event.motion.yrel;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT
                && view == View::World && !board.fields.empty()) {
                // Nearest field to the clicked map cell becomes the start field.
                const float cell = dtvr::WorldMap::kTile * zoom;
                const float mx = (event.button.x - x) / cell, my = (event.button.y - y) / cell;
                float best = 1e9f;
                for (size_t i = 0; i < board.fields.size(); ++i) {
                    const float dx = board.fields[i].x + 0.5f - mx, dy = board.fields[i].y + 0.5f - my;
                    if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; startField = int(i); }
                }
                targets = roll > 0 ? dtvr::fieldsAfterSteps(board, startField, roll) : std::vector<int>{};
                const auto& sf = board.fields[size_t(startField)];
                std::cout << "Startfeld " << startField << " (" << int(sf.x) << "," << int(sf.y) << "), Typ "
                          << sf.kind << ": ";
                // Field actions dispatched at seg2:15FC (see docs/formats.md).
                if (sf.kind == 1 || sf.kind == 2) {
                    const auto range = dtvr::moneyRange(sf.kind, month);
                    std::cout << (sf.kind == 1 ? "Geld erhalten " : "Geld bezahlen ") << range.minimum << "-"
                              << range.maximum << " x Inflation (Monat " << month % 12 << ")";
                } else if (sf.kind == 3) std::cout << "naechste Ereigniskarte";
                else if (sf.kind == 4 || sf.kind == 6) {
                    std::cout << "Stadt";
                    for (const auto& t : towns)
                        if (t.x == sf.x && t.y == sf.y) std::cout << " " << dtvr::oemToUtf8(t.name);
                    std::cout << " (als Ziel: 300000 x Inflation)";
                } else if (sf.kind == 5) std::cout << "Kartenfeld";
                std::cout << "\n";
            }
            if (event.type == SDL_MOUSEWHEEL) {
                int mx, my; SDL_GetMouseState(&mx, &my);
                const float direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.f : 1.f;
                const float next = std::clamp(zoom * std::pow(1.2f, direction * event.wheel.y), 0.1f, 32.f);
                x = mx - (mx - x) * next / zoom;
                y = my - (my - y) * next / zoom;
                zoom = next;
            }
        }
        SDL_SetRenderDrawColor(renderer.get(), 20, 23, 30, 255);
        if (SDL_RenderClear(renderer.get()) != 0) throw std::runtime_error(SDL_GetError());
        SDL_FRect target{x, y, width * zoom, height * zoom};
        if (SDL_RenderCopyF(renderer.get(), texture.get(), nullptr, &target) != 0)
            throw std::runtime_error(SDL_GetError());
        if (view == View::World && graph) {
            // Field graph from europe.str: cell centres, links up/right/down/left.
            const float cell = dtvr::WorldMap::kTile * zoom;
            auto cx = [&](int c) { return x + (c + 0.5f) * cell; };
            auto cy = [&](int c) { return y + (c + 0.5f) * cell; };
            // Colours of the marker tile under each kind (layer-1 tile 15 + kind).
            static const SDL_Color kinds[7] = {{0, 0, 0, 255}, {30, 144, 255, 255}, {230, 60, 20, 255},
                {240, 200, 0, 255}, {230, 230, 230, 255}, {190, 60, 190, 255}, {110, 110, 110, 255}};
            SDL_SetRenderDrawColor(renderer.get(), 0, 255, 255, 255);
            for (const auto& f : board.fields)
                for (int k : {1, 2}) // right and down; links are symmetric
                    if (f.next[k] != dtvr::Field::kNone) {
                        const auto& n = board.fields[size_t(f.next[k])];
                        SDL_RenderDrawLineF(renderer.get(), cx(f.x), cy(f.y), cx(n.x), cy(n.y));
                    }
            const float r = std::max(2.f, cell * 0.25f);
            for (const auto& f : board.fields) {
                const SDL_Color c = kinds[f.kind < 7 ? f.kind : 0];
                SDL_SetRenderDrawColor(renderer.get(), c.r, c.g, c.b, 255);
                const SDL_FRect box{cx(f.x) - r, cy(f.y) - r, 2 * r, 2 * r};
                SDL_RenderFillRectF(renderer.get(), &box);
            }
            SDL_SetRenderDrawColor(renderer.get(), 255, 255, 255, 255);
            for (const auto& t : towns) {
                const SDL_FRect box{cx(t.x) - 2 * r, cy(t.y) - 2 * r, 4 * r, 4 * r};
                SDL_RenderDrawRectF(renderer.get(), &box);
            }
        }
        if (view == View::World && !board.fields.empty()) {
            // Start field and the fields a move of exactly `roll` steps can end on.
            const float cell = dtvr::WorldMap::kTile * zoom;
            const float r = std::max(4.f, cell * 0.6f);
            auto mark = [&](int index, SDL_Color c, float size) {
                const auto& f = board.fields[size_t(index)];
                SDL_SetRenderDrawColor(renderer.get(), c.r, c.g, c.b, 255);
                for (int t = 0; t < 2; ++t) {
                    const SDL_FRect box{x + (f.x + 0.5f) * cell - size - t, y + (f.y + 0.5f) * cell - size - t,
                                        2 * (size + t), 2 * (size + t)};
                    SDL_RenderDrawRectF(renderer.get(), &box);
                }
            };
            if (playView) {
                playView->draw(renderer.get(), x, y, zoom);
            } else {
                for (int t : targets) mark(t, {0, 255, 0, 255}, r);
                if (roll > 0 || graph) mark(startField, {255, 255, 255, 255}, r * 1.5f);
            }
        }
        if (!screenshot.empty()) {
            int w, h; SDL_GetRendererOutputSize(renderer.get(), &w, &h);
            Surface shot(SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
            if (!shot || SDL_RenderReadPixels(renderer.get(), nullptr, SDL_PIXELFORMAT_ARGB8888,
                                              shot->pixels, shot->pitch) != 0)
                throw std::runtime_error(SDL_GetError());
            if (SDL_SaveBMP(shot.get(), screenshot.c_str()) != 0) throw std::runtime_error(SDL_GetError());
            screenshot.clear();
        }
        SDL_RenderPresent(renderer.get());
        if (smoke) running = false;
        else SDL_Delay(16);
    }
    return 0;
}

int main(int argc, char** argv) {
    try { return run(argc, argv); }
    catch (const std::exception& error) {
        std::cerr << "Fehler: " << error.what() << '\n';
        return 1;
    }
}
