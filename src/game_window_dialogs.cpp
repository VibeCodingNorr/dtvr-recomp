// SPDX-License-Identifier: GPL-3.0-only
// Input of the game's dialogs (game_window.hpp): Risiko, the investment offer, Nägel,
// the Flugzeug booking, the target choice, the card window, the card shop and the
// town dialog ESTATES. Each returns true when it took the event.
#include "game_window.hpp"

#include "board.hpp"

namespace dtvr {

bool GameWindow::handleGameDialog(const SDL_Event& event) {
    const bool click = event.type == SDL_MOUSEBUTTONDOWN;
    SaveView& saveDialog = screen_.saveDialog();
    // Risiko (seg7:6BF0): any key but Escape or the CHANCE area presses, Escape or
    // the STOP area stops; computers press inside the tick.
    if (play_.game().phase() == Game::Phase::Risiko && !play_.game().isComputer(play_.game().currentPlayer())
        && !screen_.messages().open() && !menu_.isOpen() && !saveDialog.isOpen()) {
        int button = 0;
        if (click && event.button.button == SDL_BUTTON_LEFT)
            button = screen_.risiko().buttonAt(event.button.x, event.button.y);
        else if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.sym != SDLK_F8)
            button = event.key.keysym.sym == SDLK_ESCAPE ? 2 : 1;
        if (screen_.risiko().blinking(SDL_GetTicks())) button = 0; // seg7:6C0D: a press is pending
        if (button) {
            screen_.risiko().press(button, SDL_GetTicks());
            if (button == 1 ? play_.game().risikoPress() : play_.game().risikoStop()) report();
        }
        if (button || event.type == SDL_KEYDOWN || click) return true;
    }
    if (screen_.invest().isOpen() && !screen_.messages().open()) { // seg8:7098
        const InvestView::Result r = screen_.invest().handle(event);
        if (r.sound >= 0) sound_.play(0, r.sound);
        if (r.done) {
            screen_.invest().close();
            if (r.amount > 0 && play_.game().invest(r.amount)) report();
            else { play_.game().declineInvestment(); report(); }
        }
        return true;
    }
    if (screen_.nails().isOpen()) { // seg3:1E5E
        const int d = screen_.nails().handle(event);
        if (d != 0) screen_.nails().close();
        if (d > 0 && play_.game().playCard(nailsCardSlot_, d)) report();
        return true;
    }
    if (screen_.flight().isOpen()) { // seg3:3384
        const FlightView::Result r = screen_.flight().handle(event);
        if (const int fx = screen_.flight().takeSound(); fx >= 0) sound_.play(2, fx);
        if (r.closed) screen_.messages().push(Notice{8300, {}}); // Bodennebel over Brussels
        if (r.cancel) screen_.flight().close();
        if (!r.town.empty()) {
            screen_.flight().close();
            // seg3:32CE: the town whose name equals the booked one (_fstricmp).
            const int town = townByName(towns_, r.town);
            if (town >= 0 && play_.game().playCard(flightCardSlot_, town)) report();
        }
        return true;
    }
    if (screen_.selectPlayer().isOpen()) { // SELPLAYER (seg2:5386)
        const SbButtonGroup::Input input = screen_.selectPlayer().handle(event);
        if (input.sound >= 0) sound_.play(2, input.sound);
        if (input.id >= 0 && targetCardSlot_ == -2) {
            // seg2:5E93: the chosen driver changes between human and computer; for
            // the current one the computer turn starts or ends (seg5:6CE8 / 6BF2).
            screen_.selectPlayer().close();
            Game& g = play_.game();
            if (g.setComputer(input.id, !g.isComputer(input.id))) report();
        } else if (input.id >= 0) {
            screen_.selectPlayer().close();
            if (play_.game().playCard(targetCardSlot_, input.id)) report();
        } else if (input.cancel) {
            screen_.selectPlayer().close();
        }
        return true;
    }
    if (handleCardWindow(event)) return true;
    if (screen_.cardShop().isOpen()) { // seg3:4640
        const auto action = screen_.cardShop().handle(event, play_.game());
        using Kind = CardShopView::Action::Kind;
        Game& g = play_.game();
        if (action.sound >= 0) sound_.play(2, action.sound);
        if (action.kind == Kind::Buy || action.kind == Kind::Sell) {
            const bool done = action.kind == Kind::Buy ? g.buyCard(action.value) : g.sellCard(action.value);
            sound_.play(2, done ? 2 : 7); // seg3:4782 / 4850: Plopp02 or Zaehler2
            if (done && action.kind == Kind::Sell) screen_.cardShop().afterSale(g);
            if (done) report();
        } else if (action.kind == Kind::Close) { // Beenden: the stop ends the turn
            screen_.cardShop().close();
            g.endTurn();
            report();
        }
        return true;
    }
    return handleEstates(event);
}

