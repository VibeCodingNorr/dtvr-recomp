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

// $XDG_DATA_HOME/dtvr-recomp/saves, else ~/.local/share/dtvr-recomp/saves: the German
// edition's directory and, before round E3, the one directory of both editions.
std::filesystem::path defaultSaveDirectory();
// The save directory of an edition (recomp, round E3; docs/save-format.md "Save slots"):
// German keeps defaultSaveDirectory(), every other edition has its own sibling
// (EditionProfile::saveDirectory), so no edition overwrites another one's slots or autosave.
std::filesystem::path saveDirectory(Edition edition);

// Migration of round E3, once per edition: when `own` does not exist yet and differs
// from `shared` (the former common directory), copies the slots, their descriptions and
// the autosave of `edition` from `shared` into `own`. Copies only, never moves or
// deletes: the originals stay and show as foreign saves in the other edition's dialog.
// `copied` counts the files copied. False with `error` when copying fails.
bool adoptEditionSaves(const std::filesystem::path& shared, const std::filesystem::path& own, Edition edition,
                       int& copied, std::string& error);

// The recomp message when a save of edition `saveEdition` (id) cannot be loaded by
// `running`: in English for an English edition (language letter 'E'), else German.
std::string foreignSaveMessage(Edition running, const std::string& saveEdition);
// The save dialog's label of a slot of edition `foreign` (empty: own, unchanged):
// "[US] " or "[DE] " (EditionProfile::tag, "[?] " for an unknown id) before it.
std::string markForeignSave(const std::string& label, const std::string& foreign);

// seg8:B413: "%Fs (%d) - %Fs" with text 301 + month % 12, year + 1 and the names of
// the players in play joined by " - " (seg8:BD58, at most 79 characters, "..." when cut).
std::string saveDescription(const Game& game, const std::map<int, std::string>& texts);

class SaveSlots {
public:
    explicit SaveSlots(std::filesystem::path directory, Edition edition = Edition::GermanRetail)
        : directory_(std::move(directory)), edition_(edition) {}
    // The stored description, empty for an unused slot (the dialog shows text "leer").
    std::string description(int slot) const;
    bool save(int slot, const Game& game, const std::string& description, std::string& error) const;
    // Loading keeps the game's running random generator (SaveCodec::Random::Session) and
    // then does what rally.exe does after reading a save (Game::resumeLoaded).
    bool load(int slot, Game& game, std::string& error) const;
    std::filesystem::path file(int slot) const;
    // Autosave of the turn start (seg8:A870: `%s\as.dat` and profile Save/AS = 1);
    // "Spiel fortsetzen" loads it (seg8:A8DF).
    bool saveAuto(const Game& game, std::string& error) const;
    bool loadAuto(Game& game, std::string& error) const;
    bool hasAuto() const;
    std::filesystem::path autoFile() const { return directory_ / "as.sav"; }
    // Recomp (round E3): the edition id of a slot or of the autosave when it belongs to
    // another edition than this one's, else empty (also for an absent or unreadable file:
    // loading reports those). Reads only the beginning of the file.
    std::string foreignSlot(int slot) const;
    std::string foreignAuto() const;
    // "Spiel fortsetzen" is offered (not grayed) only for an autosave of this edition.
    bool hasOwnAuto() const { return hasAuto() && foreignAuto().empty(); }
    Edition edition() const { return edition_; }
    const std::filesystem::path& directory() const { return directory_; }

private:
    std::map<int, std::string> readIndex() const;
    std::filesystem::path directory_;
    Edition edition_;
};

} // namespace dtvr
