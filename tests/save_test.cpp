// SPDX-License-Identifier: GPL-3.0-only
// Save format: exact round trip, identical continuation, rejected input.
#include "save.hpp"
#include "save_slots.hpp"
#include "settings.hpp"

#include <cstdio>
#include <fstream>
#include <filesystem>
#include <map>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// Ring of 13 fields: London and Rom (kind 6), towns with one property, yellow and blue fields.
static Board ring(std::vector<Town>& towns) {
    Board b;
    b.fields.resize(13);
    for (int i = 0; i < 13; ++i) {
        Field& f = b.fields[size_t(i)];
        f.x = uint8_t(i * 10);
        f.kind = i % 3 == 0 ? 4 : i % 3 == 1 ? 3 : 1;
        f.next = {Field::kNone, int16_t((i + 1) % 13), Field::kNone, int16_t((i + 12) % 13)};
    }
    b.fields[0].kind = 6;
    b.fields[12].kind = 6;
    towns.resize(2);
    towns[0].x = 0; towns[0].name = "London";
    towns[1].x = 120; towns[1].name = "Rom";
    for (int i = 3; i < 12; i += 3) {
        Town t;
        t.x = uint16_t(i * 10);
        t.name = "Stadt";
        t.entries[0].name = "Haus";
        t.entries[0].value = 10000;
        t.entries[0].words = {4, 1, 0};
        towns.push_back(t);
    }
    return b;
}

static void playTurns(Game& g, int turns) {
    for (int i = 0; i < turns && g.phase() != Game::Phase::GameOver; ++i) g.runComputerTurn();
}

