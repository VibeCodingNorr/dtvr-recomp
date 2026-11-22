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

class MusicStream;

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
    // seg1:3579: a WAV file by path (finish, voices); same channel rules.
    void playFile(int channel, const std::filesystem::path& file);
    // seg1:3579 / 3661: a voice plays with a wave device (DS:9552) and [Setup] Voice
    // (DS:B5D4), regardless of Sound-F/X; otherwise nothing sounds, but the channel
    // counts as busy for the voice's length (the TimerCount path of seg1:3E64), so
    // scenes wait as they would with sound.
    void playVoice(int channel, const std::filesystem::path& file);
    void setVoiceEnabled(bool enabled) { voiceEnabled_ = enabled; }
    void stop(int channel);
    // Whether a sound still plays on the channel (seg1:3E64 waits for voices).
    bool busy(int channel);
    // Must outlive this audio device; all sources share one stereo mixer.
    void setMusic(MusicStream* music);

private:
    struct Voice { const std::vector<int16_t>* samples = nullptr; size_t pos = 0; int loops = 0; };
    static void callback(void* self, Uint8* stream, int len);
    void mix(int16_t* out, size_t samples);
    const std::vector<int16_t>* load(int sound);
    bool convert(const std::filesystem::path& path, std::vector<int16_t>& out) const;
    std::vector<int16_t> fileSamples_; // the last file of playFile

    std::filesystem::path root_;
    bool initialized_ = false;
    SDL_AudioDeviceID device_ = 0;
    SDL_AudioSpec spec_{};
    bool enabled_ = true;
    bool voiceEnabled_ = true;
    std::array<Uint32, 8> silentUntil_{}; // a voice that is not played (SDL_GetTicks)
    std::array<std::vector<int16_t>, kSoundCount> cache_;
    std::array<bool, kSoundCount> loaded_{};
    std::array<Voice, 8> voices_{};
    MusicStream* music_ = nullptr;
};

} // namespace dtvr
