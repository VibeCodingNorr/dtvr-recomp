// SPDX-License-Identifier: GPL-3.0-only
// Presentation time for the optional animation tempo (recomb extension, not in
// rally.exe; docs/animation-tempo.md). Real elapsed milliseconds are accumulated at
// the current rate (1, 2 or 4); a rate change counts the time since the last update
// at the old rate, and paused time (the recomb menu) is not added. Only cosmetic
// timing reads it: event animations and the vehicles' drive. Input, deadlines,
// message reading time, music, speech and films keep real time. No SDL.
#pragma once

#include <cstdint>

namespace dtvr {

class PresentationClock {
public:
    static bool validRate(int rate) { return rate == 1 || rate == 2 || rate == 4; }
    // Accounts the real time since the previous update; the first update only starts.
    void update(uint32_t realMs);
    // Accounts the elapsed time at the old rate, then continues at `rate` (invalid: 1).
    void setRate(int rate, uint32_t realMs);
    void setPaused(bool paused, uint32_t realMs);
    uint32_t now() const { return now_; }
    int rate() const { return rate_; }
    bool paused() const { return paused_; }

private:
    bool started_ = false, paused_ = false;
    uint32_t lastReal_ = 0, now_ = 0;
    int rate_ = 1;
};

} // namespace dtvr