bool GameWindow::handleCardWindow(const SDL_Event& event) {
    if (!cards_.isOpen()) return false; // the card buttons; Escape / Backspace cancel (WM_COMMAND 0xFFFF)
    const bool click = event.type == SDL_MOUSEBUTTONDOWN;
    // SBBUTTON (seg1:0E78): a card plays when the button is released over it,
    // or by space / Return on the focused card; arrows move the focus.
    CardView::Input input;
    if (click && event.button.button == SDL_BUTTON_LEFT)
        input = cards_.mouseDown(event.button.x, event.button.y);
    else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
        input = cards_.mouseUp(event.button.x, event.button.y);
    else if (event.type == SDL_KEYDOWN)
        input = cards_.key(event.key.keysym.sym, event.key.keysym.mod);
    if (input.sound >= 0) sound_.play(2, input.sound);
    if (input.cancel) { // seg4:0B0A: mask 1 back to the town dialog, 2 back to the turn menu
        if (play_.game().yellowCardPending()) sound_.play(2, 7); // mask 0: refused with sound 7
        else cards_.close();
        return true;
    }
    if (input.id >= 0) {
        const int slot = input.id;
        // seg2:51EA: a card with a target asks for the player when more than one is
        // possible (excluded: the player; Tschüss excludes nobody, argument 0xFF).
        const Game& g = play_.game();
        if (g.yellowCardPending()) { // seg3:1057: discard mode, the clicked card is dropped
            if (play_.game().discardForCard(slot)) report();
            cards_.close();
            return true;
        }
        const CardKind kind = cardKind(g.deck().typeOfSlot(g.hand(g.currentPlayer())[size_t(slot)]));
        if (cardNeedsTarget(kind) || kind == CardKind::Tschuess) {
            std::vector<int> candidates;
            std::array<uint8_t, kPlayerSlots> status{};
            for (int p = 0; p < kPlayerSlots; ++p) {
                status[size_t(p)] = g.players()[size_t(p)].status;
                if (status[size_t(p)] && (kind == CardKind::Tschuess || p != g.currentPlayer()))
                    candidates.push_back(p);
            }
            if (candidates.size() > 1) {
                targetCardSlot_ = slot;
                screen_.selectPlayer().open(candidates, status);
                return true;
            }
            if (candidates.size() == 1 && play_.game().playCard(slot, candidates.front())) report();
            return true;
        }
        if (kind == CardKind::Naegel && g.nailsInWater()) { // seg3:1CCF: into the water, no window
            if (play_.game().playCard(slot, 0)) report();
            return true;
        }
        if (kind == CardKind::Naegel) { // seg3:1C96: the direction window
            nailsCardSlot_ = slot;
            screen_.nails().open(g.nailsDirections());
            return true;
        }
        if (kind == CardKind::Flugzeug) { // seg3:31D5: the booking window
            flightCardSlot_ = slot;
            screen_.flight().open(g.eastOpen(), g.remainingStages() == 0);
            return true;
        }
        if (play_.handleKey(SDL_Keycode(SDLK_1 + slot), 0)) report();
        return true;
    }
    // A right click does nothing: the canvas passes it to RALLY (seg8:018C), which
    // opens RIGHTBTN only with DS:96A0, cleared while the card window is up (seg3:092D).
    if (event.type == SDL_KEYDOWN || click) return true; // the window keeps the keyboard
    return false;
}

bool GameWindow::handleEstates(const SDL_Event& event) {
    EstatesView& estates = screen_.estates();
    if (!estates.isOpen() || cards_.isOpen()) return false;
    const auto action = estates.handle(event, screen_.estateRows());
    using Kind = EstatesView::Action::Kind;
    Game& g = play_.game();
    if (action.kind == Kind::Row) {
        const bool done = estates.tab() == EstatesTab::Sell ? g.sellProperty(action.entry)
                                                            : g.buyProperty(action.entry);
        sound_.play(2, done ? 2 : 7); // seg1:5598 / 55D0 (5633 / 566B): Plopp02 or Zaehler2
        if (done) {
            estates.afterAction(int(screen_.estateRows().size()));
            report();
        }
    } else if (action.kind == Kind::Tab) {
        sound_.play(2, 2); // seg1:5436 / 546D: Plopp02
    } else if (action.kind == Kind::Cards) { // the card window for property cards
        sound_.play(2, 2); // seg1:54B2
        if (g.openCardWindow() && cards_.open(g, 1)) {
            cardWindowState_ = turnState();
            cardsFromEstates_ = true;
        }
    } else if (action.kind == Kind::ForcedSell) { // seg1:57A4 -> seg1:8327
        const bool sold = g.forcedSell(action.town, action.entry);
        sound_.play(2, sold ? 2 : 7); // seg1:57F7 / 58DE
        if (sold) {
            estates.afterForcedSale(int(screen_.estateRows().size()));
            // seg1:5857: no sellable value left or cash >= 0 in the forced variant: unlock,
            // wait 750 ms (seg6:0F60(0x2EE)) and post VK_BACK twice (seg1:58A7 / 58BB):
            // the first leaves the sale mode, the second closes the dialog (round 147).
            if (estates.forced() && !g.forcedSaleLocked()) {
                estates.unlock();
                forcedCloseAt_ = SDL_GetTicks() + 750;
            }
            report();
        }
    } else if (action.kind == Kind::Close && estates.forced()) { // seg1:5748: locked: Zaehler2
        if (g.endTurn()) {
            estates.close();
            report();
        } else {
            sound_.play(2, 7);
        }
    } else if (action.kind == Kind::Close && estates.statistics()) {
        estates.close();
    } else if (action.kind == Kind::Close) { // leaving the town ends the turn (seg1:4DBC, seg2:161D)
        if (action.sound) sound_.play(2, 2); // seg1:54F8
        estates.close();
        g.endTurn();
        report();
    }
    return true; // ESTATES has the focus (seg1:4D79): keys and clicks stay with it
}

} // namespace dtvr
