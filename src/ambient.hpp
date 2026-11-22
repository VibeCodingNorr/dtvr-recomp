// SPDX-License-Identifier: GPL-3.0-only
// Ambient map animations of rally.exe ([Setup] Animations, round 103; no SDL):
// seg8:7CFB finds every cell whose tile starts one of the 51 definitions of the
// table DS:7B62, and seg8:83A0 advances them once per idle pass (seg2:1C37),
// writing the frames' tiles into the map (seg6:273B) and sometimes playing a
// sound. Presentation only: the tiles change a copy of the map, not the game's,
// and the random numbers come from the caller (the original draws them from the
// game's rand at a rate that depends on waiting, so they cannot be reproduced).
#pragma once

#include "rules.hpp"
#include "worldmap.hpp"

#include <cstdint>
#include <vector>

namespace dtvr {

struct AmbientDefinition {
    uint16_t tile;          // +0: the top-left tile that marks an object
    uint8_t layer;          // +2: 0 or 1
    uint8_t width, height;  // +4 / +5: cells
    uint8_t delay;          // +6: passes per frame
    uint8_t repeats;        // +7: extra runs before a pause, 0xFF = endless
    uint16_t pause;         // +8: random(pause) passes between runs
    uint16_t sound;         // +0xA: sound number for channel 6, 0 none
    uint8_t chance;         // +0xC: the sound plays when random(chance) == 0
    std::vector<uint16_t> frames; // +0xD: first tile of each frame (row-major cells), 0-terminated
};
const std::vector<AmbientDefinition>& ambientDefinitions();

struct AmbientCell { int layer, x, y; };

class AmbientAnimations {
public:
    // seg8:7CFB: scan both layers of every cell (tile & 0xFFF).
    void build(const WorldMap& map);
    size_t size() const { return objects_.size(); }
    // seg8:83A0, one pass: changes `map`, appends the changed cells and the sounds.
    void tick(WorldMap& map, MsRandom& rng, std::vector<AmbientCell>& changed, std::vector<int>& sounds);

private:
    struct Object {
        const AmbientDefinition* def = nullptr;
        int x = 0, y = 0;         // cells (+4 / +6 >> 4)
        int countdown = 0;        // +0xC
        int delay = 0;            // +0xE
        int repeats = 0;          // +0xF
        int frame = 0;            // +0x10 (byte, 0xFF before a run)
        bool restart = false;     // +0x11
    };
    std::vector<Object> objects_;
};

} // namespace dtvr
