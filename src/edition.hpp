// SPDX-License-Identifier: GPL-3.0-only
// Edition profiles (no SDL): which original rally.exe the user's data belongs to and
// what follows from it for resource selection. Evidence and the address pairs
// (D = German, R = US English retail) are in docs/media-variants.md "Edition profiles".
//
// The edition is identified only by the SHA-256 of the original executable, never by
// the presence of files: either rally.exe itself in the program directory (an
// extracted disc or a copied game folder) or the `executable_sha256` field of the
// manifest.json that tools/install.py writes next to `data/` (the installer removes
// rally.exe from the installed data). Unknown hashes are rejected.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dtvr {

enum class Edition {
    GermanRetail,  // Die total verrückte Rallye, rally.exe aa3dde36… (the reference of the rebuild)
    EnglishRetail, // Dr. Drago's Madcap Chase, US retail CD, RALLY.EXE a8374ca9… (German rules, English resources)
};

struct EditionProfile {
    Edition edition;
    const char* id;          // stable identifier (manifest field "edition", save record "edition")
    const char* name;        // German display name for messages
    const char* exeSha256;   // lower-case hex
    char languageLetter;     // dat/lan.inf, upper case (rally.exe compares with case)
    std::vector<int> canvasWidths; // canvases rally.exe can choose (seg5:1227)
    // Original files this profile needs below the program directory (looked up without
    // regard to case). A matching EXE hash alone does not prove the data complete.
    std::vector<std::string> requiredFiles;
    std::string overviewBitmap; // the viewer's overview / sprite palette
    // false: recognised but not yet validated with a complete turn; the game modes refuse it
    // unless the developer option --dev-unfinished-edition allows it (no such profile at
    // present: both retail editions are playable).
    bool playable;
};

const EditionProfile& editionProfile(Edition edition);
// All known profiles, German first.
const std::vector<EditionProfile>& editionProfiles();
// The profile of an executable hash (case-insensitive hex), nullptr when unknown.
const EditionProfile* profileForExecutableHash(const std::string& sha256);

// seg5:1227 with the profile's canvases: the largest that fits the screen, or the
// requested width (0 = none). Throws std::invalid_argument when the profile does not
// have the requested canvas.
int chooseCanvasWidth(const EditionProfile& profile, int screenWidth, int screenHeight, int requestedWidth);

// The save dialog's label of an unused slot: German "leer" (DS:80D7, the
// GetPrivateProfileString default of seg8:B6B4); US retail LoadString(language library
// DS:B5C4, 11, buffer, 20) (R seg8:B7EE-B7FC), i.e. string 11 clipped to 19 bytes.
std::string emptySlotLabel(Edition edition, const std::string& string11);
// The Credits picture of menu command 1026, relative to butt/<art dir>: German
// "Credits.bmp" (D seg4:0720 pushes DS:009A for seg6:0A4D); US retail
// wsprintf("%c\credits.bmp" DS:009A, letter DS:A0F0) (R seg4:0701-070F) before seg6:0A4D.
std::string creditsPicture(Edition edition, char languageLetter);

// Where the hash came from.
struct EditionEvidence {
    enum class Source { Executable, Manifest } source = Source::Executable;
    std::filesystem::path file; // rally.exe or manifest.json
    std::string sha256;
};
struct DetectedEdition {
    const EditionProfile* profile = nullptr;
    EditionEvidence evidence;
};

// The top-level "executable_sha256" (and optional "edition") of an installation
// manifest text (format dtvr-recomp-installation or the former dtvr-recomb-installation).
// Throws std::runtime_error for invalid JSON, another format or a missing / malformed hash.
struct ManifestEdition { std::string sha256, edition; };
ManifestEdition parseManifestEdition(const std::string& text);

// The edition of a program directory (the one holding dat/lan.inf): rally.exe in it,
// else `<program>/../../manifest.json` of an installation (<install>/data/program).
// When both exist they must agree. Throws std::runtime_error (German message) when no
// evidence exists, the hash is unknown or the sources contradict each other.
DetectedEdition detectEdition(const std::filesystem::path& programDir);

// The required files of `profile` missing below `programDir`.
std::vector<std::string> missingEditionFiles(const EditionProfile& profile, const std::filesystem::path& programDir);
// Throws std::runtime_error when files are missing or the lan.inf letter does not
// belong to the profile (no mixing of an EXE with another edition's language data).
void validateEditionData(const EditionProfile& profile, const std::filesystem::path& programDir, char languageLetter);

} // namespace dtvr
