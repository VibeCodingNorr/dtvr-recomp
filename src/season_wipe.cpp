// SPDX-License-Identifier: GPL-3.0-only
#include "season_wipe.hpp"

#include <algorithm>

namespace dtvr {

std::vector<BlindStrip> blindStrips(int height) {
    std::vector<BlindStrip> strips;
    strips.reserve(kBlindStrips);
    for (int i = 0; i < kBlindPasses; ++i)      // [bp-0x14] < 0x28
        for (int j = 0; j < 20; ++j) {          // [bp-0x18] < 0x14
            if (i - j < 0 || i - j > 19) continue; // seg8:0E98 / 0EA5
            const long top = (long(j) * 19 + i) * long(height) / 400;
            strips.push_back(BlindStrip{int(top), int(top) + 2});
        }
    return strips;
}

int blindStripsAfter(int passes) {
    int count = 0;
    for (int i = 0; i < std::min(passes, kBlindPasses); ++i) count += std::min(i, 19) - std::max(0, i - 19) + 1;
    return count;
}

SeasonWipe::Stage SeasonWipe::stage(std::uint32_t now) const {
    if (!running_) return Stage::Idle;
    const std::uint32_t t = now - start_;
    if (t < kStageMs) return Stage::Out;
    if (t < 2 * kStageMs) return Stage::In;
    return Stage::Idle;
}

int SeasonWipe::stripsShown(std::uint32_t now) const {
    const Stage s = stage(now);
    if (s == Stage::Idle) return 0;
    const std::uint32_t t = (now - start_) - (s == Stage::In ? kStageMs : 0);
    return blindStripsAfter(int(t / kPassMs) + 1);
}

std::vector<BlindRun> SeasonWipe::blackRows(int height, std::uint32_t now) const {
    const Stage s = stage(now);
    if (s == Stage::Idle || height <= 0) return {};
    std::vector<bool> drawn(size_t(height), false);
    const std::vector<BlindStrip> strips = blindStrips(height);
    const int shown = std::min(stripsShown(now), int(strips.size()));
    for (int k = 0; k < shown; ++k)
        for (int y = std::max(0, strips[size_t(k)].top); y < std::min(height, strips[size_t(k)].bottom); ++y)
            drawn[size_t(y)] = true;
    // Out: the drawn strips show the black bitmap; In: the rows not drawn yet still do.
    const bool blackWhenDrawn = s == Stage::Out;
    std::vector<BlindRun> runs;
    for (int y = 0; y < height;) {
        if (drawn[size_t(y)] != blackWhenDrawn) { ++y; continue; }
        const int top = y;
        while (y < height && drawn[size_t(y)] == blackWhenDrawn) ++y;
        runs.push_back(BlindRun{top, y - top});
    }
    return runs;
}

} // namespace dtvr
