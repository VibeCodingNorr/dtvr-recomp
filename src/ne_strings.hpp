// SPDX-License-Identifier: GPL-3.0-only
// String table (RT_STRING) and menu (RT_MENU) resources of 16-bit NE files such
// as the language file dat/<lang>/rally_<lang>.dat. Bounds-checked; no SDL.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace dtvr {

// Text id -> raw Windows-1252 bytes. Throws std::runtime_error on a malformed file.
std::map<int, std::string> readNeStrings(const std::vector<uint8_t>& data);

// One entry of a menu resource (MENUITEMTEMPLATE). Flags as in Windows:
// 0x01 grayed, 0x02 disabled, 0x08 checked, 0x10 popup, 0x80 last item.
struct NeMenuItem {
    int id = 0;
    uint16_t flags = 0;
    std::string text;   // raw Windows-1252
};
// The items of the first popup of the named menu resource (as GetSubMenu(menu, 0)),
// name compared without regard to case. Empty if there is no such menu; throws
// std::runtime_error on a malformed file.
std::vector<NeMenuItem> readNeMenu(const std::vector<uint8_t>& data, const std::string& name);

// Windows-1252 (the encoding of the Win16 string resources) to UTF-8.
std::string ansiToUtf8(const std::string& text);

// Texts 120-127 (seg2: player record +1): fixed driver names of the eight slots.
constexpr int kDriverNameText = 120;

} // namespace dtvr
