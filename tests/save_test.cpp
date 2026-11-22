// SPDX-License-Identifier: GPL-3.0-only
// Save format: exact round trip, identical continuation, rejected input.
#include "save.hpp"

#include <cstdio>
#include <filesystem>

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
    CHECK(saved.rfind("DTVR-RECOMB-SAVE 1\n", 0) == 0 && saved.find("\nend\n") != std::string::npos);

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

    // Rejected input leaves the game unchanged.
    const std::string before = SaveCodec::encode(b);
    auto rejects = [&](const std::string& text) {
        std::string why;
        const bool ok = SaveCodec::decode(b, text, why);
        return !ok && !why.empty() && SaveCodec::encode(b) == before;
    };
    CHECK(rejects(""));
    CHECK(rejects("SOMETHING ELSE 1\n"));
    CHECK(rejects("DTVR-RECOMB-SAVE 2\nend\n"));
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
    const auto dir = std::filesystem::temp_directory_path() / "dtvr-recomb-save-test";
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "test.sav").string();
    std::string text;
    CHECK(writeSaveFile(path, saved, error) && readSaveFile(path, text, error) && text == saved);
    CHECK(!std::filesystem::exists(path + ".tmp"));
    CHECK(!readSaveFile((dir / "missing.sav").string(), text, error));
    std::filesystem::remove_all(dir);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("save_test: OK");
    return failures != 0;
}
