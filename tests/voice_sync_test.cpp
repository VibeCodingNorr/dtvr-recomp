// SPDX-License-Identifier: GPL-3.0-only
#include "voice_sync.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using namespace dtvr;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}

void put16(std::vector<uint8_t>& d, size_t o, uint32_t v) { d[o] = uint8_t(v); d[o + 1] = uint8_t(v >> 8); }
void put32(std::vector<uint8_t>& d, size_t o, uint32_t v) { put16(d, o, v & 0xFFFF); put16(d, o + 2, v >> 16); }

std::vector<uint8_t> record(const char* name, uint32_t length, std::vector<VoiceSyncEntry> entries) {
    std::vector<uint8_t> d(0xD0, 0);
    for (size_t i = 0; name[i]; ++i) d[i] = uint8_t(name[i]);
    put32(d, 0x14, length);
    for (size_t i = 0; i < entries.size(); ++i) {
        put16(d, 0x68 + 10 * i, entries[i].text);
        put32(d, 0x6A + 10 * i, entries[i].start);
        put32(d, 0x6E + 10 * i, entries[i].end);
    }
    return d;
}

} // namespace

int main() {
    std::vector<uint8_t> data = record("part01a.wav", 1100, {{40010, 0, 330}, {40011, 331, 550}, {40012, 880, 1100}});
    const auto second = record("next_b_b.wav", 500, {{42111, 0, 1091475}});
    data.insert(data.end(), second.begin(), second.end());
    const auto table = parseVoiceSync(data);
    check(table.size() == 2 && table[0].name == "part01a.wav" && table[0].length == 1100, "records");
    check(table[0].entries.size() == 10 && table[0].entries[1].text == 40011 && table[0].entries[1].end == 550, "entries");
    const VoiceSyncRecord* r = findVoiceSync(table, "PART01A.WAV");
    check(r == &table[0] && !findVoiceSync(table, "part01b.wav"), "lookup without case");

    // Position = ms * 11; 0 and beyond the length give no text; bounds inclusive.
    check(voiceSyncText(*r, 0) == 0, "position 0");
    check(voiceSyncText(*r, 30) == 40010, "330 bytes = end of the first entry");
    check(voiceSyncText(*r, 31) == 40011, "341 bytes");
    check(voiceSyncText(*r, 60) == 0, "660 bytes: gap");
    check(voiceSyncText(*r, 100) == 40012, "1100 bytes: last byte");
    check(voiceSyncText(*r, 101) == 0, "past the length");
    check(voiceSyncText(table[1], 40) == 42111 && voiceSyncText(table[1], 46) == 0, "end beyond length");

    // Polling: show new ids, clear after two zero polls following text, never re-show the last id.
    SubtitleTrack t;
    t.start(r, 1000);
    check(t.poll(1000) == 0, "first poll at position 0");
    check(t.poll(1010) == 40010, "first text");
    check(t.poll(1040) == 40011, "second text");
    check(t.poll(1060) == 40011, "gap: one zero poll keeps the text");
    check(t.poll(1061) == 0, "gap: second zero poll clears");
    check(t.poll(1062) == 0, "stays cleared");
    check(t.poll(1090) == 40012, "third text");
    check(t.poll(1200) == 40012 && t.poll(1201) == 0, "voice over: cleared after two zero polls");
    t.start(r, 2000); // same record again: the last id set (40012) is not shown anew
    check(t.poll(2095) == 0, "replay: id 40012 stays hidden");
    check(t.poll(2010) == 40010, "replay: another id is shown");
    t.stop();
    check(t.poll(5000) == 0, "stopped");

    bool threw = false;
    try { parseVoiceSync(std::vector<uint8_t>(0xD1, 0)); } catch (const std::runtime_error&) { threw = true; }
    check(threw, "size not a multiple of 0xD0");
    threw = false;
    try { parseVoiceSync(std::vector<uint8_t>(0xD0, 'a')); } catch (const std::runtime_error&) { threw = true; }
    check(threw, "unterminated name");
    check(parseVoiceSync({}).empty(), "empty file");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
