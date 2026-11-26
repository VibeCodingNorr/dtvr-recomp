// SPDX-License-Identifier: GPL-3.0-only
// The key logic of game controllers, free of SDL calls (key codes only) so it can be
// tested without a device:
//   JoystickRepeater  the original joystick 1 of rally.exe (seg2:8A30, round 79):
//                     direction and buttons 1 / 2 as keys with the 80 ms repetition.
//   PadInput          that layer unchanged plus the RECOMP ADDITION of the other pad
//                     buttons (Start, Back, X, Y, LB, RB, right stick), the release of a
//                     held direction when the pad stops counting (unplugged, option off,
//                     window unfocused) and the neutral start of held buttons.
// Evidence and the mapping in docs/formats.md "Joystick" / "Recomp pad layer".
#pragma once

#include "pad_map.hpp"

#include <SDL_keycode.h>

#include <vector>

namespace dtvr {

// One synthesized key: pressed or released, with the modifier it carries (Shift+Tab).
struct PadKey {
    SDL_Keycode key = SDLK_UNKNOWN;
    bool down = true;
    Uint16 mod = KMOD_NONE;
};

// seg2:8A30 for one joystick.
class JoystickRepeater {
public:
    // `direction` is SDLK_UP / DOWN / LEFT / RIGHT or SDLK_UNKNOWN; bit 0 of `buttons`
    // is button 1, bit 1 button 2.
    std::vector<PadKey> tick(SDL_Keycode direction, unsigned buttons);

private:
    SDL_Keycode previous_ = SDLK_UNKNOWN; // DS:94FC
    int repeat_ = 0;                      // DS:A0E2
    unsigned previousButtons_ = 0;        // DS:A0E6
};

// Button actions of the merged pad state, named after their default buttons (SDL
// GameController names, Xbox labels); which physical button triggers each is the
// PadMap's choice (translate below).
enum PadButton : unsigned {
    kPadA = 1u << 0,     // button 1 of the game port (original)
    kPadB = 1u << 1,     // button 2 of the game port (original)
    kPadX = 1u << 2,
    kPadY = 1u << 3,
    kPadBack = 1u << 4,  // View / Back / Select
    kPadStart = 1u << 5, // Menu / Start
    kPadLB = 1u << 6,
    kPadRB = 1u << 7,
};

// What the pads report in one tick.
struct PadState {
    SDL_Keycode direction = SDLK_UNKNOWN;  // d-pad or left stick (seg2:8C2B order)
    SDL_Keycode rightStick = SDLK_UNKNOWN; // right stick, same check order
    unsigned buttons = 0;                  // PadButton bits
};

// What the pads report physically in one tick, all pads merged.
struct PadRaw {
    unsigned buttons = 0;                  // padBit(PadPhysical) bits, triggers included
    SDL_Keycode leftStick = SDLK_UNKNOWN;  // seg2:8C2B order (up, down, left, right)
    SDL_Keycode rightStick = SDLK_UNKNOWN;
};

// The physical state through the mapping: the bound buttons of the four directions
// and the driving stick give `direction` (seg2:8C2B order over their union), the
// other stick `rightStick`, the other bound buttons their PadButton bits. With the
// default map this is the fixed assignment of pad rounds 1-3.
PadState translate(const PadMap& map, const PadRaw& raw);

// Waiting for the button a remapped action shall get (Steuerung page): buttons held
// when the wait starts are no choice; after the wait (chosen or cancelled) the pad
// stays silent until everything is released, so the chosen button is not at once
// also its new action.
class PadCapture {
public:
    struct Result {
        bool translate = true; // false: the pad layer gets nothing this tick
        int captured = -1;     // a PadPhysical newly pressed while capturing
    };
    Result tick(const PadRaw& raw, bool capturing);

private:
    unsigned previous_ = 0;
    bool capturing_ = false;
    bool waitRelease_ = false;
};

// Which window has the input, as far as the recomp buttons need to know. The game
// window fills it every tick (game_window_pad.cpp).
struct PadContext {
    bool aIsSpace = false;      // PLAYER / OPTION: Return is Start / nothing there, Space toggles
    bool backOpensMenu = false; // Backspace reaches RIGHTBTN (the map) or closes it (Zurück)
    bool surroundings = false;  // Umfeld is open: the right stick scrolls with arrows
    bool turnMenu = false;      // ONFIELD has the input: Y is "Karte einsetzen" (1031)
    bool yIsF9 = false;         // PLAYER / OPTION have the input: Y is F9 (their recomp extras)
    bool nameField = false;     // a PLAYER name field has the focus: A opens the on-screen keyboard, Y nothing
    bool keyboard = false;      // the on-screen keyboard is open: Start is done, Y shift
    bool townCards = false;     // the town dialog's header has the input: Y is its Karte item
};

// Result of one tick: keys for the SDL queue and the actions that are no key.
struct PadOutput {
    std::vector<PadKey> keys;
    bool pauseMenu = false; // Start: open / close the recomp menu (its Pause page)
    bool handCards = false; // Y in ONFIELD: WM_COMMAND 1031
    bool keyboard = false;       // A on a name field: open the on-screen keyboard (no key)
    bool keyboardAccept = false; // Start in the on-screen keyboard: take the name
    bool keyboardShift = false;  // Y in the on-screen keyboard: capitals on / off
    bool townCards = false;      // Y in the town dialog: its header item Karte
    // Right stick in Umfeld: one scroll step (an arrow) for GameScreen::handleSurroundings,
    // called directly, never an SDL key event (it must not become a driving direction).
    SDL_Keycode scroll = SDLK_UNKNOWN;
    int captured = -1; // a PadPhysical pressed while the Steuerung page waits for one (PadCapture)
};

class PadInput {
public:
    // One 80 ms tick while the pad counts ([Setup] Joystick on, a pad connected,
    // the window focused). The first tick after `release` (or ever) takes the
    // buttons held at that moment as already pressed.
    PadOutput tick(const PadState& state, const PadContext& context);
    // The pad stops counting: key-up of a held direction (the original clears its
    // steering direction DS:A77A only when the stick is centred; a vanished pad would
    // otherwise keep the car driving), and the next tick starts neutral again.
    PadOutput release();
    // Pads were connected or removed: buttons held now are not fresh presses.
    void devicesChanged(unsigned heldButtons) { suppressed_ |= heldButtons; }
    bool active() const { return active_; }

private:
    JoystickRepeater original_;
    JoystickRepeater rightStick_;
    unsigned previous_ = 0;   // recomp buttons after suppression, last tick
    unsigned suppressed_ = 0; // held since the start / a device change, ignored until released
    bool active_ = false;
};

} // namespace dtvr
