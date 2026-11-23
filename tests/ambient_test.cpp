// SPDX-License-Identifier: GPL-3.0-only
// Ambient map animations (round 103): definitions from DS:7B62, scan, frame cycle.
#include "ambient.hpp"

#include <cstdio>
#include <cstdlib>

using namespace dtvr;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}
WorldMap mapWith(int layer, int x, int y, uint16_t value) {
    std::vector<uint8_t> data(size_t(WorldMap::kSize) * WorldMap::kSize * WorldMap::kLayers * 2, 0);
    const size_t i = (size_t(layer) * WorldMap::kSize + size_t(y)) * WorldMap::kSize + size_t(x);
    data[i * 2] = uint8_t(value);
    data[i * 2 + 1] = uint8_t(value >> 8);
    return WorldMap(data);
}
} // namespace

int main() {
    const auto& defs = ambientDefinitions();
    check(defs.size() == 51, "51 definitions");
    check(defs[0].tile == 0xA7A && defs[0].width == 3 && defs[0].height == 2 && defs[0].frames.size() == 4,
          "first definition (DS:71F6)");
    for (const auto& d : defs) check(!d.frames.empty() && d.width > 0 && d.height > 0, "every definition has frames");

    // A 1 x 1 object: DS:7290, tile 0xAD5, delay 3, two repeats, pause 10, seven frames.
    const AmbientDefinition* def = nullptr;
    for (const auto& d : defs) if (d.tile == 0xAD5) def = &d;
    check(def && def->delay == 3 && def->repeats == 2 && def->pause == 10 && def->frames.size() == 7, "DS:7290");
    WorldMap map = mapWith(0, 10, 20, uint16_t(0x4000 | 0xAD5)); // mirrored: the flag stays
    AmbientAnimations a;
    MsRandom rng(1);
    MsRandom expected(1);
    map.setTile(0, 100, 100, 0xAD5); // a second object in the middle of the world
    a.build(map);
    check(a.size() == 2 && a.listed() == 0, "two objects found, none listed before a view");
    std::vector<AmbientCell> changed;
    std::vector<int> sounds;
    a.tick(map, rng, changed, sounds);
    check(changed.empty(), "nothing moves without the list");
    // seg8:81E2: the view is (origin in tiles, + size in pixels) against pixel rectangles:
    // with the origin (100, 100) the object at cell (100, 100) (pixel 1600) is not listed,
    // the one at (10, 20) (pixels 160 / 320) is.
    const int pause = expected.random(10); // seg8:8277: the start pause random(+8)
    a.setView(100, 100, 688, 688, map, rng, changed);
    check(a.listed() == 1 && changed.empty(), "rally.exe lists the object near the top left");
    a.setView(0, 0, 0, 0, map, expected, changed); // a view touching neither (pause drawn from another generator)
    check(a.listed() == 0, "an empty view lists nothing");
    check(changed.size() == 1 && map.cell(0, 10, 20) == (0x4000 | 0xADB), "the unlisted object shows its last frame");
    map.setTile(0, 10, 20, uint16_t(0x4000 | 0xAD5));
    rng = MsRandom(1);
    a.setView(100, 100, 688, 688, map, rng, changed);
    changed.clear();
    int passes = 0;
    while (map.cell(0, 10, 20) != (0x4000 | 0xAD6) && passes < 100) {
        changed.clear();
        a.tick(map, rng, changed, sounds);
        ++passes;
    }
    // pause passes, then frame 0 (0xFF + 1), three passes of delay, then frame 1.
    check(passes == pause + 1 + 3 + 1, "the start waits for its random pause");
    check(changed.size() == 1 && changed[0].x == 10 && changed[0].y == 20, "the cell changes");
    check(map.cell(0, 10, 20) == (0x4000 | 0xAD6), "frame 1, flags kept");
    changed.clear();
    for (int i = 0; i < 3; ++i) a.tick(map, rng, changed, sounds); // three passes of delay
    check(changed.empty() && map.cell(0, 10, 20) == (0x4000 | 0xAD6), "delay 3 holds the frame");
    a.tick(map, rng, changed, sounds);
    check(map.cell(0, 10, 20) == (0x4000 | 0xAD7), "then frame 2");
    // Run through: 7 frames x 4 passes per run, 1 + 2 repeats, then a pause.
    int runs = 0, lastFrame0 = 0;
    for (int i = 0; i < 200; ++i) {
        a.tick(map, rng, changed, sounds);
        if (map.cell(0, 10, 20) == (0x4000 | 0xAD5) && lastFrame0 == 0) ++runs;
        lastFrame0 = map.cell(0, 10, 20) == (0x4000 | 0xAD5);
    }
    check(runs >= 2, "the run repeats");
    check(sounds.empty(), "no sound for this definition");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
