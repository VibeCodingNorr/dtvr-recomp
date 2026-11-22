// SPDX-License-Identifier: GPL-3.0-only
#include "sound.hpp"

#include "sdl_assets.hpp"

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
    want.freq = 22050;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
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
    if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, 1, spec_.freq) >= 0) {
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
    voices_[size_t(channel)] = {samples, 0, loops == 0xFFFF ? -1 : loops};
    SDL_UnlockAudioDevice(device_);
}

void SoundPlayer::stop(int channel) {
    if (!device_ || channel < 0 || channel > 7) return;
    SDL_LockAudioDevice(device_);
    voices_[size_t(channel)] = {};
    SDL_UnlockAudioDevice(device_);
}

void SoundPlayer::callback(void* self, Uint8* stream, int len) {
    static_cast<SoundPlayer*>(self)->mix(reinterpret_cast<int16_t*>(stream), size_t(len) / 2);
}

void SoundPlayer::mix(int16_t* out, size_t samples) {
    for (size_t i = 0; i < samples; ++i) {
        int sum = 0;
        for (Voice& v : voices_) {
            if (!v.samples) continue;
            if (v.pos >= v.samples->size()) {
                if (v.loops == 0) { v = {}; continue; }
                if (v.loops > 0) --v.loops;
                v.pos = 0;
            }
            sum += (*v.samples)[v.pos++];
        }
        out[i] = int16_t(std::clamp(sum, -32768, 32767));
    }
}

} // namespace dtvr
