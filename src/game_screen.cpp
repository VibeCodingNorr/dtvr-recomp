// SPDX-License-Identifier: GPL-3.0-only
#include "game_screen.hpp"

#include "ne_strings.hpp"
#include "panel.hpp"
#include "sbs.hpp"
#include "worldmap.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

// seg2:2D43: portrait letter per slot.
constexpr char kPortraitLetters[8] = {'D', 'F', 'E', 'M', 'U', 'J', 'I', 'V'};

SDL_Rect toSdl(const ScreenRect& r) { return {r.x, r.y, r.w, r.h}; }

std::string upper(std::string s) {
    for (char& c : s) c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

GameScreen::GameScreen(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout,
                       const Board& board, const std::vector<Town>& towns, const Game& game,
                       std::map<int, std::string> texts)
    : renderer_(renderer), root_(root), layout_(layout), board_(board), towns_(towns), game_(game),
      texts_(std::move(texts)), slot_(renderer, root), messages_(renderer, root, layout, texts_, text_),
      menu_(renderer, layout, text_, [this](const std::string& name) { return sprite(name); }),
      cards_(renderer, root, layout) {
    seenSpin_ = game.spinSerial();
    std::cout << "Schrift: " << text_.fontDescription() << "\n";
    // SBS frames (seg6:406D looks them up by name in all loaded files): the panel
    // set of the resolution and the vehicles of the drivers in play (<letter>.sbs).
    loadSprites(fs::path("sbs") / (std::to_string(layout_.artDir) + "_assort.sbs"));
    loadSprites(fs::path("sbs") / "assorted.sbs"); // MENUN / MENUG of the menu window
    for (int p = 0; p < kPlayerSlots; ++p)
        if (game_.players()[size_t(p)].status)
            loadSprites(fs::path("sbs") / (std::string(1, char(std::tolower(kPortraitLetters[p]))) + ".sbs"));
}

void GameScreen::loadSprites(const fs::path& relative) {
    const fs::path file = findCaseInsensitive(root_, relative);
    const fs::path paletteFile = findCaseInsensitive(root_, fs::path("bmp") / std::to_string(layout_.artDir) / "info.bmp");
    if (file.empty() || paletteFile.empty()) { std::cerr << "SBS fehlt: " << relative.string() << "\n"; return; }
    const auto palette = loadBmp(paletteFile); // the game palette (identical in all bitmaps checked)
    for (const SbsFrame& frame : parseSbs(readFile(file)).frames) {
        Surface surface(SDL_CreateRGBSurfaceWithFormat(0, frame.width, frame.height, 8, SDL_PIXELFORMAT_INDEX8),
                        SDL_FreeSurface);
        if (!surface || !palette->format->palette) continue;
        SDL_SetSurfacePalette(surface.get(), palette->format->palette);
        for (int y = 0; y < frame.height; ++y)
            std::copy_n(&frame.pixels[size_t(y) * frame.width], frame.width,
                        static_cast<uint8_t*>(surface->pixels) + y * surface->pitch);
        SDL_SetColorKey(surface.get(), SDL_TRUE, 0); // index 0 is transparent for SBS frames
        sprites_.emplace(upper(frame.name), Texture(SDL_CreateTextureFromSurface(renderer_, surface.get()), SDL_DestroyTexture));
    }
}

SDL_Texture* GameScreen::sprite(const std::string& name) {
    auto it = sprites_.find(upper(name)); // rally.exe asks for "zahl_0", the file holds "ZAHL_0"
    return it == sprites_.end() ? nullptr : it->second.get();
}

SDL_Texture* GameScreen::art(const fs::path& relative) {
    const std::string key = relative.generic_string();
    if (auto it = art_.find(key); it != art_.end()) return it->second.get();
    Texture texture(nullptr, SDL_DestroyTexture);
    const fs::path path = findCaseInsensitive(root_, relative);
    if (!path.empty()) {
        auto surface = loadBmp(path);
        texture.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
    } else {
        std::cerr << "Grafik fehlt: " << key << "\n";
    }
    SDL_Texture* raw = texture.get();
    art_.emplace(key, std::move(texture));
    return raw;
}

SDL_Point GameScreen::viewOrigin() const {
    // Assumption until the scrolling code is traced: the viewport is centred on
    // the current player's field and kept inside the 4096 x 4096 world.
    constexpr int side = WorldMap::kSize * WorldMap::kTile;
    const int field = game_.playerField(game_.currentPlayer());
    const Field& f = board_.fields[size_t(field)];
    const int cx = f.x * WorldMap::kTile + WorldMap::kTile / 2, cy = f.y * WorldMap::kTile + WorldMap::kTile / 2;
    const int w = layout_.viewport.w, h = layout_.viewport.h;
    return {std::clamp(cx - w / 2, 0, side - w) / WorldMap::kTile * WorldMap::kTile,
            std::clamp(cy - h / 2, 0, side - h) / WorldMap::kTile * WorldMap::kTile};
}

void GameScreen::drawMap() {
    // The map window clears its WinG bitmap (PATBLT BLACKNESS, seg6:171E) and draws
    // the world from the tile origin DS:956A/9568 into DS:94DA..94DE.
    const SDL_Rect map = toSdl(layout_.map);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, &map);
    const int season = seasonForMonth(game_.calendar().month);
    if (season != worldSeason_) {
        auto surface = loadWorld(root_, game_.calendar().month, true);
        world_.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        worldSeason_ = season;
    }
    if (!world_) return;
    const SDL_Point origin = viewOrigin();
    const SDL_Rect source{origin.x, origin.y, layout_.viewport.w, layout_.viewport.h};
    const SDL_Rect target{map.x + layout_.viewport.x, map.y + layout_.viewport.y, source.w, source.h};
    SDL_RenderCopy(renderer_, world_.get(), &source, &target);
    // Vehicles (seg2:1846): frame of the facing's animation (table DS:1994, mode 1:
    // <letter>UP / R / DOWN / L 1-4, delay 4 ticks). The code read so far puts the
    // frame's top-left corner at (field * 16, field * 16 + 2 - height) minus the map
    // origin (seg2:1A91), which leaves the car one cell right of and half a cell
    // below the road; the frames are therefore centred on the field cell here
    // (-16, -8) until the missing offset is traced (docs/formats.md, round 24).
    static const char* kFacing[5] = {"", "UP", "R", "DOWN", "L"};
    const Uint32 tick = SDL_GetTicks() / SlotView::kTickMs;
    SDL_RenderSetClipRect(renderer_, &target);
    for (int p = 0; p < kPlayerSlots; ++p) {
        if (!game_.players()[size_t(p)].status) continue;
        const Field& f = board_.fields[size_t(game_.playerField(p))];
        const bool driving = p == game_.currentPlayer() && game_.phase() == Game::Phase::Move;
        const int frameNo = driving ? int(tick / 4 % 4) + 1 : 1;
        const std::string name = std::string(1, kPortraitLetters[p]) + kFacing[std::clamp(game_.facing(p), 1, 4)]
                                 + std::to_string(frameNo);
        SDL_Texture* vehicle = sprite(name);
        if (!vehicle) continue;
        int w = 0, h = 0;
        SDL_QueryTexture(vehicle, nullptr, nullptr, &w, &h);
        const SDL_Rect box{target.x + f.x * WorldMap::kTile - origin.x - 16,
                           target.y + f.y * WorldMap::kTile - origin.y - 8, w, h};
        SDL_RenderCopy(renderer_, vehicle, nullptr, &box);
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
}

void GameScreen::drawPortrait() {
    // PORTRAIT (seg2:2E38): bmp/<n>/P<status>/<letter>p.bmp, "pd" while the driver
    // carries Dr. Drago (seg2:2FCC); the window is the bitmap plus 16 (seg2:2EEC).
    const int p = game_.currentPlayer();
    const int status = game_.players()[size_t(p)].status;
    const bool drago = game_.drago().holder == p;
    const std::string name = std::string(1, kPortraitLetters[p]) + (drago ? "pd.bmp" : "p.bmp");
    SDL_Texture* portrait = art(fs::path("bmp") / std::to_string(layout_.artDir) / ("P" + std::to_string(status)) / name);
    if (!portrait) return;
    int w = 0, h = 0;
    SDL_QueryTexture(portrait, nullptr, nullptr, &w, &h);
    const ScreenRect r = layout_.portrait();
    const SDL_Rect target{r.x, r.y, w, h};
    SDL_RenderCopy(renderer_, portrait, nullptr, &target);
}

void GameScreen::drawPlayerInfo() {
    // PLINFO (seg5:6119) at table index 9 with the packed size of index 3: info.bmp,
    // then the slots of DS:3936 (seg5:61C5).
    SDL_Texture* info = art(fs::path("bmp") / std::to_string(layout_.artDir) / "info.bmp");
    if (!info) return;
    const ScreenRect panel = layout_.playerInfo();
    const SDL_Rect target = toSdl(panel);
    SDL_RenderCopy(renderer_, info, nullptr, &target);
    const PlayerInfo pi = playerInfo(game_, board_, towns_, texts_);
    const int res = layout_.width == 1024 ? 0 : layout_.width == 800 ? 1 : 2;
    static constexpr int kItalic[3] = {24, 19, 16}, kRegular[3] = {18, 15, 12}; // seg5:622F
    auto slot = [&](int i) {
        ScreenRect r = panelSlot(layout_, i);
        r.x += panel.x;
        r.y += panel.y;
        return r;
    };
    const SDL_Color white{255, 255, 255, 255}, brown{67, 27, 0, 255}; // palette 0xFF and 0x45
    TextStyle headline{kItalic[res], true, true, true, 2, white};      // slots 0, 1 (0x121, shadow 2)
    TextStyle line{kRegular[res], true, false, false, 0, brown};       // slots 2, 4, 6
    text_.draw(renderer_, pi.monthLine, slot(0), headline);
    text_.draw(renderer_, pi.money, slot(1), headline);
    text_.draw(renderer_, pi.destination, slot(2), line);
    text_.draw(renderer_, pi.stepsLabel, slot(4), line);
    text_.draw(renderer_, pi.fieldKind, slot(6), line);
    auto digits = [&](const std::array<int, 2>& d, const ScreenRect& r) { // seg5:69DC: second at x + its width
        SDL_Texture* tens = sprite("zahl_" + std::to_string(d[0]));
        SDL_Texture* units = sprite("zahl_" + std::to_string(d[1]));
        int w = 0, h = 0;
        if (tens) { SDL_QueryTexture(tens, nullptr, nullptr, &w, &h); SDL_Rect t{r.x, r.y, w, h}; SDL_RenderCopy(renderer_, tens, nullptr, &t); }
        if (units) { SDL_QueryTexture(units, nullptr, nullptr, &w, &h); SDL_Rect t{r.x + w, r.y, w, h}; SDL_RenderCopy(renderer_, units, nullptr, &t); }
    };
    digits(pi.distance, slot(3));
    digits(pi.steps, slot(5));
    if (SDL_Texture* icon = pi.fieldIcon.empty() ? nullptr : sprite(pi.fieldIcon)) {
        int w = 0, h = 0;
        SDL_QueryTexture(icon, nullptr, nullptr, &w, &h);
        const ScreenRect r = slot(7);
        const SDL_Rect t{r.x, r.y, w, h};
        SDL_RenderCopy(renderer_, icon, nullptr, &t);
    }
}

void GameScreen::draw() {
    // Canvas (seg8:0000): butt/<n>/bk.bmp fills the client area.
    if (SDL_Texture* background = art(fs::path("butt") / std::to_string(layout_.artDir) / "bk.bmp"))
        SDL_RenderCopy(renderer_, background, nullptr, nullptr);
    drawMap();
    drawPortrait();
    drawPlayerInfo();
    const Uint32 now = SDL_GetTicks();
    if (game_.spinSerial() != seenSpin_) { // a new spin: the slot machine runs over the map
        seenSpin_ = game_.spinSerial();
        slot_.start(game_.lastReels(), now);
    }
    slot_.draw(layout_.map, now);
    cards_.draw();
    menu_.draw();
    messages_.draw(game_);
}

int runGameWindow(PlayView& play, const fs::path& root, const Board& board, const std::vector<Town>& towns,
                  const GameWindowOptions& options) {
    SDL_DisplayMode display{};
    if (SDL_GetDesktopDisplayMode(0, &display) != 0) display.w = 1024, display.h = 768;
    const ScreenLayout layout = layoutForScreen(display.w, display.h, options.canvasWidth);
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("Die total verrueckte Rallye", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, layout.width,
                         layout.height, 0),
        SDL_DestroyWindow);
    if (!window) throw std::runtime_error(SDL_GetError());
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
        SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE), SDL_DestroyRenderer);
    if (!renderer) throw std::runtime_error(SDL_GetError());
    std::map<int, std::string> texts; // language file (texts in UTF-8)
    std::vector<uint8_t> language;    // also holds the menu resources
    if (const fs::path file = findCaseInsensitive(root, "dat/d/rally_d.dat"); !file.empty()) language = readFile(file);
    if (!language.empty())
        for (const auto& [id, raw] : readNeStrings(language)) texts.emplace(id, ansiToUtf8(raw));
    GameScreen screen(renderer.get(), root, layout, board, towns, play.game(), std::move(texts));
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
    auto command = [&](int id) { // WM_COMMAND of the main window (seg4:0B76)
        menu.close();
        switch (id) {
        case 1030: if (play.game().roll()) report(); break;     // Würfeln: seg2:6426(1, 5, 1)
        case 1031:                                               // Karte einsetzen
            if (!play.game().openCardWindow()) { openMenu("ONFIELD"); break; } // blocked: menu stays
            if (cards.open(play.game())) cardWindowState = turnState(); // seg3:0887(2)
            break;
        case 1033: openMenu("MAIN1"); break;                     // Hauptmenü
        case 1064: running = false; break;                       // Rallye beenden (seg4:0EDF)
        default: break; // Zoom, Übersicht, Umfeld, Statistik, Neues Spiel, Speichern ...: not built yet
        }
    };
    if (!options.openMenu.empty()) openMenu(options.openMenu);
    if (options.openCards && cards.open(play.game())) cardWindowState = turnState();
    if (options.rollAtStart && play.game().roll()) report();
    report();
    for (int frame = 1; running; ++frame) {
        for (Notice& n : play.game().takeNotices()) screen.messages().push(std::move(n));
        // Message window and menu are modal (seg6:0FB4, SetCapture in seg5:578C).
        // A played card changes the turn state and closes the card window.
        if (cards.isOpen() && turnState() != cardWindowState) cards.close();
        const bool modal = screen.messages().open() || menu.isOpen() || cards.isOpen();
        if (!modal && play.update(SDL_GetTicks())) report();
        if (!modal && turnMenuDue() && !options.rollAtStart) openMenu("ONFIELD");
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            const bool click = event.type == SDL_MOUSEBUTTONDOWN;
            if (screen.messages().open()) {
                if (event.type == SDL_KEYDOWN || click) screen.messages().dismiss();
                continue;
            }
            if (menu.isOpen()) {
                if (const int id = menu.handle(event)) command(id);
                continue;
            }
            if (cards.isOpen()) { // the card buttons; Escape or the right button closes the window
                if (click && event.button.button == SDL_BUTTON_LEFT) {
                    const int slot = cards.slotAt(event.button.x, event.button.y);
                    if (slot >= 0 && play.handleKey(SDL_Keycode(SDLK_1 + slot), 0)) report();
                    continue;
                }
                if (click && event.button.button == SDL_BUTTON_RIGHT) { cards.close(); continue; }
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) { cards.close(); continue; }
            }
            if (event.type != SDL_KEYDOWN) continue;
            const SDL_Keycode key = event.key.keysym.sym;
            const bool risiko = play.game().phase() == Game::Phase::Risiko;
            if ((key == SDLK_ESCAPE || key == SDLK_BACKSPACE) && !risiko) openMenu("RIGHTBTN"); // seg4:01D1
            else if (play.handleKey(key == SDLK_ESCAPE ? SDLK_BACKSPACE : key, event.key.keysym.mod)) report();
        }
        SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
        SDL_RenderClear(renderer.get());
        screen.draw();
        const bool last = options.smokeTest && frame >= options.frames;
        if (!options.screenshot.empty() && (last || !options.smokeTest)) {
            Surface shot(SDL_CreateRGBSurfaceWithFormat(0, layout.width, layout.height, 32, SDL_PIXELFORMAT_ARGB8888),
                         SDL_FreeSurface);
            if (!shot || SDL_RenderReadPixels(renderer.get(), nullptr, SDL_PIXELFORMAT_ARGB8888, shot->pixels,
                                              shot->pitch) != 0
                || SDL_SaveBMP(shot.get(), options.screenshot.c_str()) != 0)
                throw std::runtime_error(SDL_GetError());
        }
        SDL_RenderPresent(renderer.get());
        if (last) break;
        SDL_Delay(16);
    }
    return 0;
}

} // namespace dtvr
