// SPDX-License-Identifier: GPL-3.0-only
// The number of the stage count window TEMP (seg8:08D6, src/stages_view.cpp) without
// SDL: how rally.exe reads the field and the RECOMP ADDITION of stepping it with keys
// (the pad's d-pad and LB / RB). Evidence in docs/formats.md "Stage count window".
#pragma once

#include <string>

namespace dtvr {

constexpr int kMaxStages = 300; // seg8:0A45: 0x12C

// seg8:0A0D: the first four characters (GetWindowText(…, 5)) read by atoi.
int parseStageCount(const std::string& text);

// Recomp: the count `by` steps on, clamped to the slider's range 1..300. It starts from
// the field when that holds an accepted value (1..300), otherwise from `current`.
int stepStageCount(const std::string& field, int current, int by);

} // namespace dtvr
