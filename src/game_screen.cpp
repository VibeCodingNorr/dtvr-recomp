// SPDX-License-Identifier: GPL-3.0-only
#include "game_screen.hpp"

#include "panel.hpp"
#include "sbs.hpp"
#include "worldmap.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <iostream>

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
      cards_(renderer, root, layout), cardShop_(renderer, root, layout, texts_, text_), selectPlayer_(renderer, root, layout), flight_(renderer, root, layout, texts_, text_), nails_(renderer, root, layout, texts_, text_), invest_(renderer, root, layout, texts_, text_), risiko_(renderer, root, layout),
      estates_(renderer, layout, texts_, text_,
               [this](const std::string& name) {
                   SDL_Texture* t = find(buySprites_, name);
                   return t ? t : sprite(name);
               }),
      slots_(defaultSaveDirectory()),
      save_(renderer, layout, texts_, text_, [this](const std::string& name) { return find(lsSprites_, name); }),
      setup_(renderer, root, layout, text_),
      overview_(renderer, root, layout, board, [this](const std::string& name) { return sprite(name); }),
      settings_(loadSettings(defaultSettingsFile())), options_(renderer, root, layout),
      finish_(renderer, root, layout.width, layout.height), victory_(renderer, root, layout.width, layout.height),
      balance_(renderer, root, layout.width, layout.height),
      stage_(renderer, root, layout.width, layout.height, board, towns) {
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

MapViewSize GameScreen::viewSize() const {
    const int z = zoomFactor();
    return MapViewSize{layout_.viewport.w / z, layout_.viewport.h / z, z == 2};
}

void GameScreen::centreOnPlayer() {
    const int p = game_.currentPlayer();
    if (p < 0) return;
    const Field& f = board_.fields[size_t(game_.playerField(p))];
    SDL_Point at{f.x * WorldMap::kTile, f.y * WorldMap::kTile};
    if (shownField_[size_t(p)] >= 0) at = shown_[size_t(p)];
    origin_ = centredOrigin(at.x, at.y, viewSize()); // seg6:3064 with the vehicle position
    originSet_ = true;
}

void GameScreen::eventFocus(int cellX, int cellY) {
    const int p = game_.currentPlayer();
    if (cellX < 0 || cellY < 0) {
        if (!focus_.active) return;
        if (focus_.zoomed && p >= 0) zoom_[size_t(p)] = false; // seg6:2E54(1) when it was raised
        origin_ = focus_.saved;                                  // DS:956A / 9568 restored
        originSet_ = focus_.savedSet;
        focus_ = EventFocus{};
        return;
    }
    if (focus_.active && focus_.x == cellX && focus_.y == cellY) return;
    if (!focus_.active) {
        focus_.active = true;
        focus_.saved = origin_;
        focus_.savedSet = originSet_;
        focus_.zoomed = p >= 0 && !zoom_[size_t(p)];
        if (focus_.zoomed) zoom_[size_t(p)] = true;              // seg6:2E54(2): zoom 1
    }
    focus_.x = cellX;
    focus_.y = cellY;
    origin_ = centredOrigin(cellX * WorldMap::kTile, cellY * WorldMap::kTile, viewSize()); // seg6:3064(x << 4, y << 4)
    originSet_ = true;
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
            if (!linked || (at.x != target.x && at.y != target.y)) {
                at = target;
                if (p == game_.currentPlayer()) centreOnPlayer(); // Tschüss / Flugzeug: seg3:29E7 / 3314
            }
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
        // seg2:239F: every movement tick of the current vehicle keeps it 32 pixels inside
        // the view (seg2:29EB); a step that leaves it scrolls by seg6:30F8.
        if (p == game_.currentPlayer() && originSet_ && ticks > 0)
            edgeScroll(origin_, at.x, at.y, at.x + WorldMap::kTile, at.y + WorldMap::kTile, viewSize());
    }
    // seg2:044E at a player change and seg2:64E5 at a spin centre on the player.
    if (game_.turnSerial() != seenTurn_ || game_.spinSerial() != seenCentreSpin_ || !originSet_) {
        seenTurn_ = game_.turnSerial();
        seenCentreSpin_ = game_.spinSerial();
        centreOnPlayer();
    }
}

bool GameScreen::vehicleMoving(int player) const {
    if (player < 0 || shownField_[size_t(player)] < 0) return false;
    const Field& f = board_.fields[size_t(game_.playerField(player))];
    const SDL_Point at = shown_[size_t(player)];
    return at.x != f.x * WorldMap::kTile || at.y != f.y * WorldMap::kTile;
}

