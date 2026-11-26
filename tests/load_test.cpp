// SPDX-License-Identifier: GPL-3.0-only
// Loading as rally.exe's loader (Game::resumeLoaded, rules revision 49): seg8:AF67-B06A
// and the turn start seg2:0407 (docs/formats.md, "Loading a saved game").
#include "save.hpp"
#include "save_slots.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <sstream>
#include <utility>

using namespace dtvr;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// Ring of 13 fields linked right / left only: the start facing seg2:5067 is 2 (right)
// on every field. Six kind 6 towns: chooseDestination compares the drawn candidate
// index with the players' fields, so fewer of them can leave no destination on a small
// ring. Three kind 4 towns with one property.
static Board ring(std::vector<Town>& towns) {
    Board b;
    b.fields.resize(13);
    for (int i = 0; i < 13; ++i) {
        Field& f = b.fields[size_t(i)];
        f.x = uint8_t(i * 10);
        f.kind = i % 3 == 0 ? 4 : i % 3 == 1 ? 3 : 1;
        f.next = {Field::kNone, int16_t((i + 1) % 13), Field::kNone, int16_t((i + 12) % 13)};
    }
    const std::array<std::pair<int, const char*>, 6> destinations = {
        {{0, "London"}, {1, "Paris"}, {4, "Wien"}, {7, "Prag"}, {10, "Bern"}, {12, "Rom"}}};
    for (const auto& [field, name] : destinations) {
        b.fields[size_t(field)].kind = 6;
        Town t;
        t.x = uint16_t(field * 10);
        t.name = name;
        towns.push_back(t);
    }
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

// Replaces the record `key` (first word of a line, plus the slot for per-slot records).
static std::string withRecord(const std::string& save, const std::string& key, const std::string& line) {
    std::istringstream in(save);
    std::string l, out;
    while (std::getline(in, l)) out += (l.rfind(key + ' ', 0) == 0 ? line : l) + '\n';
    return out;
}

static std::string record(const std::string& save, const std::string& key) {
    std::istringstream in(save);
    std::string l;
    while (std::getline(in, l))
        if (l.rfind(key + ' ', 0) == 0) return l;
    return {};
}

static std::string hexDouble(double v) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%a", v);
    return buffer;
}

// Loads `text` the way SaveSlots does into a game of another session (seed 7).
struct Loaded {
    Game game;
    MsRandom before; // the session generator after decode, before resumeLoaded
    int serialBefore = 0;
    Loaded(const Board& board, const std::vector<Town>& towns, const std::string& text)
        : game(board, towns, 1, 0, 7, 1) {
        std::string error;
        CHECK(SaveCodec::decode(game, text, error, SaveCodec::Random::Session));
        before = game.sessionRandom();
        serialBefore = game.turnSerial();
        game.resumeLoaded();
    }
};

static void compactHandRule() {
    // seg2:3CF1: k = 0..6, a gap at k pulls k + 1..7 down once; k is not looked at again.
    Hand h{255, 255, 3, 4, 255, 5, 255, 255};
    compactHand(h);
    CHECK((h == Hand{255, 3, 4, 5, 255, 255, 255, 255}));
    Hand full{0, 1, 2, 3, 4, 5, 6, 7};
    compactHand(full);
    CHECK((full == Hand{0, 1, 2, 3, 4, 5, 6, 7}));
    Hand one{1, 255, 2, 255, 255, 255, 255, 9};
    compactHand(one);
    CHECK((one == Hand{1, 2, 255, 255, 9, 255, 255, 255}));
    Hand empty = emptyHand();
    compactHand(empty);
    CHECK(empty == emptyHand());
}

