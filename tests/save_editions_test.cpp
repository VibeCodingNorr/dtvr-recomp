// SPDX-License-Identifier: GPL-3.0-only
// Round E3: save directories per edition, the one-time copy from the former common
// directory, foreign slots and autosaves (Continue grayed), the foreign-save message.
#include "save.hpp"
#include "save_slots.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace dtvr;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// A ring of four fields with one town: enough for the save codec.
static Board ring(std::vector<Town>& towns) {
    Board b;
    b.fields.resize(4);
    for (int i = 0; i < 4; ++i) {
        b.fields[size_t(i)].x = uint8_t(i * 10);
        b.fields[size_t(i)].kind = i == 0 ? 6 : 1;
        b.fields[size_t(i)].next = {Field::kNone, int16_t((i + 1) % 4), Field::kNone, int16_t((i + 3) % 4)};
    }
    towns.resize(1);
    towns[0].name = "London";
    return b;
}

static std::string readAll(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

int main() {
    std::vector<Town> towns;
    const Board board = ring(towns);
    std::string error;
    Game german(board, towns, 1, 0, 3, 1);
    Game english(board, towns, 1, 0, 4, 1);
    english.setEdition(Edition::EnglishRetail);
    const std::string de = SaveCodec::encode(german), en = SaveCodec::encode(english);

    // The edition of a save without decoding it.
    CHECK(SaveCodec::editionOf(de) == "de-retail" && SaveCodec::editionOf(en) == "en-us-retail");
    {
        std::string v7 = de;
        v7.erase(v7.find("\nedition "), 18);
        v7.replace(0, v7.find('\n'), "DTVR-RECOMP-SAVE 7");
        CHECK(SaveCodec::editionOf(v7) == "de-retail");                       // formats 1-7 are German
        CHECK(SaveCodec::editionOf("DTVR-RECOMB-SAVE 3\nrules 1\n") == "de-retail"); // former name
        CHECK(SaveCodec::editionOf("DTVR-RECOMP-SAVE 8\nrules 1\nend\n").empty()); // record missing
        CHECK(SaveCodec::editionOf("DTVR-RECOMP-SAVE 99\nedition de-retail\n").empty());
        CHECK(SaveCodec::editionOf("irgendwas\n").empty() && SaveCodec::editionOf("").empty());
    }

    // Directories: German keeps the former common directory, English has its own sibling.
    const fs::path base = fs::temp_directory_path() / "dtvr-recomp-save-editions-test";
    fs::remove_all(base);
    setenv("XDG_DATA_HOME", base.c_str(), 1);
    CHECK(saveDirectory(Edition::GermanRetail) == defaultSaveDirectory());
    CHECK(defaultSaveDirectory() == base / "dtvr-recomp" / "saves");
    CHECK(saveDirectory(Edition::EnglishRetail) == base / "dtvr-recomp" / "saves-en-us-retail");
    CHECK(saveDirectory(Edition::EnglishRetail) != saveDirectory(Edition::GermanRetail));

    // The former common directory (rounds E2 and before): German slots 0 and 2, English
    // slots 1 and 3, an English autosave.
    const fs::path shared = defaultSaveDirectory(), own = saveDirectory(Edition::EnglishRetail);
    const SaveSlots common(shared, Edition::GermanRetail);
    CHECK(common.save(0, german, "Januar (1) - Anna", error) && common.save(2, german, "Mai (1) - Anna", error));
    CHECK(common.save(1, english, "January (1) - Bob", error) && common.save(3, english, "March (1) - Bob", error));
    CHECK(common.saveAuto(english, error));
    const std::string sharedIndex = readAll(shared / "saves.ini");

    // German: nothing to copy, its directory stays where it was.
    int copied = -1;
    CHECK(adoptEditionSaves(shared, saveDirectory(Edition::GermanRetail), Edition::GermanRetail, copied, error) && copied == 0);
    // Foreign slots and the foreign autosave in the German dialog: marked, Continue grayed.
    CHECK(common.foreignSlot(0).empty() && common.foreignSlot(1) == "en-us-retail");
    CHECK(common.foreignSlot(2).empty() && common.foreignSlot(3) == "en-us-retail");
    CHECK(common.foreignSlot(4).empty() && common.foreignSlot(-1).empty() && common.foreignSlot(9).empty());
    CHECK(common.hasAuto() && common.foreignAuto() == "en-us-retail" && !common.hasOwnAuto());
    CHECK(markForeignSave("January (1) - Bob", "en-us-retail") == "[US] January (1) - Bob");
    CHECK(markForeignSave("Januar (1) - Anna", "de-retail") == "[DE] Januar (1) - Anna");
    CHECK(markForeignSave("x", "fr-retail") == "[?] x" && markForeignSave("leer", "") == "leer");
    {
        Game target(board, towns, 1, 0, 5, 1);
        const std::string before = SaveCodec::encode(target);
        CHECK(!common.load(1, target, error) && !common.loadAuto(target, error));
        CHECK(SaveCodec::encode(target) == before); // a refused load changes nothing
    }

    // English: its slots and the autosave are copied once, with their descriptions.
    CHECK(adoptEditionSaves(shared, own, Edition::EnglishRetail, copied, error) && copied == 3);
    const SaveSlots mine(own, Edition::EnglishRetail);
    CHECK(mine.description(1) == "January (1) - Bob" && mine.description(3) == "March (1) - Bob");
    CHECK(mine.description(0).empty() && mine.description(2).empty());
    CHECK(mine.hasOwnAuto() && mine.foreignAuto().empty() && mine.foreignSlot(1).empty());
    CHECK(readAll(mine.file(1)) == readAll(common.file(1)));
    // Never moved or deleted: the common directory is unchanged.
    CHECK(fs::exists(common.file(1)) && fs::exists(common.file(3)) && fs::exists(common.autoFile()));
    CHECK(readAll(shared / "saves.ini") == sharedIndex);
    {
        Game target(board, towns, 1, 0, 6, 1);
        target.setEdition(Edition::EnglishRetail);
        CHECK(mine.load(3, target, error) && mine.loadAuto(target, error));
    }
    // Only once: an existing directory is never filled again (a deleted save stays deleted).
    fs::remove(mine.file(3));
    CHECK(adoptEditionSaves(shared, own, Edition::EnglishRetail, copied, error) && copied == 0);
    CHECK(!fs::exists(mine.file(3)));
    // Saves of the two editions no longer meet: the German autosave leaves the English one.
    CHECK(common.saveAuto(german, error) && common.hasOwnAuto() && mine.hasOwnAuto());
    CHECK(SaveCodec::editionOf(readAll(mine.autoFile())) == "en-us-retail");

    // A German save copied by hand into the English directory: marked, Continue grayed.
    fs::copy_file(common.autoFile(), mine.autoFile(), fs::copy_options::overwrite_existing);
    CHECK(mine.foreignAuto() == "de-retail" && !mine.hasOwnAuto());
    fs::copy_file(common.file(0), mine.file(5));
    CHECK(mine.foreignSlot(5) == "de-retail");

    // No common directory, or nothing of the edition in it: no directory is created.
    {
        const fs::path empty = base / "elsewhere";
        CHECK(adoptEditionSaves(base / "missing", empty, Edition::EnglishRetail, copied, error) && copied == 0);
        const fs::path germanOnly = base / "german-only";
        CHECK(SaveSlots(germanOnly).save(0, german, "x", error));
        CHECK(adoptEditionSaves(germanOnly, empty, Edition::EnglishRetail, copied, error) && copied == 0);
        CHECK(!fs::exists(empty));
    }

    // The message in the running edition's language.
    const std::string toGerman = foreignSaveMessage(Edition::GermanRetail, "en-us-retail");
    CHECK(toGerman.find("Dieser Spielstand") == 0 && toGerman.find("Madcap") != std::string::npos);
    const std::string toEnglish = foreignSaveMessage(Edition::EnglishRetail, "de-retail");
    CHECK(toEnglish.find("This saved game") == 0 && toEnglish.find("verrückte") != std::string::npos);
    CHECK(foreignSaveMessage(Edition::GermanRetail, "xx").find("unbekannten Ausgabe (xx)") != std::string::npos);

    fs::remove_all(base);
    if (failures) return 1;
    std::puts("save-editions: OK");
    return 0;
}