void GameScreen::applySettings(const Settings& settings) {
    settings_ = settings;
    worldSeason_ = -1; // reload the world with or without the Views tiles
    std::string error;
    if (!saveSettings(defaultSettingsFile(), settings_, error)) std::cerr << "Optionen: " << error << "\n";
}

bool GameScreen::startFinish(int field, int player, Uint32 now) {
    int town = -1;
    if (field >= 0)
        for (size_t t = 0; t < towns_.size(); ++t)
            if (towns_[t].x == board_.fields[size_t(field)].x && towns_[t].y == board_.fields[size_t(field)].y) town = int(t);
    if (town < 0 || player < 0 || player >= kPlayerSlots) return false;
    if (!finish_.start(kPortraitLetters[player], towns_[size_t(town)].name, now)) return false;
    // seg7:2C48: butt/snd/z.wav (seg1:3579)
    if (sound_) sound_->playFile(0, findCaseInsensitive(root_, fs::path("butt") / "snd" / "z.wav"));
    return true;
}

bool GameScreen::startAnnouncement(int mode, int variant, Uint32 now) {
    return stage_.start(mode, variant, game_.destination(), sound_, now);
}

bool GameScreen::startBalance(Uint32 now) {
    return balance_.start(game_.balanceIntro(), game_.balanceEntries(), game_.humansLeft(), sound_, now);
}

bool GameScreen::startVictory(Uint32 now) {
    if (!victory_.start(game_.ranking(), now)) return false;
    // seg7:7BAB: "%s\%c\VOICE\SIEG\SIEG.WAV" (seg1:3579)
    if (sound_) sound_->playFile(0, findCaseInsensitive(root_, fs::path("butt") / "d" / "voice" / "sieg" / "sieg.wav"));
    return true;
}

void GameScreen::toggleZoom() {
    const int p = game_.currentPlayer();
    if (p >= 0) zoom_[size_t(p)] = !zoom_[size_t(p)];
    centreOnPlayer(); // seg4:0883 / 0966 -> seg2:3216
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
    if (originSet_) return {origin_.x * WorldMap::kTile, origin_.y * WorldMap::kTile}; // DS:956A / 9568
    // Before the first centring: centred on the current player's field.
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
        auto surface = loadWorld(root_, game_.calendar().month, settings_.value[Settings::Views] != 0); // [Setup] Views
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
    if (sound_) sound_->setEnabled(settings_.value[Settings::Wave] != 0);
    if (!picture_.empty()) {
        if (SDL_Texture* t = art(fs::path("butt") / std::to_string(layout_.artDir) / picture_))
            SDL_RenderCopy(renderer_, t, nullptr, nullptr);
        return;
    }
    if (options_.isOpen()) { // OPTION covers the canvas
        options_.draw();
        return;
    }
    if (title_ && setup_.isOpen()) { // PLAYER replaces the title (seg4:0DA9)
        setup_.draw();
        return;
    }
    if (title_) {
        if (SDL_Texture* t = art(fs::path("butt") / std::to_string(layout_.artDir) / "d" / "t.bmp"))
            SDL_RenderCopy(renderer_, t, nullptr, nullptr);
        save_.draw(slots_);
        menu_.draw();
        messages_.draw(game_); // the quit question (seg4:0EDF) also opens over the title
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
        if (sound_) sound_->play(3, 5, 0xFFFF); // Walze10 endless while the reels turn (seg2:78B1)
        reelsSounding_ = true;
    }
    if (reelsSounding_ && slot_.stopped(now)) { // count-up: Zaehler1 (seg2:7C83), channel 3 flushed (7E74)
        reelsSounding_ = false;
        if (sound_) {
            sound_->stop(3);
            sound_->play(4, 6);
        }
    }
    slot_.draw(layout_.map, now);
    if (estates_.isOpen()) {
        std::array<char, 8> letters{};
        for (size_t i = 0; i < 8; ++i) letters[i] = kPortraitLetters[i]; // seg2:2D43
        estates_.draw(estateRows(), letters);
    }
    // seg7:674D: the window follows the one-reel spin (seg2:6426).
    if (game_.phase() == Game::Phase::Risiko && slot_.stopped(now)) risiko_.draw(game_.risiko().light, now);
    overview_.draw(game_);
    finish_.draw(SDL_GetTicks());
    victory_.draw(SDL_GetTicks());
    balance_.draw(SDL_GetTicks());
    stage_.draw(SDL_GetTicks());
    cardShop_.draw(game_);
    cards_.draw(sprite("dis_d")); // "dis_%c" with the language letter DS:A0F0 (seg1:190D)
    selectPlayer_.draw();
    flight_.draw();
    nails_.draw();
    invest_.draw();
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

} // namespace dtvr