int main() {
    compactHandRule();
    std::vector<Town> towns;
    const Board board = ring(towns);

    Game a(board, towns, 3, 10, 11, 3);
    for (int p = 0; p < 3; ++p) a.setComputer(p, true);
    for (int i = 0; i < 7 && a.phase() != Game::Phase::GameOver; ++i) a.runComputerTurn();
    CHECK(a.phase() == Game::Phase::Roll);
    const int cur = a.currentPlayer();
    const int other = (cur + 1) % 3;
    const std::string base = withRecord(SaveCodec::encode(a), "drago", "drago -1 0 5");

    // seg8:AFA7-B029 -> seg2:5067: every slot in play takes the first linked direction
    // (right on the ring); empty slots keep the file's facing.
    {
        Loaded l(board, towns, withRecord(base, "facing", "facing 4 4 4 4 4 4 4 4"));
        for (int p = 0; p < kPlayerSlots; ++p) CHECK(l.game.facing(p) == (p < 3 ? 2 : 4));
        for (int p = 0; p < 3; ++p) CHECK(l.game.positionJumps(p) == 1);
        CHECK(l.game.turnSerial() == l.serialBefore + 1);
        CHECK(l.game.phase() == Game::Phase::Roll && l.game.currentPlayer() == cur);
    }

    // seg8:B05D: not held -> no draw at all; the file's countdown stays.
    {
        Loaded l(board, towns, base);
        CHECK(l.game.sessionRandom().state() == l.before.state());
        CHECK(l.game.drago().holder == -1 && l.game.drago().countdown == 5);
    }
    // Held by the current player: seg2:9C97 -> B477 draws random(3) + 4 once, then the turn
    // start seg2:B3A4 counts down once (no second draw: 4..7 - 1 never reaches 0).
    {
        const std::string text = withRecord(base, "drago", "drago " + std::to_string(cur) + " 1 1");
        Loaded l(board, towns, text);
        MsRandom expect = l.before;
        const int countdown = expect.random(3) + 4;
        CHECK(l.game.sessionRandom().state() == expect.state());
        CHECK(l.game.drago().holder == cur && l.game.drago().form == 1);
        CHECK(l.game.drago().countdown == countdown - 1);
        CHECK(l.game.dragoPlacements() == 1 && l.game.dragoFormChanges() == 0);
    }
    // Held by another player: the draw, but no countdown at the current player's turn start.
    {
        Loaded l(board, towns, withRecord(base, "drago", "drago " + std::to_string(other) + " 2 1"));
        MsRandom expect = l.before;
        const int countdown = expect.random(3) + 4;
        CHECK(l.game.sessionRandom().state() == expect.state());
        CHECK(l.game.drago().holder == other && l.game.drago().form == 2 && l.game.drago().countdown == countdown);
    }

    // seg2:0473 -> seg2:3CF1: the current player's hand order at the turn start; the other
    // hands are left as they are.
    {
        std::istringstream words(record(base, "hand " + std::to_string(cur)).substr(7));
        std::vector<std::string> cards;
        for (std::string w; words >> w;) if (w != "255") cards.push_back(w);
        CHECK(cards.size() >= 2);
        std::string gapped = "hand " + std::to_string(cur) + " 255 " + cards[0] + " 255 " + cards[1];
        for (int k = 4; k < 8; ++k) gapped += " 255";
        const std::string otherGap = "hand 5 255 " + cards[0] + " 255 255 255 255 255 255";
        std::string text = withRecord(base, "hand " + std::to_string(cur), gapped);
        Loaded l(board, towns, withRecord(text, "hand 5", otherGap));
        const Hand& h = l.game.hand(cur);
        CHECK(std::to_string(h[0]) == cards[0] && std::to_string(h[1]) == cards[1] && h[2] == Deck::kTaken);
        CHECK(l.game.hand(5)[0] == Deck::kTaken && std::to_string(l.game.hand(5)[1]) == cards[0]);
    }

    // seg4:066F: DS:16 is cleared before loading; seg8:69F0(1): DS:A113 of all towns;
    // seg5:186A(DS:A100, 1): the month's season.
    {
        std::istringstream state(record(base, "state").substr(6));
        std::vector<std::string> v;
        for (std::string w; state >> w;) v.push_back(w);
        CHECK(v.size() == 9);
        v[5] = "1"; // East open
        v[7] = "3"; // a stale season
        std::string line = "state";
        for (const std::string& w : v) line += ' ' + w;
        Loaded l(board, towns, withRecord(withRecord(base, "state", line), "demo", "demo 1"));
        CHECK(!l.game.demoEnding());
        const std::string saved = SaveCodec::encode(l.game);
        std::istringstream after(record(saved, "state").substr(6));
        std::vector<std::string> w;
        for (std::string x; after >> x;) w.push_back(x);
        CHECK(w.size() == 9 && w[5] == "1" && w[8] == hexDouble(westPropertyValue(board, towns, 0)));
        CHECK(w[7] == std::to_string(seasonForMonth(l.game.calendar().month)));
    }

    // A save outside a turn start (the rebuild's own menu, a move): the loader's steps
    // only; the turn start is not repeated (no countdown, the hand keeps its gaps).
    {
        Game m(board, towns, 2, 1, 5, 1);
        CHECK(m.roll() && m.step(Direction::Right) && m.phase() == Game::Phase::Move);
        const int mover = m.currentPlayer();
        std::string text = withRecord(SaveCodec::encode(m), "drago", "drago " + std::to_string(mover) + " 0 2");
        std::istringstream words(record(text, "hand " + std::to_string(mover)).substr(7));
        std::string first;
        words >> first;
        text = withRecord(text, "hand " + std::to_string(mover),
                          "hand " + std::to_string(mover) + " 255 " + first + " 255 255 255 255 255 255");
        Loaded l(board, towns, text);
        MsRandom expect = l.before;
        CHECK(l.game.drago().countdown == expect.random(3) + 4);
        CHECK(l.game.sessionRandom().state() == expect.state());
        CHECK(l.game.phase() == Game::Phase::Move && l.game.stepsTaken() == 1);
        CHECK(l.game.hand(mover)[0] == Deck::kTaken && std::to_string(l.game.hand(mover)[1]) == first);
    }

    // Headless games of computers that are saved to a slot, loaded into another session and
    // continued, again and again: every game still ends and the loaded state stays valid.
    const fs::path dir = fs::temp_directory_path()
                         / ("dtvr-load-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const SaveSlots slots(dir);
    int loads = 0, ended = 0;
    for (uint16_t seed = 1; seed <= 12; ++seed) {
        auto g = std::make_unique<Game>(board, towns, 4, 8, seed, 3);
        for (int p = 0; p < 4; ++p) g->setComputer(p, true);
        std::string error;
        for (int turn = 0; turn < 4000 && g->phase() != Game::Phase::GameOver; ++turn) {
            if (turn % 7 == 3 && g->phase() == Game::Phase::Roll) {
                CHECK(slots.save(int(seed % kSaveSlots), *g, "x", error));
                auto next = std::make_unique<Game>(board, towns, 1, 0, uint16_t(seed * 31 + turn), 1);
                CHECK(slots.load(int(seed % kSaveSlots), *next, error));
                ++loads;
                // The loaded state is a valid save again (round trip of the codec).
                Game copy(board, towns, 1, 0, 1, 1);
                CHECK(SaveCodec::decode(copy, SaveCodec::encode(*next), error)
                      && SaveCodec::encode(copy) == SaveCodec::encode(*next));
                for (int p = 0; p < kPlayerSlots; ++p) CHECK(next->handCount(p) <= 8);
                const DragoState& d = next->drago();
                CHECK(d.holder < 0 || (d.countdown >= 3 && d.countdown <= 7));
                g = std::move(next);
            }
            g->runComputerTurn();
        }
        ended += g->phase() == Game::Phase::GameOver;
        if (g->phase() != Game::Phase::GameOver)
            std::fprintf(stderr, "seed %d: phase %d player %d stages %d month %d dest %d\n", seed, int(g->phase()), g->currentPlayer(), g->remainingStages(), g->calendar().month, g->destination());
    }
    CHECK(loads > 50);
    CHECK(ended == 12);
    std::error_code ec;
    fs::remove_all(dir, ec);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    return failures ? 1 : 0;
}
