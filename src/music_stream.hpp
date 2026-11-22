// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "music_catalog.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace dtvr {

// Decoder work happens on a worker. The audio callback only consumes a bounded
// stereo PCM ring and never reads files, allocates memory or waits for its lock.
class MusicStream {
public:
    struct Status { double position = 0; bool finished = false; std::string error; };
    MusicStream();
    ~MusicStream();
    MusicStream(const MusicStream&) = delete;
    MusicStream& operator=(const MusicStream&) = delete;
    void load(const MusicTrack& track, double position = 0);
    void stop();
    void pause(bool paused);
    void volume(int percent);
    Status status() const;
    // Fills an even number of interleaved signed-16 samples at 44,100 Hz stereo.
    void render(int16_t* output, size_t samples);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dtvr
