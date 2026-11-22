#include "sbs.hpp"

#include <algorithm>
#include <stdexcept>

namespace dtvr {
namespace {

constexpr size_t kHeaderSize = 20;
constexpr size_t kEntrySize = 14;   // char name[10], uint32 offset
constexpr size_t kNameSize = 10;
constexpr size_t kFrameHeaderSize = 8; // uint16 width, uint16 height, 4 bytes (zero in all known files)

uint16_t u16(const std::vector<uint8_t>& d, size_t at) {
    if (at > d.size() || d.size() - at < 2) throw std::runtime_error("SBS: abgeschnitten");
    return uint16_t(d[at] | d[at + 1] << 8);
}

uint32_t u32(const std::vector<uint8_t>& d, size_t at) {
    return uint32_t(u16(d, at)) | uint32_t(u16(d, at + 2)) << 16;
}

} // namespace

SbsFile parseSbs(const std::vector<uint8_t>& d) {
    if (d.size() < kHeaderSize || d[0] != 'S' || d[1] != 'B' || d[2] != 'S' || d[3] != 'F')
        throw std::runtime_error("SBS: keine SBSF-Signatur");
    const size_t count = u16(d, 6);
    const size_t directory = u16(d, 18);
    if (directory < kHeaderSize || directory > d.size()
        || (d.size() - directory) / kEntrySize < count)
        throw std::runtime_error("SBS: Verzeichnis ausserhalb der Datei");

    SbsFile file;
    file.frames.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const size_t entry = directory + i * kEntrySize;
        SbsFrame frame;
        for (size_t c = 0; c < kNameSize && d[entry + c] != 0; ++c)
            frame.name.push_back(char(d[entry + c]));
        const size_t offset = u32(d, entry + kNameSize);
        if (offset > d.size() || d.size() - offset < kFrameHeaderSize)
            throw std::runtime_error("SBS: Bildkopf ausserhalb der Datei: " + frame.name);
        frame.width = u16(d, offset);
        frame.height = u16(d, offset + 2);
        // Rows are padded to 4 bytes and stored bottom-up, as in BMP.
        const size_t stride = (size_t(frame.width) + 3) & ~size_t(3);
        const size_t body = offset + kFrameHeaderSize;
        if (frame.height != 0 && (d.size() - body) / frame.height < stride)
            throw std::runtime_error("SBS: Bilddaten abgeschnitten: " + frame.name);
        frame.pixels.resize(size_t(frame.width) * frame.height);
        for (size_t y = 0; y < frame.height; ++y) {
            const auto row = d.begin() + body + (frame.height - 1 - y) * stride;
            std::copy(row, row + frame.width, frame.pixels.begin() + y * frame.width);
        }
        file.frames.push_back(std::move(frame));
    }
    return file;
}

} // namespace dtvr
