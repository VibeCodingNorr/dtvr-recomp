// SPDX-License-Identifier: GPL-3.0-only
#include "osk_pad.hpp"

#include <algorithm>
#include <cmath>

namespace dtvr {
namespace {

bool reached(std::uint32_t now, std::uint32_t due) { return std::int32_t(now - due) >= 0; } // wraps like SDL ticks

OskCommand command(OskDirection d) {
    switch (d) {
    case OskDirection::Up: return OskCommand::Up;
    case OskDirection::Down: return OskCommand::Down;
    case OskDirection::Left: return OskCommand::Left;
    default: return OskCommand::Right;
    }
}

} // namespace

std::uint32_t HoldRepeat::interval(std::uint32_t repeating) const {
    if (repeating >= timing_.ramp || timing_.first <= timing_.fast) return timing_.fast;
    return timing_.first - std::uint32_t(std::uint64_t(timing_.first - timing_.fast) * repeating / timing_.ramp);
}

int HoldRepeat::update(bool held, std::uint32_t now) {
    if (!held) {
        held_ = false;
        return 0;
    }
    if (!held_) { // the press itself: at once
        held_ = true;
        next_ = repeatStart_ = now + timing_.delay;
        return 1;
    }
    if (!reached(now, next_)) return 0;
    next_ += interval(next_ - repeatStart_);
    if (reached(now, next_)) next_ = now + interval(now - repeatStart_); // a late call: no burst
    return 1;
}

OskDirection StickHysteresis::update(float x, float y) {
    const float ax = std::fabs(x), ay = std::fabs(y), travel = std::max(ax, ay);
    // A diagonal counts as its dominant axis; an exact tie as vertical (seg2:8C2B's order).
    OskDirection dominant = OskDirection::None;
    if (travel > 0) dominant = ay >= ax ? (y < 0 ? OskDirection::Up : OskDirection::Down)
                                        : (x < 0 ? OskDirection::Left : OskDirection::Right);
    const auto along = [&](OskDirection d) {
        switch (d) {
        case OskDirection::Up: return -y;
        case OskDirection::Down: return y;
        case OskDirection::Left: return -x;
        case OskDirection::Right: return x;
        default: return 0.0f;
        }
    };
    if (current_ == OskDirection::None) {
        if (travel >= kEngage) current_ = dominant;
    } else if (dominant != current_ && travel >= kEngage) {
        current_ = dominant; // turned to another direction past the engage point
    } else if (along(current_) < kRelease) {
        current_ = OskDirection::None;
    }
    return current_;
}

void OskPad::reset() {
    active_ = false;
    direction_ = OskDirection::None;
    stick_.reset();
    move_.reset();
    erase_.reset();
    caretLeft_.reset();
    caretRight_.reset();
}

std::vector<OskCommand> OskPad::tick(const PadMap& map, const OskPadRaw& raw, std::uint32_t now) {
    std::vector<OskCommand> out;
    if (!active_) { // neutral start
        reset();
        active_ = true;
        suppressed_ = raw.buttons;
        previous_ = raw.buttons;
        directionSuppressed_ = true;
    }
    suppressed_ &= raw.buttons;
    const unsigned buttons = raw.buttons & ~suppressed_;
    const unsigned pressed = buttons & ~previous_;
    previous_ = buttons;
    const auto on = [&](PadAction a) { return (buttons & padBit(map.of(a))) != 0; };
    const auto fresh = [&](PadAction a) { return (pressed & padBit(map.of(a))) != 0; };
    // A free trigger (bound to no action) is a keyboard shortcut.
    const auto freeTrigger = [&](PadPhysical p) {
        return map.actionOf(p) == PadAction::kCount && (pressed & padBit(p)) != 0;
    };

    // Directions: the bound direction buttons in seg2:8C2B's order, else the stick.
    const OskDirection stick = stick_.update(raw.stickX, raw.stickY);
    OskDirection d = on(PadAction::Up)     ? OskDirection::Up
                     : on(PadAction::Down) ? OskDirection::Down
                     : on(PadAction::Left) ? OskDirection::Left
                     : on(PadAction::Right) ? OskDirection::Right
                                            : stick;
    if (d == OskDirection::None) directionSuppressed_ = false;
    if (directionSuppressed_) d = OskDirection::None; // a stick pushed when the keyboard opened
    if (d != direction_) { // a new direction steps at once and starts its own curve
        move_.reset();
        direction_ = d;
    }
    for (int n = move_.update(d != OskDirection::None, now); n > 0; --n) out.push_back(command(d));

    if (fresh(PadAction::Cards)) out.push_back(OskCommand::Shift);
    if (fresh(PadAction::Confirm)) out.push_back(OskCommand::Press);
    if (freeTrigger(PadPhysical::RT)) out.push_back(OskCommand::Space);
    for (int n = erase_.update(on(PadAction::Erase), now); n > 0; --n) out.push_back(OskCommand::Erase);
    for (int n = caretLeft_.update(on(PadAction::PrevPage), now); n > 0; --n) out.push_back(OskCommand::CaretLeft);
    for (int n = caretRight_.update(on(PadAction::NextPage), now); n > 0; --n) out.push_back(OskCommand::CaretRight);
    if (freeTrigger(PadPhysical::LT)) out.push_back(OskCommand::FocusDone);
    if (fresh(PadAction::Pause)) out.push_back(OskCommand::Accept);
    if (fresh(PadAction::Cancel)) out.push_back(OskCommand::Cancel);
    return out;
}

} // namespace dtvr
