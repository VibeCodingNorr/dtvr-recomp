// SPDX-License-Identifier: GPL-3.0-only
#include "event_scenes.hpp"

#include <algorithm>
#include <cstdlib>

namespace dtvr {
namespace {

// Definitions built on the handler's stack (u16 delay, u8 0, u8 count, names), the
// SBS set, record +0xA and the position added to the place's cell * 16.
// Sturm seg8:1C01: DS:5F0E.., sbs\sturm.sbs (DS:5F3A), repetitions 4, (8, -0x1D).
using Motion = ScenePhase::Motion;
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
// Dürre seg8:2386: sonne.sbs (DS:6239); the sun S01/S02 (delay 7, repetitions 1000)
// sinks from the view top - 0x14 by 2 per pass at x = town * 16 + 0x12 until it is
// within 0x32 of the town row (seg8:27A3), then three definitions play there.
const EventScene kDrought{"sonne.sbs",
                          {{{"S01", "S02"}, 7, 1000, Motion::Descend},
                           {{"S03", "S04", "S05", "S06", "S07", "S08", "S09", "S08", "S10", "S09", "S09", "S11",
                             "S05", "S04", "S03", "S01", "S02", "S01", "S12", "S13", "S14", "S15", "S16"},
                            6, 0},
                           {{"S17", "S18", "S19", "S20"}, 6, 3},
                           {{"S21", "S22", "S23", "S24", "S01", "S02", "S01", "S02", "S01", "S02"}, 6, 0}},
                          0x12, 0};
// Nessie seg8:2A32: nessie.sbs (DS:680E); first at Loch Ness (record at 0x4D5 / 0x248,
// centred on 0x4D0 / 0x240, seg6:0F60(500) before and (1000) after), then centred on
// the place + 1 cell and three definitions at the place * 16 + (0x12, 0x17)
// (seg8:336F ff.), seg6:0F60(1000) at the end.
const EventScene kNessie{
    "nessie.sbs",
    {{{"N01", "N02", "N03", "N04", "N05", "N06", "N06", "N06", "N06", "N06", "N06", "N06", "N06", "N06",
       "N06", "N07", "N08", "N09", "N09", "N09", "N09", "N09", "N09", "N10", "N11", "N12", "N12", "N12",
       "N12", "N12", "N12", "N12", "N12", "N12", "N12", "N11", "N10", "N09", "N09", "N09", "N09", "N09",
       "N09", "N08", "N07", "N06", "N06", "N06", "N06", "N13", "N14", "N15", "N16", "N17", "N18", "N19",
       "N20", "N21", "N21", "N22", "N23", "N23", "N23", "N23", "N23", "N23", "N24", "N25", "N26", "N27",
       "N27", "N27", "N27", "N27", "N27", "N27"},
      5, 0, Motion::None, 500, 1000, true, {0x4D5, 0x248}, true, true, {0x4D0, 0x240}},
     {{"NH01", "NH01", "NH01", "NH01", "NH01", "NH01", "NH01", "NH01", "NH01", "NH01", "NH02", "NH03", "NH04",
       "NH05", "NH06", "NH07", "NH02", "NH03", "NH04", "NH05", "NH06", "NH01"},
      5, 0, Motion::None, 0, 0, false, {}, true, false, {16, 16}},
     {{"NH08", "NH09", "NH10", "NH11", "NH12", "NH13", "NH14", "NH15", "NH14", "NH13", "NH14", "NH15", "NH14",
       "NH13", "NH14", "NH15", "NH14", "NH16", "NH17", "NH18", "NH19", "NH20", "NH21", "NH22", "NH23", "NH24",
       "NH25", "NH26", "NH27", "NH28", "NH29", "NH30", "NH31", "NH32", "NH33", "NH34", "NH35", "NH36", "NH37",
       "NH38", "NH39", "NH40", "NH23", "NH41", "NH42", "NH43"},
      5, 0},
     {{"NH44", "NH45", "NH46", "NH46", "NH46", "NH46", "NH46", "NH46", "NH45", "NH44", "NH43", "NH42", "NH42",
       "NH42", "NH42", "NH42", "NH42", "NH42", "NH42"},
      5, 0, Motion::None, 0, 1000}},
    0x12, 0x17, 0, 0, false, true};
// The lake cells of seg8:2A32 (stack tables bp-0x2B8 / -0x2D2, index DS:680C).
constexpr int kLakeX[13] = {18, 61, 82, 91, 78, 105, 112, 144, 146, 148, 179, 190, 133};
constexpr int kLakeY[13] = {13, 67, 85, 93, 135, 107, 98, 83, 65, 33, 82, 82, 130};
// Regen seg8:42B0: regen.sbs (DS:6C36); the cloud R00 (repetitions 1000) flies to
// the obstacle * 16 - (0, 0x11), then R01-R08 and R09-R20 (repetitions 1) there.
const EventScene kRain{"regen.sbs",
                       {{{"R00"}, 6, 1000, Motion::Flight},
                        {{"R01", "R02", "R03", "R04", "R05", "R06", "R07", "R08"}, 4, 0},
                        {{"R09", "R10", "R11", "R12", "R13", "R14", "R15", "R16", "R09", "R17", "R18",
                          "R19", "R20", "R14", "R15", "R16", "R09", "R17", "R18", "R19", "R20", "R14",
                          "R15", "R16", "R09", "R10", "R11", "R12", "R13", "R14", "R15", "R16"},
                         4, 1}},
                       0, -0x11};
// Schnee seg8:535E: schnee.sbs (DS:7017); SN01 flies to the obstacle * 16 + (5, -0x19),
// then SN01-SN30 and SN31-SN46 (repetitions 2).
const EventScene kSnow{"schnee.sbs",
                       {{{"SN01"}, 6, 1000, Motion::Flight},
                        {{"SN01", "SN02", "SN03", "SN04", "SN05", "SN06", "SN07", "SN08", "SN09", "SN10",
                          "SN11", "SN12", "SN13", "SN14", "SN15", "SN16", "SN17", "SN18", "SN19", "SN20",
                          "SN21", "SN22", "SN23", "SN24", "SN25", "SN26", "SN27", "SN28", "SN29", "SN30"},
                         4, 0},
                        {{"SN31", "SN32", "SN33", "SN34", "SN35", "SN36", "SN37", "SN38", "SN39", "SN40",
                          "SN41", "SN42", "SN43", "SN44", "SN45", "SN46"},
                         4, 2}},
                       5, -0x19};
// Hochwasser seg8:4964: flood.sbs (DS:6E1D); RG00 flies to the town * 16 - (0, 0x11),
// then RG01-RG26 (46 frames, delay 8).
const EventScene kFlood{"flood.sbs",
                        {{{"RG00"}, 6, 1000, Motion::Flight},
                         {{"RG01", "RG02", "RG03", "RG03", "RG02", "RG04", "RG05", "RG05", "RG06", "RG07",
                           "RG08", "RG08", "RG09", "RG07", "RG06", "RG10", "RG11", "RG06", "RG12", "RG13",
                           "RG14", "RG15", "RG16", "RG17", "RG14", "RG15", "RG16", "RG17", "RG14", "RG15",
                           "RG16", "RG17", "RG18", "RG19", "RG20", "RG21", "RG21", "RG22", "RG21", "RG21",
                           "RG22", "RG22", "RG23", "RG24", "RG25", "RG26"},
                          8, 0}},
                        0, -0x11};

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
    if (text >= 5141 && text <= 5143) return &kDrought;
    if (text >= 5101 && text <= 5103) return &kNessie;
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

std::vector<SceneStep> eventSceneTimeline(const EventScene& scene, int cellX, int cellY, int variant, int viewWidth,
                                          int viewHeight, int viewTop) {
    std::vector<SceneStep> steps;
    if (scene.nessieLake && variant >= 0 && variant < 13) { // seg8:336F: the lake, not the town
        cellX = kLakeX[variant];
        cellY = kLakeY[variant];
    }
    const int corner = variant;
    const std::vector<ScenePoint> places = eventScenePositions(scene, cellX, cellY);
    auto wait = [&](int ms) {
        for (int i = 0; i < ms / 20; ++i) steps.push_back(SceneStep{});
    };
    auto shifted = [&](ScenePoint d) {
        std::vector<ScenePoint> at = places;
        for (ScenePoint& p : at) p = {p.x + d.x, p.y + d.y};
        return at;
    };
    wait(scene.waitBefore);
    ScenePoint rest{}; // offset the later phases keep (the drought's sinking height)
    for (const ScenePhase& phase : scene.phases) {
        wait(phase.waitBefore);
        const size_t first = steps.size();
        RecordState r{0, phase.repetitions, 0};
        auto at = [&](ScenePoint d) { return phase.fixed ? std::vector<ScenePoint>{phase.at} : shifted(d); };
        if (phase.motion == Motion::Flight) {
            // seg8:517B: start = target + {(-c, -e), (c, -e), (c, e)}[corner] with
            // c = width / 2 + 40, e = height / 2 + 40; seg8:5243 then moves 2 pixels per
            // pass per axis (an axis within one step snaps) until both are within a step.
            const int c = viewWidth / 2 + kFlightMargin, e = viewHeight / 2 + kFlightMargin;
            static const int kSx[3] = {-1, 1, 1}, kSy[3] = {-1, -1, 1};
            const int k = corner >= 0 && corner < 3 ? corner : 0;
            ScenePoint d{kSx[k] * c, kSy[k] * e};
            for (int guard = 0; guard < 100000; ++guard) {
                r.step(phase); // repetitions 1000: the cloud keeps cycling while it flies
                steps.push_back({phase.frames[size_t(r.frame)], at(d)});
                if (std::abs(d.x) <= kFlightStep && std::abs(d.y) <= kFlightStep) break;
                d.x = std::abs(d.x) > kFlightStep ? d.x + (d.x < 0 ? kFlightStep : -kFlightStep) : 0;
                d.y = std::abs(d.y) > kFlightStep ? d.y + (d.y < 0 ? kFlightStep : -kFlightStep) : 0;
            }
        } else if (phase.motion == Motion::Descend && !places.empty()) {
            // seg8:25CD: y starts at the view top - 0x14; after each pass y += 2 until
            // |town row * 16 - y| <= 0x32 (seg8:27A3 ff.).
            ScenePoint d{0, viewTop - 0x14 - places[0].y};
            for (int guard = 0; guard < 100000; ++guard) {
                r.step(phase);
                steps.push_back({phase.frames[size_t(r.frame)], at(d)});
                d.y += 2;
                if (std::abs(places[0].y + d.y - cellY * 16) <= 0x32) break;
            }
            rest = d;
        } else {
            while (r.step(phase)) steps.push_back({phase.frames[size_t(r.frame)], at(rest)});
        }
        if (phase.recentre && first < steps.size() + 1) {
            const ScenePoint camera = phase.cameraFixed ? phase.camera
                                                        : ScenePoint{cellX * 16 + phase.camera.x, cellY * 16 + phase.camera.y};
            const size_t mark = first - std::min(first, size_t(phase.waitBefore / 20)); // before the phase's wait
            if (mark < steps.size()) { steps[mark].recentre = true; steps[mark].camera = camera; }
        }
        wait(phase.waitAfter);
    }
    wait(scene.waitAfter);
    return steps;
}

} // namespace dtvr
