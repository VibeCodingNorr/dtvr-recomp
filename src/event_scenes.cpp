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
    return nullptr;
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
