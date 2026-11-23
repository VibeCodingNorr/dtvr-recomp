// SPDX-License-Identifier: GPL-3.0-only
// dat/lan.inf: the language letter of the installation (seg5:0239) with synthetic files.
#include "data_paths.hpp"

#include <cstdio>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using namespace dtvr;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}

struct TempRoot {
    fs::path path;
    TempRoot() {
        path = fs::temp_directory_path() / ("dtvr-lan-" + std::to_string(std::random_device{}()));
        fs::create_directories(path);
    }
    ~TempRoot() { std::error_code e; fs::remove_all(path, e); }
    TempRoot(const TempRoot&) = delete;
    TempRoot& operator=(const TempRoot&) = delete;
    void write(const fs::path& relative, const std::string& bytes) const {
        fs::create_directories((path / relative).parent_path());
        std::ofstream(path / relative, std::ios::binary) << bytes;
    }
};

bool throws(const fs::path& root) {
    try { readLanguageLetter(root); } catch (const std::runtime_error&) { return true; }
    return false;
}

} // namespace

int main() {
    {
        TempRoot r;
        r.write("dat/lan.inf", "D\r\n"); // the German copy
        check(readLanguageLetter(r.path) == 'D', "D CR LF keeps the stored case");
        check(languageDir('D') == "d", "directory lower-case");
        check(languageLibraryPath('D') == fs::path("dat/d/rally_d.dat"), "library path");
    }
    {
        TempRoot r;
        r.write("DAT/LAN.INF", "e"); // no line end, names in other case
        check(readLanguageLetter(r.path) == 'e', "e without line end, case-insensitive lookup");
        check(languageLibraryPath('e') == fs::path("dat/e/rally_e.dat"), "English library path");
    }
    { TempRoot r; check(throws(r.path), "missing file"); }
    { TempRoot r; r.write("dat/lan.inf", ""); check(throws(r.path), "empty file"); }
    { TempRoot r; r.write("dat/lan.inf", "1\r\n"); check(throws(r.path), "digit"); }
    { TempRoot r; r.write("dat/lan.inf", "\r\n"); check(throws(r.path), "line end only"); }
    { TempRoot r; r.write("dat/lan.inf", std::string("\xC4", 1)); check(throws(r.path), "non-ASCII byte"); }
    {
        TempRoot r;
        fs::create_directories(r.path / "dat/lan.inf"); // a directory, not a file
        check(throws(r.path), "directory instead of file");
    }
    check(parseLanguageLetter({'Z', 0}) == 'Z' && parseLanguageLetter({'a'}) == 'a', "letter bounds");
    for (uint8_t c : {uint8_t('@'), uint8_t('['), uint8_t('`'), uint8_t('{'), uint8_t(0)}) {
        bool threw = false;
        try { parseLanguageLetter({c}); } catch (const std::runtime_error&) { threw = true; }
        check(threw, "bytes next to the letter ranges");
    }
    return failures ? 1 : 0;
}
