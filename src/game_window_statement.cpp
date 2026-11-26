// SPDX-License-Identifier: GPL-3.0-only
// The monthly statement STAT in the game window (game_window.hpp, round R1): when it
// opens for Game::Phase::Statement, its timeout on a computer's turn, its input and the
// player change that goes on when it has closed. Evidence in docs/formats.md
// "Monthly statement".
#include "game_window.hpp"

namespace dtvr {

void GameWindow::updateStatement() {
    StatView& stat = screen_.statement();
    Game& g = play_.game();
    if (screen_.title() && stat.isOpen()) stat.close(); // a demo ended (seg1:01D7) behind it
    // seg2:07E1 / 12A3 open STAT after the modal windows before it: the messages of the
    // change (sit-outs) or of the Master-Rallye reward (seg2:1297 waits for it), the finish.
    if (!screen_.title() && g.phase() == Game::Phase::Statement && g.statementSerial() != statementSeen_ && !moving_
        && !screen_.messages().open() && !g.hasNotices() && pendingFilms_.empty() && !screen_.film().active()
        && !screen_.finish().active() && g.finishSerial() == finishSeen_ && !screen_.eventSceneActive(screen_.sceneNow())) {
        statementSeen_ = g.statementSerial();
        stat.open(statSheet(g), SDL_GetTicks());
        statementShown_ = true;
    }
    // seg6:0FB4 with DS:A7BE set: while the current player DS:94E2 is a computer (seg2:5C02)
    // it posts 0x7EF after twice (DS:859E) the message window's time, 2500 ms (DS:85AE) times
    // the pause factor, or 100 ms once a demo ends (DS:16); seg1:0176 turns 0x7EF into
    // WM_KEYDOWN VK_BACK, which destroys STAT (seg1:98E2).
    if (statementShown_ && stat.isOpen()) {
        const int p = g.statementPlayer();
        if (p >= 0 && g.isComputer(p)) {
            const Uint32 wait = g.demoEnding() ? 2 * 100
                                               : 2 * Uint32(2500 * screen_.settings().pauseFactor())
                                                     / Uint32(screen_.settings().computerTempo); // recomp: Computerzüge
            if (SDL_GetTicks() - stat.openedAt() > wait) stat.close();
        }
    }
    // seg1:97D7: the modal loop ends with the window; seg2:07E6 (seg1:8CBA) and the rest of
    // nextPlayer, or the victory (seg7:71C4) after the Master-Rallye.
    if (statementShown_ && !stat.isOpen()) {
        statementShown_ = false;
        if (g.statementFinish()) report();
    }
}

bool GameWindow::handleStatementEvent(const SDL_Event& event) {
    StatView& stat = screen_.statement();
    // The recomp menu (Escape, F8) and a save dialog keep their input; SaveSlots refuses
    // the save in Phase::Statement (it could not be loaded).
    if (!stat.isOpen() || recompMenu_.isOpen() || screen_.saveDialog().isOpen()) return false;
    switch (event.type) {
    case SDL_KEYDOWN: case SDL_KEYUP: case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: case SDL_MOUSEMOTION:
    case SDL_MOUSEWHEEL: break;
    default: return false; // quit, window and controller events
    }
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        const SDL_Keycode k = event.key.keysym.sym;
        // The display keys (F11, Alt+Return, F12) and F8 of the recomp menu stay with the
        // game window's input; Alt+Return is WM_SYSKEYDOWN, which STAT does not handle.
        if (k == SDLK_F11 || k == SDLK_F12 || k == SDLK_F8
            || ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && (event.key.keysym.mod & KMOD_ALT)))
            return false;
        if (event.type == SDL_KEYUP && k == heldArrow_) heldArrow_ = SDLK_UNKNOWN;
    }
    const Game& g = play_.game();
    const int p = g.statementPlayer();
    if (statementShown_ && p >= 0 && g.isComputer(p)) {
        // seg1:00D2 drops keys and buttons while a computer is the current player; only
        // seg1:01D7's Escape / right button reaches the game window (the end of a demo).
        const bool endDemo = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                             || (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT);
        return !(endDemo && !g.humansLeft());
    }
    if (stat.closes(event)) stat.close(); // seg1:98CA / 9A7A -> DestroyWindow
    return true;
}

} // namespace dtvr
