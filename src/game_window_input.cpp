// SPDX-License-Identifier: GPL-3.0-only
// Input of the game window (game_window.hpp) in the order of rally.exe's modal
// windows: display keys, films, intro, sights, the steering cross, the recomp menu,
// messages, menus, the scenes, the full-canvas windows, the slot machine, overview
// and surroundings, then the game's dialogs (game_window_dialogs.cpp), the map and
// the main window's keys.
#include "game_window.hpp"

#include "music_menu.hpp"

#include <ctime>
#include <iostream>

namespace dtvr {
namespace {

// Widescreen extension: mouse input of the full-canvas windows drawn in the 4:3 stage.
SDL_Event toStage(SDL_Event event, const ScreenLayout& layout) {
    const int dx = layout.stage().x;
    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) event.button.x -= dx;
    else if (event.type == SDL_MOUSEMOTION) event.motion.x -= dx;
    return event;
}

bool isArrow(SDL_Keycode k) { return k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT || k == SDLK_RIGHT; }

} // namespace

void GameWindow::handleEvent(const SDL_Event& event) {
    SaveView& saveDialog = screen_.saveDialog();
    if (event.type == SDL_QUIT) running_ = false;
    controller_.handleEvent(event);
    if (event.type == SDL_WINDOWEVENT) {
        // WM_ACTIVATEAPP (seg4:0CAE): music, voice and effects pause with the focus.
        if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) { music_.setFocused(false); sound_.setFocused(false); }
        if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) { music_.setFocused(true); sound_.setFocused(true); }
    }
    { // F11 / Alt+Enter, F12 and the window size (recomp, [Display])
        Settings s = screen_.settings();
        bool changed = false;
        if (displayControl_.handle(event, s, changed)) {
            if (changed) applySettings(s);
            heldArrow_ = SDLK_UNKNOWN;
            return;
        }
    }
    if (screen_.film().active() && (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN))
        return; // the FLIC window runs modally (seg6:0FB4)
    if (screen_.intro().active()) { screen_.intro().handle(event); return; } // seg7:2118, modal
    if (screen_.sight().isOpen()) { screen_.sight().handle(event); return; } // seg6:4DCB, modal
    if (screen_.viewStopped() && event.type == SDL_KEYDOWN
        && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)) {
        // Enter at a view point: seg2:0BC3 -> seg2:16AA -> seg6:4DCB(x, y).
        const SDL_Point c = screen_.viewStopCell();
        screen_.sight().open(c.x, c.y);
        return;
    }
    if (screen_.mouseControl().isOpen()) { // MCTRL (seg4:103B): the cursor stays in the cross
        bool closed = false;
        const SDL_Keycode key = screen_.mouseControl().handle(event, closed);
        if (key != SDLK_UNKNOWN && play_.handleKey(key, 0)) report(); // posted to the main window
        if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN
            || event.type == SDL_MOUSEBUTTONUP || closed)
            return;
    }
    // F8: Audio of the recomp menu, except where rally.exe's button windows use F1-F8
    // (OPTION seg1:B60A, PLAYER); the options window has its own entry for it.
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F8
        && (recompMenu_.isOpen() || (!screen_.options().isOpen() && !screen_.setup().isOpen()))) {
        if (event.key.repeat) {} else if (recompMenu_.isOpen()) recompMenu_.menu().close();
        else recompMenu_.menu().openAt(menuSettings(), !screen_.title(), "Audio");
        heldArrow_ = SDLK_UNKNOWN;
        return;
    }
    if (recompMenu_.isOpen()) { handleRecompMenu(event); return; }
    if (event.type == SDL_KEYUP && event.key.keysym.sym == heldArrow_) heldArrow_ = SDLK_UNKNOWN;
    const bool click = event.type == SDL_MOUSEBUTTONDOWN;
    if (screen_.messages().open() && screen_.eventSceneActive(screen_.presentationNow())) return; // animation first
    if (screen_.messages().question()) { // seg2:2404: obstacle question (text 600)
        if (const int answer = screen_.messages().handle(event); answer) {
            const int text = screen_.messages().frontText();
            screen_.messages().dismiss();
            sound_.play(0, 2); // seg6:5627: Plopp02 when the window closes
            if (text == 700) { // seg4:0F20
                if (answer == 6) running_ = false;
                else if (quitReopensMain_) openMenu("MAIN1");
            } else if (play_.game().answerObstacle(answer == 6)) {
                report();
            }
        }
        return;
    }
    if (screen_.messages().open()) {
        if (event.type == SDL_KEYDOWN || click) screen_.messages().dismiss();
        return;
    }
    if (menu_.isOpen()) {
        // Recomp deviation: in MAIN, Escape would post the checked item "Neues Spiel"
        // (seg5:543D); it opens the recomp menu instead. Other menus keep seg5:543D.
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && menu_.name() == "MAIN") {
            if (!event.key.repeat) recompMenu_.menu().open(menuSettings(), false);
            return;
        }
        const int id = menu_.handle(event);
        if (const int s = menu_.takeSound(); s >= 0) sound_.play(2, s);
        if (id) command(id);
        else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && !event.key.repeat)
            recompMenu_.menu().open(menuSettings(), !screen_.title()); // Escape does nothing here in rally.exe
        return;
    }
    const bool endDemo = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                         || (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT);
    if (!screen_.title() && !play_.game().humansLeft() && endDemo) { // seg1:021D: a game of computers ends
        play_.game().requestDemoEnd();
        screen_.messages().clear();
        menu_.close();
        openTitle();
        return;
    }
    if (screen_.announcement().active()) { // ZIEL seg7:50D6
        const bool key = event.type == SDL_KEYDOWN
                         && (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_SPACE
                             || event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER);
        if (key || (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT))
            screen_.announcement().skip(SDL_GetTicks());
        return;
    }
    if (screen_.balance().active()) { // a click lets the show master go on (seg8:8D5C)
        const SDL_Keycode k = event.type == SDL_KEYDOWN ? event.key.keysym.sym : SDLK_UNKNOWN;
        const int button = event.type == SDL_MOUSEBUTTONDOWN ? event.button.button : 0;
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE || button == SDL_BUTTON_LEFT)
            screen_.balance().click(); // seg8:99BA / 99D3
        else if (k == SDLK_ESCAPE || button == SDL_BUTTON_RIGHT)
            screen_.balance().escape(SDL_GetTicks()); // seg8:99AC / 99DC
        return;
    }
    if (screen_.victory().active()) { // seg7:83EE: Esc / Space / Enter or a left click
        if (screen_.victory().closes(event, SDL_GetTicks())) screen_.victory().stop();
        return;
    }
    if (screen_.finish().active()) { // ZIEL seg7:50D6: DS:A29C = 0 ends the loop seg7:3536..3677
        const SDL_Keycode k = event.type == SDL_KEYDOWN ? event.key.keysym.sym : SDLK_UNKNOWN;
        if (k == SDLK_ESCAPE || k == SDLK_SPACE || k == SDLK_RETURN || k == SDLK_KP_ENTER
            || (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)) {
            sound_.stop(0); // seg5:607B(-1)
            screen_.finish().stop();
        }
        return;
    }
    if (screen_.pictureShown()) { // PICT seg6:0C8E: WM_KEYDOWN and WM_LBUTTONDOWN destroy it
        if (event.type == SDL_KEYDOWN
            || (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)) {
            screen_.closePicture();
            if (screen_.title()) openTitle();
        }
        return;
    }
    if (screen_.options().isOpen()) {
        const OptionsView::Result r = screen_.options().handle(toStage(event, layout_));
        if (const int fx = screen_.options().takeSound(); fx >= 0) sound_.play(2, fx);
        if (r == OptionsView::Result::RecompMenu) { // graphics, sound sets and more (recomp)
            recompMenu_.menu().openAt(menuSettings(), !screen_.title(), "Grafik");
            return;
        }
        if (r != OptionsView::Result::None) screen_.applySettings(screen_.options().settings());
        if (r == OptionsView::Result::Close) {
            screen_.options().close();
            if (screen_.title()) openTitle();
        }
        return;
    }
    // SLOT (seg2:6855) has the focus while it is on the map: Enter and Space stop the
    // reels or let it slide out (seg2:6B4A); other input waits (modal).
    // The window captures the mouse (seg2:68CE): a left click anywhere posts Enter
    // (seg2:695C), the right button (also its double click) Backspace (seg2:6970); a
    // second left click of a double click (WM_LBUTTONDBLCLK) is not handled.
    if (screen_.slotActive(screen_.presentationNow()) && !screen_.title()) {
        const Uint32 now = screen_.presentationNow();
        const SDL_Keycode key = event.type == SDL_KEYDOWN && !event.key.repeat ? event.key.keysym.sym : SDLK_UNKNOWN;
        const bool left = event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
                          && event.button.clicks % 2 == 1;
        const bool right = event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT;
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE || left) {
            screen_.slotKey(now);
        } else if (key == SDLK_ESCAPE || key == SDLK_BACKSPACE || right) {
            // seg2:69AD: only the Würfeln of a human (DS:9AD8, seg2:5C02) is taken back.
            Game& g = play_.game();
            bool reelsDrawn = false;
            if (g.rollCancellable() && !g.isComputer(g.currentPlayer()) && screen_.slotCancel(now, reelsDrawn)
                && g.cancelRoll(reelsDrawn))
                report(); // the turn menu opens again (seg2:068F)
        }
        if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) return;
    }
    if (screen_.overview().isOpen()) { screen_.overview().handle(event); return; }
    if (screen_.handleSurroundings(event)) return;
    if (screen_.stages().isOpen()) { // seg8:08D6, modal (seg6:0FB4)
        const StagesView::Result r = screen_.stages().handle(event);
        if (const int fx = screen_.stages().takeSound(); fx >= 0) sound_.play(2, fx);
        if (r == StagesView::Result::Accept) {
            Settings s = screen_.settings(); // seg8:0A6A: [Default] Rounds
            s.rounds = screen_.stages().stages();
            screen_.applySettings(s);
            std::string error;
            if (play_.newGame(pendingStatus_, pendingNames_, pendingSeed_, error, s.rounds)) {
                play_.game().setComputerExtraCards(screen_.settings().computerExtraCards);
                startGame(true);
            } else { std::cerr << "Neues Spiel: " << error << "\n"; openTitle(); }
        }
        return;
    }
    if (screen_.setup().isOpen()) { // seg1:0A60
        const SetupView::Result r = screen_.setup().handle(toStage(event, layout_));
        if (const int fx = screen_.setup().takeSound(); fx >= 0) sound_.play(2, fx);
        if (r == SetupView::Result::Start) {
            pendingSeed_ = uint16_t(SDL_GetTicks() ^ uint32_t(std::time(nullptr))); // seg1:02A9: time
            pendingStatus_ = screen_.setup().status();
            pendingNames_ = screen_.setup().names();
            // The extension's switch is kept in [Extensions] like the original's profile values.
            if (screen_.setup().extraCards() != screen_.settings().computerExtraCards) {
                Settings s = screen_.settings();
                s.computerExtraCards = screen_.setup().extraCards();
                screen_.applySettings(s);
            }
            screen_.setup().close();
            screen_.stages().open(screen_.settings().rounds); // seg4:0E54: TEMP, not in the demo
        } else if (r == SetupView::Result::Cancel) {
            screen_.setup().close();
            openTitle();
        }
        return;
    }
    if (saveDialog.isOpen()) { // seg8:B2F0: modal until Ok or Nein
        const SaveView::Result r = saveDialog.handle(event);
        if (r == SaveView::Result::Confirm && !saveDialog.saving()) { // load (title)
            std::string error;
            saveDialog.close();
            if (screen_.saveSlots().load(saveDialog.slot(), play_.game(), error)) startGame();
            else { // seg8:B3F9: Zaehler2
                sound_.play(2, 7);
                std::cerr << "Laden fehlgeschlagen: " << error << "\n";
                openTitle();
            }
            return;
        }
        if (r == SaveView::Result::Close && screen_.title()) { saveDialog.close(); openTitle(); return; }
        if (r == SaveView::Result::Confirm) {
            std::string error;
            const int slot = saveDialog.slot();
            const bool saved = screen_.saveSlots().save(slot, play_.game(), saveDescription(play_.game(), screen_.texts()), error);
            if (!saved) sound_.play(6, 17); // seg8:B4DC: Fate01 when saving fails
            if (saved)
                std::cout << "Gespeichert: " << screen_.saveSlots().file(slot).string() << "\n";
            else
                std::cerr << "Speichern fehlgeschlagen: " << error << "\n";
        }
        if (r != SaveView::Result::None) saveDialog.close();
        return;
    }
    if (handleGameDialog(event)) return;
    handleKey(event);
}

