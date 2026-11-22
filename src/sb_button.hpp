// SPDX-License-Identifier: GPL-3.0-only
// The SBBUTTON control of rally.exe (a BUTTON superclass, seg1:0D7E, procedure
// seg1:0E78; rounds 60-62) for a group of sibling buttons: focus (state bit 4, red
// inside frame), pressed (bit 1, picture at (1, 1)), disabled (bit 0x10) and the
// keyboard walk between siblings. Evidence in docs/formats.md "SBBUTTON".
#pragma once

#include <SDL.h>

#include <vector>

namespace dtvr {

struct SbButton {
    int id = 0;
    SDL_Rect rect{};
    bool enabled = true;
};

class SbButtonGroup {
public:
    // `focus` is the id that keeps the focus after creation, -1 for none.
    void reset(std::vector<SbButton> buttons, int focus);
    void clear() { buttons_.clear(); focus_ = pressed_ = -1; }
    void setEnabled(int id, bool enabled);
    const std::vector<SbButton>& buttons() const { return buttons_; }
    int focus() const { return focus_; }
    void setFocus(int id) { focus_ = indexOf(id) >= 0 ? id : -1; } // -1: another control has it

    // `sound` is the effect for channel 2 (-1 none); `id` an activated button.
    struct Input { int id = -1; bool cancel = false; int sound = -1; };
    Input mouseDown(int x, int y);  // seg1:1311
    Input mouseUp(int x, int y);    // seg1:1398
    // seg1:10B4: arrows / Tab move the focus, space activates, Escape / Backspace
    // cancel; Return activates the focused button only with `returnActivates`
    // (bit 8 of the window data, seg1:0FD7), otherwise it is left to the caller.
    Input key(SDL_Keycode key, Uint16 mod, bool returnActivates);

    // Where the picture of a button goes (pressed: one pixel right and down).
    SDL_Rect pictureRect(const SbButton& button) const;
    // The red PS_INSIDEFRAME frame of width 5 around the focused button (seg1:19B6).
    void drawFocus(SDL_Renderer* renderer) const;

private:
    int indexOf(int id) const;
    Input activate(int index) const;
    bool moveFocus(int dx, int dy);

    std::vector<SbButton> buttons_;
    int focus_ = -1, pressed_ = -1;
};

} // namespace dtvr
