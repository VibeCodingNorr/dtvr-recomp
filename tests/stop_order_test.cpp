// SPDX-License-Identifier: GPL-3.0-only
// The order of the windows at a stop (src/stop_order.hpp, Game's field follow-ups):
// finish, reward message, town dialog, announcement, Dr. Drago, next player
// (seg2:11A6 ff., docs/formats.md "Order of the windows at a stop").
#include "game.hpp"
#include "save.hpp"
#include "save_slots.hpp"
#include "stop_order.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

namespace {

// The fork of stage_drago_test: London (field 0) -- right -- field 1, and a one-way road
// from London down to field 2. All three are towns (kind 6) with entries worth 1000.
Board towns3(std::vector<Town>& towns) {
    Board b;
    b.fields.resize(3);
    for (int i = 0; i < 3; ++i) {
        b.fields[size_t(i)].kind = 6;
        b.fields[size_t(i)].x = uint8_t(i * 4);
    }
    b.fields[0].next = {Field::kNone, 1, 2, Field::kNone};
    b.fields[1].next[3] = 0;
    towns.resize(3);
    towns[0].name = "London";
    for (int i = 0; i < 3; ++i) {
        towns[size_t(i)].x = b.fields[size_t(i)].x;
        if (i) towns[size_t(i)].name = "T" + std::to_string(i);
        for (auto& e : towns[size_t(i)].entries) { e.name = "Kiosk"; e.value = 1000; e.words[0] = 5; }
    }
    return b;
}

} // namespace

