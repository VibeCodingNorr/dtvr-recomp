// SPDX-License-Identifier: GPL-3.0-only
// Game controllers in the game window: the original joystick tick (seg2:8A30, keys
// into the SDL queue) and the RECOMP ADDITION of the other pad buttons, which need to
// know which window has the input (PadContext) and run the actions that are no key,
// with the on-screen keyboard for player names.
// Mapping and evidence: docs/formats.md "Joystick" / "Recomp pad layer".
#include "game_window.hpp"

namespace dtvr {

PadContext GameWindow::padContext() {
    const Game& g = play_.game();
    const bool recomp = recompMenu_.isOpen();
    const bool message = screen_.messages().open();
    const bool risiko = g.phase() == Game::Phase::Risiko;
    PadContext c;
    c.keyboard = !recomp && keyboard_.isOpen();
    // OPTION (returnActivates = false) and PLAYER (Return = Start): A toggles like Space.
    c.aIsSpace = !recomp && !c.keyboard && (screen_.options().isOpen() || screen_.setup().isOpen());
    // A PLAYER name field (SBINPUT) has the focus: A opens the on-screen keyboard.
    c.nameField = c.aIsSpace && !screen_.options().isOpen() && screen_.setup().editingField() >= 0;
    // Backspace opens RIGHTBTN in the map's keys (seg4:01D1, handleKey) when no window
    // took it; in RIGHTBTN it chooses the checked "Zurück" (seg5:543D).
    const bool map = !modal_ && !screen_.estates().isOpen() && !risiko;
    const bool rightMenu = !recomp && !message && menu_.isOpen() && menu_.name() == "RIGHTBTN";
    c.backOpensMenu = map || rightMenu;
    c.surroundings = !recomp && !message && screen_.surroundings();
    // ONFIELD's item 1031 has no MF_GRAYED / MF_DISABLED in the language file (flags 0).
    // Never while PLAYER (and its name fields or the on-screen keyboard) has the input.
    const bool setup = screen_.setup().isOpen() || keyboard_.isOpen();
    c.turnMenu = !recomp && !message && !setup && menu_.isOpen() && menu_.name() == "ONFIELD";
    // PLAYER: F9 toggles the extra cards; OPTION: F9 opens the recomp menu (Grafik).
    c.yIsF9 = !recomp && !c.keyboard && (screen_.options().isOpen() || screen_.setup().isOpen());
    // The town dialog takes keys and clicks (seg1:4D79) when no window in front of it does
    // (modal_ covers messages, menus, the card window, the slot machine, scenes and the
    // other dialogs; the statistics variant is excluded by cardsShortcut); Y is its header
    // item Karte while no tab is open.
    c.townCards = !modal_ && !risiko && !screen_.mouseControl().isOpen() && !screen_.stages().isOpen()
                  && !setup && screen_.estates().cardsShortcut();
    return c;
}

void GameWindow::updatePad() {
    if (keyboard_.isOpen() && !screen_.setup().isOpen()) keyboard_.close(); // PLAYER went away
    const PadContext c = padContext();
    PadOutput a;
    if (PadState scripted; scripted_.takePad(scripted)) { // --press Pad:… (smoke tests)
        a = controller_.scripted(scripted, c);
    } else if (!options_.smokeTest) {
        const bool focused = (SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS) != 0; // seg2:8A35 GetFocus
        a = controller_.update(SDL_GetTicks(), screen_.settings().value[Settings::Joystick] != 0, focused, c);
    } else {
        return;
    }
    if (a.pauseMenu) { // Start: the recomp menu as Escape opens it on the map (toggle like F8)
        const bool blocked = (quitAsked_ && screen_.messages().question()) || screen_.film().active()
                             || screen_.intro().active() || screen_.sight().isOpen()
                             || screen_.mouseControl().isOpen();
        if (recompMenu_.isOpen()) recompMenu_.menu().close();
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
    if (a.townCards && c.townCards // Y: as a click on the header item Karte (seg1:5FB3)
        && screen_.estates().chooseCards().kind == EstatesView::Action::Kind::Cards)
        openEstateCards();
    if (a.keyboard && c.nameField) openKeyboard();
    if (a.keyboardShift && c.keyboard) {
        keyboard_.toggleShift();
        if (const int fx = keyboard_.takeSound(); fx >= 0) sound_.play(2, fx);
    }
    if (a.keyboardAccept && c.keyboard) closeKeyboard(keyboard_.accept());
}

void GameWindow::openKeyboard() {
    const int slot = screen_.setup().editingField();
    if (slot < 0) return;
    keyboard_.open(slot, screen_.setup().fieldText(slot), kNameFieldLimit); // the field's 19 (seg1:1D28)
    sound_.play(2, 2); // Plopp02 like an activated SBBUTTON
}

// Modal over PLAYER: keys (also the pad's), text and the mouse go to the keyboard.
bool GameWindow::handleKeyboard(const SDL_Event& event) {
    if (!keyboard_.isOpen()) return false;
    if (!screen_.setup().isOpen()) { keyboard_.close(); return false; }
    const bool input = event.type == SDL_KEYDOWN || event.type == SDL_KEYUP || event.type == SDL_TEXTINPUT
                       || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP
                       || event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEWHEEL;
    if (!input) return false;
    const ScreenKeyboardView::Result r = keyboard_.handle(event);
    if (const int fx = keyboard_.takeSound(); fx >= 0) sound_.play(2, fx);
    if (r != ScreenKeyboardView::Result::None) closeKeyboard(r);
    return true;
}

void GameWindow::closeKeyboard(ScreenKeyboardView::Result result) {
    if (const int fx = keyboard_.takeSound(); fx >= 0) sound_.play(2, fx);
    if (result == ScreenKeyboardView::Result::Done) screen_.setup().setName(keyboard_.slot(), keyboard_.text());
    keyboard_.close(); // the name field keeps the focus
}

} // namespace dtvr
