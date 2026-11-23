// SPDX-License-Identifier: GPL-3.0-only
// The mixing rule of one output sample (round 132, docs/formats.md "Sound mixing"),
// no SDL. WaveMix sums its effect channels and clips them to the sample range; the
// voice came from its own wave device and the music from the MIDI device, all at
// full volume (seg5:2164 / 2194), and they met in the sound card's analogue mixer:
// their sum is bent softly towards full scale above a knee instead of being cut.
#pragma once

#include <cstdint>

namespace dtvr {

constexpr int kMixKnee = 24576; // three quarters of full scale

int16_t softLimit(int sum);
// music: the rendered sample; effects: the sum of the WaveMix channels; voice: the voice.
int16_t mixSample(int music, int effects, int voice);

} // namespace dtvr
