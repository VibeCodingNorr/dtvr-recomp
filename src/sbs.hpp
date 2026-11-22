// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dtvr {

// One named 8-bit indexed frame from an SBSF container.
// Pixels are stored top-down, width * height bytes, without row padding.
struct SbsFrame {
    std::string name;
    uint16_t width = 0;
    uint16_t height = 0;
    std::vector<uint8_t> pixels;
};

struct SbsFile {
    std::vector<SbsFrame> frames;
};

// Parses an SBSF container. Layout is documented in docs/formats.md.
// Throws std::runtime_error on any malformed or truncated input.
SbsFile parseSbs(const std::vector<uint8_t>& data);

} // namespace dtvr
