// SPDX-License-Identifier: GPL-3.0-only
// The nine save slots of the save dialog (seg8:B157; round 33). The original
// writes `<dir>\slot<n>` and a description "Month (year) - names" into its
// profile (section Save, key Slot<n>, default "leer"). The rebuild keeps the
// same slots and descriptions, but in its own native format (save.hpp) in the
// user's data directory. No SDL.
#pragma once

#include "game.hpp"

#include <filesystem>
#include <map>
#include <string>

namespace dtvr {

constexpr int kSaveSlots = 9; // slots 0..8 (seg8:B750, seg8:B53A)

// $XDG_DATA_HOME/dtvr-recomp/saves, else ~/.local/share/dtvr-recomp/saves.
std::filesystem::path defaultSaveDirectory();

// seg8:B413: "%Fs (%d) - %Fs" with text 301 + month % 12, year + 1 and the names of
// the players in play joined by " - " (seg8:BD58, at most 79 characters, "..." when cut).
std::string saveDescription(const Game& game, const std::map<int, std::string>& texts);

class SaveSlots {
public:
    explicit SaveSlots(std::filesystem::path directory) : directory_(std::move(directory)) {}
    // The stored description, empty for an unused slot (the dialog shows text "leer").
    std::string description(int slot) const;
    bool save(int slot, const Game& game, const std::string& description, std::string& error) const;
    // Loading keeps the game's running random generator (SaveCodec::Random::Session).
    bool load(int slot, Game& game, std::string& error) const;
    std::filesystem::path file(int slot) const;
    // Autosave of the turn start (seg8:A870: `%s\as.dat` and profile Save/AS = 1);
    // "Spiel fortsetzen" loads it (seg8:A8DF).
    bool saveAuto(const Game& game, std::string& error) const;
    bool loadAuto(Game& game, std::string& error) const;
    bool hasAuto() const;
    std::filesystem::path autoFile() const { return directory_ / "as.sav"; }

private:
    std::map<int, std::string> readIndex() const;
    std::filesystem::path directory_;
};

} // namespace dtvr
