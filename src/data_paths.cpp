// SPDX-License-Identifier: GPL-3.0-only
#include "data_paths.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace fs = std::filesystem;

namespace dtvr {

fs::path findCaseInsensitive(const fs::path& root, const fs::path& relative) {
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return s;
    };
    fs::path current = root;
    for (const auto& part : relative) {
        const fs::path exact = current / part;
        if (fs::exists(exact)) { current = exact; continue; }
        if (!fs::is_directory(current)) return {};
        fs::path found;
        for (const auto& entry : fs::directory_iterator(current))
            if (lower(entry.path().filename().string()) == lower(part.string())) { found = entry.path(); break; }
        if (found.empty()) return {};
        current = found;
    }
    return current;
}

char parseLanguageLetter(const std::vector<uint8_t>& data) {
    if (data.empty()) throw std::runtime_error("dat/lan.inf ist leer");
    const uint8_t c = data[0];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
        throw std::runtime_error("dat/lan.inf: erstes Byte ist kein Buchstabe (0x" +
                                 std::string(1, "0123456789ABCDEF"[c >> 4]) + "0123456789ABCDEF"[c & 15] + ")");
    return char(c);
}

char readLanguageLetter(const fs::path& root) {
    const fs::path file = findCaseInsensitive(root, fs::path("dat") / "lan.inf");
    if (file.empty() || !fs::is_regular_file(file))
        throw std::runtime_error("dat/lan.inf fehlt in " + root.string() + " (Sprachbuchstabe der Installation)");
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("Nicht lesbar: " + file.string());
    return parseLanguageLetter({std::istreambuf_iterator<char>(in), {}});
}

std::string languageDir(char language) {
    return std::string(1, char(std::tolower(static_cast<unsigned char>(language))));
}

fs::path languageLibraryPath(char language) {
    const std::string c = languageDir(language);
    return fs::path("dat") / c / ("rally_" + c + ".dat");
}

} // namespace dtvr
