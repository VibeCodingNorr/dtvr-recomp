// SPDX-License-Identifier: GPL-3.0-only
// Presentation order of the windows at a stop (no SDL; round "stage win order"). rally.exe
// runs every window of a field action synchronously (docs/formats.md "Order of the windows
// at a stop"): at a reached destination the finish ZIEL (seg7:2628), then the reward's
// message (seg6:51EA), then the town dialog ESTATES (seg1:4BDD), then the announcement
// (seg7:372F) and only then the forced sale (seg1:4E39) and the player change (seg2:06FC).
// Game decides the rules at once; these gates keep the window from showing a later window
// behind an earlier one.
#pragma once

#include <climits>

namespace dtvr {

// What the game window knows about the sequences of the current stop.
struct StopSequences {
    bool finishDue = false;       // Game::finishSerial not shown yet, or ZIEL still running
    bool announcementDue = false; // Game::announceSerial not shown yet, or its window running
    int finishNotice = 0;         // Game::finishNotice: the reward's message and later ones
    int announceNotice = 0;       // Game::announceNotice: notices after the announcement
};

// Notices numbered at or above the result wait for the sequence that precedes them.
inline int noticeLimit(const StopSequences& s) {
    int limit = INT_MAX;
    if (s.finishDue && s.finishNotice < limit) limit = s.finishNotice;
    if (s.announcementDue && s.announceNotice < limit) limit = s.announceNotice;
    return limit;
}

// The window state that a modal window of rally.exe waits for.
struct ModalState {
    bool moving = false;        // the drawn vehicle has not reached its field
    bool messageOpen = false;   // the message window (seg6:51EA -> 0FB4)
    bool noticesQueued = false; // notices (or films) the window has not shown yet
    bool filmDue = false;       // a FLIC queued or running (seg7:62AA)
};

// seg2:131D / 11DF: the finish starts when the vehicle stands on the destination and the
// messages before the arrival have been closed (they are modal).
bool finishMayStart(bool due, const ModalState& m, bool noticesBeforeFinish);

// seg2:141A / 1588 -> seg1:4BDD: the town dialog of a human stop opens once per stop,
// after the finish and the reward's message (seg2:1297 / 13D0 return only when closed).
bool townDialogMayOpen(bool shownThisStop, const StopSequences& s, const ModalState& m);

// seg2:1453 / 1486 -> seg7:372F after the town dialog: the finish is over, the town dialog
// closed and the messages before the announcement have been shown.
bool announcementMayStart(bool due, const StopSequences& s, const ModalState& m, bool townDialogOpen,
                          bool noticesBeforeAnnouncement);

// Windows that follow the stop (the year balance seg8:88C6 and STAT seg1:972F inside
// nextPlayer, the forced sale seg1:4E39 at seg2:1618, the victory seg7:71C4) wait until
// the finish and the announcement have run.
inline bool stopSequencesDone(const StopSequences& s) { return !s.finishDue && !s.announcementDue; }

} // namespace dtvr
