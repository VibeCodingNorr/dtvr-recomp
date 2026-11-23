// SPDX-License-Identifier: GPL-3.0-only
// Sound effects as rally.exe plays them through WaveMix (round 40): the table of
// seg1:2E90 ff. maps sound numbers 0-38 to snd/fx/<name>.wav; seg1:2C9B(channel,
// sound, loops) plays one on channel 0-7 (a new sound replaces the channel's,
// loops 0xFFFF = endless), seg1:317D flushes a channel. Mixed with SDL audio; no
// sound without an audio device.
//
// Mixing as in rally.exe (round 132, docs/formats.md "Sound mixing"): WaveMix sums the
// effect channels without attenuation (WaveMixConfigureInit: mono, 11 kHz, seg1:2C10);
// a voice is played outside WaveMix after WaveMixActivate(FALSE) (seg1:33E6), so the
// effects are silent until it has ended (seg1:3D86); losing the focus pauses the voice
// or deactivates WaveMix (seg4:0D19). Music and effects reached the speakers through
// separate devices, both set to full volume at start (seg5:2164 / 2194): the sum of
// the two is limited softly instead of being clipped.
#pragma once

#include <SDL.h>

#include <array>
#include <atomic>
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
    // [Audio] MasterVolume 0..100 for everything this device plays (music included).
    void setMasterVolume(int percent) { master_ = percent < 0 ? 0 : percent > 100 ? 100 : percent; }
    // WM_ACTIVATEAPP (seg4:0CAE): without the focus a voice pauses, the effects stop.
    void setFocused(bool focused);
    // Whether a sound still plays on the channel (seg1:3E64 waits for voices).
    bool busy(int channel);
    // Must outlive this audio device; all sources share one stereo mixer.
    void setMusic(MusicStream* music);

private:
    struct Voice { const std::vector<int16_t>* samples = nullptr; size_t pos = 0; int loops = 0; };
    void flushEffects();              // WaveMixActivate(FALSE): every effect channel stops
    bool effectsBlocked() const { return voiceChannel_ >= 0 || !focused_; }
    static void callback(void* self, Uint8* stream, int len);
    void mix(int16_t* out, size_t samples);
    const std::vector<int16_t>* load(int sound);
    bool convert(const std::filesystem::path& path, std::vector<int16_t>& out) const;
    std::vector<int16_t> fileSamples_; // the last file of playFile
    std::vector<int16_t> voiceSamples_; // the voice played outside WaveMix
    int voiceChannel_ = -1;             // its channel while it sounds (callback clears it)
    bool focused_ = true;
    std::atomic<int> master_{100};

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
