// SPDX-License-Identifier: GPL-3.0-only
// Loading original art from the user's installation with SDL (viewer and game
// window share these helpers).
#pragma once

#include <SDL.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace dtvr {

using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;
using Texture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

std::vector<uint8_t> readFile(const std::filesystem::path& path);
Surface loadBmp(const std::filesystem::path& path);

// The original ran on a case-insensitive file system (rally.exe asks for e.g.
// "P1\Dp.bmp", the disc holds "p1/dp.bmp"). Resolves `relative` below `root`
// component by component, ignoring ASCII case; empty if nothing matches.
std::filesystem::path findCaseInsensitive(const std::filesystem::path& root, const std::filesystem::path& relative);

// Composes europe.map from the season's SBB tiles as rally.exe does (loader
// seg5:186A, cell lookup seg6:26B6, draw passes seg6:28D5); 8-bit with the SBB palette.
Surface loadWorld(const std::filesystem::path& root, int month, bool views);

} // namespace dtvr
