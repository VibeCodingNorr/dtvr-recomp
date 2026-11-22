// SPDX-License-Identifier: GPL-3.0-only
#include "game_screen.hpp"

#include "worldmap.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

// seg2:2D43: portrait letter per slot.
constexpr char kPortraitLetters[8] = {'D', 'F', 'E', 'M', 'U', 'J', 'I', 'V'};

SDL_Rect toSdl(const ScreenRect& r) { return {r.x, r.y, r.w, r.h}; }

} // namespace

GameScreen::GameScreen(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout,
                       const Board& board, const Game& game)
    : renderer_(renderer), root_(root), layout_(layout), board_(board), game_(game) {}

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
    // Provisional markers until the vehicle sprites (SBS) are placed as in the original.
    static const SDL_Color colours[8] = {{255, 60, 60, 255}, {60, 160, 255, 255}, {60, 220, 60, 255},
        {255, 160, 0, 255}, {200, 60, 255, 255}, {0, 230, 230, 255}, {255, 255, 255, 255}, {120, 120, 120, 255}};
    SDL_RenderSetClipRect(renderer_, &target);
    for (int p = 0; p < kPlayerSlots; ++p) {
        if (!game_.players()[size_t(p)].status) continue;
        const Field& f = board_.fields[size_t(game_.playerField(p))];
        const SDL_Rect box{target.x + f.x * WorldMap::kTile - origin.x + 3 + p % 3,
                           target.y + f.y * WorldMap::kTile - origin.y + 3 + p / 3, 10, 10};
        SDL_SetRenderDrawColor(renderer_, colours[p].r, colours[p].g, colours[p].b, 255);
        SDL_RenderFillRect(renderer_, &box);
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
    // PLINFO (seg5:6119) at table index 9 with the packed size of index 3: info.bmp.
    SDL_Texture* info = art(fs::path("bmp") / std::to_string(layout_.artDir) / "info.bmp");
    if (!info) return;
    const SDL_Rect target = toSdl(layout_.playerInfo());
    SDL_RenderCopy(renderer_, info, nullptr, &target);
}

void GameScreen::draw() {
    // Canvas (seg8:0000): butt/<n>/bk.bmp fills the client area.
    if (SDL_Texture* background = art(fs::path("butt") / std::to_string(layout_.artDir) / "bk.bmp"))
        SDL_RenderCopy(renderer_, background, nullptr, nullptr);
    drawMap();
    drawPortrait();
    drawPlayerInfo();
}

int runGameWindow(PlayView& play, const fs::path& root, const Board& board, const GameWindowOptions& options) {
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
    GameScreen screen(renderer.get(), root, layout, board, play.game());
    auto report = [&] {
        play.printLog(std::cout);
        SDL_SetWindowTitle(window.get(), play.title().c_str());
    };
    report();
    bool running = true;
    while (running) {
        if (play.update(SDL_GetTicks())) report();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type != SDL_KEYDOWN) continue;
            if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
            else if (play.handleKey(event.key.keysym.sym, event.key.keysym.mod)) report();
        }
        SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
        SDL_RenderClear(renderer.get());
        screen.draw();
        if (!options.screenshot.empty()) {
            Surface shot(SDL_CreateRGBSurfaceWithFormat(0, layout.width, layout.height, 32, SDL_PIXELFORMAT_ARGB8888),
                         SDL_FreeSurface);
            if (!shot || SDL_RenderReadPixels(renderer.get(), nullptr, SDL_PIXELFORMAT_ARGB8888, shot->pixels,
                                              shot->pitch) != 0
                || SDL_SaveBMP(shot.get(), options.screenshot.c_str()) != 0)
                throw std::runtime_error(SDL_GetError());
        }
        SDL_RenderPresent(renderer.get());
        if (options.smokeTest) break;
        SDL_Delay(16);
    }
    return 0;
}

} // namespace dtvr
