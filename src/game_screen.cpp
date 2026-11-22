// SPDX-License-Identifier: GPL-3.0-only
#include "game_screen.hpp"

#include "ne_strings.hpp"
#include "panel.hpp"
#include "sbs.hpp"
#include "worldmap.hpp"

#include <algorithm>
#include <array>
#include <ctime>
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
      cards_(renderer, root, layout),
      estates_(renderer, layout, texts_, text_,
               [this](const std::string& name) {
                   SDL_Texture* t = find(buySprites_, name);
                   return t ? t : sprite(name);
               }),
      slots_(defaultSaveDirectory()),
      save_(renderer, layout, texts_, text_, [this](const std::string& name) { return find(lsSprites_, name); }),
      setup_(renderer, root, layout, text_),
      overview_(renderer, root, layout, board, [this](const std::string& name) { return sprite(name); }) {
    seenSpin_ = game.spinSerial();
    shownField_.fill(-1);
    std::cout << "Schrift: " << text_.fontDescription() << "\n";
    // SBS frames (seg6:406D looks them up by name in all loaded files): the panel
    // set of the resolution and the vehicles of the drivers in play (<letter>.sbs).
    loadSprites(fs::path("sbs") / (std::to_string(layout_.artDir) + "_assort.sbs"));
    loadSprites(fs::path("sbs") / "assorted.sbs"); // MENUN / MENUG of the menu window
    loadSprites(fs::path("sbs") / "buy.sbs", buySprites_); // town dialog (DS:3FCC, seg1:4C45)
    loadSprites(fs::path("sbs") / "ls.sbs", lsSprites_);   // save dialog (DS:8081)
    loadDriverSprites();
}

void GameScreen::loadDriverSprites() {
    for (int p = 0; p < kPlayerSlots; ++p) {
        if (!game_.players()[size_t(p)].status) continue;
        const std::string letter(1, kPortraitLetters[p]);
        if (find(sprites_, letter + "R1")) continue; // already loaded
        loadSprites(fs::path("sbs") / (std::string(1, char(std::tolower(kPortraitLetters[p]))) + ".sbs"));
    }
}

SDL_Texture* GameScreen::find(const SpriteSet& set, const std::string& name) {
    auto it = set.find(upper(name));
    return it == set.end() ? nullptr : it->second.get();
}

void GameScreen::loadSprites(const fs::path& relative, SpriteSet& into) {
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
        into.emplace(upper(frame.name), Texture(SDL_CreateTextureFromSurface(renderer_, surface.get()), SDL_DestroyTexture));
    }
}

