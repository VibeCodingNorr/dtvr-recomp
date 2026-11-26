// SPDX-License-Identifier: GPL-3.0-only
// [Setup] Colorcycle as a rule input: seg5:186A (called by the event trigger seg8:1624
// at seg8:1652-1663) returns non-zero only for a new tile directory, which with
// Colorcycle off (DS:B5DC = 0, seg5:1929-1953) is always 1, so the trigger never skips.
#include "game.hpp"
#include "save.hpp"
#include "worldmap.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static Board ring(std::vector<Town>& towns) {
    Board b;
    b.fields.resize(13);
    for (int i = 0; i < 13; ++i) {
        Field& f = b.fields[size_t(i)];
        f.x = uint8_t(i); f.y = 0; f.kind = 1;
        f.next = {Field::kNone, int16_t((i + 1) % 13), Field::kNone, int16_t((i + 12) % 13)};
    }
    b.fields[0].kind = 6;
    b.fields[12].kind = 6;
    towns.resize(2);
    towns[0].x = 0; towns[0].name = "London";
    towns[1].x = 12; towns[1].name = "Rom";
    return b;
}

static void computers(Game& g) {
    for (int p = 0; p < kPlayerSlots; ++p)
        if (g.players()[size_t(p)].status) g.setComputer(p, true);
}

// One player change of the trigger: the month after it and whether the trigger went on
// to the countdown (DS:A108 decremented or drawn anew, never left as it was).
struct Tick { int month; bool countdown; };

// Plays until `months` is reached and records every trigger.
static std::vector<Tick> play(Game& g, int months) {
    std::vector<Tick> ticks;
    for (int guard = 0; guard < 20000 && g.calendar().month < months && g.phase() != Game::Phase::GameOver; ++guard) {
        const EventClock before = g.eventClock();
        g.runComputerTurn();
        const EventClock after = g.eventClock();
        CHECK(after.turns - before.turns <= 1);
        if (after.turns != before.turns) ticks.push_back({g.calendar().month, after.countdown != before.countdown});
    }
    return ticks;
}

static bool newSeason(int month) { return month % 3 == 2; } // 2, 5, 8, 11: map/0, 1, 2, 3

