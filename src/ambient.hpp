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
    // seg8:7CFB: scan both layers of every cell (tile & 0xFFF); the list is empty.
    void build(const WorldMap& map);
    size_t size() const { return objects_.size(); }
    size_t listed() const { return listed_.size(); }
    // seg8:7F7D (from the redraw seg2:05A8 after every change of the view): the listed
    // objects show their last frame, then the list DS:A05E is rebuilt with the objects
    // whose rectangle (pixels, seg8:7EED) touches (originX, originY, originX + viewWidth,
    // originY + viewHeight) — rally.exe takes the origin DS:956A / 9568 in *tiles* here
    // and the view size DS:94DE / 94E0 in pixels (seg8:81E2), so the list holds objects
    // near the world's top left rather than the visible ones — at most 150 (seg8:828B),
    // each started as seg8:8277 does: pause random(range), delay 0, the definition's
    // repeats, frame 0xFF, restart 1.
    void setView(int originX, int originY, int viewWidth, int viewHeight, WorldMap& map, MsRandom& rng,
                 std::vector<AmbientCell>& changed);
    // seg8:83A0, one pass over the listed objects: changes `map`, appends the changed
    // cells and the sounds.
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
    void writeFrame(const Object& o, WorldMap& map, std::vector<AmbientCell>& changed) const;
    std::vector<Object> objects_;
    std::vector<size_t> listed_; // DS:A05E: indices into objects_
};

} // namespace dtvr
