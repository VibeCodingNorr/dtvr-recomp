// SPDX-License-Identifier: GPL-3.0-only
#include "sound.hpp"

#include "audio_mix.hpp"
#include "sdl_assets.hpp"
#include "music_stream.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace fs = std::filesystem;

namespace dtvr {

const char* soundFile(int index) {
    static const char* const kFiles[kSoundCount] = {
        "Plopp01.wav", "Fate03.wav",   "Plopp02.wav",  "Lachen01.wav", "Hebel01.wav",  "Walze10.wav",
        "Zaehler1.wav", "Zaehler2.wav", "Taster01.wav", "Carb01.wav",   "Hupe01.wav",   "Hupe02.wav",
        "Hupe03.wav",  "Hupe05.wav",   "moneywin.wav", "Sfx08.wav",    "Elefant.wav",  "Fate01.wav",
        "Platsch.wav", "Hebel05.wav",  "Schlaf01.wav", "Gamble01.wav", "Gamble02.wav", "Gamble03.wav",
        "Gamble04.wav", "Woelfe1.wav", "NHorn1.wav",   "Huhn1.wav",    "Kuehe1a.wav",  "Lachen02.wav",
        "Schweine.wav", "Baer1.wav",   "pf1.wav",      "pf2.wav",      "w.wav",        "MEMO01.wav",
        "MEMO02.wav",  "MEMO03.wav",   "MEMO04.wav"};
    return index >= 0 && index < kSoundCount ? kFiles[index] : "";
}

SoundPlayer::SoundPlayer(const fs::path& root) : root_(root) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
    initialized_ = true;
    SDL_AudioSpec want{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = &SoundPlayer::callback;
    want.userdata = this;
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &spec_, 0);
    if (device_ == 0) { std::cerr << "Ton: " << SDL_GetError() << "\n"; return; }
    SDL_PauseAudioDevice(device_, 0);
}

SoundPlayer::~SoundPlayer() {
    if (device_) SDL_CloseAudioDevice(device_);
    if (initialized_) SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

const std::vector<int16_t>* SoundPlayer::load(int sound) {
    if (sound < 0 || sound >= kSoundCount) return nullptr;
    if (loaded_[size_t(sound)]) return cache_[size_t(sound)].empty() ? nullptr : &cache_[size_t(sound)];
    loaded_[size_t(sound)] = true;
    convert(findCaseInsensitive(root_, fs::path("snd") / "fx" / soundFile(sound)), cache_[size_t(sound)]);
    return cache_[size_t(sound)].empty() ? nullptr : &cache_[size_t(sound)];
}

void SoundPlayer::playFile(int channel, const fs::path& file) {
    if (!device_ || !enabled_ || channel < 0 || channel > 7) return;
    std::vector<int16_t> samples;
    if (!convert(file, samples)) return;
    SDL_LockAudioDevice(device_);
    if (effectsBlocked()) { SDL_UnlockAudioDevice(device_); return; } // WaveMix is not active
    for (Voice& v : voices_) if (v.samples == &fileSamples_) v = {};
    fileSamples_ = std::move(samples);
    voices_[size_t(channel)] = {&fileSamples_, 0, 0};
    SDL_UnlockAudioDevice(device_);
}

bool SoundPlayer::convert(const fs::path& path, std::vector<int16_t>& out) const {
    SDL_AudioSpec spec{};
    Uint8* data = nullptr;
    Uint32 length = 0;
    if (path.empty() || !SDL_LoadWAV(path.string().c_str(), &spec, &data, &length)) return false;
    SDL_AudioCVT cvt{};
    if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, spec_.channels, spec_.freq) >= 0) {
        std::vector<Uint8> buffer(size_t(length) * size_t(cvt.len_mult));
        std::memcpy(buffer.data(), data, length);
        cvt.buf = buffer.data();
        cvt.len = int(length);
        if (SDL_ConvertAudio(&cvt) == 0) {
            out.resize(size_t(cvt.len_cvt) / 2);
            std::memcpy(out.data(), buffer.data(), out.size() * 2);
        }
    }
    SDL_FreeWAV(data);
    return !out.empty();
}

void SoundPlayer::play(int channel, int sound, int loops) {
    if (!device_ || !enabled_ || channel < 0 || channel > 7) return; // seg1:2CAF: Wave off -> nothing
    const std::vector<int16_t>* samples = load(sound);
    if (!samples) return;
    SDL_LockAudioDevice(device_);
    if (!effectsBlocked()) voices_[size_t(channel)] = {samples, 0, loops == 0xFFFF ? -1 : loops}; // else WaveMix is off
    SDL_UnlockAudioDevice(device_);
}

