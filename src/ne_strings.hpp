// SPDX-License-Identifier: GPL-3.0-only
// String table resources (RT_STRING) of 16-bit NE files such as the language
// file dat/<lang>/rally_<lang>.dat. Bounds-checked; no SDL.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace dtvr {

// Text id -> raw Windows-1252 bytes. Throws std::runtime_error on a malformed file.
std::map<int, std::string> readNeStrings(const std::vector<uint8_t>& data);

// Windows-1252 (the encoding of the Win16 string resources) to UTF-8.
std::string ansiToUtf8(const std::string& text);

// Texts 120-127 (seg2: player record +1): fixed driver names of the eight slots.
constexpr int kDriverNameText = 120;

} // namespace dtvr
