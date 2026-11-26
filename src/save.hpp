// SPDX-License-Identifier: GPL-3.0-only
// Native, versioned save format (docs/architecture.md, "Save files"; format in
// docs/save-format.md). Independent of the original game's save files.
#pragma once

#include "game.hpp"

#include <cstdint>
#include <string>

namespace dtvr {

constexpr int kSaveFormat = 7;     // layout (1: no Risiko record, 2: no demo record, 3: no stages record,
                                   // 4: no extensions record, 5: no worth records, 6: no flow records)
constexpr int kRulesRevision = 49; // rule reconstruction round the state belongs to (26: draws of the
                                   // blocked card window and the investment offer; 27: draws at the
                                   // stage end, seg2:12D8 / 136F / 1231 and seg7:3B4E; 28: the
                                   // year balance's draws, seg8:88DF / 8C49 / 8C57; 29: one more
                                   // per player, seg8:8E38; 30: the next destination before the
                                   // announcement's draw and without the old stage start; 31: the
                                   // scene start random(3) of rain, snow and flood, seg8:517B; 32:
                                   // Nägel fall into the water on a seaway, seg3:1CCF; 33: a shut-in
                                   // player's move is shortened or ends, seg2:61B7; 34: Nägel fall
                                   // into the water on sea fields, the plane state +0x2D = 2;
                                   // 35: the closing draw of the year balance, seg8:9605;
                                   // 36: Nägel only towards an available direction, seg3:225A;
                                   // 37: the TimeOut fines after "n Felder" cards, seg2:3B0E;
                                   // 38: the forced sale after an arrival in debt, seg1:4E39;
                                   // 39: Dr. Drago's start side random(3) at a form change,
                                   // seg2:459C; 40: the year balance's announcement and
                                   // closing draws follow the show, seg8:8E38 / 9605;
                                   // 41: Schulden at any balance, seg3:17E0; Abkaufen buys
                                   // free entries, seg1:806C; 42: the settlement only for
                                   // players in play, entries 0..7, values cleared per
                                   // player, the year's rent and worth, seg1:85F9 / 8BC6;
                                   // 43: Konto with one player in play, seg3:15C9; the
                                   // year balance's closing letter per spoken result,
                                   // seg8:913D; 44: the horn draw random(1) on a field
                                   // with another driver, seg2:2676; Konferenz copies
                                   // the facing, seg3:3092; 45: the Master-Rallye start
                                   // cells random(12 - n) and Dr. Drago to the leader,
                                   // seg2:5CDE / 5FCA; Brussels as the last destination,
                                   // seg7:4B89; seg2:04AD re-places Dr. Drago after Punch,
                                   // Flugzeug and Tschüss; start facing seg2:5067;
                                   // 46: the month's cash flow +0x4A and +0x62,
                                   // seg2:59E6 / 5A47 / seg1:8A33; +0x52 / +0x4A cleared
                                   // after every month, seg1:8CBA; STAT, seg1:972F;
                                   // 47: Dr. Drago at a stage change by the distances
                                   // from the destination reached, unsigned, seg2:1495;
                                   // 48: Tschüss / Flugzeug carry the player out of the
                                   // zoomed canvas view, inclusive, then random(3),
                                   // seg3:28ED / seg2:46C8; Flugzeug sets the player down
                                   // there first, seg3:3272; Mitspieler only at the turn
                                   // start, the computer from its card choice, seg5:6BF2;
                                   // 49: loading as rally.exe's loader, seg8:AF67-B06A and
                                   // the turn start seg2:0407 (Game::resumeLoaded); the
                                   // hand order seg2:3CF1 at every turn start and after
                                   // the card shop, seg2:0473 / seg3:48B1 / 49CC)

// FNV-1a over the board and town records, so a save is only loaded with the data
// it was written for.
uint64_t dataFingerprint(const Board& board, const std::vector<Town>& towns);

class SaveCodec {
public:
    // The complete simulation state as text (messages and derived data excluded).
    static std::string encode(const Game& game);
    // Validates everything first; on failure `game` is unchanged and `error` says why.
    // Random::Session keeps the game's running random generator, as rally.exe does when
    // it loads a game: its saves do not contain the C runtime's seed DS:838E, which is
    // seeded once at program start (seg5:0029 -> seg1:02A9) and runs on (round 130).
    // Restore (the default) is for exact copies: a new game, taking a roll back.
    enum class Random { Restore, Session };
    static bool decode(Game& game, const std::string& text, std::string& error, Random random = Random::Restore);
};

// Writes atomically (temporary file in the same directory, then rename).
bool writeSaveFile(const std::string& path, const std::string& text, std::string& error);
bool readSaveFile(const std::string& path, std::string& text, std::string& error);

} // namespace dtvr
