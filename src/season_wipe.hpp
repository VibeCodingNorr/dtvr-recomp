// SPDX-License-Identifier: GPL-3.0-only
// The venetian-blind wipe of the map window at a season change (no SDL). The event
// trigger seg8:1624 reloads the season's tiles with seg5:186A(DS:A100, 0); when that
// returns non-zero (a new tile directory, only with [Setup] Colorcycle on: seg5:1929)
// it fills the map's WinG bitmap black (FillRect DS:A7B2, DS:94DA, BLACK_BRUSH at
// seg8:1667), wipes it in with seg8:0E20(map window DS:A6F0, mode 0) at seg8:1682,
// redraws the map with the new tiles (seg2:05A8(0) at seg8:168C), sets the season
// palette (seg5:3C79) and wipes the new map in the same way (seg8:169F); then it
// returns 1 and no event happens. Evidence in docs/formats.md "Season change".
#pragma once

#include <cstdint>
#include <vector>

namespace dtvr {

// Mode 0 of seg8:0E20 (seg8:0E61-0EF6): for pass i = 0..39 and blind j = 0..19 with
// 0 <= i - j <= 19 it invalidates and updates the client rows top = (19j + i) * height
// / 400 (seg10:213A / 20A0, 32-bit) to top + 2 over the whole width. 20 blinds of
// 20 lines each; blind j starts one pass after blind j - 1. No random number, no sound.
struct BlindStrip { int top = 0, bottom = 0; };
constexpr int kBlindPasses = 40;
constexpr int kBlindStrips = 400;
std::vector<BlindStrip> blindStrips(int height); // in drawing order
int blindStripsAfter(int passes);                // strips drawn by the first `passes` passes

struct BlindRun { int top = 0, height = 0; };

class SeasonWipe {
public:
    enum class Stage { Idle, Out, In };
    // Recomp timing (not in rally.exe): mode 0 has no delay (unlike modes 1-5, which
    // wait seg6:0CDF(20) every 16 cells), so the original's speed was that of 400
    // synchronous repaints of the map window on the player's machine.
    static constexpr std::uint32_t kPassMs = 10;
    static constexpr std::uint32_t kStageMs = kPassMs * kBlindPasses;

    void start(std::uint32_t now) { start_ = now; running_ = true; }
    void stop() { running_ = false; }
    Stage stage(std::uint32_t now) const;
    bool active(std::uint32_t now) const { return stage(now) != Stage::Idle; }
    // Strips of the current stage already drawn.
    int stripsShown(std::uint32_t now) const;
    // The client rows of the map window (height `height`) that are black at `now`:
    // the old map is blacked out strip by strip, then the new map revealed the same way.
    std::vector<BlindRun> blackRows(int height, std::uint32_t now) const;

private:
    std::uint32_t start_ = 0;
    bool running_ = false;
};

} // namespace dtvr
