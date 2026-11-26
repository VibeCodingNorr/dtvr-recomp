// SPDX-License-Identifier: GPL-3.0-only
#include "finale_scenes.hpp"

#include <cctype>

namespace dtvr {
namespace {

constexpr char kLetters[8] = {'D', 'F', 'E', 'M', 'U', 'J', 'I', 'V'}; // seg2:2D43

char lowerLetter(int slot) { return char(std::tolower(static_cast<unsigned char>(kLetters[slot & 7]))); }

// The finish records (seg7:2C55 ff., DS:30 + 0x40 * i): position +0x14 / +0x16, start
// frame +0xC and the definitions (delay, frames) in the data segment.
const std::vector<SceneRecord>& finishDefinitions() {
    static const std::vector<SceneRecord> kRecords = {
        {209, 182, 0, 4, {"pauke00", "pauke01", "pauke02", "pauke03", "pauke00", "pauke01", "pauke02", "pauke03",
                          "pauke00", "pauke01", "pauke02", "pauke03"}}, // DS:4594
        {339, 221, 1, 4, {"duo00", "duo01", "duo02", "duo03", "duo04", "duo05", "duo06", "duo07", "duo08", "duo09",
                          "duo10", "duo11"}}, // DS:419C
        {130, 206, 0, 4, {"fl00", "fl01", "fl02", "fl02", "fl02", "fl05", "fl06", "fl07", "fl08", "fl09", "fl10",
                          "fl02"}}, // DS:427C
        {267, 209, 0, 4, {"tuba00", "tuba01", "tuba02", "tuba03", "tuba00", "tuba01", "tuba02", "tuba03", "tuba00",
                          "tuba01", "tuba02", "tuba02"}}, // DS:461C
        {25, 214, 2, 4, {"ge00", "ge01", "ge02", "ge03", "ge04", "ge05", "ge06", "ge07", "ge08", "ge08", "ge10",
                         "ge11", "ge12", "ge13", "ge14", "ge15", "ge11", "ge11", "ge11", "ge11"}}, // DS:4314
        {79, 182, 3, 4, {"ha00", "ha01", "ha02", "ha03", "ha04", "ha05", "ha06", "ha07", "ha08", "ha09", "ha10",
                         "ha11"}}, // DS:4420
        {405, 175, 4, 4, {"pa00", "pa01", "pa02", "pa03", "pa04", "pa05", "pa06", "pa07", "pa08", "pa09", "pa10",
                          "pa11"}}, // DS:4500
        {180, 189, 0, 4, {"jz00", "jz01", "jz02", "jz03", "jz04", "jz05", "jz06", "jz07", "jz08", "jz09", "jz00",
                          "jz11"}}, // DS:4490
        {460, 244, 3, 1, {"CAR1", "CAR2", "CAR3", "CAR2"}}, // DS:4760, the car
        {269, 304, 3, 4, {"hut00", "hut01", "hut02", "hut03", "hut04", "hut05", "hut06", "hut07", "hut08", "hut09",
                          "hut10", "hut11"}}, // DS:43B0
        {175, 280, 0, 3, {"bal00", "bal01", "bal02", "bal03", "bal04", "bal05", "bal06", "bal07", "bal08", "bal09",
                          "bal10", "bal11"}}, // DS:4120
        {78, 296, 2, 4, {"fr00", "fr01", "fr02", "fr02", "fr04", "fr05", "fr06", "fr02", "fr08", "fr09", "fr10",
                         "fr11"}}, // DS:420C
        {29, 294, 4, 4, {"wi00", "wi01", "wi02", "wi03", "wi04", "wi05", "wi06", "wi07", "wi08", "wi09", "wi10",
                         "wi11"}}, // DS:468C
        {347, 300, 3, 4, {"zp00", "zp01", "zp02", "zp03", "zp04", "zp05", "zp06", "zp07", "zp08", "zp09", "zp10",
                          "zp11"}}, // DS:46FC
        {0, 82, 2, 4, {"pfahl"}},    // DS:4736
        {117, 129, 0, 4, {"lampe"}}, // DS:4744
    };
    return kRecords;
}
// +0xD of the finish records (seg7:2CA4 .. 34FF): 0 = plain copy (seg2:913A).
constexpr bool kFinishKeyed[FinishScene::kRecords] = {true, true, false, true, false, false, false, true,
                                                      true, true, true,  true, true,  true,  true,  true};

// The victory records (seg7:7516 ff.); slots 7 and 8 stay empty, 9 is the winner.
const std::vector<SceneRecord>& victoryDefinitions() {
    static const std::vector<SceneRecord> kRecords = {
        {0, 147, 0, 4, {"ml00", "ml01", "ml02", "ml03", "ml04", "ml05", "ml06", "ml07", "ml08", "ml09", "ml10", "ml11",
                        "ml12", "ml13", "ml14", "ml15", "ml16"}}, // DS:557A
        {311, 144, 0, 3, {"mr00", "mr01", "mr02", "mr03", "mr04", "mr05", "mr06", "mr07", "mr08", "mr09", "mr10",
                          "mr11", "mr12", "mr13", "mr14", "mr15", "mr16"}}, // DS:5618
        {365, 200, 0, 4, {"kon00", "kon01", "kon02", "kon03", "kon04", "kon05", "kon06", "kon07", "kon08", "kon09",
                          "kon10", "kon11"}}, // DS:56A8
        {305, 195, 0, 4, {"hutman-1", "hutman-2"}}, // DS:5518
        {405, 260, 0, 1, {"CAR1"}},                 // DS:5828, the winner's car
        {8, 198, 0, 4, {"kap00", "kap01", "kap02", "kap03", "kap04", "kap05", "kap06", "kap07", "kap08", "kap09",
                        "kap10", "kap11"}}, // DS:5724
        {298, 289, 0, 4, {"zp00", "zp01", "zp02", "zp03", "zp04", "zp05", "zp06", "zp07", "zp07", "zp07", "zp07",
                          "zp07", "zp07", "zp07", "zp07", "zp07", "zp07", "zp07", "zp08", "zp09", "zp10",
                          "zp11"}}, // DS:57C6
        {}, {}, {}, // slots 7, 8 (empty); 9 is set from winner()
        {39, 239, 0, 3, {"rd014", "rd012", "rd011", "rd012", "rd014", "rd012", "rd011", "rd012", "rd011", "rd012",
                         "rd013", "rd012", "rd013", "rd012", "rd014", "rd012"}}, // DS:4800 (intro\redet.sbs)
    };
    return kRecords;
}
// Per slot: +0x8 countdown, +0xA repetitions, +0xD (seg7:7516..7B86).
struct VictorySetup { int countdown, repeats; bool keyed; };
constexpr VictorySetup kVictorySetup[VictoryScene::kRecords] = {
    {0, 999, false}, {1, 999, false}, {2, 999, false}, {2, 9999, true}, {2, 9999, true}, {0, 999, true},
    {1, 999, true},  {0, 0, false},   {0, 0, false},   {1, 999, true},  {0, 9999, true}};

} // namespace

void SceneTrack::step() {
    if (!def || def->frames.empty()) return;
    const int old = countdown--; // seg2:6E2B
    if (old > 0) return;
    countdown = def->delay; // seg2:6E52
    ++index;                // seg2:6E5F
    if (int(def->frames.size()) > index) return; // seg2:6E7C: count > index
    index = 0;
    if (repeats > 0) { --repeats; return; } // seg2:6EAD
    def = nullptr; // seg2:6F4D: no callback, no follow-up
}

PaintRecord SceneTrack::paint() const {
    PaintRecord r;
    r.active = def != nullptr;
    if (def && index >= 0 && size_t(index) < def->frames.size()) r.frame = def->frames[size_t(index)];
    r.x = x;
    r.y = y;
    r.keyed = keyed;
    return r;
}

std::vector<std::string> FinishScene::files(char driverLetter) {
    std::vector<std::string> f = {"pauke.sbs", "duo.sbs",    "floete.sbs", "tuba.sbs",   "genie.sbs", "happy.sbs",
                                  "paar.sbs",  "jazz.sbs",   "",           "hut.sbs",    "ballon.sbs", "frau.sbs",
                                  "winken.sbs", "zoepfe.sbs", "pfahl.sbs", "lampe.sbs"};
    f[kCar] = std::string(1, char(std::tolower(static_cast<unsigned char>(driverLetter)))) + "zielcar.sbs";
    return f;
}

const std::vector<SceneFill>& FinishScene::fills() {
    static const std::vector<SceneFill> kFills = {
        {0, 0xD1, 0xB6}, {1, 0x153, 0xDD}, {2, 0x82, 0xCE}, {4, 0x19, 0xD6}, {5, 0x4F, 0xB6}, {6, 0x195, 0xAF}};
    return kFills;
}

FinishScene::FinishScene() : tracks_(kRecords) {
    for (size_t i = 0; i < kRecords; ++i) {
        const SceneRecord& d = finishDefinitions()[i];
        SceneTrack& t = tracks_[i];
        t.def = &d;
        t.x = d.x;
        t.y = d.y;
        t.index = d.first;
        t.countdown = 0;  // +0x8 = 0 for every record
        t.repeats = 999;  // +0xA = 0x3E7
        t.keyed = kFinishKeyed[i];
    }
}

bool FinishScene::carGone() const { return tracks_[kCar].x < kCarGone; }

void FinishScene::pass() {
    if (passes_ < 0x7530) ++passes_; // seg7:3536
    tracks_[kCar].x -= 2;            // seg7:3564
    for (SceneTrack& t : tracks_) t.step();
}

std::vector<PaintRecord> FinishScene::records() const {
    std::vector<PaintRecord> r;
    r.reserve(tracks_.size());
    for (const SceneTrack& t : tracks_) r.push_back(t.paint());
    return r;
}

std::vector<std::string> VictoryScene::files(const std::vector<int>& ranking) {
    std::vector<std::string> f = {"sieg/many-l.sbs",  "sieg/many-r.sbs", "sieg/konfetti.sbs", "sieg/hutman.sbs",
                                  "sieg/kapelle.sbs", "sieg/zoepfe.sbs", "intro/redet.sbs"}; // DS:5E6A
    if (!ranking.empty()) f.push_back("ziel/" + std::string(1, lowerLetter(ranking[0])) + "zielcar.sbs"); // DS:5EA1
    for (size_t place = 0; place < ranking.size() && place < 3; ++place) // seg7:72EC / 7340 / 7369
        f.push_back("sieg/" + std::string(1, lowerLetter(ranking[place])) + "_winner.sbs");
    return f;
}

const std::vector<SceneFill>& VictoryScene::fills() {
    static const std::vector<SceneFill> kFills = {{0, 0, 0x93}, {1, 0x137, 0x90}, {2, 0x16D, 0xC8}};
    return kFills;
}

const SceneRecord& VictoryScene::winner(int slot) {
    // DS:5D6C / DS:5D8E[slot + 1]: x = 0xE4 - offset; y is set from the frame height.
    static const SceneRecord kWinners[8] = {
        {0xE4 - 27, 0, 0, 4, {"DW01", "DW02", "DW03", "DW04", "DW04", "DW04", "DW04", "DW04", "DW04", "DW03", "DW02",
                              "DW01", "DW01", "DW01", "DW01", "DW01"}},
        {0xE4 - 34, 0, 0, 4, {"FW01", "FW02", "FW03", "FW02", "FW01", "FW04", "FW05", "FW06", "FW05", "FW04"}},
        {0xE4 - 42, 0, 0, 4, {"EW01", "EW02", "EW03", "EW04", "EW05", "EW05", "EW05", "EW05", "EW04", "EW03", "EW02",
                              "EW01", "EW01", "EW01", "EW01"}},
        {0xE4 - 39, 0, 0, 4, {"MW01", "MW02", "MW03", "MW04", "MW05", "MW04", "MW03", "MW02", "MW01", "MW02", "MW03",
                              "MW04", "MW05", "MW10", "MW10", "MW11", "MW11", "MW11", "MW10", "MW04", "MW03",
                              "MW02"}},
        {0xE4 - 30, 0, 0, 4, {"UW01", "UW02", "UW03", "UW04", "UW05", "UW06", "UW07", "UW07", "UW07", "UW07", "UW07",
                              "UW06", "UW05", "UW04", "UW03", "UW02", "UW01", "UW01", "UW01", "UW01"}},
        {0xE4 - 20, 0, 0, 4, {"JW01", "JW02", "JW03", "JW04", "JW05", "JW06", "JW07", "JW07", "JW07", "JW07", "JW07",
                              "JW06", "JW05", "JW04", "JW03", "JW02", "JW01", "JW01", "JW01", "JW01"}},
        {0xE4 - 27, 0, 0, 4, {"IW01", "IW02", "IW03", "IW04", "IW05", "IW05", "IW05", "IW04", "IW03", "IW02", "IW01",
                              "IW01", "IW01", "IW01", "IW01"}},
        {0xE4 - 28, 0, 0, 4, {"VW01", "VW02", "VW03", "VW04", "VW05", "VW05", "VW05", "VW05", "VW04", "VW03", "VW02",
                              "VW01", "VW01", "VW01", "VW01"}},
    };
    return kWinners[slot & 7];
}

std::string VictoryScene::placeFrame(int slot) { return std::string(1, kLetters[slot & 7]) + "W00"; }

void VictoryScene::placePosition(size_t place, int width, int height, int& x, int& y) {
    static const int kCentre[2] = {0xB9, 0x10F}, kFoot[2] = {0xDC, 0xE3}; // seg7:7A48 / 7A19, 7AE9 / 7ABA
    const size_t i = place >= 2 ? 1 : 0;
    x = kCentre[i] - int(unsigned(width) >> 1); // shr ax, 1 / sub / neg
    y = kFoot[i] - height;
}

VictoryScene::VictoryScene(int winnerSlot, int winnerHeight) : tracks_(kRecords) {
    for (size_t i = 0; i < kRecords; ++i) {
        const SceneRecord& d = i == kWinner ? winner(winnerSlot) : victoryDefinitions()[i];
        if (d.frames.empty()) continue; // slots 7 and 8
        SceneTrack& t = tracks_[i];
        t.def = &d;
        t.x = d.x;
        t.y = i == kWinner ? 0xCA - winnerHeight : d.y; // seg7:79A0
        t.index = d.first;
        t.countdown = kVictorySetup[i].countdown;
        t.repeats = kVictorySetup[i].repeats;
        t.keyed = kVictorySetup[i].keyed;
    }
}

void VictoryScene::pass(bool talking, bool cue, bool playing) {
    if (passes_ < 0x7530) ++passes_; // seg7:7BCD
    SceneTrack& speaker = tracks_[kSpeaker];
    if (talking) { speaker.index = 0; speaker.countdown = 2; }           // seg7:7C0F / 7C19
    if (cue || !playing) { speaker.index = 1; speaker.countdown = 2; }   // seg7:7C3C / 7C46
    for (SceneTrack& t : tracks_) t.step(); // seg7:7C9F
}

std::vector<PaintRecord> VictoryScene::records() const {
    std::vector<PaintRecord> r;
    r.reserve(tracks_.size());
    for (const SceneTrack& t : tracks_) r.push_back(t.paint());
    return r;
}

} // namespace dtvr
