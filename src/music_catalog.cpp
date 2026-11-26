// SPDX-License-Identifier: GPL-3.0-only
#include "music_catalog.hpp"
#include "save.hpp"

#include <json-c/json.h>
#include <vorbis/vorbisfile.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace dtvr {
namespace {

json_object* member(json_object* object, const char* key, json_type type) {
    json_object* value = nullptr;
    if (!json_object_object_get_ex(object, key, &value) || !json_object_is_type(value, type))
        throw std::runtime_error(std::string("Invalid music catalog field: ") + key);
    return value;
}
std::string stringValue(json_object* object, const char* key) {
    auto* value = member(object, key, json_type_string);
    const std::string text(json_object_get_string(value), size_t(json_object_get_string_len(value)));
    if (text.find('\0') != std::string::npos) throw std::runtime_error("NUL in music catalog");
    return text;
}
double number(json_object* object, const char* key) {
    json_object* value = nullptr;
    if (!json_object_object_get_ex(object, key, &value)
        || !(json_object_is_type(value, json_type_int) || json_object_is_type(value, json_type_double)))
        throw std::runtime_error("Missing music timeline");
    const double n = json_object_get_double(value);
    if (!std::isfinite(n)) throw std::runtime_error("Invalid music timeline");
    return n;
}
void checkVorbis(const fs::path& path, double duration) {
    OggVorbis_File file{};
    if (ov_fopen(path.string().c_str(), &file) != 0) throw std::runtime_error("Unreadable Ogg Vorbis cue");
    struct Close { OggVorbis_File* file; ~Close() { ov_clear(file); } } close{&file};
    const vorbis_info* info = ov_info(&file, -1);
    if (!info || ov_streams(&file) != 1 || info->channels != 2 || info->rate != 44100
        || std::abs(ov_time_total(&file, -1) - duration) > 0.05)
        throw std::runtime_error("Unsupported Ogg channels, rate or duration");
}

} // namespace

bool validMusicPackId(const std::string& id) {
    return !id.empty() && id.size() <= 48 && ((id[0] >= 'a' && id[0] <= 'z') || (id[0] >= '0' && id[0] <= '9'))
        && std::all_of(id.begin(), id.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
        });
}

MusicPack readMusicPack(const fs::path& directory) {
    if (fs::is_symlink(directory) || !fs::is_directory(directory)) throw std::runtime_error("Linked music pack");
    const fs::path root = fs::canonical(directory), catalog = root / "pack.json";
    if (fs::is_symlink(catalog) || !fs::is_regular_file(catalog) || fs::file_size(catalog) > 1024 * 1024)
        throw std::runtime_error("Missing or oversized music catalog");
    std::ifstream input(catalog);
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    std::unique_ptr<json_tokener, decltype(&json_tokener_free)> parser(json_tokener_new_ex(16), json_tokener_free);
    if (!parser) throw std::runtime_error("Cannot allocate music parser");
    json_tokener_set_flags(parser.get(), JSON_TOKENER_STRICT | JSON_TOKENER_VALIDATE_UTF8);
    std::unique_ptr<json_object, decltype(&json_object_put)> json(
        json_tokener_parse_ex(parser.get(), text.c_str(), int(text.size())), json_object_put);
    if (!json || json_tokener_get_error(parser.get()) != json_tokener_success
        || text.find_first_not_of(" \n\r\t", json_tokener_get_parse_end(parser.get())) != std::string::npos)
        throw std::runtime_error("Malformed music catalog");
    const std::string packKind = stringValue(json.get(), "source_kind");
    if (json_object_get_int(member(json.get(), "schema_version", json_type_int)) != 1
        || (packKind != "soundfont-render" && packKind != "cd-hybrid" && packKind != "release-ogg-hybrid")
        || !json_object_get_boolean(member(json.get(), "asset_complete", json_type_boolean)))
        throw std::runtime_error("Unsupported music catalog version/type");
    MusicPack pack;
    pack.id = stringValue(json.get(), "id");
    pack.name = stringValue(json.get(), "name");
    if (!validMusicPackId(pack.id) || pack.id != root.filename().string() || pack.name.empty() || pack.name.size() > 320
        || std::any_of(pack.name.begin(), pack.name.end(), [](unsigned char c) { return c < 32; }))
        throw std::runtime_error("Invalid music pack identity");
    auto* order = member(json.get(), "background_order", json_type_array);
    if (json_object_array_length(order) != 6) throw std::runtime_error("Incomplete background playlist");
    for (int i = 0; i < 6; ++i) {
        auto* entry = json_object_array_get_idx(order, size_t(i));
        if (!json_object_is_type(entry, json_type_string) || std::string(json_object_get_string(entry)) != "g" + std::to_string(i))
            throw std::runtime_error("Invalid background playlist");
    }
    auto* cues = member(json.get(), "cues", json_type_object);
    if (json_object_object_length(cues) != int(kMusicCues.size())) throw std::runtime_error("Incomplete music pack");
    for (const char* cue : kMusicCues) {
        auto* entry = member(cues, cue, json_type_object);
        const fs::path relative = fs::path("cues") / (std::string(cue) + ".ogg");
        const double end = number(entry, "nominal_end_seconds"), duration = number(entry, "media_duration_seconds");
        // A rendering ends 2 s after its MIDI end (the synth's tail); a CD recording of
        // a cue (cd-hybrid sets) ends with the recording itself. A release's Ogg track
        // (release-ogg-hybrid sets, not original) is copied unmodified and ends at its
        // measured music end, before the trailing zero separator (docs/cd-audio.md).
        json_object* source = nullptr;
        const std::string kind = json_object_object_get_ex(entry, "source", &source) ? stringValue(entry, "source")
                                                                                    : "soundfont-render";
        const bool recording = kind == "cd-recording", release = kind == "release-ogg-recording";
        if ((kind != "soundfont-render" && !recording && !release) || (recording && packKind != "cd-hybrid")
            || (release && packKind != "release-ogg-hybrid"))
            throw std::runtime_error("Invalid cue source");
        double tail = recording ? 0 : 2;
        if (release) {
            tail = number(entry, "trailing_silence_seconds");
            if (tail < 0 || tail > 6) throw std::runtime_error("Invalid release Ogg silence");
        }
        if (stringValue(entry, "file") != relative.generic_string() || end < 0 || end > 3600
            || std::abs(duration - end - tail) > 0.05
            || json_object_get_boolean(member(entry, "loop", json_type_boolean)))
            throw std::runtime_error("Invalid cue path or timeline");
        const fs::path path = root / relative;
        if (fs::is_symlink(root / "cues") || fs::is_symlink(path) || !fs::is_regular_file(path)
            || fs::file_size(path) > 128 * 1024 * 1024 || fs::canonical(path) != path)
            throw std::runtime_error("Missing, linked or oversized music cue");
        checkVorbis(path, duration);
        pack.tracks.emplace(cue, MusicTrack{path, end});
    }
    return pack;
}