// The recomp menu over the paused game (docs/pause-menu.md).
void GameWindow::handleRecompMenu(const SDL_Event& event) {
    using Action = RecompMenu::Action;
    const Action a = recompMenu_.handle(event);
    if (a == Action::Changed) {
        applySettings(recompMenu_.menu().settings());
        if (const ScreenLayout& next = screen_.settings().widescreen ? wideLayout_ : baseLayout_;
            next.width != layout_.width) { // widescreen on / off at once
            layout_ = next;
            displayControl_.layoutChanged();
            screen_.layoutChanged();
        }
        displayControl_.apply(screen_.settings());
        if (screen_.options().isOpen()) screen_.options().open(screen_.settings()); // shows the same switches
    } else if (a == Action::MusicChanged) {
        std::string error;
        if (!applyMusicChoice(music_, recompMenu_.menu().music(), musicConfig_, error))
            std::cerr << "Musik: " << error << "\n";
    } else if (a != Action::None && a != Action::Resume) {
        recompMenu_.menu().close();
        if (a == Action::SaveGame) screen_.saveDialog().open(true);       // as Spiel speichern (1062)
        else if (a == Action::LoadGame) screen_.saveDialog().open(false);
        else if (a == Action::OriginalMenu) openMenu("RIGHTBTN");
        else if (a == Action::Quit) askQuit(false);
    }
    heldArrow_ = SDLK_UNKNOWN;
}

