// SPDX-License-Identifier: GPL-3.0-only
#include "intro_script.hpp"

#include "rules.hpp"

#include <string>

namespace dtvr {
namespace {

struct Definition {
    uint16_t offset;
    int delay;
    std::vector<std::string> frames;
};

std::vector<std::string> numbered(const char* prefix, int first, int last) {
    std::vector<std::string> v;
    for (int i = first; i <= last; ++i) {
        std::string n = std::to_string(i);
        if (n.size() < 2) n = "0" + n;
        v.push_back(prefix + n);
    }
    return v;
}

std::vector<std::string> join(std::vector<std::string> a, const std::vector<std::string>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

std::vector<std::string> repeat(const char* name, int n) { return std::vector<std::string>(size_t(n), name); }

// The animation definitions of the intro in the data segment (u16 delay, u8 0,
// u8 count, far pointers to the names); offsets are the DS addresses.
const std::vector<Definition>& definitions() {
    static const std::vector<Definition> d = {
        {0x05B2, 999, {"zo00"}},
        {0x0560, 4, numbered("zo", 0, 17)},
        {0x0614, 4, join(repeat("tu00", 9), numbered("tu", 9, 17))},
        {0x0666, 999, {"tu17"}},
        {0x068C, 4, numbered("tu", 18, 23)},
        {0x06AE, 999, {"tu23"}},
        {0x06D0, 4, {"tu24", "tu25", "tu26", "tu27", "tu00"}},
        {0x06EE, 999, {"tu00"}},
        {0x0720, 2, numbered("pic", 23, 29)},
        {0x0746, 99, {"pic01"}},
        {0x075A, 4, {"kop00", "kop01"}},
        {0x07B4, 4, {"kop02", "kop02", "kop04", "kop03", "kop04", "kop04", "kop02", "kop04", "kop02", "kop03", "kop04",
                     "kop02", "kop04"}},
        {0x07FE, 4, {"kop07", "kop00", "kop99"}},
        {0x0820, 2, {"lampe022", "lampe023"}},
        {0x0836, 99, {"lampe000"}},
        {0x08C8, 4, join(numbered("lt1", 0, 21), {"lt100"})},
        {0x09D6, 10, join(join(repeat("lt203", 10), repeat("lt205", 2)), join(repeat("lt203", 15), {"lt212", "lt212"}))},
        {0x0B1A, 4, join(repeat("lt203", 27), {"lt211", "lt212", "lt213", "lt212", "lt213", "lt212", "lt211"})},
        {0x0C18, 4, {"lt200", "lt201", "lt202", "lt203", "lt203", "lt203", "lt203", "lt207", "lt208", "lt208", "lt208",
                     "lt211", "lt212", "lt213", "lt212", "lt215", "lt208", "lt217", "lt217"}},
        {0x0CDC, 6, join(numbered("p1", 0, 21), {"p100"})},
        {0x0D4C, 6, {"p203", "p204", "p205"}},
        {0x0DBC, 4, {"p200", "p201", "p202", "p203", "p204", "p205", "p206", "p207", "p208", "p209", "p210", "p211",
                     "p212", "p213", "p214", "p215", "p210", "p217", "p217"}},
        {0x0EDE, 2, {"rd013", "rd014", "rd012", "rd014", "rd014", "rd012", "rd014", "rd013", "rd014", "rd012", "rd014",
                     "rd014", "rd012", "rd014", "rd000", "rd001", "rd002", "rd003", "rd004", "rd004", "rd004", "rd004",
                     "rd004", "rd005", "rd006", "rd007", "rd008", "rd008", "rd009", "rd010", "rd012", "rd011", "rd012",
                     "rd013", "rd014"}},
        {0x0FF2, 2, {"rd101", "rd100", "rd101", "rd101", "rd100", "rd100", "rd100", "rd102", "rd103", "rd102", "rd101",
                     "rd100", "rd101", "rd100", "rd100", "rd101", "rd100", "rd101", "rd100", "rd100", "rd101", "rd101"}},
        {0x107C, 2, {"sm_rlyy1", "sm_rlyy2", "sm_rlyy3", "sm_rlyy4", "sm_rlyy5"}},
        {0x10A6, 2, {"sm_rlyy5", "sm_rlyy6"}},
        {0x10E0, 2, {"sm_rlyy5", "sm_rlyy4", "sm_rlyy3", "sm_rlyy2", "sm_rlyy1"}},
        {0x110A, 2, {"sm_rlyy1", "sm_rlyy1"}},
        {0x112E, 999, {"red_foot"}},
        {0x113C, 999, {"wall1"}},
        {0x114A, 999, {"wall2"}},
        {0x1164, 3, {"fate_hut", "fate_hu2"}},
        {0x117E, 3, {"fate1", "fate1b"}},
        {0x1190, 3, {"fate2"}},
    };
    return d;
}

const Definition* findDefinition(uint16_t def) {
    for (const auto& d : definitions())
        if (d.offset == def) return &d;
    return nullptr;
}

} // namespace

std::vector<std::string> introDefinitionFrames(uint16_t def) {
    const Definition* d = findDefinition(def);
    return d ? d->frames : std::vector<std::string>{};
}

IntroScript::IntroScript(const std::vector<VoiceSyncRecord>* sync) : sync_(sync) {}

void IntroScript::set(int i, uint16_t def, int x, int y, int reps, bool keyed, int countdown) {
    Record& r = records_[size_t(i)];
    r = Record{};
    r.def = def;
    r.keyed = keyed;
    r.x = x;
    r.y = y;
    r.reps = reps;
    r.countdown = countdown;
}

std::vector<IntroEvent> IntroScript::start() {
    // seg7:0078 .. 016F: the transparent pixels of every frame of these files take
    // club.bmp's pixels at the given place (seg7:1E3C); club.bmp stays unchanged.
    std::vector<IntroEvent> events = {{IntroEvent::FillFile, "paar2", 0x165, 0x9E},
                                      {IntroEvent::FillFile, "bild", 0x19D, 0x3A},
                                      {IntroEvent::FillFile, "leute1", 0x81, 0xB0},
                                      {IntroEvent::FillFile, "paar1", 0x165, 0x9E}};
    // seg7:0177 .. 0447: records 3 to 7; the filled ones are copied without a key
    // (+0xD = 0: seg7:0260, 02F0, 0380).
    set(3, 0x06EE, 0x22, 0x70, 9999, true);
    set(4, 0x08C8, 0x81, 0xB0, 0x2706, false, 1);
    set(5, 0x0CDC, 0x165, 0x9E, 9999, false, 2);
    set(6, 0x0746, 0x19D, 0x3A, 9999, false);
    set(7, 0x0836, 0x172, 0, 9999, true);
    return events;
}

void IntroScript::play(const char* name, uint32_t nowMs, std::vector<IntroEvent>& events) {
    events.push_back({IntroEvent::Voice, name, 0, 0});
    voice_ = sync_ ? findVoiceSync(*sync_, std::string(name) + ".wav") : nullptr;
    voiceStart_ = nowMs;
}

bool IntroScript::playing(uint32_t nowMs) const { return voice_ && voiceSyncPlaying(*voice_, nowMs - voiceStart_); }
bool IntroScript::talking(uint32_t nowMs) const { return voice_ && voiceSyncTalking(*voice_, nowMs - voiceStart_); }
bool IntroScript::cue(uint32_t nowMs) const { return voice_ && voiceSyncCue(*voice_, nowMs - voiceStart_); }

void IntroScript::moveHat() {
    Record& hat = records_[0];
    if (state_ >= 7 && hat.def == 0x1164 && (passes_ & 1) == 0) { // seg7:13D5
        if ((hat.x & 1) == 0) {
            hat.x += 2; // to the right while even
        } else {
            hat.x -= 2;
            if (hat.x < 0x127) --hat.y;
            if (hat.x < 0x122) --hat.y;
            if (hat.x < 0x11C) hat.y += 3;
            if (hat.x < 0x118) hat.y += 2;
            if (hat.x < 0x113) { // seg7:1493: the wall piece and the falling Fate
                set(1, 0x114A, 0x110, 0, 9999, true);
                set(0, 0x117E, 0x111, -4, 9999, true);
            }
        }
        if (records_[0].x == 0x158) records_[0].x = 0x157; // seg7:15A8: turn back
    }
    Record& fate = records_[0];
    if (state_ >= 7 && fate.def == 0x117E) { // seg7:15BC
        if (fate.y < 0 && (passes_ & 0x1F) == 0) ++fate.y;
        if (fate.y < 0x2C && fate.y >= 0) ++fate.y;
    }
}

void IntroScript::logic(uint32_t nowMs, std::vector<IntroEvent>& events) {
    auto next = [this](int state) { state_ = state; passes_ = 0; };
    Record* r = records_.data();
    if (passes_ == 250 && state_ == 0) { // seg7:0561: the door opens
        next(1);
        set(3, 0x0614, 0x22, 0x70, 0, true);
    }
    if (passes_ == 90 && state_ == 1) { // seg7:060C
        next(2);
        set(3, 0x0666, 0x22, 0x70, 9999, true);
        set(2, 0x075A, 0x6C, 0x84, 0, true);
    }
    if (passes_ == 10 && state_ == 2) { // seg7:0746: the head speaks
        next(3);
        set(2, 0x07B4, 0x6C, 0x84, 999, true);
        play("rally01b", nowMs, events);
    }
    if (state_ == 3 && talking(nowMs)) { r[2].frame = 0; r[2].countdown = 2; } // seg7:0822
    if (!playing(nowMs) && state_ == 3) {                                         // seg7:084B
        next(4);
        set(2, 0x07FE, 0x6C, 0x84, 0, true);
    }
    if (passes_ >= 15 && state_ == 4) { // seg7:08F9: the zoom starts, the door closes
        next(5);
        set(2, 0x05B2, 0x41, 0x69, 999, true);
        set(3, 0x068C, 0x22, 0x70, 0, true);
        play("tuerauf1", nowMs, events);
    }
    if (passes_ == 12 && state_ == 5) { // seg7:0A65
        next(6);
        set(3, 0x0560, 0x41, 0x69, 0, true);
        set(2, 0x06AE, 0x22, 0x70, 999, true);
        set(4, 0x09D6, 0x81, 0xB0, 999, true);
        events.push_back({IntroEvent::CloseFile, "leute1", 0, 0}); // seg7:0C2F (DS:9D64)
        set(5, 0x0D4C, 0x165, 0x9E, 999, false, 2);
        events.push_back({IntroEvent::CloseFile, "paar1", 0, 0}); // seg7:0CE2 (DS:9D68)
    }
    if (passes_ >= 0 && passes_ < 0x54 && state_ == 6) r[3].y = passes_ * 50 / 0x54 + 0x69; // seg7:0D05
    if (passes_ >= 0 && passes_ < 0x28 && state_ == 6) r[3].x = 0x41 - passes_;             // seg7:0D33
    if (passes_ == 15 && state_ == 6) set(2, 0x06D0, 0x22, 0x70, 0, true);                         // seg7:0D5A
    if (state_ == 6 && !playing(nowMs) && !doorClosed_) {                                    // seg7:0DFA
        doorClosed_ = true;
        play("tuerzu", nowMs, events);
    }
    if (passes_ == 0x20 && state_ == 6) { // seg7:0E4E: the picture falls, the lamp swings
        set(6, 0x0720, 0x19D, 0x3A, 0, false);
        set(7, 0x0820, 0x172, 0, 0, true);
    }
    if (passes_ == 0x23 && state_ == 6) set(2, 0x06EE, 0x22, 0x70, 999, true); // seg7:0F7E
    if (state_ == 6 && !playing(nowMs)) {                                 // seg7:101E: the speaker
        next(7);
        events.push_back({IntroEvent::StampFrame, "tu00", 0x22, 0x70});
        events.push_back({IntroEvent::CloseFile, "tuer", 0, 0}); // seg7:1076 (DS:9D50)
        set(2, 0x09D6, 0x81, 0xB0, 999, true);
        set(4, 0x0EDE, 0x11, 0x9B, 999, true);
        set(12, 0x112E, 0x0A, 0x112, 999, true);
        events.push_back({IntroEvent::CloseFile, "zoom", 0, 0}); // seg7:1249 (DS:9D44)
        play("rally02", nowMs, events);
    }
    if (state_ == 10 && r[1].def != 0x113C && r[1].def != 0x114A) { // seg7:129E: wall and hat
        set(1, 0x113C, 0xC9, 0x9D, 9999, true);
        set(0, 0x1164, 0xD2, 0xB3, 9999, true);
    }
    moveHat();
    if (state_ == 7 && !playing(nowMs)) { // seg7:161B
        state_ = 8;
        r[2].def = 0x0B1A; // frame index kept
        r[2].reps = 0;
        r[2].countdown = 0;
        r[2].follow = 0x09D6;
        play("rally04", nowMs, events);
    }
    if (state_ >= 8 && r[2].def == 0x09D6) r[2].reps = 999; // seg7:16B1
    if (state_ == 8 && !playing(nowMs)) {                   // seg7:16DF
        state_ = 9;
        r[4].def = 0x0FF2;
        r[4].frame = 0;
        play("rally05", nowMs, events);
    }
    if (state_ == 9 && !playing(nowMs)) { // seg7:1744
        state_ = 10;
        r[4].def = 0x0EDE;
        r[4].frame = 0;
        play("rally07a", nowMs, events);
    }
    if (state_ == 9 && talking(nowMs)) { r[4].frame = 2; r[4].countdown = 2; } // seg7:17A9
    if (state_ == 10) {                                                        // seg7:17D2
        if (cue(nowMs) && r[4].def != 0x107C && r[4].def != 0x10A6) {
            r[4].reps = 0;
            --r[4].x;
            r[4].y -= 12;
            r[4].def = 0x107C;
            r[4].frame = 0;
            r[4].follow = 0x10A6;
        }
        if (r[4].def == 0x10A6) r[4].reps = 999;
    }
    if (state_ == 10 && !playing(nowMs)) { // seg7:1884: everyone cheers
        next(11);
        r[4].reps = 0;
        r[4].def = 0x10E0;
        r[4].frame = 0;
        r[4].follow = 0x110A;
        set(2, 0x0C18, 0x81, 0xB0, 0, true);
        set(5, 0x0DBC, 0x165, 0x9E, 0, false, 2);
        r[0].def = 0x1190;
        r[0].frame = 0;
    }
    if (passes_ == 0x69 && state_ == 11) { // seg7:1A0D: lightning, the club photo
        play("blitz01", nowMs, events); // after the subtitle window closed (seg7:1A21)
        events.push_back({IntroEvent::Flash, "clubfoto", 0, 0});
        for (auto& rec : records_) rec.def = 0;
    }
    if (passes_ == 0x131 && state_ == 11) events.push_back({IntroEvent::Wipe, "zeitung", 0, 0}); // seg7:1B2B
}

void IntroScript::step() {
    // seg2:6DCB for every record in index order.
    for (auto& r : records_) {
        if (r.def == 0) continue;
        const Definition* d = findDefinition(r.def);
        if (!d) { r.def = 0; continue; }
        const int old = r.countdown--;
        if (old > 0) continue;
        r.countdown = d->delay;
        if (++r.frame < int(d->frames.size())) continue;
        r.frame = 0;
        if (r.reps > 0) { --r.reps; continue; }
        r.def = r.follow; // the follow-up replaces it and is cleared, or the record is freed
        r.follow = 0;
    }
}

std::vector<PaintRecord> IntroScript::records() const {
    std::vector<PaintRecord> out(records_.size());
    for (size_t i = 0; i < records_.size(); ++i) {
        const Record& r = records_[i];
        const Definition* d = r.def ? findDefinition(r.def) : nullptr;
        if (!d || r.frame < 0 || r.frame >= int(d->frames.size())) continue;
        out[i] = PaintRecord{true, d->frames[size_t(r.frame)], r.x, r.y, r.keyed};
    }
    return out;
}

IntroPass IntroScript::pass(uint32_t nowMs) {
    IntroPass out;
    if (finished_) return out;
    if (passes_ < 30000) ++passes_; // seg7:04D6
    logic(nowMs, out.events);
    out.draw = state_ < 11 || passes_ < 0x69; // seg7:1BCE
    if (out.draw) { // seg7:1C18 restore (74E4), 1C28 step (6DCB), 1C46 show (6F69)
        step();
        out.records = records();
    }
    if (passes_ == 0x22B && state_ == 11) { // seg7:1C8A
        finished_ = true;
        out.events.push_back({IntroEvent::End, "", 0, 0});
    }
    return out;
}

std::vector<WipeCell> wipeMode4(int width, int height, MsRandom& rand) {
    std::vector<WipeCell> cells;
    bool used[20][20] = {};
    for (int i = 0; i < 400; ++i) { // seg8:1357
        int x = 0, y = 0;
        for (;;) {
            x = rand.rand() % 20;
            y = rand.rand() % 20;
            if (!used[y][x] && y <= i / 20 + 2) break; // seg8:138E / 13A3
        }
        used[y][x] = true;
        cells.push_back({x * width / 20, y * height / 20, (x + 1) * width / 20, (y + 1) * width / 20});
    }
    return cells;
}

} // namespace dtvr
