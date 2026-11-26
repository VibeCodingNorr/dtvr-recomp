// SPDX-License-Identifier: GPL-3.0-only
#include "pad_map.hpp"

#include <sstream>

namespace dtvr {
namespace {

constexpr const char* kPhysicalNames[kPadPhysicalCount] = {
    "A", "B", "X", "Y", "Back", "Start", "LeftStick", "RightStick", "LB", "RB",
    "DPadUp", "DPadDown", "DPadLeft", "DPadRight", "LT", "RT"};
constexpr const char* kPhysicalLabels[kPadPhysicalCount] = {
    "A", "B", "X", "Y", "Ansicht", "Start", "Linker Stick (Klick)", "Rechter Stick (Klick)", "LB", "RB",
    "Kreuz hoch", "Kreuz runter", "Kreuz links", "Kreuz rechts", "LT", "RT"};
constexpr const char* kActionNames[kPadActionCount] = {
    "Up", "Down", "Left", "Right", "Confirm", "Cancel", "Pause", "OriginalMenu", "Erase", "Cards",
    "PrevPage", "NextPage"};

} // namespace

const char* padPhysicalName(PadPhysical p) {
    const int i = int(p);
    return i >= 0 && i < kPadPhysicalCount ? kPhysicalNames[i] : "";
}

const char* padPhysicalLabel(PadPhysical p) {
    const int i = int(p);
    return i >= 0 && i < kPadPhysicalCount ? kPhysicalLabels[i] : "";
}

const char* padActionName(PadAction a) {
    const int i = int(a);
    return i >= 0 && i < kPadActionCount ? kActionNames[i] : "";
}

bool padPhysicalFromName(const std::string& name, PadPhysical& p) {
    for (int i = 0; i < kPadPhysicalCount; ++i)
        if (name == kPhysicalNames[i]) { p = PadPhysical(i); return true; }
    return false;
}

bool padActionFromName(const std::string& name, PadAction& a) {
    for (int i = 0; i < kPadActionCount; ++i)
        if (name == kActionNames[i]) { a = PadAction(i); return true; }
    return false;
}

PadAction PadMap::actionOf(PadPhysical p) const {
    for (int i = 0; i < kPadActionCount; ++i)
        if (button[size_t(i)] == p) return PadAction(i);
    return PadAction::kCount;
}

PadAction PadMap::assign(PadAction a, PadPhysical p) {
    const int ai = int(a);
    if (ai < 0 || ai >= kPadActionCount || int(p) < 0 || int(p) >= kPadPhysicalCount) return PadAction::kCount;
    const PadAction other = actionOf(p);
    if (other == a) return PadAction::kCount;
    if (other != PadAction::kCount) button[size_t(other)] = button[size_t(ai)]; // the swap
    button[size_t(ai)] = p;
    return other;
}

bool PadMap::valid() const {
    unsigned used = 0;
    for (PadPhysical p : button) {
        if (int(p) < 0 || int(p) >= kPadPhysicalCount || (used & padBit(p))) return false;
        used |= padBit(p);
    }
    return true;
}

std::string PadMap::encode() const {
    std::ostringstream o;
    for (int i = 0; i < kPadActionCount; ++i) o << kActionNames[i] << '=' << padPhysicalName(button[size_t(i)]) << '\n';
    o << "Sticks=" << (swapSticks ? "Swapped" : "Normal") << '\n';
    return o.str();
}

PadMap PadMap::decode(const std::vector<std::pair<std::string, std::string>>& lines) {
    PadMap m;
    const auto defaults = defaultButtons();
    std::array<int, kPadActionCount> seen{};
    int sticksSeen = 0;
    for (const auto& [key, value] : lines) {
        if (key == "Sticks") {
            ++sticksSeen;
            if (value == "Swapped") m.swapSticks = true;
            else if (value == "Normal") m.swapSticks = false;
            continue;
        }
        PadAction a;
        PadPhysical p;
        if (!padActionFromName(key, a)) continue;
        ++seen[size_t(a)];
        if (padPhysicalFromName(value, p)) m.button[size_t(a)] = p;
    }
    if (sticksSeen > 1) m.swapSticks = false;
    for (int i = 0; i < kPadActionCount; ++i)
        if (seen[size_t(i)] > 1) m.button[size_t(i)] = defaults[size_t(i)];
    // Each pass resets at least one non-default entry of a shared button; the defaults
    // are distinct, so this ends with every action on its own button.
    for (bool clash = true; clash;) {
        clash = false;
        for (int i = 0; i < kPadActionCount; ++i)
            for (int j = 0; j < kPadActionCount; ++j) {
                if (i == j || m.button[size_t(i)] != m.button[size_t(j)]) continue;
                for (int k : {i, j})
                    if (m.button[size_t(k)] != defaults[size_t(k)]) { m.button[size_t(k)] = defaults[size_t(k)]; clash = true; }
            }
    }
    return m;
}

} // namespace dtvr
