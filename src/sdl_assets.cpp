// SPDX-License-Identifier: GPL-3.0-only
#include "sdl_assets.hpp"

#include "sbs.hpp"
#include "worldmap.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace dtvr {

std::vector<uint8_t> readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Nicht lesbar: " + path.string());
    return {std::istreambuf_iterator<char>(in), {}};
}

Surface loadBmp(const fs::path& path) {
    Surface image(SDL_LoadBMP(path.c_str()), SDL_FreeSurface);
    if (!image) throw std::runtime_error(path.string() + ": " + SDL_GetError());
    return image;
}

Surface loadWorld(const fs::path& root, int month, bool views, bool seasons, std::vector<Tile>* tilesOut) {
    const int season = seasons ? seasonForMonth(month) : 1;
    std::vector<Tile> tiles;
    Surface palette(nullptr, SDL_FreeSurface);
    for (int part = 0;; ++part) {
        std::ostringstream name;
        name << "part" << std::setw(4) << std::setfill('0') << part << ".sbb";
        const auto path = root / "map" / std::to_string(season) / name.str();
        if (!fs::is_regular_file(path)) break; // the original also stops at the first missing part
        auto strip = loadBmp(path);
        if (strip->format->BitsPerPixel != 8 || !strip->format->palette || strip->h != 16 * 192)
            throw std::runtime_error("Unerwartetes SBB-Format: " + path.string());
        const auto part_tiles = splitSbb(static_cast<const uint8_t*>(strip->pixels), strip->w, strip->h, strip->pitch);
        tiles.insert(tiles.end(), part_tiles.begin(), part_tiles.end());
        if (!palette) palette = std::move(strip);
    }
    if (!palette) throw std::runtime_error("Keine SBB-Dateien fuer Jahreszeit " + std::to_string(season));
    const WorldMap map(readFile(root / "map/europe.map"));
    const auto pixels = renderWorld(map, tiles, views);
    constexpr int side = WorldMap::kSize * WorldMap::kTile;
    Surface world(SDL_CreateRGBSurfaceWithFormat(0, side, side, 8, SDL_PIXELFORMAT_INDEX8), SDL_FreeSurface);
    if (!world || SDL_SetSurfacePalette(world.get(), palette->format->palette) != 0)
        throw std::runtime_error(SDL_GetError());
    for (int y = 0; y < side; ++y)
        std::copy_n(&pixels[size_t(y) * side], side, static_cast<uint8_t*>(world->pixels) + y * world->pitch);
    if (tilesOut) *tilesOut = std::move(tiles);
    return world;
}

std::string upperAscii(std::string s) {
    for (char& c : s) c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

void loadSbsTextures(SDL_Renderer* renderer, const fs::path& file, const SDL_Palette* palette,
                     std::map<std::string, Texture>& into, int transparent) {
    if (file.empty() || !palette) return;
    for (const SbsFrame& frame : parseSbs(readFile(file)).frames) {
        Surface surface(SDL_CreateRGBSurfaceWithFormat(0, frame.width, frame.height, 8, SDL_PIXELFORMAT_INDEX8),
                        SDL_FreeSurface);
        if (!surface) continue;
        SDL_SetSurfacePalette(surface.get(), const_cast<SDL_Palette*>(palette));
        for (int y = 0; y < frame.height; ++y)
            std::copy_n(&frame.pixels[size_t(y) * frame.width], frame.width,
                        static_cast<uint8_t*>(surface->pixels) + y * surface->pitch);
        if (transparent >= 0) SDL_SetColorKey(surface.get(), SDL_TRUE, Uint32(transparent));
        into.emplace(upperAscii(frame.name),
                     Texture(SDL_CreateTextureFromSurface(renderer, surface.get()), SDL_DestroyTexture));
    }
}

} // namespace dtvr
