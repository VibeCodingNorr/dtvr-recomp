// SPDX-License-Identifier: GPL-3.0-only
#include "record_canvas.hpp"

#include "sbs.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace dtvr {

PaintImage toPaintImage(const SDL_Surface* s) {
    PaintImage image(s->w, s->h);
    for (int y = 0; y < s->h; ++y)
        std::copy_n(static_cast<const uint8_t*>(s->pixels) + y * s->pitch, s->w, &image.pixels[size_t(y) * size_t(s->w)]);
    return image;
}

bool RecordCanvas::setBackdrop(const SDL_Surface* bitmap) {
    if (!bitmap || bitmap->format->BitsPerPixel != 8 || !bitmap->format->palette || bitmap->w != width_
        || bitmap->h != height_)
        return false;
    backdrop_ = toPaintImage(bitmap);
    const SDL_Palette* p = bitmap->format->palette;
    palette_.assign(p->colors, p->colors + p->ncolors);
    dib_ = PaintImage(width_, height_);
    screen_ = PaintImage(width_, height_);
    frames_.clear();
    slots_.clear();
    painter_.reset();
    dirty_ = true;
    return true;
}

void RecordCanvas::loadFile(size_t slot, const std::filesystem::path& file) {
    if (slots_.size() <= slot) slots_.resize(slot + 1);
    if (file.empty()) return;
    std::ifstream in(file, std::ios::binary);
    const std::vector<uint8_t> data{std::istreambuf_iterator<char>(in), {}};
    SbsFile sbs;
    try { sbs = parseSbs(data); } catch (const std::exception&) { return; }
    for (SbsFrame& f : sbs.frames) {
        PaintImage image(f.width, f.height);
        image.pixels = std::move(f.pixels);
        const std::string name = upperAscii(f.name);
        // seg6:406D searches the slots in order: an earlier file's frame of the same name wins.
        if (frames_.emplace(name, std::move(image)).second) slots_[slot].push_back(name);
    }
}

void RecordCanvas::fillFile(size_t slot, int x, int y) {
    if (slot >= slots_.size()) return;
    for (const std::string& name : slots_[slot]) {
        const auto it = frames_.find(name);
        if (it != frames_.end()) fillTransparent(it->second, backdrop_, x, y);
    }
}

void RecordCanvas::clear() {
    std::fill(dib_.pixels.begin(), dib_.pixels.end(), uint8_t(0));
    std::fill(screen_.pixels.begin(), screen_.pixels.end(), uint8_t(0));
    dirty_ = true;
}

void RecordCanvas::pass(const std::vector<PaintRecord>& records, bool present) {
    painter_.restore(backdrop_, dib_);
    painter_.show(records, frames_, dib_);
    if (present) {
        painter_.present(dib_, screen_);
        dirty_ = true;
    }
}

void RecordCanvas::paint(const std::vector<PaintRecord>& records) {
    painter_.paint(records, frames_, backdrop_, dib_, screen_, PaintRect{0, 0, width_, height_});
    dirty_ = true;
}

SDL_Texture* RecordCanvas::texture(SDL_Renderer* renderer) {
    if (!dirty_ && texture_) return texture_.get();
    if (screen_.pixels.empty() || palette_.empty()) return nullptr;
    Surface s(SDL_CreateRGBSurfaceWithFormat(0, width_, height_, 8, SDL_PIXELFORMAT_INDEX8), SDL_FreeSurface);
    if (!s) return nullptr;
    SDL_SetPaletteColors(s->format->palette, palette_.data(), 0, int(std::min<size_t>(palette_.size(), 256)));
    for (int y = 0; y < height_; ++y)
        std::copy_n(&screen_.pixels[size_t(y) * size_t(width_)], width_, static_cast<uint8_t*>(s->pixels) + y * s->pitch);
    texture_.reset(SDL_CreateTextureFromSurface(renderer, s.get()));
    dirty_ = false;
    return texture_.get();
}

} // namespace dtvr
