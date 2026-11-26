// SPDX-License-Identifier: GPL-3.0-only
// The yellow field's sounds (seg2:110D / 1175) and seg1:01D7: Escape or the right button
// on a computer's turn ends a demo or turns the computer into a human.
#include "game.hpp"

#include <cstdio>
#include <memory>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// A ring of 13 fields ten cells apart, London (kind 6) at 0 and Rom (kind 6) at 12,
// the fields between of `kind`.
static Board ring(int kind, std::vector<Town>& towns) {
    Board b;
    b.fields.resize(13);
    for (int i = 0; i < 13; ++i) {
        Field& f = b.fields[size_t(i)];
        f.x = uint8_t(i * 10);
        f.kind = uint16_t(kind);
        f.next = {Field::kNone, int16_t((i + 1) % 13), Field::kNone, int16_t((i + 12) % 13)};
    }
    b.fields[0].kind = 6;
    b.fields[12].kind = 6;
    towns.resize(2);
    towns[0].x = 0; towns[0].name = "London";
    towns[1].x = 120; towns[1].name = "Rom";
    return b;
}

static void driveRight(Game& g) {
    g.roll();
    while (g.phase() == Game::Phase::Move) g.step(Direction::Right);
}

static bool hasCue(const std::vector<SoundCue>& cues, int channel, int sound) {
    for (const SoundCue& c : cues)
        if (c.channel == channel && c.sound == sound) return true;
    return false;
}

static void yellowSounds() {
    std::vector<Town> towns;
    const Board yb = ring(3, towns);
    { // seg2:1127-117B: random(13) for the text, Hebel05 on channel 2, then the message.
        Game g(yb, towns, 1, 0, 2, 1, nullptr, nullptr, false);
        g.setRandomEvents(false);
        g.takeSounds();
        g.takeNotices();
        driveRight(g);
        CHECK(g.phase() == Game::Phase::Arrived);
        const auto notices = g.takeNotices();
        const auto cues = g.takeSounds();
        int seq = -1;
        for (const Notice& n : notices)
            if (n.text >= 4201 && n.text <= 4213) seq = n.seq;
        CHECK(seq >= 0);
        bool found = false;
        for (const SoundCue& c : cues)
            if (c.channel == 2 && c.sound == 19) found = c.afterNotice == seq; // with the message's opening
        CHECK(found);
        CHECK(!hasCue(cues, 3, 15));
    }
    { // seg2:110D-1113: a full hand sounds Sfx08 on channel 3 before seg3:1057 (text 403).
        Game g(yb, towns, 1, 0, 5, 8, nullptr, nullptr, false);
        g.setRandomEvents(false);
        g.takeSounds();
        g.takeNotices();
        CHECK(g.handCount(0) == 8);
        driveRight(g);
        CHECK(g.yellowCardPending());
        auto cues = g.takeSounds();
        const auto notices = g.takeNotices();
        CHECK(!notices.empty() && notices.back().text == 403);
        bool found = false;
        for (const SoundCue& c : cues)
            if (c.channel == 3 && c.sound == 15) found = !notices.empty() && c.afterNotice == notices.back().seq;
        CHECK(found);
        CHECK(!hasCue(cues, 2, 19)); // the card's sound follows the discard
        CHECK(g.discardForCard(0));
        cues = g.takeSounds();
        CHECK(hasCue(cues, 2, 19) && !hasCue(cues, 3, 15));
    }
}

