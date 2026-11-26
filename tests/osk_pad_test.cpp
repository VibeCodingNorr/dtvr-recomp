// SPDX-License-Identifier: GPL-3.0-only
// The recomp on-screen keyboard's pad feel: repeat curve, stick hysteresis, the pad
// commands through the (remapped) PadMap, and the caret of the keyboard model.
#include "osk.hpp"
#include "osk_pad.hpp"

#include <cstdio>
#include <vector>

using namespace dtvr;
using C = OskCommand;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

namespace {

int count(const std::vector<OskCommand>& v, OskCommand c) {
    int n = 0;
    for (OskCommand x : v) n += x == c;
    return n;
}

void repeatCurve() {
    HoldRepeat r; // 250 delay, 110 -> 40 over 1000 ms
    CHECK(r.update(false, 0) == 0);
    CHECK(r.update(true, 1000) == 1);          // immediate first step
    CHECK(r.update(true, 1001) == 0);
    CHECK(r.update(true, 1249) == 0);          // still in the delay
    CHECK(r.update(true, 1250) == 1);          // first repetition after 250 ms
    CHECK(r.update(true, 1359) == 0);
    CHECK(r.update(true, 1360) == 1);          // 110 ms later
    CHECK(r.interval(0) == 110 && r.interval(500) == 75 && r.interval(1000) == 40 && r.interval(5000) == 40);
    // Steps per second while held, simulated at 60 fps: accelerating.
    HoldRepeat h;
    int t = 0, early = 0, late = 0;
    for (; t <= 3000; t += 16) {
        const int n = h.update(true, std::uint32_t(t));
        if (t > 250 && t <= 750) early += n;
        if (t > 2000 && t <= 2500) late += n;
    }
    CHECK(early >= 4 && early <= 6);  // ~100 ms intervals right after the delay
    CHECK(late >= 11 && late <= 14);  // ~40 ms (frame-quantised) when fully accelerated
    // Release resets: the next press is immediate and waits the full delay again.
    CHECK(h.update(false, 3010) == 0);
    CHECK(h.update(true, 3020) == 1 && h.update(true, 3100) == 0 && h.update(true, 3269) == 0
          && h.update(true, 3270) == 1);
    // A late call yields one step, not a burst.
    HoldRepeat l;
    l.update(true, 0);
    CHECK(l.update(true, 2000) == 1 && l.update(true, 2001) == 0);
    // Tick wrap-around.
    HoldRepeat w;
    CHECK(w.update(true, 0xFFFFFF00u) == 1 && w.update(true, 0xFFFFFF00u + 250) == 1);
}

void stickHysteresis() {
    StickHysteresis s;
    CHECK(s.update(0.45f, 0) == OskDirection::None);    // below engage
    CHECK(s.update(0.5f, 0) == OskDirection::Right);    // engages at 0.5
    CHECK(s.update(0.4f, 0) == OskDirection::Right);    // held above release
    CHECK(s.update(0.36f, 0.1f) == OskDirection::Right);
    CHECK(s.update(0.34f, 0) == OskDirection::None);    // released below 0.35
    CHECK(s.update(0.45f, 0) == OskDirection::None);    // needs engage again
    CHECK(s.update(0, -0.8f) == OskDirection::Up);
    // Diagonal: the dominant axis; turning past engage switches.
    s.reset();
    CHECK(s.update(0.7f, 0.6f) == OskDirection::Right);
    CHECK(s.update(0.42f, 0.45f) == OskDirection::Right); // dominant down below engage: stays
    CHECK(s.update(0.3f, 0.7f) == OskDirection::Down);
    CHECK(s.update(-0.6f, -0.6f) == OskDirection::Up);    // exact tie: vertical
    CHECK(s.update(-0.9f, 0.2f) == OskDirection::Left);
}

void padCommands() {
    const PadMap m;
    OskPad p;
    // Neutral start: A held when the keyboard opens does not type.
    CHECK(p.tick(m, {padBit(PadPhysical::A)}, 0).empty());
    CHECK(p.tick(m, {padBit(PadPhysical::A)}, 16).empty());
    CHECK(p.tick(m, {}, 32).empty());
    auto o = p.tick(m, {padBit(PadPhysical::A)}, 48);
    CHECK(o.size() == 1 && o[0] == C::Press);                        // on the press itself
    CHECK(p.tick(m, {padBit(PadPhysical::A)}, 1000).empty());         // A does not repeat
    // D-pad: immediate, then the curve.
    p.tick(m, {}, 1100);
    o = p.tick(m, {padBit(PadPhysical::DPadRight)}, 1116);
    CHECK(o.size() == 1 && o[0] == C::Right);
    CHECK(p.tick(m, {padBit(PadPhysical::DPadRight)}, 1300).empty());
    CHECK(count(p.tick(m, {padBit(PadPhysical::DPadRight)}, 1366), C::Right) == 1);
    // Another direction: at once, its own delay.
    o = p.tick(m, {padBit(PadPhysical::DPadDown)}, 1370);
    CHECK(o.size() == 1 && o[0] == C::Down);
    CHECK(p.tick(m, {padBit(PadPhysical::DPadDown)}, 1500).empty());
    p.tick(m, {}, 1600);
    // The stick with hysteresis.
    OskPadRaw stick;
    stick.stickX = -0.6f;
    CHECK(count(p.tick(m, stick, 1700), C::Left) == 1);
    stick.stickX = -0.4f;
    CHECK(p.tick(m, stick, 1716).empty());                           // still held, in the delay
    stick.stickX = -0.3f;
    CHECK(p.tick(m, stick, 1732).empty());
    stick.stickX = -0.6f;
    CHECK(count(p.tick(m, stick, 1748), C::Left) == 1);              // released, pressed again
    p.tick(m, {}, 1800);
    // X erases at once and repeats; Y shift, Start accept, B cancel on the press.
    CHECK(count(p.tick(m, {padBit(PadPhysical::X)}, 2000), C::Erase) == 1);
    CHECK(count(p.tick(m, {padBit(PadPhysical::X)}, 2250), C::Erase) == 1);
    p.tick(m, {}, 2300);
    CHECK(count(p.tick(m, {padBit(PadPhysical::Y)}, 2400), C::Shift) == 1);
    CHECK(count(p.tick(m, {padBit(PadPhysical::Start)}, 2416), C::Accept) == 1);
    CHECK(count(p.tick(m, {padBit(PadPhysical::B)}, 2432), C::Cancel) == 1);
    // LB / RB: the caret; free triggers: RT space, LT the focus to Fertig.
    p.tick(m, {}, 2448);
    CHECK(count(p.tick(m, {padBit(PadPhysical::LB)}, 2500), C::CaretLeft) == 1);
    CHECK(count(p.tick(m, {padBit(PadPhysical::RB)}, 2516), C::CaretRight) == 1);
    CHECK(count(p.tick(m, {padBit(PadPhysical::RT)}, 2532), C::Space) == 1);
    CHECK(count(p.tick(m, {padBit(PadPhysical::LT)}, 2548), C::FocusDone) == 1);
    // The original stays untouched: Back (OriginalMenu) and the stick clicks do nothing.
    p.tick(m, {}, 2564);
    CHECK(p.tick(m, {padBit(PadPhysical::Back) | padBit(PadPhysical::LeftStick)}, 2580).empty());
    // reset() starts neutral again.
    p.reset();
    CHECK(p.tick(m, {padBit(PadPhysical::X)}, 3000).empty());
}

void remappedCommands() {
    PadMap m;
    m.assign(PadAction::Confirm, PadPhysical::RT); // A now has no action
    m.assign(PadAction::Erase, PadPhysical::LT);
    m.assign(PadAction::Up, PadPhysical::Y);       // Y up, Cards gets the d-pad's up
    OskPad p;
    p.tick(m, {}, 0);
    CHECK(count(p.tick(m, {padBit(PadPhysical::RT)}, 10), C::Press) == 1);  // bound: no space
    CHECK(count(p.tick(m, {padBit(PadPhysical::RT)}, 20), C::Space) == 0);
    p.tick(m, {}, 30);
    auto o = p.tick(m, {padBit(PadPhysical::LT)}, 40);
    CHECK(count(o, C::Erase) == 1 && count(o, C::FocusDone) == 0);         // bound: no jump
    p.tick(m, {}, 50);
    CHECK(p.tick(m, {padBit(PadPhysical::A)}, 60).empty());                 // A unbound
    p.tick(m, {}, 70);
    CHECK(count(p.tick(m, {padBit(PadPhysical::Y)}, 80), C::Up) == 1);
    p.tick(m, {}, 90);
    CHECK(count(p.tick(m, {padBit(PadPhysical::DPadUp)}, 100), C::Shift) == 1);
    // Remapped page buttons move the caret.
    m.assign(PadAction::PrevPage, PadPhysical::B);
    p.tick(m, {}, 110);
    CHECK(count(p.tick(m, {padBit(PadPhysical::B)}, 120), C::CaretLeft) == 1);
}

void caret() {
    ScreenKeyboard k;
    k.open("Abc", 5);
    CHECK(k.caret() == 3);
    CHECK(k.caretStep(true).kind == ScreenKeyboard::Result::Kind::None);     // at the end
    CHECK(k.caretStep(false).sound == 8 && k.caret() == 2);
    CHECK(k.type("x").kind == ScreenKeyboard::Result::Kind::Typed && k.text() == "Abxc" && k.caret() == 3);
    CHECK(k.space().sound == 2 && k.text() == "Abx c" && k.caret() == 4);
    CHECK(k.type("y").kind == ScreenKeyboard::Result::Kind::Refused && k.text() == "Abx c"); // limit 5 counts the tail
    CHECK(k.erase().sound == 8 && k.text() == "Abxc" && k.caret() == 3);
    k.moveCaret(false); k.moveCaret(false); k.moveCaret(false);
    CHECK(k.caret() == 0 && !k.moveCaret(false));
    CHECK(k.erase().sound == 7 && k.text() == "Abxc");                       // nothing before the caret
    CHECK(k.type("\xc3\xa4").kind == ScreenKeyboard::Result::Kind::Typed && k.text() == "\xc3\xa4" "Abxc" && k.caret() == 1);
    CHECK(k.type("-").kind == ScreenKeyboard::Result::Kind::Refused);        // SBINPUT's filter
    CHECK(k.focusDone().sound == 8 && k.cells()[size_t(k.focus())].kind == ScreenKeyboard::Kind::Done);
    CHECK(k.focusDone().kind == ScreenKeyboard::Result::Kind::None);          // already there
    CHECK(k.move(ScreenKeyboard::Move::Up) && k.cells()[size_t(k.focus())].row == 3);
}

} // namespace

int main() {
    repeatCurve();
    stickHysteresis();
    padCommands();
    remappedCommands();
    caret();
    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
