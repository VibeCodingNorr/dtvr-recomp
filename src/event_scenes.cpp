// SPDX-License-Identifier: GPL-3.0-only
#include "event_scenes.hpp"

#include <cstdlib>

namespace dtvr {
namespace {

// Definitions built on the handler's stack (u16 delay, u8 0, u8 count, names), the
// SBS set, record +0xA and the position added to the place's cell * 16.
// Sturm seg8:1C01: DS:5F0E.., sbs\sturm.sbs (DS:5F3A), repetitions 4, (8, -0x1D).
const EventScene kStorm{"sturm.sbs", {{{"ST1", "ST2", "ST3", "ST4", "ST5", "ST6"}, 6, 4}}, 8, -0x1D};
// Räuber seg8:1E70: sbs\raub.sbs (DS:60E7), the victim's field * 16 + (0xC, 4).
const EventScene kRobbers{"raub.sbs",
                          {{{"RA01", "RA02", "RA03", "RA04", "RA05", "RA06", "RA07", "RA08", "RA09", "RA10",
                             "RA11", "RA12", "RA13", "RA14", "RA15", "RA16", "RA17", "RA18", "RA19", "RA20",
                             "RA21", "RA16", "RA17", "RA18", "RA19", "RA20", "RA21", "RA16", "RA17", "RA18",
                             "RA19", "RA20", "RA21", "RA16", "RA17", "RA18", "RA22", "RA23", "RA24"},
                            5, 0}},
                          0xC, 4};
// Vulkan seg8:36FD: sbs\vulkan.sbs (DS:69FF), the place (DS:69F0) * 16 + (0xD, 0x11).
const EventScene kVolcano{"vulkan.sbs",
                          {{{"V01", "V02", "V03", "V04", "V01", "V02", "V03", "V04", "V01", "V02", "V05", "V06",
                             "V07", "V08", "V09", "V01", "V02", "V03", "V04", "V01", "V02", "V03", "V04", "V01",
                             "V02", "V05", "V10", "V11", "V12", "V13", "V14", "V15", "V16", "V17", "V18", "V19",
                             "V20", "V21", "V22", "V23", "V24", "V25", "V26", "V27", "V28", "V29", "V26", "V27",
                             "V28", "V29", "V26", "V27", "V28", "V29"},
                            5, 0}},
                          0xD, 0x11};
// Steinschlag seg8:5A76: sbs\stein.sbs (DS:717F), the place * 16 + (7, -0x17).
const EventScene kRockfall{"stein.sbs",
                           {{{"st01", "st02", "st03", "st04", "st05", "st06", "st07", "st08", "st09", "st10",
                              "st11", "st12", "st13", "st14", "st15", "st16", "st17", "st18", "st19", "st20",
                              "st21", "st22", "st23", "st24", "st25", "st26", "st27", "st28", "st29", "st30",
                              "st31", "st32", "st33"},
                             3, 0}},
                           7, -0x17};
// Erdbeben seg8:3C0B: equake.sbs (DS:6AC9); two records with the same definition
// at the cracks of the place, seg6:0F60(2000) before and (500) after the loop.
const EventScene kQuake{"equake.sbs",
                        {{{"BR01", "BR02", "BR03", "BR04", "BR05", "BR06", "BR07", "BR08", "BR09", "BR10",
                           "BR11", "BR12", "BR13", "BR14", "BR15", "BR15", "BR15", "BR15"},
                          5, 0}},
                        0, 0, 2000, 500, true};
// Regen seg8:42B0: regen.sbs (DS:6C36); the cloud R00 (repetitions 1000) flies to
// the obstacle * 16 - (0, 0x11), then R01-R08 and R09-R20 (repetitions 1) there.
const EventScene kRain{"regen.sbs",
                       {{{"R00"}, 6, 1000},
                        {{"R01", "R02", "R03", "R04", "R05", "R06", "R07", "R08"}, 4, 0},
                        {{"R09", "R10", "R11", "R12", "R13", "R14", "R15", "R16", "R09", "R17", "R18",
                          "R19", "R20", "R14", "R15", "R16", "R09", "R17", "R18", "R19", "R20", "R14",
                          "R15", "R16", "R09", "R10", "R11", "R12", "R13", "R14", "R15", "R16"},
                         4, 1}},
                       0, -0x11, 0, 0, false, true};
// Schnee seg8:535E: schnee.sbs (DS:7017); SN01 flies to the obstacle * 16 + (5, -0x19),
// then SN01-SN30 and SN31-SN46 (repetitions 2).
const EventScene kSnow{"schnee.sbs",
                       {{{"SN01"}, 6, 1000},
                        {{"SN01", "SN02", "SN03", "SN04", "SN05", "SN06", "SN07", "SN08", "SN09", "SN10",
                          "SN11", "SN12", "SN13", "SN14", "SN15", "SN16", "SN17", "SN18", "SN19", "SN20",
                          "SN21", "SN22", "SN23", "SN24", "SN25", "SN26", "SN27", "SN28", "SN29", "SN30"},
                         4, 0},
                        {{"SN31", "SN32", "SN33", "SN34", "SN35", "SN36", "SN37", "SN38", "SN39", "SN40",
                          "SN41", "SN42", "SN43", "SN44", "SN45", "SN46"},
                         4, 2}},
                       5, -0x19, 0, 0, false, true};
// Hochwasser seg8:4964: flood.sbs (DS:6E1D); RG00 flies to the town * 16 - (0, 0x11),
// then RG01-RG26 (46 frames, delay 8).
const EventScene kFlood{"flood.sbs",
                        {{{"RG00"}, 6, 1000},
                         {{"RG01", "RG02", "RG03", "RG03", "RG02", "RG04", "RG05", "RG05", "RG06", "RG07",
                           "RG08", "RG08", "RG09", "RG07", "RG06", "RG10", "RG11", "RG06", "RG12", "RG13",
                           "RG14", "RG15", "RG16", "RG17", "RG14", "RG15", "RG16", "RG17", "RG14", "RG15",
                           "RG16", "RG17", "RG18", "RG19", "RG20", "RG21", "RG21", "RG22", "RG21", "RG21",
                           "RG22", "RG22", "RG23", "RG24", "RG25", "RG26"},
                          8, 0}},
                        0, -0x11, 0, 0, false, true};

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
    bool step(const ScenePhase& p) {
        const int old = countdown--;
        if (old > 0) return true;
        countdown = p.delay;
        if (++frame < int(p.frames.size())) return true;
        frame = 0;
        if (repetitions > 0) { --repetitions; return true; }
        return false; // no callback, no follow-up definition: freed
    }
};

constexpr int kFlightStep = 2, kFlightMargin = 0x28; // seg8:46EC, 450B

} // namespace

