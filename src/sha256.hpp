// SPDX-License-Identifier: GPL-3.0-only
// SHA-256 (FIPS 180-4) for identifying original files by hash (no SDL). Used to
// recognise the edition of a rally.exe (src/edition.hpp); not a security boundary.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dtvr {

// Lower-case hexadecimal digest of `size` bytes at `data`.
std::string sha256Hex(const uint8_t* data, size_t size);
inline std::string sha256Hex(const std::vector<uint8_t>& data) { return sha256Hex(data.data(), data.size()); }
// Digest of a file, read in blocks. Throws std::runtime_error when it cannot be read.
std::string sha256FileHex(const std::filesystem::path& file);

} // namespace dtvr
