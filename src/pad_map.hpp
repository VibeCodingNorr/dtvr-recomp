// SPDX-License-Identifier: GPL-3.0-only
// RECOMP ADDITION: which physical pad button triggers which pad action (the remapping
// of the Steuerung page, docs/pause-menu.md). The game itself always receives the
// same actions (Return, Escape, arrows ... of src/pad_input.cpp); only the button
// that triggers them is chosen here. No SDL.
//
// Rules: every action is bound to exactly one physical button and no button to two
// actions, so an assignment to a taken button swaps the two actions' buttons and
// no action can ever become unreachable (the Start / confirm / back / d-pad needed
// to open and walk the recomp menu included). The sticks are no buttons: one drives
// (like the d-pad, the original joystick), the other scrolls Umfeld; their roles can
// only be swapped as a pair. The Guide / Home button is left to the system.
#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace dtvr {

// Physical inputs that can be bound (SDL GameController names, Xbox labels). The
// triggers count as buttons when pressed past half way.
enum class PadPhysical : int {
    A, B, X, Y, Back, Start, LeftStick, RightStick, LB, RB, DPadUp, DPadDown, DPadLeft, DPadRight, LT, RT,
    kCount
};
constexpr int kPadPhysicalCount = int(PadPhysical::kCount);
constexpr unsigned padBit(PadPhysical p) { return 1u << unsigned(p); }

// Actions of the pad layer. Up..Cancel are the original joystick (seg2:8A30), the
// rest the recomp layer.
enum class PadAction : int {
    Up, Down, Left, Right, // arrows (d-pad by default)
    Confirm,               // button 1: Return (A)
    Cancel,                // button 2: Escape (B)
    Pause,                 // recomp menu (Start)
    OriginalMenu,          // RIGHTBTN via Backspace (View / Back)
    Erase,                 // Backspace (X)
    Cards,                 // hand cards / F9 (Y)
    PrevPage,              // Shift+Tab (LB)
    NextPage,              // Tab (RB)
    kCount
};
constexpr int kPadActionCount = int(PadAction::kCount);

struct PadMap {
    std::array<PadPhysical, kPadActionCount> button = defaultButtons();
    bool swapSticks = false; // false: left stick drives, right stick scrolls Umfeld

    static constexpr std::array<PadPhysical, kPadActionCount> defaultButtons() {
        return {PadPhysical::DPadUp, PadPhysical::DPadDown, PadPhysical::DPadLeft, PadPhysical::DPadRight,
                PadPhysical::A,      PadPhysical::B,        PadPhysical::Start,    PadPhysical::Back,
                PadPhysical::X,      PadPhysical::Y,        PadPhysical::LB,       PadPhysical::RB};
    }
    PadPhysical of(PadAction a) const { return button[size_t(a)]; }
    // The action bound to `p`, or kCount when none.
    PadAction actionOf(PadPhysical p) const;
    // Binds `p` to `a`. When another action had `p`, it takes `a`'s former button
    // and is returned (else kCount): every action stays bound, no button twice.
    PadAction assign(PadAction a, PadPhysical p);
    bool isDefault() const { return button == defaultButtons() && !swapSticks; }
    // Every action bound and no button twice (always true after assign / decode).
    bool valid() const;
    bool operator==(const PadMap& o) const { return button == o.button && swapSticks == o.swapSticks; }
    bool operator!=(const PadMap& o) const { return !(*this == o); }

    // The [Gamepad] lines of the recomp settings ("Confirm=A", ..., "Sticks=Normal").
    std::string encode() const;
    // From the key / value pairs of that section: unknown keys or values are ignored,
    // an action given twice keeps its default, and buttons then bound to two actions
    // go back to their defaults until every action has its own button.
    static PadMap decode(const std::vector<std::pair<std::string, std::string>>& lines);
};

// Names in the settings file (stable, English) and labels for the menu (German).
const char* padPhysicalName(PadPhysical p);
const char* padActionName(PadAction a);
const char* padPhysicalLabel(PadPhysical p);
bool padPhysicalFromName(const std::string& name, PadPhysical& p);
bool padActionFromName(const std::string& name, PadAction& a);

} // namespace dtvr