int main() {
    std::vector<Town> towns;
    const Board board = ring(towns);

    Game a(board, towns, 3, 2, 11, 3);
    for (int p = 0; p < 3; ++p) a.setComputer(p, true);
    playTurns(a, 25);
    const std::string saved = SaveCodec::encode(a);
    CHECK(saved.rfind("DTVR-RECOMP-SAVE 5\n", 0) == 0 && saved.find("\nend\n") != std::string::npos);

    // Load into a game started differently: the state, and every later turn, match.
    Game b(board, towns, 1, 0, 999, 1);
    std::string error;
    CHECK(SaveCodec::decode(b, saved, error));
    CHECK(SaveCodec::encode(b) == saved);
    playTurns(a, 40);
    playTurns(b, 40);
    CHECK(SaveCodec::encode(a) == SaveCodec::encode(b));
    CHECK(a.phase() == b.phase() && a.currentPlayer() == b.currentPlayer() && a.cash(0) == b.cash(0));

    // A save in the middle of a move keeps the steps (undo still works).
    Game m(board, towns, 2, 1, 5, 1);
    CHECK(m.roll());
    CHECK(m.step(Direction::Right));
    const std::string moving = SaveCodec::encode(m);
    Game n(board, towns, 1, 0, 1, 1);
    CHECK(SaveCodec::decode(n, moving, error) && n.stepsTaken() == 1 && n.phase() == Game::Phase::Move);
    CHECK(n.step(Direction::Left) && n.stepsTaken() == 0);

    // Format 1 (round 18, no Risiko record) still loads.
    {
        std::string v1 = saved;
        v1.replace(0, 18, "DTVR-RECOMP-SAVE 1");
        const size_t r = v1.find("\nrisiko ");
        v1.erase(r, v1.find('\n', r + 1) - r);
        const size_t d = v1.find("\ndemo ");
        v1.erase(d, v1.find('\n', d + 1) - d);
        Game c(board, towns, 1, 0, 3, 1);
        CHECK(SaveCodec::decode(c, v1, error) && SaveCodec::encode(c) == saved);
    }

    // Format 3 (round 26, no stages record): the total is taken from the stages left.
    {
        Game staged(board, towns, 1, 5, 3, 1);
        std::string v3 = SaveCodec::encode(staged);
        CHECK(v3.find("\nstages 5\n") != std::string::npos);
        v3.replace(0, 18, "DTVR-RECOMP-SAVE 3");
        const size_t st = v3.find("\nstages ");
        v3.erase(st, v3.find('\n', st + 1) - st);
        Game c(board, towns, 1, 9, 4, 1);
        CHECK(SaveCodec::decode(c, v3, error) && c.totalStages() == c.remainingStages());
        std::string missing = SaveCodec::encode(staged);
        const size_t ms = missing.find("\nstages ");
        missing.erase(ms, missing.find('\n', ms + 1) - ms);
        CHECK(!SaveCodec::decode(c, missing, error)); // format 4 needs it
    }

    // Format 5 (round 98): the recomp extension record; format 4 saves load with it off.
    {
        Game ext(board, towns, 1, 2, 5, 1);
        ext.setComputerExtraCards(true);
        const std::string v5 = SaveCodec::encode(ext);
        CHECK(v5.find("\nextensions 1\n") != std::string::npos);
        Game d(board, towns, 1, 0, 6, 1);
        CHECK(SaveCodec::decode(d, v5, error) && d.computerExtraCards());
        std::string v4 = v5;
        v4.replace(0, 18, "DTVR-RECOMP-SAVE 4");
        const size_t es = v4.find("\nextensions ");
        v4.erase(es, v4.find('\n', es + 1) - es);
        CHECK(SaveCodec::decode(d, v4, error) && !d.computerExtraCards());
        // Saves of the project's former name (DTVR Recomb) still load; other magics do not.
        std::string legacy = v5;
        legacy.replace(0, 16, "DTVR-RECOMB-SAVE");
        CHECK(SaveCodec::decode(d, legacy, error) && d.computerExtraCards());
        legacy.replace(0, 16, "DTVR-RECOMX-SAVE");
        CHECK(!SaveCodec::decode(d, legacy, error));
        std::string missing = v5;
        const size_t ms = missing.find("\nextensions ");
        missing.erase(ms, missing.find('\n', ms + 1) - ms);
        CHECK(!SaveCodec::decode(d, missing, error)); // format 5 needs it
    }

    // Rejected input leaves the game unchanged.
    const std::string before = SaveCodec::encode(b);
    auto rejects = [&](const std::string& text) {
        std::string why;
        const bool ok = SaveCodec::decode(b, text, why);
        return !ok && !why.empty() && SaveCodec::encode(b) == before;
    };
    CHECK(rejects(""));
    CHECK(rejects("SOMETHING ELSE 1\n"));
    CHECK(rejects("DTVR-RECOMP-SAVE 4\nend\n"));
    CHECK(rejects(saved.substr(0, saved.size() / 2)));          // truncated
    std::string bad = saved;
    bad.replace(bad.find("rng "), 4, "rnx ");
    CHECK(rejects(bad));                                         // unknown key
    bad = saved;
    const size_t deck = bad.find("\ndeck ");
    bad.insert(bad.find('\n', deck + 1), " 7");
    CHECK(rejects(bad));                                         // extra value
    bad = saved;
    const size_t turn = bad.find("\nturn ") + 6;
    bad.replace(turn, 1, "9");
    CHECK(rejects(bad));                                         // phase out of range
    std::vector<Town> otherTowns = towns;
    otherTowns[2].entries[0].value = 20000;
    Game other(board, otherTowns, 1, 0, 1, 1);
    std::string why;
    CHECK(!SaveCodec::decode(other, saved, why) && why.find("Originaldaten") != std::string::npos);

    // Files are written atomically and read back unchanged.
    const auto dir = std::filesystem::temp_directory_path() / "dtvr-recomp-save-test";
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "test.sav").string();
    std::string text;
    CHECK(writeSaveFile(path, saved, error) && readSaveFile(path, text, error) && text == saved);
    CHECK(!std::filesystem::exists(path + ".tmp"));
    CHECK(!readSaveFile((dir / "missing.sav").string(), text, error));

    // Save slots (round 33): description as seg8:B413, slot files and the index.
    const std::map<int, std::string> texts{{301, "Januar"}, {302, "Februar"}};
    const std::string description = saveDescription(a, texts);
    CHECK(description.find(" (") != std::string::npos && description.find(a.playerName(0)) != std::string::npos);
    const SaveSlots slots(dir / "slots");
    CHECK(slots.description(3).empty());
    CHECK(slots.save(3, a, description, error) && slots.description(3) == description);
    CHECK(slots.save(5, b, "zweiter\nStand", error) && slots.description(5) == "zweiter Stand");
    CHECK(slots.description(3) == description && slots.description(4).empty());
    Game c(board, towns, 1, 0, 7, 1);
    CHECK(slots.load(3, c, error) && SaveCodec::encode(c) == SaveCodec::encode(a));
    CHECK(!slots.load(4, c, error) && !slots.save(9, a, "x", error));
    CHECK(!slots.hasAuto() && slots.saveAuto(b, error) && slots.hasAuto());
    CHECK(slots.loadAuto(c, error) && SaveCodec::encode(c) == SaveCodec::encode(b));

    // Options (round 39): defaults 1, Subtitle 0; joystick has three states.
    Settings settings;
    CHECK(settings.value[Settings::Views] == 1 && settings.subtitle == 0);
    settings.toggle(Settings::Views);
    settings.toggle(Settings::Joystick);
    settings.toggle(Settings::Joystick);
    CHECK(settings.value[Settings::Views] == 0 && settings.value[Settings::Joystick] == 0);
    settings.toggle(Settings::Joystick);
    CHECK(settings.value[Settings::Joystick] == 1); // 1 -> 2 -> 0 -> 1
    const auto settingsFile = dir / "config" / "settings.ini";
    CHECK(saveSettings(settingsFile, settings, error));
    const Settings back = loadSettings(settingsFile);
    CHECK(back.value == settings.value && back.subtitle == 0 && back.pause == 0 && back.showIntro == 1);
    CHECK(Settings::decode("[Setup]\nShowIntro=0\n").showIntro == 0);
    // [Default] Rounds (round 108, window TEMP): default 99, kept through encode / decode.
    {
        Settings r;
        CHECK(r.rounds == 99);
        r.rounds = 250;
        CHECK(Settings::decode(r.encode()).rounds == 250 && Settings::decode("[Default]\nRounds=12\n").rounds == 12);
        // [Display] TextSmoothing (round 110): 0..2, default and bad values 1.
        CHECK(r.textSmoothing == 1 && Settings::decode("TextSmoothing=2\n").textSmoothing == 2);
        CHECK(Settings::decode("TextSmoothing=0\n").textSmoothing == 0 && Settings::decode("TextSmoothing=5\n").textSmoothing == 1);
        r.textSmoothing = 0;
        CHECK(Settings::decode(r.encode()).textSmoothing == 0);
    }
    CHECK(Settings::decode("Pause=3\n").pause == 3 && Settings::decode("Pause=9\n").pause == 0);
    CHECK(Settings::decode("Views=7\nMidi=x\nWave=0\n").value[Settings::Views] == 1);
    CHECK(Settings::decode("Wave=0\n").value[Settings::Wave] == 0 && loadSettings(dir / "none.ini").value[0] == 1);
    // [Display] (round 63): window by default, whole-number scaling on.
    CHECK(!back.fullscreen && back.integerScale && !back.widescreen); // widescreen is optional, off
    CHECK(back.animationTempo == 1 && back.computerTempo == 1 && Settings::decode("AnimationTempo=4\n").animationTempo == 2
          && Settings::decode("ComputerTempo=4\n").computerTempo == 4 && Settings::decode("ComputerTempo=3\n").computerTempo == 1
          && Settings::decode("AnimationTempo=3\n").animationTempo == 1 && Settings::decode("AnimationTempo=2\n").animationTempo == 2);
    settings.fullscreen = true;
    settings.integerScale = false;
    settings.widescreen = true;
    settings.animationTempo = 2;
    settings.computerTempo = 4;
    CHECK(saveSettings(settingsFile, settings, error));
    const Settings shown = loadSettings(settingsFile);
    CHECK(shown.fullscreen && !shown.integerScale && shown.widescreen && shown.animationTempo == 2 && shown.computerTempo == 4 && shown.value == settings.value);
    // The former name's user files are copied once into the new folders (round 126).
    {
        namespace fs = std::filesystem;
        const fs::path base = dir / "xdg";
        fs::create_directories(base / "dtvr-recomb" / "saves");
        std::ofstream(base / "dtvr-recomb" / "saves" / "slot1.sav") << "alt";
        std::ofstream(base / "dtvr-recomb" / "settings.ini") << "Wave=0\n";
        std::string err;
        CHECK(adoptFormerName(base / "dtvr-recomp" / "saves", err) && fs::exists(base / "dtvr-recomp" / "saves" / "slot1.sav"));
        CHECK(fs::exists(base / "dtvr-recomb" / "saves" / "slot1.sav")); // copied, not moved
        CHECK(adoptFormerName(base / "dtvr-recomp" / "settings.ini", err)
              && loadSettings(base / "dtvr-recomp" / "settings.ini").value[Settings::Wave] == 0);
        std::ofstream(base / "dtvr-recomb" / "settings.ini") << "Wave=1\nMidi=0\n";
        CHECK(adoptFormerName(base / "dtvr-recomp" / "settings.ini", err) // the new file is kept
              && loadSettings(base / "dtvr-recomp" / "settings.ini").value[Settings::Midi] == 1);
        CHECK(adoptFormerName(base / "dtvr-recomp" / "music.ini", err) && !fs::exists(base / "dtvr-recomp" / "music.ini"));
        CHECK(adoptFormerName(base / "other" / "x.ini", err));
    }
    std::filesystem::remove_all(dir);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("save_test: OK");
    return failures != 0;
}
