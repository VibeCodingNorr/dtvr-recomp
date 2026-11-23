// SPDX-License-Identifier: GPL-3.0-only
#include "voice_sync.hpp"

#include "data_paths.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace dtvr {
namespace {

constexpr size_t kRecord = 0xD0, kName = 0x14, kLength = 0x14, kTalk = 0x18, kEntries = 0x68, kEntry = 10, kSlots = 10,
                 kCue = 0xCC;

uint16_t u16(const std::vector<uint8_t>& d, size_t o) { return uint16_t(d[o] | d[o + 1] << 8); }
uint32_t u32(const std::vector<uint8_t>& d, size_t o) { return uint32_t(u16(d, o)) | uint32_t(u16(d, o + 2)) << 16; }

bool sameName(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
}

} // namespace

std::vector<VoiceSyncRecord> parseVoiceSync(const std::vector<uint8_t>& data) {
    if (data.size() % kRecord != 0) throw std::runtime_error("sync.dat: size is not a multiple of 0xD0");
    std::vector<VoiceSyncRecord> table;
    table.reserve(data.size() / kRecord);
    for (size_t base = 0; base < data.size(); base += kRecord) {
        VoiceSyncRecord r;
        size_t n = 0;
        while (n < kName && data[base + n] != 0) ++n;
        if (n == kName) throw std::runtime_error("sync.dat: unterminated name");
        r.name.assign(reinterpret_cast<const char*>(&data[base]), n);
        r.length = u32(data, base + kLength);
        for (size_t i = 0; i < kSlots; ++i) {
            const size_t o = base + kEntries + i * kEntry;
            r.entries.push_back(VoiceSyncEntry{u16(data, o), u32(data, o + 2), u32(data, o + 6)});
            const size_t t = base + kTalk + i * 8;
            r.talk.push_back(VoiceSyncEntry{0, u32(data, t), u32(data, t + 4)});
        }
        r.cue = u32(data, base + kCue);
        table.push_back(std::move(r));
    }
    return table;
}

std::vector<VoiceSyncRecord> loadVoiceSync(const std::filesystem::path& root, char language) {
    // seg1:31D4: "%s\\%c\\sync.dat" with the dat directory and the language letter.
    const std::filesystem::path dir = findCaseInsensitive(root, std::filesystem::path("dat") / languageDir(language) / "sync.dat");
    if (dir.empty()) return {};
    std::ifstream in(dir, std::ios::binary);
    const std::vector<uint8_t> data{std::istreambuf_iterator<char>(in), {}};
    try { return parseVoiceSync(data); } catch (const std::exception&) { return {}; }
}

const VoiceSyncRecord* findVoiceSync(const std::vector<VoiceSyncRecord>& table, const std::string& name) {
    for (const auto& r : table)
        if (!r.name.empty() && sameName(r.name, name)) return &r;
    return nullptr;
}

namespace {
// seg1:3F46: 0 once beyond the length.
uint64_t position(const VoiceSyncRecord& record, uint32_t elapsedMs) {
    const uint64_t pos = uint64_t(elapsedMs) * 11; // seg1:3FCC: x * 11 by shifts
    return pos > record.length ? 0 : pos;           // seg1:3FE4
}
} // namespace

bool voiceSyncPlaying(const VoiceSyncRecord& record, uint32_t elapsedMs) {
    return uint64_t(elapsedMs) * 11 <= record.length; // seg1:3EFC
}

bool voiceSyncTalking(const VoiceSyncRecord& record, uint32_t elapsedMs) {
    const uint64_t pos = position(record, elapsedMs);
    if (pos == 0) return false; // seg1:409F
    for (const auto& t : record.talk)
        if (t.start <= pos && pos <= t.end) return true;
    return false;
}

bool voiceSyncCue(const VoiceSyncRecord& record, uint32_t elapsedMs) {
    const uint64_t pos = position(record, elapsedMs);
    return record.cue != 0 && record.cue < pos; // seg1:418F: 0 while cue >= position
}

uint16_t voiceSyncText(const VoiceSyncRecord& record, uint32_t elapsedMs) {
    const uint64_t pos = position(record, elapsedMs);
    if (pos == 0) return 0; // seg1:421D
    for (const auto& e : record.entries)            // seg1:4229: ten slots
        if (e.start <= pos && pos <= e.end) return e.text;
    return 0;
}

void SubtitleTrack::start(const VoiceSyncRecord* record, uint32_t nowMs) {
    record_ = record;
    startMs_ = nowMs;
}

uint16_t SubtitleTrack::poll(uint32_t nowMs) {
    const uint16_t id = record_ ? voiceSyncText(*record_, nowMs - startMs_) : 0;
    history_[2] = history_[1];
    history_[1] = history_[0];
    if (id != history_[0]) { // seg7:04F7
        history_[0] = id;
        if (id != 0 && id != set_) shown_ = set_ = id; // seg7:0508 / 0515
    } else if (id == 0 && history_[1] == 0 && history_[2] != 0) {
        shown_ = 0; // seg7:0538 .. 0559
    }
    return shown_;
}

} // namespace dtvr
