// SPDX-License-Identifier: GPL-3.0-only
#include "slot_records.hpp"

#include <algorithm>

namespace dtvr {
namespace {

// seg2:7674: reel frame positions (x, y) in the housing by number of reels.
constexpr int kReelX[5][5] = {{150}, {108, 240}, {122, 198, 274}, {166, 200, 252, 286}, {174, 222, 270, 246, 196}};
constexpr int kReelY[5][5] = {{146}, {204, 204}, {178, 162, 178}, {212, 160, 160, 212}, {154, 118, 154, 204, 204}};
// seg2:7C1E: tens and units of the sum display.
constexpr int kCountX[5][2] = {{144, 166}, {168, 190}, {192, 214}, {220, 242}, {216, 238}};
constexpr int kCountY[5] = {210, 204, 208, 212, 164};
constexpr int kLever = 0, kFirstReel = 3, kFirstGear = 8;

std::vector<std::string> numbered(const char* stem, std::initializer_list<int> numbers) {
    std::vector<std::string> out;
    for (int n : numbers) out.push_back(stem + std::to_string(n));
    return out;
}

} // namespace

SlotRecords::SlotRecords(const std::vector<int>& reels) : values_(reels) {
    if (values_.size() > 5) values_.resize(5);
    // Definitions (delay, frames) from the data segment.
    leverPull_ = {2, numbered("slthad0", {1, 2, 3, 4, 5, 6, 7, 8, 9})};                       // DS:2292
    leverBack_ = {2, {"slthad10", "slthad11", "slthad12", "slthad12", "slthad12", "slthad12", "slthad12",
                      "slthad13", "slthad14", "slthad15", "slthad16", "slthad17", "slthad18", "slthad19"}}; // DS:2338
    spin_ = {2, numbered("drehung", {1, 2, 3, 4})};                                            // DS:2398
    for (int v = 0; v < 7; ++v)                                                                // DS:2E3C[v] (DS:23BE ...)
        faces_[size_t(v)] = {4, {"walze_" + std::to_string(v) + "h", "walze_" + std::to_string(v)}};
    gearL_ = {3, numbered("slt5anl", {3, 2, 1})};                                              // DS:20B2
    gearR_ = {3, numbered("slt5anr", {1, 2, 3, 4, 5, 6})};                                     // DS:20F8
    gearU_ = {3, numbered("slt5anu", {1, 2, 3, 4, 5, 6})};                                     // DS:214A
    gear4_ = {3, numbered("slt4anm", {1, 2, 3})};                                              // DS:2182
    gear3_ = {3, numbered("slt3anm", {1, 2, 3, 4, 5, 6, 7, 8, 8, 4, 3, 2, 1})};               // DS:2208
}

void SlotRecords::pullLever() { // seg2:6A09
    if (state_ != 1) return;
    Record& r = records_[kLever];
    r = Record{};
    r.def = &leverPull_;
    r.follow = &leverBack_;
    r.callback = Callback::ReelsStart; // seg2:7671
    r.x = 0x16;
    r.y = 0x70;
    state_ = 2;
}

bool SlotRecords::stopReels() { // seg2:6AA2
    if (state_ != 2 || records_[kLever].def == &leverPull_) return false;
    for (int i = 0; i < 5; ++i) records_[size_t(kFirstReel + i)].repetitions = 0; // +0xCA, +0x10A, ...
    state_ = 3;
    return true;
}

void SlotRecords::startReels() { // seg2:7671
    started_ = true;
    const int n = int(values_.size());
    for (int i = 0; i < n; ++i) {
        Record& r = records_[size_t(kFirstReel + i)];
        r = Record{};
        r.def = &spin_;
        r.follow = &faces_[size_t(std::clamp(values_[size_t(i)], 0, 6))];
        r.repetitions = (i + 3) * 2; // seg2:77E1
        r.callback = Callback::ReelStop; // seg2:7C0A(i)
        r.argument = i;
        r.x = kReelX[n - 1][i];
        r.y = kReelY[n - 1][i];
    }
    // seg2:78B9: the gears, set to the delay written into their definitions; 100
    // repetitions for four and five reels, one run for three.
    auto gear = [&](int index, Definition& def, int delay, int repetitions, int x, int y) {
        def.delay = delay;
        Record& r = records_[size_t(index)];
        r = Record{};
        r.def = &def;
        r.repetitions = repetitions;
        r.x = x;
        r.y = y;
    };
    if (n == 5) {
        gear(kFirstGear, gearL_, 4, 100, 0x7A, 0x52);
        gear(kFirstGear + 1, gearR_, 4, 100, 0x100, 0x38);
        gear(kFirstGear + 2, gearU_, 4, 100, 0x62, 0xC0);
    } else if (n == 4) {
        gear(kFirstGear, gear4_, 4, 100, 0x8E, 0x6A);
    } else if (n == 3) {
        gear(kFirstGear, gear3_, 5, 0, 0x62, 0x82);
    }
}

void SlotRecords::reelStopped(int i) { // seg2:7C0A
    const int n = int(values_.size());
    sum_ += values_[size_t(i)];
    ++stoppedReels_;
    if (n == 5) { ++gearL_.delay; ++gearR_.delay; ++gearU_.delay; } // seg2:7E3A: the gears slow down
    else if (n == 4) ++gear4_.delay;                                // seg2:7E52
    if (n - i - 1 == 0) { // seg2:7E56: the last reel ends the gears; state 4
        for (int g = kFirstGear; g < kFirstGear + 3; ++g) records_[size_t(g)].def = nullptr;
        state_ = 4;
    }
}

void SlotRecords::advance(int index) { // seg2:6DCB for one record
    Record& r = records_[size_t(index)];
    if (!r.def) return;
    const int old = r.countdown--;
    if (old > 0) return;
    r.countdown = r.def->delay;
    if (++r.frame < int(r.def->frames.size())) return;
    r.frame = 0;
    if (r.repetitions > 0) { --r.repetitions; return; }
    const Callback callback = r.callback;
    const int argument = r.argument;
    if (r.follow) { // seg2:6EE3: the follow-up replaces the definition, the callback ends
        r.def = r.follow;
        r.follow = nullptr;
        r.callback = Callback::None;
    } else {
        r.def = nullptr;
    }
    // The callback runs before the follow-up switch in rally.exe; it only touches other
    // records and the window's state, so the order here is equivalent.
    if (callback == Callback::ReelsStart) startReels();
    else if (callback == Callback::ReelStop) reelStopped(argument);
}

void SlotRecords::pass() {
    for (size_t i = 0; i < records_.size(); ++i) {
        advance(int(i));
        if (records_[i].def) records_[i].last = records_[i].def->frames[size_t(records_[i].frame)];
    }
}

std::vector<SlotFrame> SlotRecords::frames() const {
    std::vector<SlotFrame> out;
    const int n = int(values_.size());
    for (size_t i = 0; i < records_.size(); ++i) {
        const Record& r = records_[i];
        const bool gear = int(i) >= kFirstGear;
        if (r.last.empty() || (gear && !r.def)) continue; // ended gears vanish (seg2:74E4)
        out.push_back({r.last, r.x, r.y});
    }
    if (stoppedReels_ > 0 && n > 0) { // the sum display, record 2 (seg2:7C0A)
        out.push_back({"count_" + std::to_string(sum_ / 10 % 10), kCountX[n - 1][0], kCountY[n - 1]});
        out.push_back({"count_" + std::to_string(sum_ % 10), kCountX[n - 1][1], kCountY[n - 1]});
    }
    return out;
}

} // namespace dtvr