// The map and the main window's keys (seg4:018B ff.) when no window took the event.
void GameWindow::handleKey(const SDL_Event& event) {
    const bool click = event.type == SDL_MOUSEBUTTONDOWN;
    if (click && event.button.button == SDL_BUTTON_LEFT && !modal_ && !moving_ && !screen_.title()) {
        // seg6:1798 -> seg4:0F52: a click on the map opens the steering cross there.
        const Game& g = play_.game();
        const ScreenRect map = screen_.layout().mapClient(); // WM_LBUTTONDOWN of the client
        const bool onMap = event.button.x >= map.x && event.button.x < map.x + map.w && event.button.y >= map.y
                           && event.button.y < map.y + map.h;
        if (onMap && !screen_.surroundings() && g.currentPlayer() >= 0 && !g.isComputer(g.currentPlayer())
            && (g.phase() == Game::Phase::Move || g.phase() == Game::Phase::Arrived))
            screen_.mouseControl().open(event.button.x, event.button.y);
        return;
    }
    if (event.type != SDL_KEYDOWN) return;
    const SDL_Keycode key = event.key.keysym.sym;
    if (isArrow(key) && play_.game().phase() == Game::Phase::Move) { heldArrow_ = key; return; }
    if (key == SDLK_INSERT || key == SDLK_DELETE) { screen_.toggleZoom(); return; } // seg4:0212
    if (key >= SDLK_1 && key <= SDLK_4) { // seg4:018B -> seg6:0DA0(key - 1, 1): pause and memo
        Settings s = screen_.settings();
        s.pause = int(key - SDLK_1);
        screen_.applySettings(s);
        sound_.play(0, 35 + s.pause); // MEMO01-04 (seg6:0DFD / 0DCA / 0DDB / 0DEC)
        return;
    }
    if (key == SDLK_q && !screen_.title()) { askQuit(false); return; } // seg4:0259: Q -> seg4:0EDF
    if (key == SDLK_SPACE && play_.game().phase() != Game::Phase::Risiko && !screen_.title()) {
        screen_.centreOnPlayer(); // seg4:01B2 -> seg2:3216, 05A8
        return;
    }
    const bool risiko = play_.game().phase() == Game::Phase::Risiko;
    // seg4:01D1: Escape and Backspace open RIGHTBTN; the recomp menu takes Escape
    // (its entry "Originales Spielmenü" leads to RIGHTBTN).
    if (key == SDLK_ESCAPE && !risiko && !screen_.title() && !event.key.repeat)
        recompMenu_.menu().open(menuSettings(), true);
    else if ((key == SDLK_ESCAPE || key == SDLK_BACKSPACE) && !risiko) openMenu("RIGHTBTN");
    else if (play_.handleKey(key == SDLK_ESCAPE ? SDLK_BACKSPACE : key, event.key.keysym.mod)) report();
}

} // namespace dtvr
