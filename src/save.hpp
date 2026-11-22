// SPDX-License-Identifier: GPL-3.0-only
// Native, versioned save format (docs/architecture.md, "Save files"; format in
// docs/save-format.md). Independent of the original game's save files.
#pragma once

#include "game.hpp"

#include <cstdint>
#include <string>

namespace dtvr {

constexpr int kSaveFormat = 4;     // layout (1: no Risiko record, 2: no demo record, 3: no stages record)
constexpr int kRulesRevision = 32; // rule reconstruction round the state belongs to (26: draws of the
                                   // blocked card window and the investment offer; 27: draws at the
                                   // stage end, seg2:12D8 / 136F / 1231 and seg7:3B4E; 28: the
                                   // year balance's draws, seg8:88DF / 8C49 / 8C57; 29: one more
                                   // per player, seg8:8E38; 30: the next destination before the
                                   // announcement's draw and without the old stage start; 31: the
                                   // scene start random(3) of rain, snow and flood, seg8:517B; 32:
                                   // Nägel fall into the water on a seaway, seg3:1CCF)

// FNV-1a over the board and town records, so a save is only loaded with the data
// it was written for.
uint64_t dataFingerprint(const Board& board, const std::vector<Town>& towns);

class SaveCodec {
public:
    // The complete simulation state as text (messages and derived data excluded).
    static std::string encode(const Game& game);
    // Validates everything first; on failure `game` is unchanged and `error` says why.
    static bool decode(Game& game, const std::string& text, std::string& error);
};

// Writes atomically (temporary file in the same directory, then rename).
bool writeSaveFile(const std::string& path, const std::string& text, std::string& error);
bool readSaveFile(const std::string& path, std::string& text, std::string& error);

} // namespace dtvr
