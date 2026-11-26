// SPDX-License-Identifier: GPL-3.0-only
#include "save_slots.hpp"

#include "save.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>
#include <system_error>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr const char* kIndex = "saves.ini";

// Number of UTF-8 characters and the byte offset of character `n`.
size_t utf8Length(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) n += (c & 0xC0) != 0x80;
    return n;
}
size_t utf8Offset(const std::string& s, size_t chars) {
    size_t n = 0;
    for (size_t i = 0; i < s.size(); ++i)
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80 && n++ == chars) return i;
    return s.size();
}

} // namespace

fs::path defaultSaveDirectory() {
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) return fs::path(xdg) / "dtvr-recomp" / "saves";
    if (const char* home = std::getenv("HOME"); home && *home)
        return fs::path(home) / ".local" / "share" / "dtvr-recomp" / "saves";
    return fs::path("dtvr-recomp-saves");
}

fs::path saveDirectory(Edition edition) {
    return defaultSaveDirectory().parent_path() / editionProfile(edition).saveDirectory;
}

namespace {

// The edition id of a save file from its first 4 KiB (the record is the second line).
std::string fileEdition(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return {};
    std::string head(4096, '\0');
    in.read(&head[0], std::streamsize(head.size()));
    head.resize(size_t(in.gcount()));
    return SaveCodec::editionOf(head);
}

std::string foreignTo(Edition edition, const fs::path& file) {
    const std::string id = fileEdition(file);
    return id.empty() || id == editionProfile(edition).id ? std::string() : id;
}

const EditionProfile* profileById(const std::string& id) {
    for (const auto& profile : editionProfiles())
        if (id == profile.id) return &profile;
    return nullptr;
}

} // namespace

namespace {

// fs::exists without exceptions: an inaccessible path counts as absent.
bool present(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec) && !ec;
}

bool readable(const fs::path& file) { return std::ifstream(file, std::ios::binary).is_open(); }

bool copySave(const fs::path& from, const fs::path& to, std::string& error) {
    std::string text;
    return readSaveFile(from.string(), text, error) && writeSaveFile(to.string(), text, error);
}

} // namespace

bool adoptEditionSaves(const fs::path& shared, const fs::path& own, Edition edition, EditionAdoption& result,
                       const SaveFileCopy& copy) {
    result = EditionAdoption{};
    std::error_code ec;
    if (own.lexically_normal() == shared.lexically_normal() || fs::exists(own, ec) || ec || !fs::is_directory(shared, ec))
        return true;
    const std::string id = editionProfile(edition).id;
    const SaveSlots from(shared, edition);
    auto candidate = [&](const fs::path& file) {
        if (!present(file)) return false;
        if (!readable(file)) { result.skipped.push_back(file.string()); return false; }
        return fileEdition(file) == id;
    };
    std::vector<std::pair<int, std::string>> slots;
    for (int s = 0; s < kSaveSlots; ++s)
        if (candidate(from.file(s))) slots.emplace_back(s, from.description(s));
    const bool autosave = candidate(from.autoFile());
    if (slots.empty() && !autosave) return true; // nothing of this edition: the directory follows the first save
    const fs::path temporary = own.parent_path() / (own.filename().string() + ".tmp");
    auto fail = [&](const std::string& why) {
        result.error = why;
        result.copied = 0;
        std::error_code ignored;
        fs::remove_all(temporary, ignored);
        return false;
    };
    fs::remove_all(temporary, ec); // a leftover of an interrupted copy
    if (ec) return fail("Verzeichnis " + temporary.string() + ": " + ec.message());
    fs::create_directories(temporary, ec);
    if (ec) return fail("Verzeichnis " + temporary.string() + ": " + ec.message());
    const SaveFileCopy& copyFile = copy ? copy : SaveFileCopy(copySave);
    const SaveSlots to(temporary, edition);
    std::ostringstream index;
    index << "[Save]\n";
    std::string error;
    for (const auto& [s, description] : slots) {
        if (!copyFile(from.file(s), to.file(s), error)) return fail(error);
        index << "Slot" << s << '=' << description << '\n';
        ++result.copied;
    }
    if (!slots.empty() && !writeSaveFile((temporary / kIndex).string(), index.str(), error)) return fail(error);
    if (autosave) {
        if (!copyFile(from.autoFile(), to.autoFile(), error)) return fail(error);
        ++result.copied;
    }
    fs::rename(temporary, own, ec);
    if (ec) return fail("Verzeichnis " + own.string() + ": " + ec.message());
    return true;
}

std::string foreignSaveMessage(Edition running, const std::string& saveEdition) {
    const EditionProfile* theirs = profileById(saveEdition);
    if (editionProfile(running).languageLetter == 'E') {
        const std::string which = !theirs ? "an unknown edition (" + saveEdition + ")"
                                  : theirs->edition == Edition::GermanRetail ? "the German edition \"Die total verrückte Rallye\""
                                                                             : "the US English edition \"Dr. Drago's Madcap Chase\"";
        return "This saved game belongs to " + which + " and cannot be loaded with this edition.";
    }
    const std::string which = !theirs ? "zu einer unbekannten Ausgabe (" + saveEdition + ")"
                              : theirs->edition == Edition::GermanRetail ? "zur deutschen Ausgabe \"Die total verrückte Rallye\""
                                                                         : "zur US-englischen Ausgabe \"Dr. Drago's Madcap Chase\"";
    return "Dieser Spielstand gehört " + which + " und lässt sich mit dieser Ausgabe nicht laden.";
}

