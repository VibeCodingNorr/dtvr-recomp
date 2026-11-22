// SPDX-License-Identifier: GPL-3.0-only
// Sound effects as rally.exe plays them through WaveMix (round 40): the table of
// seg1:2E90 ff. maps sound numbers 0-38 to snd/fx/<name>.wav; seg1:2C9B(channel,
// sound, loops) plays one on channel 0-7 (a new sound replaces the channel's,
// loops 0xFFFF = endless), seg1:317D flushes a channel. Mixed with SDL audio; no
// sound without an audio device.
#pragma once

#include <SDL.h>

#include <array>
#include <filesystem>
#include <vector>

namespace dtvr {

constexpr int kSoundCount = 39;
const char* soundFile(int index); // "Plopp01.wav" ...

class SoundPlayer {
public:
    explicit SoundPlayer(const std::filesystem::path& root);
    ~SoundPlayer();
    SoundPlayer(const SoundPlayer&) = delete;
    SoundPlayer& operator=(const SoundPlayer&) = delete;

    bool available() const { return device_ != 0; }
    void setEnabled(bool enabled) { enabled_ = enabled; } // [Setup] Wave (DS:B5D0)
    void play(int channel, int sound, int loops = 0);
    void stop(int channel);

private:
    struct Voice { const std::vector<int16_t>* samples = nullptr; size_t pos = 0; int loops = 0; };
    static void callback(void* self, Uint8* stream, int len);
    void mix(int16_t* out, size_t samples);
    const std::vector<int16_t>* load(int sound);

    std::filesystem::path root_;
    SDL_AudioDeviceID device_ = 0;
    SDL_AudioSpec spec_{};
    bool enabled_ = true;
    std::array<std::vector<int16_t>, kSoundCount> cache_;
    std::array<bool, kSoundCount> loaded_{};
    std::array<Voice, 8> voices_{};
};

} // namespace dtvr
