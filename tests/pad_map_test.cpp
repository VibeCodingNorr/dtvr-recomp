// SPDX-License-Identifier: GPL-3.0-only
// Tests for the pad remapping table (src/pad_map.cpp) and its [Gamepad] section in
// the recomp settings: defaults, swaps, lockout rules, round trip, malformed input.
#include "pad_map.hpp"
#include "settings.hpp"

#include <cstdio>
#include <string>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

namespace {

// Every action has a button of its own (nothing unreachable).
bool lockoutSafe(const PadMap& m) {
    if (!m.valid()) return false;
    for (int a = 0; a < kPadActionCount; ++a)
        if (m.actionOf(m.of(PadAction(a))) != PadAction(a)) return false;
    return true;
}

void defaults() {
    const PadMap m;
    CHECK(m.isDefault() && lockoutSafe(m));
    CHECK(m.of(PadAction::Confirm) == PadPhysical::A && m.of(PadAction::Cancel) == PadPhysical::B);
    CHECK(m.of(PadAction::Pause) == PadPhysical::Start && m.of(PadAction::OriginalMenu) == PadPhysical::Back);
    CHECK(m.of(PadAction::Erase) == PadPhysical::X && m.of(PadAction::Cards) == PadPhysical::Y);
    CHECK(m.of(PadAction::PrevPage) == PadPhysical::LB && m.of(PadAction::NextPage) == PadPhysical::RB);
    CHECK(m.of(PadAction::Up) == PadPhysical::DPadUp && m.of(PadAction::Right) == PadPhysical::DPadRight);
    CHECK(m.actionOf(PadPhysical::LT) == PadAction::kCount && !m.swapSticks);
    // Names round trip.
    for (int i = 0; i < kPadPhysicalCount; ++i) {
        PadPhysical p;
        CHECK(padPhysicalFromName(padPhysicalName(PadPhysical(i)), p) && p == PadPhysical(i));
        CHECK(*padPhysicalLabel(PadPhysical(i)) != '\0');
    }
    for (int i = 0; i < kPadActionCount; ++i) {
        PadAction a;
        CHECK(padActionFromName(padActionName(PadAction(i)), a) && a == PadAction(i));
    }
}

void swaps() {
    PadMap m;
    CHECK(m.assign(PadAction::Confirm, PadPhysical::LT) == PadAction::kCount);
    CHECK(m.of(PadAction::Confirm) == PadPhysical::LT && m.actionOf(PadPhysical::A) == PadAction::kCount);
    // A taken button: the other action gets the former one.
    CHECK(m.assign(PadAction::Pause, PadPhysical::B) == PadAction::Cancel);
    CHECK(m.of(PadAction::Pause) == PadPhysical::B && m.of(PadAction::Cancel) == PadPhysical::Start);
    CHECK(m.assign(PadAction::Pause, PadPhysical::B) == PadAction::kCount); // its own button: nothing
    CHECK(lockoutSafe(m) && !m.isDefault());
    // Out-of-range input changes nothing.
    const PadMap before = m;
    CHECK(m.assign(PadAction::kCount, PadPhysical::A) == PadAction::kCount && m == before);
    CHECK(m.assign(PadAction::Up, PadPhysical::kCount) == PadAction::kCount && m == before);
    // Any sequence of assignments keeps every action bound (menu actions included).
    unsigned seed = 12345;
    for (int i = 0; i < 5000; ++i) {
        seed = seed * 1103515245u + 12345u;
        m.assign(PadAction((seed >> 8) % kPadActionCount), PadPhysical((seed >> 20) % kPadPhysicalCount));
        if (!lockoutSafe(m)) { CHECK(lockoutSafe(m)); break; }
    }
}

void roundTrip() {
    Settings s;
    s.pad.assign(PadAction::Confirm, PadPhysical::RT);
    s.pad.assign(PadAction::Pause, PadPhysical::A);
    s.pad.assign(PadAction::Up, PadPhysical::Y);
    s.pad.swapSticks = true;
    s.masterVolume = 40;
    const std::string text = s.encode();
    CHECK(text.find("[Gamepad]\n") != std::string::npos && text.find("Confirm=RT\n") != std::string::npos);
    const Settings back = Settings::decode(text);
    CHECK(back.pad == s.pad && back.masterVolume == 40 && back.pad.swapSticks);
    // The original [Setup] keys are untouched by the section.
    CHECK(back.value == s.value);
    // Defaults round trip; a file without the section keeps the defaults.
    CHECK(Settings::decode(Settings{}.encode()).pad.isDefault());
    CHECK(Settings::decode("[Setup]\nWave=0\n").pad.isDefault());
    // Windows line ends.
    CHECK(Settings::decode("[Gamepad]\r\nConfirm=X\r\nErase=A\r\n").pad.of(PadAction::Confirm) == PadPhysical::X);
}

void malformed() {
    // Unknown keys, unknown buttons, empty values: ignored, the defaults stay.
    PadMap m = Settings::decode("[Gamepad]\nJump=A\nConfirm=Z\nCancel=\nPause\n=Start\nSticks=Sideways\n").pad;
    CHECK(m.isDefault());
    // An action read twice keeps its default.
    m = Settings::decode("[Gamepad]\nConfirm=LT\nConfirm=RT\n").pad;
    CHECK(m.of(PadAction::Confirm) == PadPhysical::A && lockoutSafe(m));
    // Two actions on one button: the non-default one goes back.
    m = Settings::decode("[Gamepad]\nConfirm=B\n").pad;
    CHECK(m.isDefault());
    m = Settings::decode("[Gamepad]\nConfirm=X\nErase=X\nCancel=A\n").pad;
    CHECK(lockoutSafe(m) && m.of(PadAction::Erase) == PadPhysical::X);
    CHECK(m.of(PadAction::Confirm) == PadPhysical::A && m.of(PadAction::Cancel) == PadPhysical::B);
    // A valid swap stays.
    m = Settings::decode("[Gamepad]\nConfirm=B\nCancel=A\n").pad;
    CHECK(m.of(PadAction::Confirm) == PadPhysical::B && m.of(PadAction::Cancel) == PadPhysical::A);
    // Gamepad names outside the section are no pad entries (and no other setting).
    m = Settings::decode("[Setup]\nConfirm=X\n[Gamepad]\n").pad;
    CHECK(m.isDefault());
    // A [Setup] key inside [Gamepad] is no setting.
    const Settings s = Settings::decode("[Gamepad]\nWave=0\n");
    CHECK(s.value[Settings::Wave] == 1 && s.pad.isDefault());
    // Sticks twice: normal.
    CHECK(!Settings::decode("[Gamepad]\nSticks=Swapped\nSticks=Swapped\n").pad.swapSticks);
    // Garbage bytes.
    CHECK(Settings::decode(std::string("[Gamepad]\n\xff\xfe=\x01\nUp=DPadUp\xff\n")).pad.isDefault());
}

} // namespace

int main() {
    defaults();
    swaps();
    roundTrip();
    malformed();
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("pad_map_test: OK");
    return failures != 0;
}
