// SPDX-License-Identifier: GPL-3.0-only
#include "overview_view.hpp"

#include "rules.hpp"

#include <cmath>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr char kLetters[kPlayerSlots] = {'D', 'F', 'E', 'M', 'U', 'J', 'I', 'V'}; // seg2:2D43
constexpr double kWorld = 4096.0;                                                  // DS:86DE

int scaled(int worldPixel, int bitmapSize) { // value / (4096 / size), truncated (seg10:368A)
    return int(worldPixel / (kWorld / bitmapSize));
}

} // namespace

SDL_Point overviewPoint(const Board& board, int field, int bitmapSize) {
    const Field& f = board.fields[size_t(field)];
    return {scaled(f.x * 16, bitmapSize) + 1, scaled(f.y * 16, bitmapSize) + 1}; // seg1:A8C7
}

std::vector<SDL_Point> overviewRoute(const Board& board, int from, int destination, uint16_t mask, int bitmapSize) {
    std::vector<SDL_Point> points;
    if (from < 0 || destination < 0) return points;
    const auto dist = fieldDistances(board, destination, mask);
    int at = from;
    points.push_back(overviewPoint(board, at, bitmapSize));
    for (int guard = 0; guard < 1000 && dist[size_t(at)] > 0; ++guard) { // a511: until distance 0
        int next = -1;
        for (int k = 0; k < 4 && next < 0; ++k) {
            const int n = board.fields[size_t(at)].next[size_t(k)];
            if (n == Field::kNone) continue;
            if (dist[size_t(n)] >= 0 && dist[size_t(n)] < dist[size_t(at)]) next = n;
        }
        if (next < 0) break;
        at = next;
        points.push_back(overviewPoint(board, at, bitmapSize));
    }
    return points;
}

OverviewView::OverviewView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout,
                           const Board& board, SpriteLookup sprite)
    : renderer_(renderer), root_(root), layout_(layout), board_(board), sprite_(std::move(sprite)) {}

bool OverviewView::handle(const SDL_Event& event) {
    if (!open_) return false;
    if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) {
        open_ = false;
        return true;
    }
    return false;
}

void OverviewView::draw(const Game& game) {
    if (!open_) return;
    if (!map_) { // seg1:A263: "%s\%d\map.bmp" in the butt directory
        const fs::path path = findCaseInsensitive(root_, fs::path("butt") / std::to_string(layout_.artDir) / "map.bmp");
        if (!path.empty()) {
            auto surface = loadBmp(path);
            mapSize_ = surface->w;
            map_.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        }
        if (!map_) return;
    }
    // The window covers the map window (DS:9C84..9C8A); the bitmap sits at (0, 0).
    const SDL_Rect window{layout_.map.x, layout_.map.y, layout_.map.w, layout_.map.h};
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, &window);
    const SDL_Rect dst{window.x, window.y, mapSize_, mapSize_};
    SDL_RenderCopy(renderer_, map_.get(), nullptr, &dst);
    SDL_RenderSetClipRect(renderer_, &window);
    const int p = game.currentPlayer();
    // Route in a red pen of width 2 (CreatePen(PS_INSIDEFRAME, 2, PALETTERGB(255, 0, 0))).
    const auto route = overviewRoute(board_, game.playerField(p), game.destination(), game.routeMaskForDisplay(), mapSize_);
    SDL_SetRenderDrawColor(renderer_, 255, 0, 0, 255);
    for (size_t i = 1; i < route.size(); ++i)
        for (int d = 0; d < 2; ++d)
            SDL_RenderDrawLine(renderer_, window.x + route[i - 1].x + d, window.y + route[i - 1].y + d,
                               window.x + route[i].x + d, window.y + route[i].y + d);
    // Ellipses of radius 6 with the red pen: green at the stage start (DS:A10B), blue at
    // the end of the route (seg1:A5C5 / A61D).
    auto disc = [&](SDL_Point c, SDL_Color fill) {
        for (int y = -6; y <= 6; ++y)
            for (int x = -6; x <= 6; ++x) {
                const double r = std::sqrt(double(x * x + y * y));
                if (r > 6.0) continue;
                const SDL_Color col = r > 4.5 ? SDL_Color{255, 0, 0, 255} : fill;
                SDL_SetRenderDrawColor(renderer_, col.r, col.g, col.b, 255);
                SDL_RenderDrawPoint(renderer_, window.x + c.x + x, window.y + c.y + y);
            }
    };
    if (game.origin() >= 0) disc(overviewPoint(board_, game.origin(), mapSize_), {0, 255, 0, 255});
    if (!route.empty()) disc(route.back(), {0, 0, 255, 255});
    auto flag = [&](const std::string& name, int field) {
        SDL_Texture* t = sprite_(name);
        if (!t || field < 0) return;
        int w = 0, h = 0;
        SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
        const Field& f = board_.fields[size_t(field)];
        const SDL_Rect r{window.x + scaled(f.x * 16, mapSize_), window.y + scaled(f.y * 16, mapSize_), w, h};
        SDL_RenderCopy(renderer_, t, nullptr, &r);
    };
    // Q_FL at Dr. Drago (DS:9D01 / 9D03; here the field of the driver he rides with).
    if (game.drago().holder >= 0) flag("Q_FL", game.playerField(game.drago().holder));
    // Driver flags from the player after the current one, the current one last (seg1:A72B).
    for (int k = 1; k <= kPlayerSlots; ++k) {
        const int q = (p + k) % kPlayerSlots;
        if (game.players()[size_t(q)].status) flag(std::string(1, kLetters[q]) + "_FL", game.playerField(q));
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
}

} // namespace dtvr
