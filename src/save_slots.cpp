// SPDX-License-Identifier: GPL-3.0-only
#include "save_slots.hpp"

#include "save.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
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
    if (slot < 0 || slot >= kSaveSlots || !fs::exists(file(slot))) return {};
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

bool SaveSlots::hasAuto() const { return fs::exists(autoFile()); }

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
