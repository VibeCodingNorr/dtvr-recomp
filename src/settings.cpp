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
    o << "TextSmoothing=" << textSmoothing << '\n';
    o << "[Extensions]\n";
    o << "ComputerExtraCards=" << int(computerExtraCards) << '\n';
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
        if (name == "TextSmoothing") { s.textSmoothing = v >= 0 && v <= 2 ? v : 1; continue; }
        if (name == "ComputerExtraCards") { s.computerExtraCards = v != 0; continue; }
        for (int i = 0; i < kCount; ++i)
            if (name == key(i) && v >= 0 && v < states(i)) s.value[size_t(i)] = v;
    }
    return s;
}

fs::path defaultSettingsFile() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) return fs::path(xdg) / "dtvr-recomb" / "settings.ini";
    if (const char* home = std::getenv("HOME"); home && *home)
        return fs::path(home) / ".config" / "dtvr-recomb" / "settings.ini";
    return fs::path("dtvr-recomb-settings.ini");
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
