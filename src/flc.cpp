// SPDX-License-Identifier: GPL-3.0-only
#include "flc.hpp"

#include <cstring>
#include <stdexcept>

namespace dtvr {
namespace {

uint16_t u16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t u32(const uint8_t* p) { return uint32_t(p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24); }

} // namespace

FlcFilm::FlcFilm(std::vector<uint8_t> data) : data_(std::move(data)) {
    if (data_.size() < 128) throw std::runtime_error("FLC: Kopf zu kurz");
    const uint16_t magic = u16(&data_[4]);
    if (magic != 0xAF12 && magic != 0xAF11) throw std::runtime_error("FLC: unbekannte Kennung");
    frames_ = u16(&data_[6]);
    width_ = u16(&data_[8]);
    height_ = u16(&data_[10]);
    const uint32_t speed = u32(&data_[16]);
    frameMs_ = magic == 0xAF12 ? int(speed) : int(speed * 1000 / 70);
    if (frameMs_ <= 0) frameMs_ = 70;
    if (width_ <= 0 || height_ <= 0 || width_ > 4096 || height_ > 4096 || frames_ <= 0)
        throw std::runtime_error("FLC: ungueltige Groesse");
    size_t off = magic == 0xAF12 && u32(&data_[80]) ? u32(&data_[80]) : 128;
    while (off + 16 <= data_.size() && int(offsets_.size()) <= frames_) { // frames + ring frame
        const uint32_t size = u32(&data_[off]);
        const uint16_t type = u16(&data_[off + 4]);
        if (size < 16 || off + size > data_.size()) break;
        if (type == 0xF1FA) offsets_.push_back(off);
        off += size; // prefix chunks (0xF100) are skipped
    }
    if (int(offsets_.size()) < frames_) throw std::runtime_error("FLC: Bilder fehlen");
    pixels_.assign(size_t(width_) * size_t(height_), 0);
}

bool FlcFilm::decodeNext() {
    const int count = int(offsets_.size());
    if (next_ >= count) next_ = count > frames_ ? 1 : 0; // after the ring frame: frame 1
    return decode(next_++);
}

bool FlcFilm::decode(int index) {
    if (index < 0 || index >= int(offsets_.size())) return false;
    const size_t off = offsets_[size_t(index)];
    const uint32_t size = u32(&data_[off]);
    const uint16_t chunks = u16(&data_[off + 6]);
    size_t p = off + 16;
    const size_t end = off + size;
    for (int c = 0; c < chunks; ++c) {
        if (p + 6 > end) return false;
        const uint32_t csize = u32(&data_[p]);
        const uint16_t type = u16(&data_[p + 4]);
        if (csize < 6 || p + csize > end) return false;
        if (!chunk(type, &data_[p + 6], csize - 6)) return false;
        p += csize;
    }
    next_ = index + 1;
    return true;
}

bool FlcFilm::chunk(uint16_t type, const uint8_t* p, size_t size) {
    switch (type) {
    case 4: return color(p, size, 0);   // COLOR_256
    case 11: return color(p, size, 2);  // COLOR_64
    case 7: return deltaFlc(p, size);
    case 12: return deltaFli(p, size);
    case 13: std::fill(pixels_.begin(), pixels_.end(), 0); return true;
    case 15: return byteRun(p, size);
    case 16:
        if (size < pixels_.size()) return false;
        std::memcpy(pixels_.data(), p, pixels_.size());
        return true;
    default: return true; // PSTAMP and unknown chunks
    }
}

bool FlcFilm::color(const uint8_t* p, size_t size, int shift) {
    if (size < 2) return false;
    const uint16_t packets = u16(p);
    size_t i = 2;
    int index = 0;
    for (int k = 0; k < packets; ++k) {
        if (i + 2 > size) return false;
        index += p[i];
        int count = p[i + 1] ? p[i + 1] : 256;
        i += 2;
        if (i + size_t(count) * 3 > size || index + count > 256) return false;
        for (int c = 0; c < count; ++c, ++index)
            for (int b = 0; b < 3; ++b) palette_[size_t(index) * 3 + size_t(b)] = uint8_t(p[i++] << shift);
    }
    return true;
}

bool FlcFilm::byteRun(const uint8_t* p, size_t size) {
    size_t i = 0;
    for (int y = 0; y < height_; ++y) {
        if (i >= size) return false;
        ++i; // packet count byte (unused, the width rules)
        uint8_t* row = &pixels_[size_t(y) * size_t(width_)];
        int x = 0;
        while (x < width_) {
            if (i >= size) return false;
            const int8_t n = int8_t(p[i++]);
            if (n > 0) { // run of one byte
                if (i >= size || x + n > width_) return false;
                std::memset(row + x, p[i++], size_t(n));
                x += n;
            } else if (n < 0) { // literal bytes
                const int count = -n;
                if (i + size_t(count) > size || x + count > width_) return false;
                std::memcpy(row + x, p + i, size_t(count));
                i += size_t(count);
                x += count;
            } else {
                return false;
            }
        }
    }
    return true;
}

bool FlcFilm::deltaFlc(const uint8_t* p, size_t size) {
    if (size < 2) return false;
    int lines = u16(p);
    size_t i = 2;
    int y = 0;
    while (lines > 0) {
        if (i + 2 > size) return false;
        const int16_t word = int16_t(u16(p + i));
        i += 2;
        if ((word & 0xC000) == 0xC000) { y -= word; continue; }           // skip lines
        if ((word & 0xC000) == 0x8000) {                                   // last byte of the line
            if (y >= height_) return false;
            pixels_[size_t(y) * size_t(width_) + size_t(width_ - 1)] = uint8_t(word & 0xFF);
            continue;
        }
        if (y < 0 || y >= height_) return false;
        uint8_t* row = &pixels_[size_t(y) * size_t(width_)];
        int x = 0;
        for (int packet = 0; packet < word; ++packet) {
            if (i + 2 > size) return false;
            x += p[i];
            const int8_t n = int8_t(p[i + 1]);
            i += 2;
            if (n > 0) { // literal words
                const int bytes = n * 2;
                if (i + size_t(bytes) > size || x + bytes > width_) return false;
                std::memcpy(row + x, p + i, size_t(bytes));
                i += size_t(bytes);
                x += bytes;
            } else if (n < 0) { // repeated word
                const int bytes = -n * 2;
                if (i + 2 > size || x + bytes > width_) return false;
                for (int b = 0; b < bytes; b += 2) { row[x + b] = p[i]; row[x + b + 1] = p[i + 1]; }
                i += 2;
                x += bytes;
            }
        }
        ++y;
        --lines;
    }
    return true;
}

bool FlcFilm::deltaFli(const uint8_t* p, size_t size) {
    if (size < 4) return false;
    int y = u16(p);
    const int lines = u16(p + 2);
    size_t i = 4;
    for (int l = 0; l < lines; ++l, ++y) {
        if (i >= size || y >= height_) return false;
        const int packets = p[i++];
        uint8_t* row = &pixels_[size_t(y) * size_t(width_)];
        int x = 0;
        for (int k = 0; k < packets; ++k) {
            if (i + 2 > size) return false;
            x += p[i];
            const int8_t n = int8_t(p[i + 1]);
            i += 2;
            if (n > 0) {
                if (i + size_t(n) > size || x + n > width_) return false;
                std::memcpy(row + x, p + i, size_t(n));
                i += size_t(n);
                x += n;
            } else if (n < 0) {
                if (i >= size || x - n > width_) return false;
                std::memset(row + x, p[i++], size_t(-n));
                x -= n;
            }
        }
    }
    return true;
}

} // namespace dtvr
