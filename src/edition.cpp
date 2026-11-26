// SPDX-License-Identifier: GPL-3.0-only
#include "edition.hpp"

#include "data_paths.hpp"
#include "sha256.hpp"

#include <json-c/json.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

std::string lower(std::string text) {
    for (char& c : text) c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

bool isSha256Hex(const std::string& text) {
    return text.size() == 64 && std::all_of(text.begin(), text.end(), [](char c) {
               return std::isxdigit(static_cast<unsigned char>(c)) != 0;
           });
}

std::string shortHash(const std::string& sha256) { return sha256.substr(0, 8) + "…"; }

constexpr uintmax_t kManifestLimit = 8 * 1024 * 1024; // as tools/installation_manifest.py

} // namespace

const std::vector<EditionProfile>& editionProfiles() {
    static const std::vector<EditionProfile> kProfiles = {
        {Edition::GermanRetail, "de-retail", "deutsche Ausgabe \"Die total verrückte Rallye\"",
         "aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83", 'D',
         {640, 800, 1024}, // seg5:1227: art dirs 6, 8, 1
         {"dat/lan.inf", "dat/d/rally_d.dat", "map/europe.inf", "map/europe.str", "map/europe.map", "map/europe.pit", "map/europe_d.tow",
          "map/0/part0000.sbb", "sbs/d.sbs", "butt/1/map.bmp"},
         "butt/1/map.bmp", true},
        // Full US retail CD (PROGRAM/ on its ISO). Its rules are not audited yet. Verified
        // so far: the canvas choice R seg5:1239-135B equals D seg5:1227-1349 (same
        // immediates and DS operands), and art dirs 1, 6 and 8 ship (BUTT/<n>/MAP.BMP).
        {Edition::EnglishRetail, "en-us-retail", "US-englische Verkaufsausgabe \"Dr. Drago's Madcap Chase\"",
         "a8374ca9e39d70b4508efefa782b7135c650792be36600289356a51de819225a", 'E',
         {640, 800, 1024},
         {"dat/lan.inf", "dat/e/rally_e.dat", "map/europe.inf", "map/europe.str", "map/europe.map", "map/europe.pit", "map/europe_e.tow",
          "map/0/part0000.sbb", "sbs/d.sbs", "butt/6/map.bmp", "butt/8/map.bmp", "butt/1/map.bmp"},
         "butt/6/map.bmp", false},
    };
    return kProfiles;
}

const EditionProfile& editionProfile(Edition edition) {
    for (const auto& profile : editionProfiles())
        if (profile.edition == edition) return profile;
    throw std::logic_error("unbekannte Ausgabe");
}

const EditionProfile* profileForExecutableHash(const std::string& sha256) {
    const std::string hash = lower(sha256);
    for (const auto& profile : editionProfiles())
        if (hash == profile.exeSha256) return &profile;
    return nullptr;
}

int chooseCanvasWidth(const EditionProfile& profile, int screenWidth, int screenHeight, int requestedWidth) {
    const auto& widths = profile.canvasWidths;
    if (requestedWidth) {
        if (std::find(widths.begin(), widths.end(), requestedWidth) == widths.end())
            throw std::invalid_argument("--resolution " + std::to_string(requestedWidth) + " gibt es in der "
                                        + profile.name + " nicht");
        return requestedWidth;
    }
    int chosen = widths.front(); // the smallest one when nothing fits (seg5:1301 ff.)
    for (int width : widths)
        if (screenWidth >= width && screenHeight >= width * 3 / 4) chosen = std::max(chosen, width);
    return chosen;
}

std::string emptySlotLabel(Edition edition, const std::string& string11) {
    if (edition == Edition::GermanRetail) return "leer";
    return string11.substr(0, 19); // LoadString with a 20-byte buffer (R seg8:B7EE)
}

std::string creditsPicture(Edition edition, char languageLetter) {
    if (edition == Edition::GermanRetail) return "Credits.bmp";
    return languageDir(languageLetter) + "/credits.bmp";
}