void SoundPlayer::flushEffects() { // with the device locked
    for (size_t c = 0; c < voices_.size(); ++c)
        if (int(c) != voiceChannel_) voices_[c] = {};
}

void SoundPlayer::setFocused(bool focused) {
    if (!device_) { focused_ = focused; return; }
    SDL_LockAudioDevice(device_);
    focused_ = focused;
    if (!focused) flushEffects(); // seg4:0D6E: WaveMixActivate(FALSE); a voice only pauses (0D4E)
    SDL_UnlockAudioDevice(device_);
}

void SoundPlayer::playVoice(int channel, const fs::path& file) {
    if (channel < 0 || channel > 7 || file.empty()) return;
    silentUntil_[size_t(channel)] = 0;
    if (device_ && voiceEnabled_) { // voices do not depend on Sound-F/X (seg1:3661)
        std::vector<int16_t> samples;
        if (!convert(file, samples)) return;
        SDL_LockAudioDevice(device_);
        voiceSamples_ = std::move(samples);
        voiceChannel_ = channel;
        flushEffects(); // seg1:33E6: WaveMixActivate(FALSE) before waveOutOpen
        voices_[size_t(channel)] = {&voiceSamples_, 0, 0};
        SDL_UnlockAudioDevice(device_);
        return;
    }
    stop(channel);
    SDL_AudioSpec spec{};
    Uint8* data = nullptr;
    Uint32 length = 0;
    if (!SDL_LoadWAV(file.string().c_str(), &spec, &data, &length)) return;
    const Uint32 frame = Uint32(SDL_AUDIO_BITSIZE(spec.format) / 8) * spec.channels;
    const Uint32 ms = spec.freq > 0 && frame > 0 ? Uint32(uint64_t(length / frame) * 1000 / Uint32(spec.freq)) : 0;
    SDL_FreeWAV(data);
    silentUntil_[size_t(channel)] = SDL_GetTicks() + ms;
}

bool SoundPlayer::busy(int channel) {
    if (channel < 0 || channel > 7) return false;
    if (silentUntil_[size_t(channel)] && int(silentUntil_[size_t(channel)] - SDL_GetTicks()) > 0) return true;
    if (!device_) return false;
    SDL_LockAudioDevice(device_);
    const bool playing = voices_[size_t(channel)].samples != nullptr;
    SDL_UnlockAudioDevice(device_);
    return playing;
}

void SoundPlayer::stop(int channel) {
    if (channel >= 0 && channel <= 7) silentUntil_[size_t(channel)] = 0;
    if (!device_ || channel < 0 || channel > 7) return;
    SDL_LockAudioDevice(device_);
    voices_[size_t(channel)] = {};
    if (channel == voiceChannel_) voiceChannel_ = -1; // seg1:3D86: WaveMix active again
    SDL_UnlockAudioDevice(device_);
}

void SoundPlayer::callback(void* self, Uint8* stream, int len) {
    static_cast<SoundPlayer*>(self)->mix(reinterpret_cast<int16_t*>(stream), size_t(len) / 2);
}

void SoundPlayer::mix(int16_t* out, size_t samples) {
    if (music_) music_->render(out, samples);
    else std::fill(out, out + samples, 0);
    const int master = master_.load();
    for (size_t i = 0; i < samples; ++i) {
        int effects = 0, voice = 0; // WaveMix sums and clips its channels; the voice has its own device
        for (size_t c = 0; c < voices_.size(); ++c) {
            Voice& v = voices_[c];
            if (!v.samples) continue;
            const bool isVoice = int(c) == voiceChannel_;
            if (isVoice && !focused_) continue; // seg4:0D4E: waveOutPause
            if (v.pos >= v.samples->size()) {
                if (v.loops == 0) {
                    v = {};
                    if (isVoice) voiceChannel_ = -1; // seg1:3D86: WaveMixActivate(TRUE)
                    continue;
                }
                if (v.loops > 0) --v.loops;
                v.pos = 0;
            }
            (isVoice ? voice : effects) += (*v.samples)[v.pos++];
        }
        out[i] = int16_t(mixSample(out[i], effects, voice) * master / 100);
    }
}

void SoundPlayer::setMusic(MusicStream* music) {
    if (device_) SDL_LockAudioDevice(device_);
    music_ = music;
    if (device_) SDL_UnlockAudioDevice(device_);
}

} // namespace dtvr
