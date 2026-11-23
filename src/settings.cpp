// SPDX-License-Identifier: GPL-3.0-only
#include "settings.hpp"

#include "save.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace dtvr {

const char* Settings::key(int index) {
    static const char* const kKeys[kCount] = {"Wave", "Midi", "Voice", "TimeOut",
                                              "Views", "Animations", "Colorcycle", "Joystick"};
    return index >= 0 && index < kCount ? kKeys[index] : "";
}

const char* Settings::switchName(int index) {
    static const char* const kNames[kCount] = {"fx", "midi", "spra", "zeit", "seh", "anim", "farb", "joy"};
    return index >= 0 && index < kCount ? kNames[index] : "";
}

std::string Settings::encode() const {
    std::ostringstream o;
    o << "[Setup]\n";
    for (int i = 0; i < kCount; ++i) o << key(i) << '=' << value[size_t(i)] << '\n';
    o << "Subtitle=" << subtitle << '\n';
    o << "Pause=" << pause << '\n';
    o << "ShowIntro=" << showIntro << '\n';
    o << "[Default]\n";
    o << "Rounds=" << rounds << '\n';
    o << "[Display]\n";
    o << "Fullscreen=" << int(fullscreen) << '\n';
    o << "IntegerScale=" << int(integerScale) << '\n';
    o << "Widescreen=" << int(widescreen) << '\n';
    o << "TextSmoothing=" << textSmoothing << '\n';
    o << "[Audio]\n";
    o << "MasterVolume=" << masterVolume << '\n';
    o << "[Extensions]\n";
    o << "ComputerExtraCards=" << int(computerExtraCards) << '\n';
    o << "AnimationTempo=" << animationTempo << '\n';
    o << "ComputerTempo=" << computerTempo << '\n';
    return o.str();
}

Settings Settings::decode(const std::string& text) {
    Settings s;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string name = line.substr(0, eq);
        int v = 0;
        try { v = std::stoi(line.substr(eq + 1)); } catch (const std::exception&) { continue; }
        if (name == "Subtitle") { s.subtitle = v != 0; continue; }
        if (name == "ShowIntro") { s.showIntro = v != 0; continue; }
        if (name == "Pause") { s.pause = v >= 0 && v <= 3 ? v : 0; continue; } // seg6:0D89
        if (name == "Rounds") { s.rounds = v; continue; } // the window checks 0..300 itself
        if (name == "Fullscreen") { s.fullscreen = v != 0; continue; }
        if (name == "IntegerScale") { s.integerScale = v != 0; continue; }
        if (name == "Widescreen") { s.widescreen = v != 0; continue; }
        if (name == "TextSmoothing") { s.textSmoothing = v >= 0 && v <= 2 ? v : 1; continue; }
        if (name == "ComputerExtraCards") { s.computerExtraCards = v != 0; continue; }
        if (name == "AnimationTempo") { s.animationTempo = v == 2 || v == 4 ? 2 : 1; continue; }
        if (name == "MasterVolume") { s.masterVolume = v >= 0 && v <= 100 ? v : 100; continue; }
        if (name == "ComputerTempo") { s.computerTempo = v == 2 || v == 4 ? v : 1; continue; }
        for (int i = 0; i < kCount; ++i)
            if (name == key(i) && v >= 0 && v < states(i)) s.value[size_t(i)] = v;
    }
    return s;
}

bool adoptFormerName(const fs::path& path, std::string& error) {
    fs::path former;
    bool renamed = false;
    for (const fs::path& part : path) {
        const bool match = !renamed && part == "dtvr-recomp";
        former /= match ? fs::path("dtvr-recomb") : part;
        renamed |= match;
    }
    std::error_code ec;
    if (!renamed || fs::exists(path, ec) || !fs::exists(former, ec)) return true;
    fs::create_directories(path.parent_path(), ec);
    if (!ec) fs::copy(former, path, fs::copy_options::recursive, ec);
    if (ec) { error = former.string() + ": " + ec.message(); return false; }
    return true;
}

fs::path defaultSettingsFile() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) return fs::path(xdg) / "dtvr-recomp" / "settings.ini";
    if (const char* home = std::getenv("HOME"); home && *home)
        return fs::path(home) / ".config" / "dtvr-recomp" / "settings.ini";
    return fs::path("dtvr-recomp-settings.ini");
}

Settings loadSettings(const fs::path& file) {
    std::ifstream in(file);
    if (!in) return Settings{};
    std::ostringstream text;
    text << in.rdbuf();
    return Settings::decode(text.str());
}

bool saveSettings(const fs::path& file, const Settings& settings, std::string& error) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    if (ec) { error = ec.message(); return false; }
    return writeSaveFile(file.string(), settings.encode(), error);
}

} // namespace dtvr
