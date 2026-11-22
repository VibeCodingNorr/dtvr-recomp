// SPDX-License-Identifier: GPL-3.0-only
#include "event_scenes.hpp"

namespace dtvr {
namespace {

// Definitions built on the handler's stack (u16 delay, u8 0, u8 count, names), the
// SBS set, record +0xA and the position added to the place's cell * 16.
// Sturm seg8:1C01: DS:5F0E.., sbs\sturm.sbs (DS:5F3A), repetitions 4, (8, -0x1D).
const EventScene kStorm{"sturm.sbs", {"ST1", "ST2", "ST3", "ST4", "ST5", "ST6"}, 6, 4, 8, -0x1D};
// Räuber seg8:1E70: 39 frames, sbs\raub.sbs (DS:60E7), repetitions 0, the victim's
// field * 16 + (0xC, 4) (seg8:213A ff.).
const EventScene kRobbers{"raub.sbs",
                          {"RA01", "RA02", "RA03", "RA04", "RA05", "RA06", "RA07", "RA08", "RA09", "RA10",
                           "RA11", "RA12", "RA13", "RA14", "RA15", "RA16", "RA17", "RA18", "RA19", "RA20",
                           "RA21", "RA16", "RA17", "RA18", "RA19", "RA20", "RA21", "RA16", "RA17", "RA18",
                           "RA19", "RA20", "RA21", "RA16", "RA17", "RA18", "RA22", "RA23", "RA24"},
                          5, 0, 0xC, 4};
// Vulkan seg8:36FD: 54 frames, sbs\vulkan.sbs (DS:69FF), repetitions 0, the place
// (DS:69F0 table) * 16 + (0xD, 0x11) (seg8:39A8 ff.).
const EventScene kVolcano{"vulkan.sbs",
                          {"V01", "V02", "V03", "V04", "V01", "V02", "V03", "V04", "V01", "V02", "V05", "V06",
                           "V07", "V08", "V09", "V01", "V02", "V03", "V04", "V01", "V02", "V03", "V04", "V01",
                           "V02", "V05", "V10", "V11", "V12", "V13", "V14", "V15", "V16", "V17", "V18", "V19",
                           "V20", "V21", "V22", "V23", "V24", "V25", "V26", "V27", "V28", "V29", "V26", "V27",
                           "V28", "V29", "V26", "V27", "V28", "V29"},
                          5, 0, 0xD, 0x11};
// Steinschlag seg8:5A76: 33 frames st01-st33, sbs\stein.sbs (DS:717F), delay 3,
// repetitions 0, the place * 16 + (7, -0x17) (seg8:5CAD ff.).
const EventScene kRockfall{"stein.sbs",
                           {"st01", "st02", "st03", "st04", "st05", "st06", "st07", "st08", "st09", "st10", "st11",
                            "st12", "st13", "st14", "st15", "st16", "st17", "st18", "st19", "st20", "st21", "st22",
                            "st23", "st24", "st25", "st26", "st27", "st28", "st29", "st30", "st31", "st32", "st33"},
                           3, 0, 7, -0x17};

// Erdbeben seg8:3C0B: 18 frames BR01-BR15 (delay 5, repetitions 0), equake.sbs
// (DS:6AC9); two records with the same definition at the cracks of the place
// (tables on the stack, seg8:3E8A ff.), the second only where its x is not 0;
// seg6:0F60(2000) before and seg6:0F60(500) after the loop.
const EventScene kQuake{"equake.sbs",
                        {"BR01", "BR02", "BR03", "BR04", "BR05", "BR06", "BR07", "BR08", "BR09", "BR10", "BR11",
                         "BR12", "BR13", "BR14", "BR15", "BR15", "BR15", "BR15"},
                        5, 0, 0, 0, 2000, 500, true};
// Per place (centre DS stack bp-0x66 / -0x7E = the quake places): crack A at
// (ax + 1, ay + 1) * 16, crack B at (bx + 1, by + 1) * 16 when bx != 0.
struct QuakePlace { int cx, cy, ax, ay, bx, by; };
constexpr QuakePlace kQuakePlaces[12] = {
    {0xD3, 0xE0, 0xD4, 0xE1, 0, 0},       {0x97, 0xCF, 0x98, 0xCB, 0x9C, 0xCA}, {0x7A, 0xF2, 0x73, 0xF5, 0, 0},
    {0xB9, 0xAF, 0xBA, 0xAA, 0, 0},       {0xDF, 0xA1, 0xE0, 0x9E, 0, 0},       {0x15, 0xB2, 0x16, 0xAD, 0, 0},
    {0x2F, 0xC4, 0x2A, 0xC3, 0x31, 0xC3}, {0x9F, 0x9A, 0xA0, 0x9B, 0, 0},       {0xAE, 0x8B, 0xAE, 0x8C, 0, 0},
    {0x7B, 0x6D, 0x7D, 0x6A, 0, 0},       {0x89, 0x5E, 0x85, 0x5A, 0, 0},       {0xEF, 0x37, 0xE7, 0x35, 0xF0, 0x3A}};

// seg2:6DCB for one record without follow-up: false once the record is freed.
struct RecordState {
    int countdown = 0, repetitions = 0, frame = 0; // +8, +0xA, +0xC
    bool step(const EventScene& s) {
        const int old = countdown--;
        if (old > 0) return true;
        countdown = s.delay;
        if (++frame < int(s.frames.size())) return true;
        frame = 0;
        if (repetitions > 0) { --repetitions; return true; }
        return false; // no callback, no follow-up definition: freed
    }
};

} // namespace

const EventScene* eventSceneForNotice(int text) {
    // The text is drawn after the animation (base + random(2), e.g. seg8:1DFD).
    if (text >= 5041 && text <= 5043) return &kStorm;
    if (text >= 5021 && text <= 5023) return &kRobbers;
    if ((text >= 5281 && text <= 5283) || (text >= 5301 && text <= 5303)) return &kVolcano; // Etna / Hekla
    if (text >= 5081 && text <= 5083) return &kRockfall;
    if (text >= 5201 && text <= 5203) return &kQuake; // 0x1451 + random(2) at seg8:4234
    return nullptr;
}

std::vector<ScenePoint> eventScenePositions(const EventScene& scene, int cellX, int cellY) {
    if (!scene.quakeCracks) return {ScenePoint{cellX * 16 + scene.dx, cellY * 16 + scene.dy}};
    for (const QuakePlace& q : kQuakePlaces) {
        if (q.cx != cellX || q.cy != cellY) continue;
        std::vector<ScenePoint> points{{(q.ax + 1) * 16, (q.ay + 1) * 16}}; // seg8:3E8A
        if (q.bx) points.push_back({(q.bx + 1) * 16, (q.by + 1) * 16});      // seg8:40FF
        return points;
    }
    return {};
}

// seg8:6E0F: each pass first advances the record, then draws it while it lives.
int eventScenePasses(const EventScene& scene) {
    RecordState r{0, scene.repetitions, 0};
    int passes = 0;
    while (r.step(scene) && passes < 100000) ++passes;
    return passes;
}

const char* eventSceneFrame(const EventScene& scene, int pass) {
    RecordState r{0, scene.repetitions, 0};
    for (int i = 0; i <= pass; ++i)
        if (!r.step(scene)) return "";
    return scene.frames.empty() ? "" : scene.frames[size_t(r.frame)];
}

} // namespace dtvr