std::vector<MusicPack> discoverMusicPacks(const fs::path& directory, std::vector<std::string>& issues) {
    std::vector<MusicPack> packs;
    std::error_code ec;
    if (!fs::exists(directory, ec)) return packs;
    if (fs::is_symlink(directory, ec) || !fs::is_directory(directory, ec)) {
        issues.push_back("Musikverzeichnis ist kein lokaler Ordner.");
        return packs;
    }
    size_t count = 0;
    for (fs::directory_iterator it(directory, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
        if (++count > 128) { issues.push_back("Zu viele Klangsets (maximal 128)."); break; }
        try { packs.push_back(readMusicPack(it->path())); }
        catch (const std::exception& e) { issues.push_back(it->path().filename().string() + ": " + e.what()); }
    }
    if (ec) issues.push_back("Musikverzeichnis konnte nicht gelesen werden.");
    std::sort(packs.begin(), packs.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return packs;
}

std::string musicForNotice(int text) {
    // seg8:1D03,20D6,25DE,3970,3E52,4527,4C01,5638,5C49,6A10 (cue string pushed there),
    // each handler's text base: storm 0x13B1, robbers 0x139D, drought 0x1415 (round 135:
    // e3 is the drought's, not Nessie's — Nessie seg8:2A32 starts no cue).
    if (text >= 5041 && text <= 5043) return "e1";
    if (text >= 5021 && text <= 5023) return "e2";
    if (text >= 5141 && text <= 5143) return "e3";
    if ((text >= 5281 && text <= 5283) || (text >= 5301 && text <= 5303)) return "e4";
    if (text >= 5201 && text <= 5203) return "e5";
    if (text >= 5121 && text <= 5123) return "e6";
    if (text >= 5241 && text <= 5243) return "e7";
    if (text >= 5481 && text <= 5483) return "e8";
    if (text >= 5081 && text <= 5083) return "e9";
    if (text >= 5001 && text <= 5003) return "e11";
    return {};
}

bool noticeEndsEvent(int text) {
    // The texts of the seg8 event handlers that call seg1:275E after their message:
    // those with a cue and Nessie (seg8:36CC), text base 0x13ED.
    return !musicForNotice(text).empty() || (text >= 5101 && text <= 5103);
}

MusicPreferences loadMusicPreferences(const fs::path& file) {
    MusicPreferences preferences;
    std::ifstream input(file);
    std::string line;
    for (int n = 0; n < 100 && std::getline(input, line) && line.size() < 1024; ++n) {
        if (line.rfind("Pack=", 0) == 0 && validMusicPackId(line.substr(5))) preferences.pack = line.substr(5);
        if (line.rfind("Volume=", 0) == 0) {
            try {
                size_t end = 0;
                const int volume = std::stoi(line.substr(7), &end);
                if (end == line.size() - 7 && volume >= 0 && volume <= 100) preferences.volume = volume;
            } catch (const std::exception&) {}
        }
    }
    return preferences;
}

bool saveMusicPreferences(const fs::path& file, const MusicPreferences& p, std::string& error) {
    if ((!p.pack.empty() && !validMusicPackId(p.pack)) || p.volume < 0 || p.volume > 100) {
        error = "Ungueltige Musikeinstellung";
        return false;
    }
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    if (ec) { error = ec.message(); return false; }
    return writeSaveFile(file.string(), "[Music]\nPack=" + p.pack + "\nVolume=" + std::to_string(p.volume) + "\n", error);
}

} // namespace dtvr