int main() {
    // Notices after the finish / the announcement wait for them; the earliest bound wins.
    {
        StopSequences s;
        CHECK(noticeLimit(s) == INT_MAX && stopSequencesDone(s));
        s.finishDue = true;
        s.finishNotice = 7;
        s.announceNotice = 3; // an older, finished announcement does not hold anything
        CHECK(noticeLimit(s) == 7 && !stopSequencesDone(s));
        s.announcementDue = true;
        s.announceNotice = 9;
        CHECK(noticeLimit(s) == 7);
        s.finishDue = false;
        CHECK(noticeLimit(s) == 9 && !stopSequencesDone(s));
    }
    // The finish waits for the vehicle and the messages before the arrival.
    {
        ModalState m;
        CHECK(finishMayStart(true, m, false) && !finishMayStart(false, m, false));
        CHECK(!finishMayStart(true, m, true));
        m.moving = true;
        CHECK(!finishMayStart(true, m, false));
        m.moving = false;
        m.messageOpen = true;
        CHECK(!finishMayStart(true, m, false));
    }
    // The town dialog: never behind the finish or a message, once per stop.
    {
        StopSequences s;
        ModalState m;
        CHECK(townDialogMayOpen(false, s, m) && !townDialogMayOpen(true, s, m));
        s.finishDue = true;
        CHECK(!townDialogMayOpen(false, s, m)); // the user's report: ZIEL with ESTATES behind it
        s.finishDue = false;
        m.noticesQueued = true; // the reward's message not shown yet
        CHECK(!townDialogMayOpen(false, s, m));
        m.noticesQueued = false;
        m.messageOpen = true;   // ... or still open
        CHECK(!townDialogMayOpen(false, s, m));
        m.messageOpen = false;
        m.filmDue = true;
        CHECK(!townDialogMayOpen(false, s, m));
    }
    // The announcement: after the finish, the town dialog and the messages before it.
    {
        StopSequences s;
        ModalState m;
        CHECK(announcementMayStart(true, s, m, false, false));
        CHECK(!announcementMayStart(true, s, m, true, false));
        CHECK(!announcementMayStart(true, s, m, false, true));
        s.finishDue = true;
        CHECK(!announcementMayStart(true, s, m, false, false));
        s.finishDue = false;
        m.messageOpen = true;
        CHECK(!announcementMayStart(true, s, m, false, false));
    }
    // The game: a human reaches the destination Rom of the first of two stages. The finish
    // and the reward come at the arrival; the next destination, its draws and Dr. Drago's
    // placement only after the town dialog (endTurn), each step returning false first.
    {
        std::vector<Town> towns;
        const Board board = towns3(towns);
        int checked = 0;
        for (uint16_t seed = 1; seed < 4000 && checked < 4; ++seed) {
            Game g(board, towns, 2, 2, seed, 1, nullptr, nullptr, false);
            g.setRandomEvents(false);
            if (g.destination() != 1 || g.currentPlayer() != 0 || !g.roll() || g.allowance() != 1) continue;
            const int finishes = g.finishSerial(), announcements = g.announceSerial();
            g.takeNotices();
            CHECK(g.step(Direction::Right));
            CHECK(g.phase() == Game::Phase::Arrived && g.finishSerial() == finishes + 1);
            const auto reward = g.takeNotices();
            CHECK(reward.size() == 1 && reward[0].text >= 350 && reward[0].text <= 352);
            CHECK(reward[0].seq == g.finishNotice()); // the message follows the finish
            // seg2:141A: the town dialog sees the old stage: nothing announced yet.
            CHECK(g.fieldFollowUpPending() && g.announceSerial() == announcements);
            CHECK(g.remainingStages() == 2 && g.destination() == 1 && g.dragoPlacements() == 0);
            const MsRandom before = g.sessionRandom();
            CHECK(g.buyProperty(0)); // a buy in the town dialog draws nothing
            CHECK(g.sessionRandom().state() == before.state());
            std::string error; // the rest of the stop is not part of a save
            CHECK(!SaveSlots(std::filesystem::temp_directory_path() / "dtvr-stop-order-none").saveAuto(g, error));
            CHECK(!g.endTurn()); // seg2:1438 / 1486: A104 - 1 and seg7:372F(1)
            CHECK(g.announceSerial() == announcements + 1 && g.announceMode() == 1);
            CHECK(g.remainingStages() == 1 && g.destination() != 1 && g.origin() == 1);
            CHECK(g.dragoPlacements() == 0 && g.currentPlayer() == 0);
            CHECK(!g.endTurn()); // seg2:9C97 after the announcement
            CHECK(g.dragoPlacements() == 1 && !g.fieldFollowUpPending() && g.currentPlayer() == 0);
            CHECK(g.endTurn() && g.currentPlayer() == 1); // seg2:1618: then nextPlayer
            ++checked;
        }
        CHECK(checked == 4);
    }
    // A town that is not the destination: Dr. Drago acts after the town dialog (seg2:15A6),
    // his message before the player change.
    {
        std::vector<Town> towns;
        const Board board = towns3(towns);
        int checked = 0;
        for (uint16_t seed = 1; seed < 8000 && checked < 3; ++seed) {
            Game g(board, towns, 2, 2, seed, 1, nullptr, nullptr, false);
            g.setRandomEvents(false);
            g.startDrago(1); // the holder is the slot farthest from the destination: 0 here
            if (g.destination() == 1 || g.currentPlayer() != 0 || g.drago().holder != 0 || !g.roll()
                || g.allowance() != 1)
                continue;
            g.takeNotices();
            CHECK(g.step(Direction::Right));
            CHECK(g.phase() == Game::Phase::Arrived && g.fieldFollowUpPending() && !g.hasNotices());
            const MsRandom before = g.sessionRandom();
            const bool messaged = !g.endTurn();
            CHECK(!g.fieldFollowUpPending());
            if (messaged) {
                CHECK(g.hasNotices() && g.currentPlayer() == 0 && g.phase() == Game::Phase::Arrived);
                g.takeNotices();
                CHECK(g.endTurn());
            }
            CHECK(g.currentPlayer() == 1);
            CHECK(g.sessionRandom().state() != before.state()); // seg2:B58C drew
            ++checked;
        }
        CHECK(checked == 3);
    }
    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
