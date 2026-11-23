// SPDX-License-Identifier: GPL-3.0-only
// dat/<lang>/sync.dat: subtitle timing of the voices (round 88, no SDL). Records of
// 0xD0 bytes (seg1:41EF indexes DS:9694 + 0xD0 * DS:3E46): +0x00 the WAV file name
// (NUL-terminated, 20 bytes), +0x14 the length in bytes (uint32), +0x18 ten talk
// intervals {uint32 start, uint32 end} (seg1:4071), +0x68 ten entries of {uint16 text
// id, uint32 start, uint32 end}, +0xCC a cue (uint32, seg1:414C), all in bytes of the
// 11,025 Hz 8-bit voice.
// Evidence in docs/formats.md "Subtitles".
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dtvr {

struct VoiceSyncEntry {
    uint16_t text = 0;
    uint32_t start = 0, end = 0;
};

struct VoiceSyncRecord {
    std::string name; // as stored, e.g. "part01a.wav"
    uint32_t length = 0;
    std::vector<VoiceSyncEntry> entries; // the ten slots, empty ones included
    std::vector<VoiceSyncEntry> talk;    // ten intervals (text unused)
    uint32_t cue = 0;
};

// Throws std::runtime_error when the size is not a multiple of 0xD0 or a name is
// not terminated within its 20 bytes.
std::vector<VoiceSyncRecord> parseVoiceSync(const std::vector<uint8_t>& data);
// The table of `<root>/dat/<lang>/sync.dat` for the lan.inf letter `language`; empty
// when missing or malformed.
std::vector<VoiceSyncRecord> loadVoiceSync(const std::filesystem::path& root, char language);
// The record of a voice file name (compared without regard to case), or nullptr.
const VoiceSyncRecord* findVoiceSync(const std::vector<VoiceSyncRecord>& table, const std::string& name);

// seg1:3F46: the playing position, elapsed milliseconds times 11 (TimerCount path),
// 0 once it passes the record's length; seg1:41EF: the first entry whose
// [start, end] holds it, 0 for none or position 0.
uint16_t voiceSyncText(const VoiceSyncRecord& record, uint32_t elapsedMs);
// seg1:3E64 (TimerCount path): the voice plays while elapsed ms times 11 <= length.
bool voiceSyncPlaying(const VoiceSyncRecord& record, uint32_t elapsedMs);
// seg1:4071: the position lies in one of the ten talk intervals (both inclusive).
bool voiceSyncTalking(const VoiceSyncRecord& record, uint32_t elapsedMs);
// seg1:414C: the cue is set and the position has passed it.
bool voiceSyncCue(const VoiceSyncRecord& record, uint32_t elapsedMs);

// The polling of seg7:04F2 (intro, ZIEL, Sieg and Bilanz loops alike): a new
// non-zero id is shown; the text is cleared once the lookup has been 0 for two
// polls after a non-zero one.
class SubtitleTrack {
public:
    void start(const VoiceSyncRecord* record, uint32_t nowMs);
    void stop() { record_ = nullptr; shown_ = set_ = 0; history_[0] = history_[1] = history_[2] = 0; }
    // One pass of the loop; returns the id now shown (0 = empty window).
    uint16_t poll(uint32_t nowMs);
    uint16_t shown() const { return shown_; }

private:
    const VoiceSyncRecord* record_ = nullptr;
    uint32_t startMs_ = 0;
    uint16_t shown_ = 0; // the window text (0 = cleared)
    uint16_t set_ = 0;   // [bp-0x9C]: the last id set, kept when cleared
    uint16_t history_[3] = {0, 0, 0}; // [bp-0x96], [bp-0x98], [bp-0x9A]
};

} // namespace dtvr