const EventScene* eventSceneForNotice(int text) {
    // The text is drawn after the animation (base + random(2), e.g. seg8:1DFD).
    if (text >= 5041 && text <= 5043) return &kStorm;
    if (text >= 5021 && text <= 5023) return &kRobbers;
    if ((text >= 5281 && text <= 5283) || (text >= 5301 && text <= 5303)) return &kVolcano; // Etna / Hekla
    if (text >= 5081 && text <= 5083) return &kRockfall;
    if (text >= 5201 && text <= 5203) return &kQuake; // 0x1451 + random(2) at seg8:4234
    if (text >= 5121 && text <= 5123) return &kRain;  // 0x1401 at seg8:4870
    if (text >= 5481 && text <= 5483) return &kSnow;
    if (text >= 5241 && text <= 5243) return &kFlood;
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

std::vector<SceneStep> eventSceneTimeline(const EventScene& scene, int corner, int viewWidth, int viewHeight) {
    std::vector<SceneStep> steps;
    for (size_t i = 0; i < scene.phases.size(); ++i) {
        const ScenePhase& phase = scene.phases[i];
        RecordState r{0, phase.repetitions, 0};
        if (i == 0 && scene.flight) {
            // seg8:517B: start = target + {(-c, -e), (c, -e), (c, e)}[corner] with
            // c = width / 2 + 40, e = height / 2 + 40; seg8:5243 then moves 2 pixels per
            // pass per axis (an axis within one step snaps) until both are within a step.
            const int c = viewWidth / 2 + kFlightMargin, e = viewHeight / 2 + kFlightMargin;
            static const int kSx[3] = {-1, 1, 1}, kSy[3] = {-1, -1, 1};
            const int k = corner >= 0 && corner < 3 ? corner : 0;
            ScenePoint at{kSx[k] * c, kSy[k] * e};
            for (int guard = 0; guard < 100000; ++guard) {
                r.step(phase); // repetitions 1000: the cloud keeps cycling while it flies
                steps.push_back({phase.frames[size_t(r.frame)], at});
                if (std::abs(at.x) <= kFlightStep && std::abs(at.y) <= kFlightStep) break;
                at.x = std::abs(at.x) > kFlightStep ? at.x + (at.x < 0 ? kFlightStep : -kFlightStep) : 0;
                at.y = std::abs(at.y) > kFlightStep ? at.y + (at.y < 0 ? kFlightStep : -kFlightStep) : 0;
            }
            continue;
        }
        while (r.step(phase)) steps.push_back({phase.frames[size_t(r.frame)], ScenePoint{}});
    }
    return steps;
}

} // namespace dtvr