static void takeOver() {
    std::vector<Town> towns;
    const Board board = ring(4, towns);
    const std::array<uint8_t, kPlayerSlots> slots{1, 2, 0, 0, 0, 0, 0, 0};
    { // A human's turn: seg1:00D2 drops nothing, seg1:01D7 is never reached.
        Game g(board, towns, slots, 0, 7);
        g.setRandomEvents(false);
        CHECK(g.currentPlayer() == 0 && g.computerEscapeAction() == Game::EscapeAction::None);
        CHECK(!g.takeOverComputer());
    }
    { // At the start of a computer's turn: status 1, no random draw, the human goes on.
        Game g(board, towns, slots, 0, 7);
        g.setRandomEvents(false);
        driveRight(g);
        CHECK(g.endTurn() && g.currentPlayer() == 1 && g.isComputer(1));
        CHECK(g.computerEscapeAction() == Game::EscapeAction::TakeOver);
        const uint32_t before = g.sessionRandom().state();
        CHECK(g.takeOverComputer());
        CHECK(g.sessionRandom().state() == before);
        CHECK(!g.isComputer(1) && g.players()[1].status == 1 && g.humansLeft());
        CHECK(!g.computerControls() && !g.computerAdvance() && !g.computerTurnRunning());
        CHECK(g.computerEscapeAction() == Game::EscapeAction::None && !g.takeOverComputer());
        driveRight(g); // the new human spins and drives himself
        CHECK(g.phase() == Game::Phase::Arrived && g.currentPlayer() == 1);
    }
    { // In the middle of a computer's move: the state machine stops (seg5:6CE8), the
      // remaining steps belong to the human; same draws as without the takeover so far.
        Game g(board, towns, slots, 0, 11);
        Game ref(board, towns, slots, 0, 11);
        for (Game* x : {&g, &ref}) {
            x->setRandomEvents(false);
            driveRight(*x);
            x->endTurn();
        }
        CHECK(g.computerAdvance() && ref.computerAdvance()); // card choice / spin, route
        CHECK(g.phase() == Game::Phase::Move && g.computerTurnRunning());
        const int field = g.playerField(1);
        CHECK(g.takeOverComputer());
        CHECK(g.sessionRandom().state() == ref.sessionRandom().state());
        CHECK(!g.computerTurnRunning() && !g.computerAdvance() && g.playerField(1) == field);
        CHECK(g.phase() == Game::Phase::Move && g.step(Direction::Right) && g.playerField(1) != field);
    }
    { // Demo (DS:14 = 1) without humans: the end of the demo, no takeover.
        Game g(board, towns, std::array<uint8_t, kPlayerSlots>{2, 2, 0, 0, 0, 0, 0, 0}, 50, 9);
        g.setRandomEvents(false);
        g.setDemo(true);
        CHECK(g.computerEscapeAction() == Game::EscapeAction::DemoEnd && !g.takeOverComputer());
        CHECK(g.requestDemoEnd() && g.demoEnding());
        // A game of computers outside the demo (DS:14 = 0): seg1:0241 lets the takeover through.
        g.setDemo(false);
        CHECK(g.computerEscapeAction() == Game::EscapeAction::TakeOver);
        CHECK(g.takeOverComputer() && g.humansLeft() && !g.isComputer(g.currentPlayer()));
    }
}

// Steuerlos (card 13) on the computer: DS:10 holds a status, seg1:024A refuses.
static void steerless() {
    std::vector<Town> towns;
    const Board board = ring(4, towns);
    const std::array<uint8_t, kPlayerSlots> slots{1, 2, 0, 0, 0, 0, 0, 0};
    for (uint16_t seed = 1; seed < 3000; ++seed) {
        Game g(board, towns, slots, 0, seed, 8);
        g.setRandomEvents(false);
        int slot = -1;
        for (int s = 0; s < 8; ++s)
            if (g.deck().typeOfSlot(g.hand(0)[size_t(s)]) == 13) slot = s;
        if (slot < 0) continue;
        if (!g.playCard(slot, 1)) continue;
        if (g.phase() == Game::Phase::Roll && g.currentPlayer() == 0) driveRight(g);
        while (g.phase() == Game::Phase::Move) g.step(Direction::Right);
        if (g.phase() != Game::Phase::Arrived || !g.endTurn() || g.currentPlayer() != 1) continue;
        CHECK(g.players()[1].flags & kFlagSteerless);
        CHECK(g.computerEscapeAction() == Game::EscapeAction::None && !g.takeOverComputer());
        CHECK(g.isComputer(1));
        return;
    }
    CHECK(false); // no seed with a Steuerlos card in the starting hand
}

int main() {
    yellowSounds();
    takeOver();
    steerless();
    if (!failures) std::puts("takeover-test: OK");
    return failures ? 1 : 0;
}
