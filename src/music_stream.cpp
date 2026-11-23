// SPDX-License-Identifier: GPL-3.0-only
#include "music_stream.hpp"

#include <SDL_endian.h>
#include <vorbis/vorbisfile.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace dtvr {
namespace {
constexpr size_t kRate = 44100, kChannels = 2, kCapacity = kRate, kChunk = 4096;
constexpr double kFadeFrames = kRate * 0.02;
}

struct MusicStream::Impl {
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::array<int16_t, kCapacity> ring{}; // half a second, independent of cue length
    size_t head = 0, count = 0;
    uint64_t generation = 0, played = 0, origin = 0;
    MusicTrack track;
    bool quit = false, paused = false, eof = false;
    int volume = 100;
    std::string error;
    std::thread worker;

    Impl() : worker([this] { run(); }) {}
    ~Impl() {
        { std::lock_guard<std::mutex> lock(mutex); quit = true; }
        wake.notify_all();
        worker.join();
    }
    void failure(uint64_t serial, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex);
        if (generation == serial) { error = message; eof = true; count = 0; }
    }
    void decode(const MusicTrack& cue, uint64_t serial, uint64_t offset) {
        OggVorbis_File file{};
        if (ov_fopen(cue.file.string().c_str(), &file) != 0) { failure(serial, "Ogg-Datei kann nicht geoeffnet werden."); return; }
        struct Close { OggVorbis_File* file; ~Close() { ov_clear(file); } } close{&file};
        const auto* info = ov_info(&file, -1);
        if (!info || info->channels != 2 || info->rate != long(kRate) || ov_streams(&file) != 1
            || ov_pcm_seek(&file, ogg_int64_t(offset)) != 0) {
            failure(serial, "Ungueltiges Ogg-Format oder Suchposition."); return;
        }
        const uint64_t end = uint64_t(std::llround(cue.nominalEnd * kRate));
        uint64_t decoded = offset;
        std::array<int16_t, kChunk> buffer{};
        while (decoded < end) {
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock, [&] { return quit || generation != serial || count <= kCapacity - kChunk; });
                if (quit || generation != serial) return;
            }
            int section = 0;
            const int bytes = int(std::min<uint64_t>(kChunk, (end - decoded) * kChannels) * sizeof(int16_t));
            const long read = ov_read(&file, reinterpret_cast<char*>(buffer.data()), bytes,
                                     SDL_BYTEORDER == SDL_BIG_ENDIAN, 2, 1, &section);
            if (read <= 0 || read % 4) { failure(serial, "Ogg-Datei ist beschaedigt oder endet zu frueh."); return; }
            const size_t samples = size_t(read) / sizeof(int16_t);
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (quit || generation != serial) return;
                for (size_t i = 0; i < samples; ++i) ring[(head + count + i) % kCapacity] = buffer[i];
                count += samples;
            }
            decoded += samples / kChannels;
        }
        std::lock_guard<std::mutex> lock(mutex);
        if (generation == serial) eof = true;
    }
    void run() {
        uint64_t seen = 0;
        for (;;) {
            MusicTrack request;
            uint64_t serial = 0, offset = 0;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock, [&] { return quit || generation != seen; });
                if (quit) return;
                seen = serial = generation;
                request = track;
                offset = played;
            }
            if (!request.file.empty()) {
                try { decode(request, serial, offset); }
                catch (const std::exception&) { failure(serial, "Ogg-Decodierung fehlgeschlagen."); }
            }
        }
    }
};

MusicStream::MusicStream() : impl_(std::make_unique<Impl>()) {}
MusicStream::~MusicStream() = default;

void MusicStream::load(const MusicTrack& track, double position) {
    if (!std::isfinite(track.nominalEnd) || track.nominalEnd < 0 || track.nominalEnd > 3600 || !std::isfinite(position))
        throw std::invalid_argument("Invalid Ogg timeline");
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->track = track;
    impl_->played = impl_->origin = uint64_t(std::llround(std::clamp(position, 0.0, track.nominalEnd) * kRate));
    impl_->head = impl_->count = 0;
    impl_->eof = false;
    impl_->error.clear();
    ++impl_->generation;
    impl_->wake.notify_all();
}
void MusicStream::stop() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->track = {};
    impl_->head = impl_->count = 0;
    impl_->played = impl_->origin = 0;
    impl_->eof = false;
    impl_->error.clear();
    ++impl_->generation;
    impl_->wake.notify_all();
}
void MusicStream::pause(bool paused) { std::lock_guard<std::mutex> lock(impl_->mutex); impl_->paused = paused; }
void MusicStream::volume(int percent) { std::lock_guard<std::mutex> lock(impl_->mutex); impl_->volume = std::clamp(percent, 0, 100); }
MusicStream::Status MusicStream::status() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return {double(impl_->played) / kRate, impl_->eof && impl_->count == 0, impl_->error};
}
void MusicStream::render(int16_t* output, size_t samples) {
    std::fill(output, output + samples, 0);
    std::unique_lock<std::mutex> lock(impl_->mutex, std::try_to_lock);
    if (!lock || impl_->paused) return;
    const size_t take = std::min(samples - samples % 2, impl_->count);
    const uint64_t end = uint64_t(std::llround(impl_->track.nominalEnd * kRate));
    for (size_t i = 0; i < take; i += 2) {
        const double fade = std::clamp(std::min(double(impl_->played - impl_->origin) / kFadeFrames,
                                                double(end - std::min(end, impl_->played)) / kFadeFrames), 0.0, 1.0);
        for (size_t channel = 0; channel < 2; ++channel)
            output[i + channel] = int16_t(impl_->ring[(impl_->head + i + channel) % kCapacity] * fade * impl_->volume / 100.0);
        ++impl_->played;
    }
    impl_->head = (impl_->head + take) % kCapacity;
    impl_->count -= take;
    lock.unlock();
    impl_->wake.notify_one();
}

} // namespace dtvr
