// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "music_stream.hpp"

namespace dtvr {

// Main-thread control. No decoding or audio time can change simulation/RNG state.
class MusicPlayer {
public:
    MusicPlayer(std::vector<MusicPack> packs, MusicPreferences preferences);
    void interrupt(const std::string& cue);
    void nextBackground();
    void returnToBackground();
    void tick();
    void turn(int counter);
    void setEnabled(bool enabled);
    void setFocused(bool focused);
    void choosePack(const std::string& id); // apply at the next cue boundary
    void setVolume(int percent);
    bool enabled() const { return enabled_; }
    const auto& packs() const { return packs_; }
    const auto& preferences() const { return preferences_; }
    const auto& cue() const { return cue_; }
    const auto& message() const { return message_; }
    std::string activePack() const;
    MusicStream& stream() { return stream_; }
private:
    void begin(const std::string& cue, double position = 0);
    void applyPack();
    std::vector<MusicPack> packs_;
    MusicPreferences preferences_;
    MusicStream stream_;
    int active_ = -1, next_ = 0, turn_ = 0, backgroundTurn_ = 0, failures_ = 0;
    bool enabled_ = true, focused_ = true;
    std::string cue_, background_, message_;
    double backgroundPosition_ = 0;
};

} // namespace dtvr
