// SPDX-License-Identifier: GPL-3.0-only
// Presentation order of the windows at a stop (stop_order.hpp).
#include "stop_order.hpp"

namespace dtvr {

namespace {
bool modalFree(const ModalState& m) { return !m.moving && !m.messageOpen && !m.filmDue; }
} // namespace

bool finishMayStart(bool due, const ModalState& m, bool noticesBeforeFinish) {
    return due && modalFree(m) && !noticesBeforeFinish;
}

bool townDialogMayOpen(bool shownThisStop, const StopSequences& s, const ModalState& m) {
    return !shownThisStop && !s.finishDue && modalFree(m) && !m.noticesQueued;
}

bool announcementMayStart(bool due, const StopSequences& s, const ModalState& m, bool townDialogOpen,
                          bool noticesBeforeAnnouncement) {
    return due && !s.finishDue && modalFree(m) && !townDialogOpen && !noticesBeforeAnnouncement;
}

} // namespace dtvr