int main() {
    std::vector<Town> towns;
    const Board board = ring(towns);

    // Default on: unchanged by an explicit setColorcycle(true) (same encoded state after
    // a played game); the trigger skips the event at the first change into 2, 5, 8, 11.
    {
        Game a(board, towns, 3, 40, 7, 1, nullptr, nullptr, false);
        Game b(board, towns, 3, 40, 7, 1, nullptr, nullptr, false);
        CHECK(a.colorcycle() && a.loadedSeason() == seasonForMonth(0) && a.loadedSeason() == 3);
        b.setColorcycle(true);
        CHECK(SaveCodec::encode(a) == SaveCodec::encode(b));
        computers(a);
        computers(b);
        const std::vector<Tick> ta = play(a, 12);
        play(b, 12);
        CHECK(SaveCodec::encode(a) == SaveCodec::encode(b));
        CHECK(a.seasonChanges() == 4 && a.loadedSeason() == 3);
        int skipped = 0;
        int lastMonth = 0;
        for (const Tick& t : ta) {
            const bool first = t.month != lastMonth && newSeason(t.month);
            CHECK(t.countdown == !first);
            skipped += !t.countdown;
            lastMonth = t.month;
        }
        CHECK(skipped == 4);
    }

    // Off: no season change, the countdown runs at every player change, also into
    // months 2, 5, 8 and 11; the tile directory stays 1.
    {
        Game g(board, towns, 3, 40, 7, 1, nullptr, nullptr, false);
        g.setColorcycle(false);
        CHECK(!g.colorcycle() && g.loadedSeason() == 1);
        computers(g);
        const std::vector<Tick> ticks = play(g, 12);
        CHECK(g.calendar().month == 12);
        CHECK(g.seasonChanges() == 0 && g.loadedSeason() == 1);
        bool sawSeasonMonth = false;
        for (const Tick& t : ticks) {
            CHECK(t.countdown);
            sawSeasonMonth = sawSeasonMonth || newSeason(t.month);
        }
        CHECK(sawSeasonMonth && ticks.size() > 12);
    }

    // The toggle (OPTION's close, seg1:B539 -> seg5:186A(month, 0)): the directory of
    // the setting is loaded at once, without a wipe, an event or a random draw.
    {
        Game g(board, towns, 3, 40, 7, 1, nullptr, nullptr, false);
        computers(g);
        play(g, 3); // March: map/0
        CHECK(g.calendar().month == 3 && g.loadedSeason() == 0 && g.seasonChanges() == 1);
        const uint32_t rng = g.sessionRandom().state();
        const EventClock clock = g.eventClock();
        g.setColorcycle(false);
        CHECK(g.loadedSeason() == 1 && g.seasonChanges() == 1 && g.sessionRandom().state() == rng);
        CHECK(g.eventClock().countdown == clock.countdown && g.eventClock().turns == clock.turns);
        for (const Tick& t : play(g, 9)) CHECK(t.countdown); // June (5) and September (8) included
        CHECK(g.seasonChanges() == 1 && g.loadedSeason() == 1);
        g.setColorcycle(true); // September: map/2 at once
        CHECK(g.loadedSeason() == 2 && g.seasonChanges() == 1);
        int skipped = 0;
        for (const Tick& t : play(g, 12)) skipped += !t.countdown;
        CHECK(skipped == 1 && g.seasonChanges() == 2 && g.loadedSeason() == 3); // December
    }
    // Switching on in summer (map/1 either way) changes nothing until the next season.
    {
        Game g(board, towns, 3, 40, 7, 1, nullptr, nullptr, false);
        g.setColorcycle(false);
        computers(g);
        play(g, 6);
        CHECK(g.calendar().month == 6 && g.loadedSeason() == 1);
        g.setColorcycle(true);
        CHECK(g.loadedSeason() == 1 && g.seasonChanges() == 0);
        int skipped = 0;
        for (const Tick& t : play(g, 9)) skipped += !t.countdown;
        CHECK(skipped == 1 && g.seasonChanges() == 1 && g.loadedSeason() == 2);
    }

    // The save records the flag (format 9); older formats load as on.
    {
        Game g(board, towns, 3, 40, 7, 1, nullptr, nullptr, false);
        g.setColorcycle(false);
        const std::string text = SaveCodec::encode(g);
        CHECK(text.find("\ncolorcycle 0\n") != std::string::npos);
        Game back(board, towns, 3, 40, 1, 1, nullptr, nullptr, false);
        std::string error;
        CHECK(SaveCodec::decode(back, text, error));
        CHECK(!back.colorcycle() && back.loadedSeason() == 1);
        CHECK(SaveCodec::encode(back) == text);
        back.resumeLoaded();
        CHECK(!back.colorcycle() && back.loadedSeason() == 1);

        // Format 8: no colorcycle record.
        std::string old = text;
        old.replace(old.find(" 9\n"), 3, " 8\n");
        old.erase(old.find("colorcycle 0\n"), std::string("colorcycle 0\n").size());
        Game legacy(board, towns, 3, 40, 1, 1, nullptr, nullptr, false);
        legacy.setColorcycle(false);
        CHECK(SaveCodec::decode(legacy, old, error));
        CHECK(legacy.colorcycle());
        legacy.resumeLoaded();
        CHECK(legacy.loadedSeason() == seasonForMonth(legacy.calendar().month));

        // A format 8 save with the record, a format 9 save without it, a bad value.
        std::string early = text;
        early.replace(early.find(" 9\n"), 3, " 8\n");
        CHECK(!SaveCodec::decode(legacy, early, error));
        std::string missing = text;
        missing.erase(missing.find("colorcycle 0\n"), std::string("colorcycle 0\n").size());
        CHECK(!SaveCodec::decode(legacy, missing, error));
        std::string bad = text;
        bad.replace(bad.find("colorcycle 0"), 12, "colorcycle 2");
        CHECK(!SaveCodec::decode(legacy, bad, error));
        std::string twice = text;
        twice.insert(twice.find("colorcycle 0\n"), "colorcycle 1\n");
        CHECK(!SaveCodec::decode(legacy, twice, error));
        CHECK(legacy.colorcycle()); // unchanged by the failures
    }

    if (failures) return 1;
    std::puts("colorcycle_test: OK");
    return 0;
}
