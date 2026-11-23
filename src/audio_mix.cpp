// SPDX-License-Identifier: GPL-3.0-only
#include "audio_mix.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace dtvr {

int16_t softLimit(int sum) {
    constexpr double kRange = 32767.0 - kMixKnee;
    const int magnitude = std::abs(sum);
    if (magnitude <= kMixKnee) return int16_t(sum);
    const double bent = kMixKnee + kRange * std::tanh((magnitude - kMixKnee) / kRange);
    return int16_t(sum < 0 ? -bent : bent);
}

int16_t mixSample(int music, int effects, int voice) {
    return softLimit(music + std::clamp(effects, -32768, 32767) + voice);
}

} // namespace dtvr