std::string markForeignSave(const std::string& label, const std::string& foreign) {
    if (foreign.empty()) return label;
    const EditionProfile* theirs = profileById(foreign);
    return std::string("[") + (theirs ? theirs->tag : "?") + "] " + label;
}

std::string saveDescription(const Game& game, const std::map<int, std::string>& texts) {
    std::string names;
    for (int p = 0; p < kPlayerSlots; ++p)
        if (game.players()[size_t(p)].status) names += game.playerName(p) + " - ";
    if (names.size() >= 3) names.resize(names.size() - 3); // seg8:BDE5: the last " - "
    constexpr size_t kMax = 0x50 - 1;                       // seg8:BDF5: buffer of 0x50
    if (utf8Length(names) > kMax) names = names.substr(0, utf8Offset(names, kMax - 3)) + "...";
    const unsigned month = unsigned(game.calendar().month) % 12;
    const auto it = texts.find(301 + int(month));
    const std::string monthName = it == texts.end() ? std::to_string(month + 1) : it->second;
    return monthName + " (" + std::to_string(game.calendar().year + 1) + ") - " + names;
}

fs::path SaveSlots::file(int slot) const { return directory_ / ("slot" + std::to_string(slot) + ".sav"); }

std::map<int, std::string> SaveSlots::readIndex() const {
    std::map<int, std::string> out;
    std::ifstream in(directory_ / kIndex);
    std::string line;
    while (std::getline(in, line)) { // "Slot<n>=<description>" below "[Save]"
        if (line.rfind("Slot", 0) != 0) continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        try {
            const int slot = std::stoi(line.substr(4, eq - 4));
            if (slot >= 0 && slot < kSaveSlots) out[slot] = line.substr(eq + 1);
        } catch (const std::exception&) {
        }
    }
    return out;
}

std::string SaveSlots::description(int slot) const {
    if (slot < 0 || slot >= kSaveSlots || !present(file(slot))) return {};
    const auto index = readIndex();
    const auto it = index.find(slot);
    return it == index.end() ? std::string("?") : it->second;
}

namespace {
// The year balance and the monthly statement run inside a suspended player change that the
// save format does not hold (rally.exe's windows are modal): no save is written then.
bool unsavable(const Game& game, std::string& error) {
    if (game.phase() != Game::Phase::Balance && game.phase() != Game::Phase::Statement) return false;
    error = "waehrend Jahresbilanz oder Monatsabschluss nicht speicherbar";
    return true;
}
} // namespace

bool SaveSlots::save(int slot, const Game& game, const std::string& description, std::string& error) const {
    if (slot < 0 || slot >= kSaveSlots) { error = "ungueltiger Spielstand"; return false; }
    if (unsavable(game, error)) return false;
    std::error_code ec;
    fs::create_directories(directory_, ec);
    if (ec) { error = "Verzeichnis " + directory_.string() + ": " + ec.message(); return false; }
    if (!writeSaveFile(file(slot).string(), SaveCodec::encode(game), error)) return false;
    auto index = readIndex();
    std::string clean = description;
    for (char& c : clean) if (c == '\n' || c == '\r') c = ' ';
    index[slot] = clean;
    std::ostringstream o;
    o << "[Save]\n";
    for (const auto& [n, text] : index) o << "Slot" << n << '=' << text << '\n';
    return writeSaveFile((directory_ / kIndex).string(), o.str(), error);
}

bool SaveSlots::saveAuto(const Game& game, std::string& error) const {
    if (unsavable(game, error)) return false;
    std::error_code ec;
    fs::create_directories(directory_, ec);
    if (ec) { error = "Verzeichnis " + directory_.string() + ": " + ec.message(); return false; }
    return writeSaveFile(autoFile().string(), SaveCodec::encode(game), error);
}

bool SaveSlots::loadAuto(Game& game, std::string& error) const {
    std::string text;
    if (!readSaveFile(autoFile().string(), text, error) || !SaveCodec::decode(game, text, error, SaveCodec::Random::Session))
        return false;
    game.resumeLoaded(); // seg8:A8DF -> seg8:AC9A, then seg4:061D ff.
    return true;
}

bool SaveSlots::hasAuto() const { return present(autoFile()); }

std::string SaveSlots::foreignSlot(int slot) const {
    if (slot < 0 || slot >= kSaveSlots) return {};
    return foreignTo(edition_, file(slot));
}

std::string SaveSlots::foreignAuto() const { return foreignTo(edition_, autoFile()); }

bool SaveSlots::load(int slot, Game& game, std::string& error) const {
    std::string text;
    if (slot < 0 || slot >= kSaveSlots || !readSaveFile(file(slot).string(), text, error)) {
        if (error.empty()) error = "ungueltiger Spielstand";
        return false;
    }
    if (!SaveCodec::decode(game, text, error, SaveCodec::Random::Session)) return false;
    game.resumeLoaded(); // seg8:B3E1 -> seg8:AC9A, then seg4:069B ff.
    return true;
}

} // namespace dtvr