ManifestEdition parseManifestEdition(const std::string& text) {
    std::unique_ptr<json_tokener, decltype(&json_tokener_free)> parser(json_tokener_new_ex(32), json_tokener_free);
    if (!parser) throw std::runtime_error("manifest.json: kein Speicher");
    json_tokener_set_flags(parser.get(), JSON_TOKENER_STRICT | JSON_TOKENER_VALIDATE_UTF8);
    std::unique_ptr<json_object, decltype(&json_object_put)> json(
        json_tokener_parse_ex(parser.get(), text.c_str(), int(text.size())), json_object_put);
    if (!json || json_tokener_get_error(parser.get()) != json_tokener_success
        || text.find_first_not_of(" \n\r\t", json_tokener_get_parse_end(parser.get())) != std::string::npos
        || !json_object_is_type(json.get(), json_type_object))
        throw std::runtime_error("manifest.json ist kein gültiges JSON-Objekt");
    auto string = [&](const char* key, bool required) -> std::string {
        json_object* value = nullptr;
        if (!json_object_object_get_ex(json.get(), key, &value)) {
            if (required) throw std::runtime_error(std::string("manifest.json: Feld ") + key + " fehlt");
            return {};
        }
        if (!json_object_is_type(value, json_type_string))
            throw std::runtime_error(std::string("manifest.json: Feld ") + key + " ist kein Text");
        return std::string(json_object_get_string(value), size_t(json_object_get_string_len(value)));
    };
    const std::string format = string("format", true);
    if (format != "dtvr-recomp-installation" && format != "dtvr-recomb-installation")
        throw std::runtime_error("manifest.json: unbekanntes Format " + format);
    ManifestEdition out{lower(string("executable_sha256", true)), string("edition", false)};
    if (!isSha256Hex(out.sha256)) throw std::runtime_error("manifest.json: executable_sha256 ist kein SHA-256-Wert");
    return out;
}

DetectedEdition detectEdition(const fs::path& directory) {
    fs::path programDir = fs::absolute(directory).lexically_normal();
    if (programDir.filename().empty()) programDir = programDir.parent_path(); // trailing separator
    DetectedEdition fromExe, fromManifest;
    if (const fs::path exe = findCaseInsensitive(programDir, "rally.exe"); !exe.empty() && fs::is_regular_file(exe)) {
        const std::string hash = sha256FileHex(exe);
        fromExe = {profileForExecutableHash(hash), {EditionEvidence::Source::Executable, exe, hash}};
        if (!fromExe.profile)
            throw std::runtime_error("Unbekannte rally.exe-Version (SHA-256 " + hash + "): " + exe.string()
                                     + ". Diese Ausgabe wird nicht unterstützt.");
    }
    // tools/install.py: <install>/manifest.json, data in <install>/data/program.
    const fs::path manifest = programDir.parent_path().parent_path() / "manifest.json";
    std::error_code error;
    if (programDir.parent_path().filename() == "data" && fs::is_regular_file(manifest, error)) {
        const uintmax_t size = fs::file_size(manifest, error);
        if (error || size > kManifestLimit) throw std::runtime_error("manifest.json ungültig: " + manifest.string());
        std::ifstream in(manifest, std::ios::binary);
        const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        if (!in.good() && !in.eof()) throw std::runtime_error("manifest.json nicht lesbar: " + manifest.string());
        ManifestEdition entry;
        try {
            entry = parseManifestEdition(text);
        } catch (const std::runtime_error& e) {
            throw std::runtime_error(std::string(e.what()) + " (" + manifest.string() + ")");
        }
        fromManifest = {profileForExecutableHash(entry.sha256), {EditionEvidence::Source::Manifest, manifest, entry.sha256}};
        if (!fromManifest.profile)
            throw std::runtime_error("Installation mit unbekannter rally.exe-Version (SHA-256 " + entry.sha256 + "): "
                                     + manifest.string());
        if (!entry.edition.empty() && entry.edition != fromManifest.profile->id)
            throw std::runtime_error("manifest.json: Ausgabe " + entry.edition + " passt nicht zum EXE-Hash "
                                     + shortHash(entry.sha256));
    }
    if (fromExe.profile && fromManifest.profile && fromExe.profile != fromManifest.profile)
        throw std::runtime_error("rally.exe (" + shortHash(fromExe.evidence.sha256) + ") und manifest.json ("
                                 + shortHash(fromManifest.evidence.sha256) + ") nennen verschiedene Ausgaben");
    if (fromExe.profile) return fromExe;
    if (fromManifest.profile) return fromManifest;
    throw std::runtime_error("Ausgabe nicht feststellbar: weder rally.exe in " + programDir.string()
                             + " noch manifest.json einer Installation von tools/install.py");
}

std::vector<std::string> missingEditionFiles(const EditionProfile& profile, const fs::path& programDir) {
    std::vector<std::string> missing;
    for (const auto& file : profile.requiredFiles) {
        const fs::path found = findCaseInsensitive(programDir, file);
        std::error_code error;
        if (found.empty() || !fs::is_regular_file(found, error)) missing.push_back(file);
    }
    return missing;
}

void validateEditionData(const EditionProfile& profile, const fs::path& programDir, char languageLetter) {
    const auto missing = missingEditionFiles(profile, programDir);
    if (!missing.empty()) {
        std::string list;
        for (const auto& file : missing) list += (list.empty() ? "" : ", ") + file;
        throw std::runtime_error(std::string("Originaldaten der ") + profile.name + " unvollständig, es fehlt: " + list);
    }
    if (std::toupper(static_cast<unsigned char>(languageLetter)) != profile.languageLetter)
        throw std::runtime_error(std::string("dat/lan.inf nennt die Sprache ") + languageLetter + ", die " + profile.name
                                 + " gehört zu " + profile.languageLetter);
}

} // namespace dtvr
