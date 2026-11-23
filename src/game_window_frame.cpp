// SPDX-License-Identifier: GPL-3.0-only
// What the game window does every frame (game_window.hpp): music, notices and their
// scenes, films, sounds after the drive, the automatic dialogs (town dialog, card
// shop, forced sale), autosave, the timed message window of computers, the finish,
// balance, announcement and victory sequences, the game's own update, the time
// limit and the turn menu.
#include "game_window.hpp"

#include "event_scenes.hpp"
#include "panel.hpp"

#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

namespace dtvr {

void GameWindow::updateFrame(int frame) {
    if (screen_.settings().value[Settings::Midi] != musicEnabled_) {
        musicEnabled_ = screen_.settings().value[Settings::Midi];
        music_.setEnabled(musicEnabled_ != 0);
    }
    if (!screen_.title()) music_.turn(play_.game().eventClock().turns);
    music_.tick();
    for (const auto& cue : play_.game().takeMusic()) if (!screen_.title()) music_.interrupt(cue);
    screen_.tickPresentation(SDL_GetTicks(), recompMenu_.isOpen());
    // seg2:6426 returns when the slot window has closed: then the move starts (66B9).
    if (play_.game().spinPending() && screen_.slotDone(play_.game().spinSerial(), screen_.presentationNow())
        && play_.game().finishSpin())
        report();
    screen_.advanceVehicles(screen_.presentationNow());
    moving_ = screen_.vehicleMoving(play_.game().currentPlayer());
    // The original handles a field when the vehicle has reached it (the arrival
    // runs after the drive), so its message waits for the drawn car like its sounds.
    // seg7:6A79: a CHANCE press blinks for 2.4 s before its result and the next level.
    if (play_.game().risikoPressSerial() != risikoPressSeen_) {
        risikoPressSeen_ = play_.game().risikoPressSerial();
        screen_.risiko().blink(play_.game().risikoPressLight(), SDL_GetTicks());
        play_.holdRisiko(SDL_GetTicks() + RisikoView::kBlinkMs);
    }
    if (!screen_.title() && !moving_ && !screen_.risiko().blinking(SDL_GetTicks()))
        for (Notice& n : play_.game().takeNotices()) {
            noticesTaken_ = std::max(noticesTaken_, n.seq + 1);
            if (const auto cue = musicForNotice(n.text); !cue.empty()) music_.interrupt(cue);
            if (noticeEndsEvent(n.text)) eventMusicNotice_ = n.seq; // its message ends with seg1:275E
            // seg8:6FBF: the offer text (5321 + random(2) / 5401 + random(2)) belongs
            // to the investment window, not to the message window.
            const bool offerText = n.text == 5321 || n.text == 5322 || n.text == 5401 || n.text == 5402;
            if (offerText && play_.game().investmentOffer() != EventKind::None) {
                const auto t = screen_.texts().find(n.text);
                const std::string offer = t == screen_.texts().end() ? std::string() : formatNotice(t->second, n, play_.game());
                const Game& g = play_.game();
                screen_.invest().open(g.investmentOffer() == EventKind::Professor, offer, g.cash(g.currentPlayer()));
                continue;
            }
            screen_.messages().push(std::move(n));
        }
    { // event places: the view zooms in on the place while its message is open
        const Notice* front = screen_.messages().front();
        if (front && front->focusX >= 0) {
            screen_.eventFocus(front->focusX, front->focusY);
            // seg8 handlers: the animation plays before the message (once per message).
            if (screen_.messages().frontSerial() != sceneSerial_) {
                sceneSerial_ = screen_.messages().frontSerial();
                if (const EventScene* scene = eventSceneForNotice(front->text))
                    screen_.startEventScene(*scene, front->focusX, front->focusY, front->variant, screen_.sceneNow());
            }
        } else {
            screen_.eventFocus(-1, -1);
        }
    }
    // The original plays a field's sounds when the vehicle is on it: cues wait while the
    // drawn car still drives there.
    for (const SoundCue& c : play_.game().takeSounds())
        if (!screen_.title()) pendingSounds_.push_back(c);
    screen_.setArrowHeld(heldArrow_ != SDLK_UNKNOWN); // DS:A77A for the view point stop
    // Card films (seg7:62AA): one after the other, modal, sound on channel 7.
    for (const FilmCue& f : play_.game().takeFilms())
        if (!screen_.title()) pendingFilms_.push_back(f);
    if (screen_.film().active()) {
        screen_.film().update(SDL_GetTicks());
        if (!screen_.film().active()) sound_.stop(7); // seg1:317D
    }
    if (!screen_.film().active() && !pendingFilms_.empty() && !moving_ && !screen_.messages().open()) {
        const FilmCue f = pendingFilms_.front();
        pendingFilms_.erase(pendingFilms_.begin());
        if (screen_.film().start(f.film, f.loops, layout_.width, layout_.height, SDL_GetTicks())) { // centred
            const fs::path wav = findCaseInsensitive(screen_.root(), fs::path("butt") / "snd" / f.sound);
            if (!wav.empty()) sound_.playFile(7, wav); // seg1:2D40(7, …)
        }
    }
    // The event handlers of seg8 (seg8:1E42, 2355, 2A01, 3BDA, 427F, 4933, 4EEA, 5A45,
    // 5EED, 6C41) call seg1:275E once their message has been closed: the event music
    // stops and the next background piece starts from the beginning.
    if (eventMusicNotice_ >= 0) {
        const Notice* front = screen_.messages().front();
        const int settled = front ? std::min(front->seq, noticesTaken_) : noticesTaken_;
        if (settled > eventMusicNotice_) {
            eventMusicNotice_ = -1;
            music_.nextBackground();
        }
    }
    // rally.exe's message window is modal (seg6:51EA -> 0FB4): a sound after a message
    // in the code sounds only when it has been closed, one before it with its opening
    // (seg2:0EF1 moneywin, then 0F05). Every notice before the cue has to be settled.
    if (!moving_) {
        const Notice* front = screen_.messages().front();
        const int settled = front ? std::min(front->seq, noticesTaken_) : noticesTaken_;
        size_t played = 0;
        for (; played < pendingSounds_.size(); ++played) {
            const SoundCue& c = pendingSounds_[played];
            if (c.afterNotice > settled) break; // keeps the order of the cues
            sound_.play(c.channel, c.sound, c.loops);
        }
        pendingSounds_.erase(pendingSounds_.begin(), pendingSounds_.begin() + long(played));
    }
    // Message window and menu are modal (seg6:0FB4, SetCapture in seg5:578C).
    // A played card changes the turn state and closes the card window.
    if (cards_.isOpen() && turnState() != cardWindowState_) cards_.close();
    if (cardsFromEstates_ && !cards_.isOpen()) { // seg4:0AB0 / 0B27 -> seg1:8D1A
        cardsFromEstates_ = false;
        if (screen_.estates().afterCardWindow(play_.game().propertyMode())) sound_.play(2, 2); // the posted Enter
    }
    // seg3:1057: after text 403 the card window opens in discard mode (seg3:0887(0)) and
    // stays until a card is dropped.
    if (!screen_.title() && !moving_ && play_.game().yellowCardPending() && !screen_.messages().open() && !cards_.isOpen()
        && !play_.game().isComputer(play_.game().currentPlayer()) && cards_.open(play_.game(), -1))
        cardWindowState_ = turnState();
    // The town dialog belongs to a human player's stop in a town (seg2:141A -> seg1:4BDD).
    EstatesView& estates = screen_.estates();
    {
        const Game& g = play_.game();
        const int p = g.currentPlayer();
        const int town = p >= 0 && g.phase() == Game::Phase::Arrived && !g.isComputer(p)
            ? g.townAt(g.playerField(p)) : -1;
        if (estates.statistics()) {
            // Statistik stays until it is closed.
        } else if (options_.openEstates >= 0) {
            if (!estates.isOpen()) estates.open(options_.openEstates);
        } else if (town < 0) estates.close();
        else if (moving_) {} // the dialog opens when the car has arrived
        else if (!estates.isOpen() || estates.town() != town)
            estates.open(town, play_.game().handCount(play_.game().currentPlayer()) == 0);
    }
    SaveView& saveDialog = screen_.saveDialog();
    if (screen_.title()) { // nothing of the game runs behind the title
        if (!menu_.isOpen() && !saveDialog.isOpen() && !screen_.setup().isOpen() && !screen_.stages().isOpen() && !screen_.options().isOpen()
            && !screen_.pictureShown() && !screen_.messages().open())
            openTitle();
    } else if (play_.game().humansLeft() && play_.game().turnSerial() != autosaved_
               && !play_.game().computerTurnRunning()) { // seg2:0837 -> seg8:A870 at each turn start
        std::string error;
        if (!screen_.saveSlots().saveAuto(play_.game(), error)) std::cerr << "Autosave: " << error << "\n";
        autosaved_ = play_.game().turnSerial();
    }
    // seg6:0FB4: while a computer is the current player (seg2:5C02) the modal message
    // window closes by itself after 2500 ms (DS:85AE) times the pause factor
    // (seg6:0E3A), 100 ms once a demo ends (DS:16), not while a film runs (DS:A050);
    // a human closes it.
    const Game& cur = play_.game();
    const bool computerWaits = cur.currentPlayer() >= 0 && cur.isComputer(cur.currentPlayer());
    if (!screen_.title() && (computerWaits || !cur.humansLeft()) && screen_.messages().open()
        && !screen_.film().active()) {
        const Uint32 wait = cur.demoEnding() ? 100 : Uint32(2500 * screen_.settings().pauseFactor())
                                             / Uint32(screen_.settings().computerTempo); // recomp: Computerzüge
        // The wait starts with the window: an event's animation plays before it (seg8).
        if (screen_.eventSceneActive(screen_.sceneNow())) messageShown_ = 0;
        else if (messageShown_ == 0) messageShown_ = SDL_GetTicks();
        else if (SDL_GetTicks() - messageShown_ > wait) {
            screen_.messages().dismiss();
            messageShown_ = 0;
        }
    } else {
        messageShown_ = 0;
    }
    // seg1:4E39: the forced sale of a human whose cash is negative after the stop.
    if (play_.game().forcedSale() && !estates.isOpen() && !moving_ && !screen_.messages().open()
        && !play_.game().isComputer(play_.game().currentPlayer()))
        estates.openForcedSale(play_.game().currentPlayer());
    // The card shop of a human player's stop on a card field (seg2:15AE -> seg3:4437).
    {
        const Game& g = play_.game();
        const bool shopDue = g.phase() == Game::Phase::Arrived && g.atCardField() && g.currentPlayer() >= 0
                             && !g.isComputer(g.currentPlayer());
        if (!shopDue && !options_.cardShop) screen_.cardShop().close();
        else if (shopDue && !moving_ && !screen_.cardShop().isOpen() && shopSerial_ != g.turnSerial()) {
            shopSerial_ = g.turnSerial(); // once per stop
            screen_.cardShop().open(g);
        }
    }
    screen_.intro().update(SDL_GetTicks());
    if (introShown_ && !screen_.intro().active()) { introShown_ = false; openTitle(); }
    // The finish sequence (seg2:131D / 11DF -> seg7:2628) when the car has arrived.
    screen_.finish().update(SDL_GetTicks());
    // The year balance (seg2:07C5 -> seg8:88C6) at the year change.
    screen_.balance().update(SDL_GetTicks());
    // The next destination (seg2:1486 / 1453 -> seg7:372F(1 / 3)) after the finish.
    screen_.announcement().update(SDL_GetTicks());
    if (!screen_.title() && play_.game().announceSerial() != announceSeen_ && !moving_
        && !screen_.messages().open() && !screen_.finish().active()) {
        announceSeen_ = play_.game().announceSerial();
        screen_.startAnnouncement(play_.game().announceMode(), play_.game().announceVariant(), SDL_GetTicks());
    }
    if (!screen_.title() && play_.game().balanceSerial() != balanceSeen_ && !moving_ && !screen_.messages().open()
        && !screen_.finish().active()) {
        balanceSeen_ = play_.game().balanceSerial();
        screen_.startBalance(SDL_GetTicks());
    }
    // After the Master-Rallye: the victory (seg7:71C4), then the title (WM_COMMAND 0x424).
    screen_.victory().update(SDL_GetTicks());
    if (!screen_.title() && play_.game().phase() == Game::Phase::GameOver && !screen_.finish().active()
        && !screen_.messages().open()) {
        if (!victoryShown_) { music_.interrupt("s"); victoryShown_ = screen_.startVictory(SDL_GetTicks()) || true; }
        else if (!screen_.victory().active()) { victoryShown_ = false; openTitle(); }
    }
    if (!screen_.title() && play_.game().finishSerial() != finishSeen_ && !moving_) {
        finishSeen_ = play_.game().finishSerial();
        music_.interrupt(play_.game().finishMusic());
        screen_.startFinish(play_.game().finishField(), play_.game().finishPlayer(), SDL_GetTicks());
    }
    modal_ = recompMenu_.isOpen() || screen_.messages().open() || menu_.isOpen() || cards_.isOpen() || saveDialog.isOpen()
                       || estates.statistics() || screen_.title() || screen_.overview().isOpen()
                       || screen_.surroundings() || screen_.options().isOpen() || screen_.finish().active()
                       || screen_.victory().active() || screen_.balance().active()
                       || screen_.announcement().active() || screen_.cardShop().isOpen()
                       || screen_.selectPlayer().isOpen() || screen_.flight().isOpen()
                       || screen_.nails().isOpen() || screen_.invest().isOpen() || screen_.film().active()
                       || screen_.sight().isOpen() || screen_.intro().active()
                       // the slot machine and event animations run synchronously in rally.exe
                       || screen_.slotActive(screen_.presentationNow()) || screen_.eventSceneActive(screen_.sceneNow())
                       || screen_.risiko().blinking(SDL_GetTicks());
    if (!modal_ && !moving_ && play_.update(SDL_GetTicks(), true)) report();
    // seg2:1618: after the field action (its message, town dialog, card dialog or card
    // shop run modally) the original checks the forced sale (seg1:4E39) and calls
    // nextPlayer (seg2:06FC) at once: a human does not press Enter again.
    {
        Game& g = play_.game();
        const int p = g.currentPlayer();
        if (!modal_ && !moving_ && !screen_.title() && p >= 0 && !g.isComputer(p) && g.phase() == Game::Phase::Arrived
            && !estates.isOpen() && !g.yellowCardPending() && options_.openEstates < 0 && !options_.cardShop
            && g.endTurn())
            report();
    }
    // [Setup] TimeOut: the 1000 ms timer of seg2:3ABD runs while the window is active.
    play_.game().setTimeLimit(screen_.settings().value[Settings::TimeOut] != 0);
    if (play_.game().timeLimitRunning() && !screen_.title() && !recompMenu_.isOpen() // paused in the recomp menu
        && (SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS || options_.smokeTest)) {
        if (int(SDL_GetTicks() - timeLimitTick_) >= 1000) {
            timeLimitTick_ += 1000;
            if (int(SDL_GetTicks() - timeLimitTick_) >= 1000) timeLimitTick_ = SDL_GetTicks();
            if (const int fx = play_.game().timeLimitSecond(); fx >= 0) sound_.play(6, fx);
        }
    } else {
        timeLimitTick_ = SDL_GetTicks();
    }
    // Events of nextPlayer (seg8:1624) run before the turn menu: wait for messages and
    // films the game produced in this frame but the window has not taken yet.
    if (!modal_ && !moving_ && turnMenuDue() && !options_.rollAtStart && !play_.game().hasNotices()
        && pendingFilms_.empty())
        openMenu("ONFIELD");
    // A held arrow drives on field by field once the car has reached its field.
    if (!modal_ && !moving_ && heldArrow_ != SDLK_UNKNOWN && play_.game().phase() == Game::Phase::Move
        && !play_.game().isComputer(play_.game().currentPlayer()) && play_.handleKey(heldArrow_, 0))
        report();
    // seg2:8A30 every 80 ms while [Setup] Joystick is on: keys for the focused window.
    if (!options_.smokeTest) controller_.update(SDL_GetTicks(), screen_.settings().value[Settings::Joystick] != 0);
    scripted_.pump(frame);
}

} // namespace dtvr
