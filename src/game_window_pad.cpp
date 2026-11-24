// SPDX-License-Identifier: GPL-3.0-only
// Game controllers in the game window: the original joystick tick (seg2:8A30, keys
// into the SDL queue) and the RECOMP ADDITION of the other pad buttons, which need to
// know which window has the input (PadContext) and run two actions that are no key.
// Mapping and evidence: docs/formats.md "Joystick" / "Recomp pad layer".
#include "game_window.hpp"

namespace dtvr {

void GameWindow::updatePad() {
    const Game& g = play_.game();
    const bool recomp = recompMenu_.isOpen();
    const bool message = screen_.messages().open();
    const bool risiko = g.phase() == Game::Phase::Risiko;
    PadContext c;
    // OPTION (returnActivates = false) and PLAYER (Return = Start): A toggles like Space.
    c.aIsSpace = !recomp && (screen_.options().isOpen() || screen_.setup().isOpen());
    // Backspace opens RIGHTBTN in the map's keys (seg4:01D1, handleKey) when no window
    // took it; in RIGHTBTN it chooses the checked "Zurück" (seg5:543D).
    const bool map = !modal_ && !screen_.estates().isOpen() && !risiko;
    const bool rightMenu = !recomp && !message && menu_.isOpen() && menu_.name() == "RIGHTBTN";
    c.backOpensMenu = map || rightMenu;
    c.surroundings = !recomp && !message && screen_.surroundings();
    // ONFIELD's item 1031 has no MF_GRAYED / MF_DISABLED in the language file (flags 0).
    c.turnMenu = !recomp && !message && menu_.isOpen() && menu_.name() == "ONFIELD";
    c.setup = !recomp && !screen_.options().isOpen() && screen_.setup().isOpen();

    const bool focused = (SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS) != 0; // seg2:8A35 GetFocus
    const PadOutput a = controller_.update(SDL_GetTicks(), screen_.settings().value[Settings::Joystick] != 0,
                                           focused, c);
    if (a.pauseMenu) { // Start: the recomp menu as Escape opens it on the map (toggle like F8)
        const bool blocked = (quitAsked_ && screen_.messages().question()) || screen_.film().active()
                             || screen_.intro().active() || screen_.sight().isOpen()
                             || screen_.mouseControl().isOpen();
        if (recomp) recompMenu_.menu().close();
        else if (blocked) {}
        else if (screen_.options().isOpen()) // OPTION's own recomp link opens the page Grafik
            recompMenu_.menu().openAt(menuSettings(), !screen_.title(), "Grafik");
        else recompMenu_.menu().open(menuSettings(), !screen_.title());
        heldArrow_ = SDLK_UNKNOWN;
    }
    if (a.scroll != SDLK_UNKNOWN && c.surroundings) { // right stick: Umfeld's arrow step, no key event
        SDL_Event step{};
        step.type = SDL_KEYDOWN;
        step.key.state = SDL_PRESSED;
        step.key.keysym.sym = a.scroll;
        screen_.handleSurroundings(step);
    }
    if (a.handCards && c.turnMenu) { // Y: as choosing "Karte einsetzen" in ONFIELD
        sound_.play(2, 2); // seg5:5486: Plopp02 of a chosen item
        command(1031);
    }
}

} // namespace dtvr
