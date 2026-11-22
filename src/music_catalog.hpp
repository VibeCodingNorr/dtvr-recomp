// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace dtvr {

inline constexpr std::array<const char*, 25> kMusicCues{
    "a1", "e1", "e2", "e3", "e4", "e5", "e6", "e7", "e8", "e9", "e10", "e11",
    "f1", "f2", "f3", "g0", "g1", "g2", "g3", "g4", "g5", "i", "s", "z1", "z2"};
struct MusicTrack { std::filesystem::path file; double nominalEnd = 0; };
struct MusicPack {
    std::string id, name;
    std::map<std::string, MusicTrack> tracks;
};
bool validMusicPackId(const std::string& id);
MusicPack readMusicPack(const std::filesystem::path& directory);
std::vector<MusicPack> discoverMusicPacks(const std::filesystem::path& directory,
                                        std::vector<std::string>& issues);
// Recovered event message families; no cue is invented for drought/e10.
std::string musicForNotice(int text);

struct MusicPreferences { std::string pack; int volume = 70; };
MusicPreferences loadMusicPreferences(const std::filesystem::path& file);
bool saveMusicPreferences(const std::filesystem::path& file, const MusicPreferences& preferences,
                          std::string& error);

} // namespace dtvr