SDL_Texture* GameScreen::sprite(const std::string& name) {
    return find(sprites_, name); // rally.exe asks for "zahl_0", the file holds "ZAHL_0"
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

void GameScreen::advanceVehicles(Uint32 now) {
    constexpr int kTickMs = 50, kPixelsPerTick = 2 << 1;
    const Uint32 elapsed = lastAdvance_ == 0 ? 0 : now - lastAdvance_;
    lastAdvance_ = now;
    carry_ += elapsed;
    const int ticks = int(carry_ / kTickMs);
    carry_ %= kTickMs;
    for (int p = 0; p < kPlayerSlots; ++p) {
        if (!game_.players()[size_t(p)].status) continue;
        const int field = game_.playerField(p);
        const Field& f = board_.fields[size_t(field)];
        const SDL_Point target{f.x * WorldMap::kTile, f.y * WorldMap::kTile};
        SDL_Point& at = shown_[size_t(p)];
        int& from = shownField_[size_t(p)];
        if (from != field) {
            // Only a step along a link is driven; any other change (start, flight,
            // Konferenz, a computer turn done at once) puts the vehicle there directly.
            bool linked = false;
            if (from >= 0)
                for (int16_t n : board_.fields[size_t(from)].next) linked |= n == field;
            if (!linked || (at.x != target.x && at.y != target.y)) at = target;
            from = field;
        }
        int budget = ticks * kPixelsPerTick;
        auto approach = [&](int& v, int goal) {
            const int d = std::clamp(goal - v, -budget, budget);
            v += d;
            budget -= std::abs(d);
        };
        approach(at.x, target.x);
        approach(at.y, target.y);
    }
}

bool GameScreen::vehicleMoving(int player) const {
    if (player < 0 || shownField_[size_t(player)] < 0) return false;
    const Field& f = board_.fields[size_t(game_.playerField(player))];
    const SDL_Point at = shown_[size_t(player)];
    return at.x != f.x * WorldMap::kTile || at.y != f.y * WorldMap::kTile;
}

void GameScreen::toggleZoom() {
    const int p = game_.currentPlayer();
    if (p >= 0) zoom_[size_t(p)] = !zoom_[size_t(p)];
}

int GameScreen::zoomFactor() const {
    const int p = game_.currentPlayer();
    return p >= 0 && zoom_[size_t(p)] ? 2 : 1; // DS:9C70 + 1
}

void GameScreen::startSurroundings() {
    const SDL_Point o = viewOrigin(); // seg6:3CC5: from the current origin
    surroundOrigin_ = {o.x / WorldMap::kTile, o.y / WorldMap::kTile};
    surroundings_ = true;
}

bool GameScreen::handleSurroundings(const SDL_Event& event) {
    if (!surroundings_) return false;
    // seg6:2EE9: with zoom the viewport DS:94DE / 94E0 is halved.
    const int viewW = layout_.viewport.w / zoomFactor(), viewH = layout_.viewport.h / zoomFactor();
    const int third = viewW / 3 / WorldMap::kTile; // (w / ±3) / 16, truncated (seg6:17F4)
    constexpr int side = WorldMap::kSize * WorldMap::kTile;
    auto move = [&](int dx, int dy) { // seg6:2B19 keeps the view inside the world
        const int maxX = (side - viewW) / WorldMap::kTile, maxY = (side - viewH) / WorldMap::kTile;
        surroundOrigin_.x = std::clamp(surroundOrigin_.x + dx * third, 0, maxX);
        surroundOrigin_.y = std::clamp(surroundOrigin_.y + dy * third, 0, maxY);
    };
    if (event.type == SDL_MOUSEMOTION) { // seg6:1798: strips of 0x3C pixels, corners excluded
        const int x = event.motion.x - layout_.map.x, y = event.motion.y - layout_.map.y;
        const int l = layout_.viewport.x, t = layout_.viewport.y;
        const int r = l + layout_.viewport.w, b = t + layout_.viewport.h;
        const bool midY = y >= t + 60 && y <= b - 60, midX = x >= l + 60 && x <= r - 60;
        if (midY && x >= l && x <= l + 60) move(-1, 0);
        if (midX && y >= t && y <= t + 60) move(0, -1);
        if (midY && x >= r - 60 && x <= r) move(1, 0);
        if (midX && y >= b - 60 && y <= b) move(0, 1);
        return true;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) { // seg6:176E
        surroundings_ = false;
        return true;
    }
    if (event.type == SDL_KEYDOWN) { // seg6:1991
        switch (event.key.keysym.sym) {
        case SDLK_RETURN: case SDLK_ESCAPE: case SDLK_SPACE: surroundings_ = false; break;
        case SDLK_LEFT: case SDLK_KP_4: move(-1, 0); break;
        case SDLK_UP: case SDLK_KP_8: move(0, -1); break;
        case SDLK_RIGHT: case SDLK_KP_6: move(1, 0); break;
        case SDLK_DOWN: case SDLK_KP_2: move(0, 1); break;
        default: break;
        }
        return true;
    }
    return event.type == SDL_MOUSEBUTTONDOWN;
}

SDL_Point GameScreen::viewOrigin() const {
    if (surroundings_) return {surroundOrigin_.x * WorldMap::kTile, surroundOrigin_.y * WorldMap::kTile};
    // Assumption until the scrolling code is traced: the viewport is centred on
    // the current player's field and kept inside the 4096 x 4096 world.
    constexpr int side = WorldMap::kSize * WorldMap::kTile;
    const int p = game_.currentPlayer();
    const Field& f = board_.fields[size_t(game_.playerField(p))];
    SDL_Point at{f.x * WorldMap::kTile, f.y * WorldMap::kTile};
    if (p >= 0 && shownField_[size_t(p)] >= 0) at = shown_[size_t(p)];
    const int cx = at.x + WorldMap::kTile / 2, cy = at.y + WorldMap::kTile / 2;
    const int w = layout_.viewport.w / zoomFactor(), h = layout_.viewport.h / zoomFactor();
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
    const int z = zoomFactor(); // the world area of the viewport is shown z times as large
    const SDL_Rect source{origin.x, origin.y, layout_.viewport.w / z, layout_.viewport.h / z};
    const SDL_Rect target{map.x + layout_.viewport.x, map.y + layout_.viewport.y, source.w * z, source.h * z};
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
        SDL_Point at{f.x * WorldMap::kTile, f.y * WorldMap::kTile};
        if (shownField_[size_t(p)] >= 0) at = shown_[size_t(p)];
        const bool driving = vehicleMoving(p) || (p == game_.currentPlayer() && game_.phase() == Game::Phase::Move);
        const int frameNo = driving ? int(tick / 4 % 4) + 1 : 1;
        const std::string name = std::string(1, kPortraitLetters[p]) + kFacing[std::clamp(game_.facing(p), 1, 4)]
                                 + std::to_string(frameNo);
        SDL_Texture* vehicle = sprite(name);
        if (!vehicle) continue;
        int w = 0, h = 0;
        SDL_QueryTexture(vehicle, nullptr, nullptr, &w, &h);
        const SDL_Rect box{target.x + (at.x - origin.x - 16) * z, target.y + (at.y - origin.y - 8) * z, w * z, h * z};
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
    TextStyle headline{kItalic[res], true, true, true, false, 2, white};      // slots 0, 1 (0x121, shadow 2)
    TextStyle line{kRegular[res], true, false, false, false, 0, brown};       // slots 2, 4, 6
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
    if (title_ && setup_.isOpen()) { // PLAYER replaces the title (seg4:0DA9)
        setup_.draw();
        return;
    }
    if (title_) {
        if (SDL_Texture* t = art(fs::path("butt") / std::to_string(layout_.artDir) / "d" / "t.bmp"))
            SDL_RenderCopy(renderer_, t, nullptr, nullptr);
        save_.draw(slots_);
        menu_.draw();
        return;
    }
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
    if (estates_.isOpen()) {
        std::array<char, 8> letters{};
        for (size_t i = 0; i < 8; ++i) letters[i] = kPortraitLetters[i]; // seg2:2D43
        estates_.draw(estateRows(), letters);
    }
    overview_.draw(game_);
    cards_.draw();
    save_.draw(slots_);
    menu_.draw();
    messages_.draw(game_);
}

std::vector<EstateRow> GameScreen::estateRows() const {
    if (!estates_.isOpen()) return {};
    if (estates_.statistics())
        return statisticsRows(game_, estates_.player(), estates_.category(), estates_.sort());
    return dtvr::estateRows(game_, estates_.town(), estates_.tab());
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
    if (options.loadSlot >= 0) { // before the screen: the drivers' sprites depend on the players
        std::string error;
        const SaveSlots slots(defaultSaveDirectory());
        if (!slots.load(options.loadSlot, play.game(), error))
            throw std::runtime_error("Spielstand " + std::to_string(options.loadSlot) + ": " + error);
        std::cout << "Geladen: " << slots.file(options.loadSlot).string() << "\n";
    }
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
    int autosaved = -1; // turn serial of the last autosave
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
    auto startGame = [&] {
        autosaved = -1; // the first turn of a started or loaded game is saved too
        screen.setTitle(false);
        screen.loadDriverSprites();
        report();
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
        case 1025: running = false; break;                       // Rallye beenden
        case 1023: case 1024: case 1026: openTitle(); break;     // Optionen, Demo, Credits: not built yet
        case 1030: if (play.game().roll()) report(); break;     // Würfeln: seg2:6426(1, 5, 1)
        case 1031:                                               // Karte einsetzen
            if (!play.game().openCardWindow()) { openMenu("ONFIELD"); break; } // blocked: menu stays
            if (cards.open(play.game())) cardWindowState = turnState(); // seg3:0887(2)
            break;
        case 1033: openMenu("MAIN1"); break;                     // Hauptmenü
        case 1064: running = false; break;                       // Rallye beenden (seg4:0EDF)
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
    if (options.openStatistics) screen.estates().openStatistics(play.game().currentPlayer());
    if (options.openCards && cards.open(play.game())) cardWindowState = turnState();
    if (options.rollAtStart && play.game().roll()) report();
    report();
    SDL_Keycode heldArrow = options.holdRight ? SDLK_RIGHT : SDLK_UNKNOWN; // seg4:01F2: DS:A77A held
    auto isArrow = [](SDL_Keycode k) { return k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT || k == SDLK_RIGHT; };
    for (int frame = 1; running; ++frame) {
        screen.advanceVehicles(SDL_GetTicks());
        const bool moving = screen.vehicleMoving(play.game().currentPlayer());
        if (!screen.title())
            for (Notice& n : play.game().takeNotices()) screen.messages().push(std::move(n));
        // Message window and menu are modal (seg6:0FB4, SetCapture in seg5:578C).
        // A played card changes the turn state and closes the card window.
        if (cards.isOpen() && turnState() != cardWindowState) cards.close();
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
            if (!menu.isOpen() && !saveDialog.isOpen() && !screen.setup().isOpen()) openTitle();
        } else if (play.game().humansLeft() && play.game().turnSerial() != autosaved
                   && !play.game().computerTurnRunning()) { // seg2:0837 -> seg8:A870 at each turn start
            std::string error;
            if (!screen.saveSlots().saveAuto(play.game(), error)) std::cerr << "Autosave: " << error << "\n";
            autosaved = play.game().turnSerial();
        }
        const bool modal = screen.messages().open() || menu.isOpen() || cards.isOpen() || saveDialog.isOpen()
                           || estates.statistics() || screen.title() || screen.overview().isOpen()
                           || screen.surroundings();
        if (!modal && !moving && play.update(SDL_GetTicks(), true)) report();
        if (!modal && !moving && turnMenuDue() && !options.rollAtStart) openMenu("ONFIELD");
        // A held arrow drives on field by field once the car has reached its field.
        if (!modal && !moving && heldArrow != SDLK_UNKNOWN && play.game().phase() == Game::Phase::Move
            && !play.game().isComputer(play.game().currentPlayer()) && play.handleKey(heldArrow, 0))
            report();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYUP && event.key.keysym.sym == heldArrow) heldArrow = SDLK_UNKNOWN;
            const bool click = event.type == SDL_MOUSEBUTTONDOWN;
            if (screen.messages().open()) {
                if (event.type == SDL_KEYDOWN || click) screen.messages().dismiss();
                continue;
            }
            if (menu.isOpen()) {
                if (const int id = menu.handle(event)) command(id);
                continue;
            }
            if (screen.overview().isOpen()) { screen.overview().handle(event); continue; }
            if (screen.handleSurroundings(event)) continue;
            if (screen.setup().isOpen()) { // seg1:0A60
                const SetupView::Result r = screen.setup().handle(event);
                if (r == SetupView::Result::Start) {
                    std::string error;
                    const uint16_t seed = uint16_t(SDL_GetTicks() ^ uint32_t(std::time(nullptr))); // seg1:02A9: time
                    const auto status = screen.setup().status();
                    const auto names = screen.setup().names();
                    screen.setup().close();
                    if (play.newGame(status, names, seed, error)) startGame();
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
                    else { std::cerr << "Laden fehlgeschlagen: " << error << "\n"; openTitle(); }
                    continue;
                }
                if (r == SaveView::Result::Close && screen.title()) { saveDialog.close(); openTitle(); continue; }
                if (r == SaveView::Result::Confirm) {
                    std::string error;
                    const int slot = saveDialog.slot();
                    if (screen.saveSlots().save(slot, play.game(), saveDescription(play.game(), screen.texts()), error))
                        std::cout << "Gespeichert: " << screen.saveSlots().file(slot).string() << "\n";
                    else
                        std::cerr << "Speichern fehlgeschlagen: " << error << "\n";
                }
                if (r != SaveView::Result::None) saveDialog.close();
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
            if (estates.isOpen() && !cards.isOpen()) {
                const auto action = estates.handle(event, screen.estateRows());
                using Kind = EstatesView::Action::Kind;
                Game& g = play.game();
                if (action.kind == Kind::Row) {
                    const bool done = estates.tab() == EstatesTab::Sell ? g.sellProperty(action.entry)
                                                                        : g.buyProperty(action.entry);
                    if (done) report();
                } else if (action.kind == Kind::Cards) { // the card window for property cards
                    if (g.openCardWindow() && cards.open(g)) cardWindowState = turnState();
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
            if (event.type != SDL_KEYDOWN) continue;
            const SDL_Keycode key = event.key.keysym.sym;
            if (isArrow(key) && play.game().phase() == Game::Phase::Move) { heldArrow = key; continue; }
            if (key == SDLK_INSERT || key == SDLK_DELETE) { screen.toggleZoom(); continue; } // seg4:0212
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
