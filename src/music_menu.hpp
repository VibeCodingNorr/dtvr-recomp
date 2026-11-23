// SPDX-License-Identifier: GPL-3.0-only
// The music page of the recomp menu (src/recomp_menu.hpp, Audio): the installed
// sound sets, volume and current piece of the MusicPlayer, and applying a choice.
// Presentation only: nothing here touches game rules or the RNG.
#pragma once

#include "music_player.hpp"
#include "recomp_menu.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace dtvr {

// What the Audio page shows; `issues` are the sound sets the catalog rejected.
RecompMenu::Music musicMenuState(const MusicPlayer& player, const std::vector<std::string>& issues);
// Applies the chosen set (at the next cue boundary) and volume and saves music.ini;
// false with `error` when the preferences could not be written.
bool applyMusicChoice(MusicPlayer& player, const RecompMenu::Music& music, const std::filesystem::path& preferences,
                      std::string& error);

} // namespace dtvr
