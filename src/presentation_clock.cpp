// SPDX-License-Identifier: GPL-3.0-only
#include "presentation_clock.hpp"

namespace dtvr {

void PresentationClock::update(uint32_t realMs) {
    if (started_ && !paused_) now_ += (realMs - lastReal_) * uint32_t(rate_); // wraps like SDL_GetTicks
    started_ = true;
    lastReal_ = realMs;
}

void PresentationClock::setRate(int rate, uint32_t realMs) {
    update(realMs);
    rate_ = validRate(rate) ? rate : 1;
}

void PresentationClock::setPaused(bool paused, uint32_t realMs) {
    if (paused == paused_) return;
    update(realMs);
    paused_ = paused;
}

} // namespace dtvr
