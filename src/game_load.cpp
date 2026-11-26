// SPDX-License-Identifier: GPL-3.0-only
// What rally.exe recomputes after reading a saved game (user decision 2026-11-25:
// loading behaves like the original loader). Evidence: docs/formats.md, "Loading a
// saved game"; the native file format itself is docs/save-format.md.
#include "game.hpp"

#include "worldmap.hpp"

namespace dtvr {

void Game::resumeLoaded() {
    // seg4:066F / 05EA: Spiel laden and Spiel fortsetzen clear DS:16 (the demo's end
    // request) before the file is read; DS:16 is not part of the file.
    demoEnd_ = false;

    // The loader seg8:AC9A after its reads (seg8:AF67 ff.):
    // AF67: with DS:A10F = 0 (East open) seg8:69F0(1) replaces the barrier tiles without
    // its message and draw (seg8:6A04 / 6BFB skip them for argument 1), sets DS:A10F = 0
    // and stores seg1:A15B() in DS:A113: the value of the kind 4 / 6 fields' entries with
    // the mask now 0, i.e. of all towns (seg8:6C46-6C5D). The tiles follow eastOpen_.
    if (eastOpen_) westValue_ = westPropertyValue(board_, towns_, 0);
    // AF82: seg2:43A8(DS:A10B, DS:A10D) refills DS:22; the rebuild derives it
    // (distanceTable). AF8A-AF9B: DS:94E2 = DS:A10A and the deck pointers DS:94FA /
    // DS:9C94 from the header: restored by SaveCodec::decode.
    // AFA7-B029: every slot with status != 0 gets +0x16 / +0x18 = the cell of its field
    // +0x3E times 16 and seg2:5067: the first direction with a link (frame pointer
    // +0x20), plane state +0x2D / +0x2E by the cell's bit 8. The map puts the vehicle
    // there at once (positionJumps) and takes the resting air state of that cell.
    for (int p = 0; p < kPlayerSlots; ++p) {
        PlayerSlot& slot = players_[size_t(p)];
        if (!slot.status || slot.field < 0) continue; // a field always lies on its own cell
        if (const int f = startFacing(board_.fields[size_t(slot.field)])) lastDirection_[size_t(p)] = f;
        ++jumps_[size_t(p)];
    }
    // B02C-B05A: the drivers' sprites (seg2:2C64) and Dr. Drago's (F_BIKE, F_CAR, F_BLIMB
    // freed, DS:9D32 / 9D34 = 0): presentation (GameScreen::loadDriverSprites).
    // B05D-B06A: DS:9CFC != 0xFF -> seg2:9C97(holder): his sprites, an empty trail, the
    // countdown DS:3DB4 = random(3) + 4 (seg2:B477; DS:3DB4 is not in the file) and his
    // place beside the holder (seg2:9CF9).
    if (drago_.holder >= 0) {
        placeDrago(drago_, drago_.holder, rng_);
        ++dragoPlacements_;
    }

    // The title's start of the loaded game (seg4:069B ff. after seg8:B157(0), seg4:061D
    // ff. after seg8:A8DF(0)): seg5:186A(DS:A100, 1) reloads europe.pit and the tiles of
    // the month's season (DS:2CA = that season; with [Setup] Colorcycle 0 always 1, which
    // the event trigger of the rebuild does not model either).
    loadedSeason_ = seasonForMonth(calendar_.month);
    // seg8:6709 writes the obstacles' tiles and cell flags again: derived from the table.
    messages_.clear();
    ++turnSerial_; // the turn start below; the map centres on the current player (seg2:3216)
    // seg2:0407, the turn start of DS:94E2. rally.exe saves only at a turn start before
    // the spin (the autosave seg2:0837 after seg2:0801, Spiel speichern from MAIN1, which
    // opens only from ONFIELD), so it always runs; the rebuild's saves of other phases
    // continue where they were.
    if (phase_ == Phase::Roll) beginTurn();
    // Then seg1:275E (the next background cue; the window), DS:94C4 = 1, seg5:6BF2 (a
    // computer starts at its card choice: computerAdvance) and seg2:068F (ONFIELD for a
    // human: the window's turn menu).
}

} // namespace dtvr
