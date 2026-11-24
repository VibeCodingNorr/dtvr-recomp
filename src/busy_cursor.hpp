// SPDX-License-Identifier: GPL-3.0-only
// The busy cursor of the running OS while the game loads. rally.exe sets the Windows
// 3.1 hourglass (IDC_WAIT, seg5:001B) and restores the old cursor at seg5:0207; the
// rebuild shows the system's own wait cursor over the same span (user decision
// 2026-11-25) instead of imitating the hourglass. Without a system cursor (SDL's dummy
// video driver) nothing changes.
#pragma once

#include <SDL.h>

namespace dtvr {

class BusyCursor {
public:
    BusyCursor() {
        cursor_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT); // NULL: not supported, ignore
        if (!cursor_) return;
        previous_ = SDL_GetCursor();
        SDL_SetCursor(cursor_);
        active_ = true;
    }
    BusyCursor(const BusyCursor&) = delete;
    BusyCursor& operator=(const BusyCursor&) = delete;
    ~BusyCursor() {
        release();
        if (cursor_) SDL_FreeCursor(cursor_);
    }
    // Back to the cursor from before (seg5:0207); a second call does nothing.
    void release() {
        if (!active_) return;
        active_ = false;
        SDL_SetCursor(previous_ ? previous_ : SDL_GetDefaultCursor());
    }

private:
    SDL_Cursor* cursor_ = nullptr;
    SDL_Cursor* previous_ = nullptr; // owned by SDL
    bool active_ = false;
};

} // namespace dtvr
