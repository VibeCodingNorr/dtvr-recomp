// SPDX-License-Identifier: GPL-3.0-only
// Edition profiles (src/edition.hpp): SHA-256, hash lookup, manifest parsing,
// detection from synthetic program directories and per-edition resource selection.
#include "edition.hpp"
#include "layout.hpp"
#include "sha256.hpp"

#include <cctype>
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

const std::string kGerman = "aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83";
const std::string kRetail = "a8374ca9e39d70b4508efefa782b7135c650792be36600289356a51de819225a";
const std::string kLimited = "c8b7dab6574047c6e3a3d4e600dd3f2ed23e90bd8a10de8a1bb891688d0624d9"; // not supported

struct TempRoot {
    fs::path path;
    TempRoot() {
        path = fs::temp_directory_path() / ("dtvr-edition-" + std::to_string(std::random_device{}()));
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

std::string manifest(const std::string& hash, const std::string& extra = "") {
    return "{\n  \"capability\": \"native-linux-prototype\",\n  \"files\": {\"executable_sha256\": \"x\"},\n"
           "  \"executable_sha256\": \"" + hash + "\",\n" + extra + "  \"schema_version\": 2,\n"
           "  \"format\": \"dtvr-recomp-installation\"\n}\n";
}

template <class F> bool throws(F f) {
    try { f(); } catch (const std::exception&) { return true; }
    return false;
}

void sha256Vectors() {
    // FIPS 180-4 examples.
    check(sha256Hex(nullptr, 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256 empty");
    const std::string abc = "abc";
    check(sha256Hex(reinterpret_cast<const uint8_t*>(abc.data()), abc.size())
              == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256 abc");
    const std::string two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"; // 56 bytes: two blocks
    check(sha256Hex(reinterpret_cast<const uint8_t*>(two.data()), two.size())
              == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "sha256 448 bits");
    TempRoot root;
    root.write("a.bin", std::string(1000000, 'a'));
    check(sha256FileHex(root.path / "a.bin") == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
          "sha256 file, a million 'a'");
    check(throws([&] { sha256FileHex(root.path / "missing"); }), "sha256 of a missing file throws");
}

void profiles() {
    check(editionProfiles().size() == 2, "two profiles (German, US retail)");
    const EditionProfile* german = profileForExecutableHash(kGerman);
    check(german && german->edition == Edition::GermanRetail && german->playable && german->languageLetter == 'D',
          "German hash -> German retail, playable");
    std::string upper = kRetail;
    for (char& c : upper) c = char(std::toupper(static_cast<unsigned char>(c)));
    const EditionProfile* retail = profileForExecutableHash(upper);
    check(retail && retail->edition == Edition::EnglishRetail && !retail->playable && retail->languageLetter == 'E',
          "US retail hash (upper case) -> recognised, not playable");
    check(!profileForExecutableHash(kLimited), "the limited US build is not a profile");
    check(!profileForExecutableHash(""), "empty hash unknown");
    check(!profileForExecutableHash(kGerman.substr(1)), "truncated hash unknown");
    check(&editionProfile(Edition::EnglishRetail) == retail, "profile by enum");
}

void manifests() {
    const ManifestEdition m = parseManifestEdition(manifest(kGerman));
    check(m.sha256 == kGerman && m.edition.empty(), "manifest: top-level hash, not the file table's key");
    check(parseManifestEdition(manifest(kRetail, "  \"edition\": \"en-us-retail\",\n")).edition == "en-us-retail",
          "manifest: optional edition field");
    std::string former = manifest(kGerman);
    former.replace(former.find("dtvr-recomp-installation"), 24, "dtvr-recomb-installation");
    check(parseManifestEdition(former).sha256 == kGerman, "manifest of the former name");
    check(throws([] { parseManifestEdition(""); }), "manifest: empty");
    check(throws([] { parseManifestEdition("{\"format\": \"dtvr-recomp-installation\""); }), "manifest: truncated");
    check(throws([] { parseManifestEdition(manifest(kGerman) + "x"); }), "manifest: trailing data");
    check(throws([] { parseManifestEdition("[1]"); }), "manifest: not an object");
    check(throws([] { parseManifestEdition("{\"format\": \"other\", \"executable_sha256\": \"" + kGerman + "\"}"); }),
          "manifest: other format");
    check(throws([] { parseManifestEdition("{\"format\": \"dtvr-recomp-installation\"}"); }), "manifest: no hash");
    check(throws([] { parseManifestEdition(manifest("abc")); }), "manifest: short hash");
    check(throws([] { parseManifestEdition(manifest(std::string(64, 'g'))); }), "manifest: non-hex hash");
    check(throws([] {
              parseManifestEdition("{\"format\": \"dtvr-recomp-installation\", \"executable_sha256\": 5}");
          }), "manifest: hash not a string");
}

void detection() {
    { // installation: <install>/manifest.json, <install>/data/program
        TempRoot root;
        root.write("data/program/dat/lan.inf", "D\r\n");
        root.write("manifest.json", manifest(kGerman));
        const DetectedEdition d = detectEdition(root.path / "data/program/");
        check(d.profile && d.profile->edition == Edition::GermanRetail
                  && d.evidence.source == EditionEvidence::Source::Manifest && d.evidence.sha256 == kGerman,
              "detect: installation manifest");
        root.write("manifest.json", manifest(kRetail));
        check(detectEdition(root.path / "data/program").profile->edition == Edition::EnglishRetail,
              "detect: US retail manifest");
        root.write("manifest.json", manifest(kRetail, "  \"edition\": \"de-retail\",\n"));
        check(throws([&] { detectEdition(root.path / "data/program"); }), "detect: edition field contradicts the hash");
        root.write("manifest.json", manifest(kLimited));
        check(throws([&] { detectEdition(root.path / "data/program"); }), "detect: unsupported hash in the manifest");
        root.write("manifest.json", "{");
        check(throws([&] { detectEdition(root.path / "data/program"); }), "detect: broken manifest");
    }
    { // a manifest is only read in the installation layout (parent directory "data")
        TempRoot root;
        root.write("game/program/dat/lan.inf", "D\r\n");
        root.write("manifest.json", manifest(kGerman));
        check(throws([&] { detectEdition(root.path / "game/program"); }), "detect: no evidence outside an installation");
    }
    { // an unknown rally.exe is rejected even next to a valid manifest
        TempRoot root;
        root.write("data/program/RALLY.EXE", "MZ not the original");
        root.write("manifest.json", manifest(kGerman));
        check(throws([&] { detectEdition(root.path / "data/program"); }), "detect: unknown rally.exe");
    }
}

void resources() {
    const EditionProfile& german = editionProfile(Edition::GermanRetail);
    const EditionProfile& retail = editionProfile(Edition::EnglishRetail);
    TempRoot root;
    for (const auto& file : retail.requiredFiles) {
        std::string upper = file; // the US disc has upper-case names
        for (char& c : upper) c = char(std::toupper(static_cast<unsigned char>(c)));
        root.write(upper, "x");
    }
    check(missingEditionFiles(retail, root.path).empty(), "US retail files found without regard to case");
    check(!throws([&] { validateEditionData(retail, root.path, 'E'); }), "US retail data valid with letter E");
    check(throws([&] { validateEditionData(retail, root.path, 'D'); }), "US retail EXE with German lan.inf refused");
    const auto missing = missingEditionFiles(german, root.path);
    check(missing.size() == 2 && missing[0] == "dat/d/rally_d.dat" && missing[1] == "map/europe_d.tow",
          "German files missing from English data");
    check(throws([&] { validateEditionData(german, root.path, 'D'); }), "incomplete German data refused");
    check(german.overviewBitmap == "butt/1/map.bmp" && retail.overviewBitmap == "butt/6/map.bmp", "overview per profile");

    check(chooseCanvasWidth(german, 1920, 1080, 0) == 1024 && chooseCanvasWidth(german, 800, 600, 0) == 800
              && chooseCanvasWidth(german, 320, 200, 0) == 640 && chooseCanvasWidth(german, 1024, 700, 0) == 800,
          "German canvas: the largest that fits (seg5:1227)");
    check(chooseCanvasWidth(retail, 1920, 1080, 0) == 1024, "US retail canvas: same choice (R seg5:1239)");
    check(chooseCanvasWidth(german, 1920, 1080, 640) == 640, "requested canvas");
    check(throws([&] { chooseCanvasWidth(german, 1920, 1080, 700); }), "unknown canvas refused");
    const ScreenLayout layout = layoutForScreen(retail, 1920, 1080, 800);
    check(layout.edition == Edition::EnglishRetail && layout.artDir == 8 && layout.width == 800, "layout carries the edition");
    check(layoutForScreen(1920, 1080).edition == Edition::GermanRetail, "German default layout");

    check(emptySlotLabel(Edition::GermanRetail, "empty") == "leer", "German empty slot: leer (DS:80D7)");
    check(emptySlotLabel(Edition::EnglishRetail, "empty") == "empty", "US retail empty slot: string 11");
    check(emptySlotLabel(Edition::EnglishRetail, std::string(30, 'x')) == std::string(19, 'x'), "string 11 clipped to 19");
    check(creditsPicture(Edition::GermanRetail, 'D') == "Credits.bmp", "German credits");
    check(creditsPicture(Edition::EnglishRetail, 'E') == "e/credits.bmp", "US retail credits %c\\credits.bmp");

    const SetupGeometry g = setupGeometry(640), r = setupGeometry(640, Edition::EnglishRetail);
    check(g.name[6].y == 235 && g.name[5].y == 154 && g.name[4].y == 74, "German 640 name fields");
    check(r.name[6].y == 236 && r.name[5].y == 155 && r.name[4].y == 73, "US retail 640 name fields (R DS:1CC6)");
    bool same = true;
    for (int s = 0; s < 8; ++s) {
        same = same && g.driver[size_t(s)].y == r.driver[size_t(s)].y && g.driver[size_t(s)].x == r.driver[size_t(s)].x;
        if (s < 4 || s == 7) same = same && g.name[size_t(s)].y == r.name[size_t(s)].y;
    }
    check(same, "US retail buttons and other name fields as German");
    check(setupGeometry(800, Edition::EnglishRetail).name[6].y == setupGeometry(800).name[6].y, "800 table unchanged");
}

} // namespace

int main() {
    sha256Vectors();
    profiles();
    manifests();
    detection();
    resources();
    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
