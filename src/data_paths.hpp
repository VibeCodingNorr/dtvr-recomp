// SPDX-License-Identifier: GPL-3.0-only
// Paths into the user's installation (no SDL): the case-insensitive lookup and the
// language letter of dat/lan.inf.
//
// seg5:0239 formats `%s\lan.inf` (DS:0118) with the dat directory (DS:7E), reads
// the file (seg6:027F) and keeps its first byte as the language letter DS:A0F0;
// seg5:02B1 then loads `Rally_%c.dat` (DS:0123) as the language library. The letter
// names the language subdirectories (dat\d, butt\d, bmp\<n>\d, ...). Evidence in
// docs/media-variants.md "Language letter".
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dtvr {

// The original ran on a case-insensitive file system (rally.exe asks for e.g.
// "P1\Dp.bmp", the disc holds "p1/dp.bmp"). Resolves `relative` below `root`
// component by component, ignoring ASCII case; empty if nothing matches.
std::filesystem::path findCaseInsensitive(const std::filesystem::path& root, const std::filesystem::path& relative);

// The first byte of lan.inf as stored (the German copy holds "D\r\n"). rally.exe keeps
// it unchanged in DS:A0F0 (its only writer, seg5:028C) and compares it with case in
// places (seg1:766F / 77C4 'd' only, seg7:7EC1 'd' or 'D', seg5:219C 'E' or 'e'), so
// the case is kept. seg6:027F zero-fills its buffer (GlobalAlloc 0x42, size + 1): an
// empty file yields 0, the library name becomes "Rally_" and its load fails. Throws
// std::runtime_error when the data is empty or the first byte is not an ASCII letter
// (rebuild decision: rally.exe uses any byte and only fails when the library
// `dat\<byte>\Rally_<byte>.dat` does not load).
char parseLanguageLetter(const std::vector<uint8_t>& data);
// The letter of `<root>/dat/lan.inf` (looked up without regard to case). Throws
// std::runtime_error when the file is missing, unreadable or invalid: rally.exe then
// shows its ERROR dialog (seg6:0639, template DS:164B), seg5:0225 returns 0 and
// seg5:0058 ends the start. No fallback letter.
char readLanguageLetter(const std::filesystem::path& root);
// The language subdirectory name of a letter, lower-case ("d"); Windows looked paths
// up without regard to case and findCaseInsensitive does the same here.
std::string languageDir(char language);
// `dat/<c>/rally_<c>.dat`, the language library: seg5:02B1 "Rally_%c.dat" (DS:0123),
// seg5:1B6B "%s\%c\%s" (DS:02ED) with the dat directory DS:7E, then LoadLibrary.
std::filesystem::path languageLibraryPath(char language);

} // namespace dtvr
