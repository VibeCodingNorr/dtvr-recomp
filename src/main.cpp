// SPDX-License-Identifier: GPL-3.0-only
#include "board.hpp"
#include "game.hpp"
#include "rules.hpp"
#include "sbs.hpp"
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
using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;
using Texture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

static fs::path programDirectory(fs::path root) {
    if (fs::is_regular_file(root / "program/butt/1/map.bmp")) root /= "program";
    if (!fs::is_regular_file(root / "butt/1/map.bmp"))
        throw std::runtime_error("Kein Spielverzeichnis: " + root.string());
    return root;
}

enum class View { Overview, Tiles, Sprites, World };

static Surface loadBmp(const fs::path& path) {
    Surface image(SDL_LoadBMP(path.c_str()), SDL_FreeSurface);
    if (!image) throw std::runtime_error(path.string() + ": " + SDL_GetError());
    return image;
}

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

static std::vector<uint8_t> readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Nicht lesbar: " + path.string());
    return {std::istreambuf_iterator<char>(in), {}};
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

// Composes europe.map from the season's SBB tiles as rally.exe does
// (loader seg5:186A, cell lookup seg6:26B6, draw passes seg6:28D5).
static Surface loadWorld(const fs::path& root, int month, bool views, fs::path& shown) {
    const int season = dtvr::seasonForMonth(month);
    std::vector<dtvr::Tile> tiles;
    Surface palette(nullptr, SDL_FreeSurface);
    for (int part = 0;; ++part) {
        std::ostringstream name;
        name << "part" << std::setw(4) << std::setfill('0') << part << ".sbb";
        const auto path = root / "map" / std::to_string(season) / name.str();
        if (!fs::is_regular_file(path)) break; // the original also stops at the first missing part
        auto strip = loadBmp(path);
        if (strip->format->BitsPerPixel != 8 || !strip->format->palette || strip->h != 16 * 192)
            throw std::runtime_error("Unerwartetes SBB-Format: " + path.string());
        const auto part_tiles = dtvr::splitSbb(static_cast<const uint8_t*>(strip->pixels),
                                               strip->w, strip->h, strip->pitch);
        tiles.insert(tiles.end(), part_tiles.begin(), part_tiles.end());
        if (!palette) palette = std::move(strip);
    }
    if (!palette) throw std::runtime_error("Keine SBB-Dateien fuer Jahreszeit " + std::to_string(season));
    const dtvr::WorldMap map(readFile(root / "map/europe.map"));
    const auto pixels = dtvr::renderWorld(map, tiles, views);
    constexpr int side = dtvr::WorldMap::kSize * dtvr::WorldMap::kTile;
    Surface world(SDL_CreateRGBSurfaceWithFormat(0, side, side, 8, SDL_PIXELFORMAT_INDEX8), SDL_FreeSurface);
    if (!world || SDL_SetSurfacePalette(world.get(), palette->format->palette) != 0)
        throw std::runtime_error(SDL_GetError());
    for (int y = 0; y < side; ++y)
        std::copy_n(&pixels[size_t(y) * side], side, static_cast<uint8_t*>(world->pixels) + y * world->pitch);
    shown = root / "map/europe.map";
    std::cout << "Weltkarte: Monat " << month % 12 << ", map/" << season << ", "
              << tiles.size() << " Bausteine, Views " << (views ? "an" : "aus") << "\n";
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

static int run(int argc, char** argv) {
    fs::path root, screenshot;
    bool smoke = false, views = true, graph = false, rollAtStart = false, play = false;
    int humans = 2, rounds = 99;
    int month = 2, roll = 0;
    unsigned seed = 1;
    View view = View::Overview;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "dtvr-recomb --data PFAD [--tiles|--sprites|--world [--month 0-11] [--no-views] [--graph] [--roll] [--seed N]]\n"
                         "            [--play [--players 1-8] [--rounds N] [--seed N]]\n"
                         "            [--smoke-test] [--screenshot DATEI.bmp]\n"
                         "PFAD: CD-Wurzel oder program-Verzeichnis.\n"
                         "M: Uebersicht/Bausteine/Sprites/Weltkarte | Links/Rechts: Datei bzw. Monat\n"
                         "V: Baustein 0x33 (Views) | G: Spielfelder und Staedte | Mausrad: Zoom\n"
                         "W: Wuerfeln (Automat, 1 Walze) | Rechtsklick: Startfeld waehlen\n"
                         "--play: W wuerfeln, Pfeiltasten ein Feld fahren, Enter Zug beenden\n"
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
        else if (arg == "--players" && i + 1 < argc) humans = std::stoi(argv[++i]);
        else if (arg == "--rounds" && i + 1 < argc) rounds = std::stoi(argv[++i]);
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
    // Hotseat game from the recovered rules (src/game.cpp).
    std::unique_ptr<dtvr::Game> game;
    auto report = [&] {
        if (!game) return;
        for (const auto& m : game->messages()) std::cout << m << "\n";
        static const char* phases[] = {"wuerfeln (W)", "fahren (Pfeile)", "Zug beenden (Enter)", "Spielende"};
        const int p = game->currentPlayer();
        const std::string status = "DTVR Recomb | Spieler " + std::to_string(p + 1) + " | Monat "
            + std::to_string(game->calendar().month % 12 + 1) + " | Schritte " + std::to_string(game->stepsTaken())
            + "/" + std::to_string(game->allowance()) + " | Ziel " + game->townName(game->destination())
            + " | Etappen " + std::to_string(game->remainingStages()) + " | " + phases[int(game->phase())];
        SDL_SetWindowTitle(window.get(), status.c_str());
    };
    if (play) {
        if (board.fields.empty()) throw std::runtime_error("Spielbrett fehlt");
        game = std::make_unique<dtvr::Game>(board, towns, humans, rounds, uint16_t(seed));
        graph = true;
    }
    load();
    report();
    if (game && rollAtStart) { game->roll(); report(); }
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) fit();
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
                    if (game) { game->roll(); report(); }
                    else if (view == View::World && !board.fields.empty()) rollDie();
                    break;
                case SDLK_RETURN: if (game) { game->endTurn(); report(); } break;
                case SDLK_UP: if (game) { game->step(dtvr::Direction::Up); report(); } break;
                case SDLK_DOWN: if (game) { game->step(dtvr::Direction::Down); report(); } break;
                case SDLK_RIGHT:
                    if (game) { game->step(dtvr::Direction::Right); report(); } else step(1);
                    break;
                case SDLK_LEFT:
                    if (game) { game->step(dtvr::Direction::Left); report(); } else step(-1);
                    break;
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
            if (game) {
                // Stage origin (white), destination (yellow), players (coloured, current larger).
                static const SDL_Color colours[8] = {{255, 60, 60, 255}, {60, 160, 255, 255}, {60, 220, 60, 255},
                    {255, 160, 0, 255}, {200, 60, 255, 255}, {0, 230, 230, 255}, {255, 255, 255, 255}, {120, 120, 120, 255}};
                mark(game->origin(), {255, 255, 255, 255}, r * 1.6f);
                if (game->destination() >= 0) mark(game->destination(), {255, 230, 0, 255}, r * 2.0f);
                for (int p = 0; p < dtvr::kPlayerSlots; ++p) {
                    if (!game->players()[size_t(p)].status) continue;
                    const auto& f = board.fields[size_t(game->playerField(p))];
                    const SDL_Color c = colours[p];
                    const float size = (p == game->currentPlayer() ? 0.9f : 0.5f) * r;
                    const SDL_FRect box{x + (f.x + 0.5f) * cell - size + p, y + (f.y + 0.5f) * cell - size + p,
                                        2 * size, 2 * size};
                    SDL_SetRenderDrawColor(renderer.get(), c.r, c.g, c.b, 255);
                    SDL_RenderFillRectF(renderer.get(), &box);
                }
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
