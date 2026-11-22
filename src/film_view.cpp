// SPDX-License-Identifier: GPL-3.0-only
#include "film_view.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace fs = std::filesystem;

namespace dtvr {

bool FilmView::start(int film, int loops, int canvasWidth, int canvasHeight, Uint32 now) {
    film_.reset();
    const fs::path path = findCaseInsensitive(root_, fs::path("butt") / "flc" / ("karte" + std::to_string(film) + ".flc"));
    if (path.empty()) return false;
    std::ifstream in(path, std::ios::binary);
    std::vector<uint8_t> data{std::istreambuf_iterator<char>(in), {}};
    try {
        film_ = std::make_unique<FlcFilm>(std::move(data));
    } catch (const std::runtime_error&) {
        return false;
    }
    if (!film_->decodeNext()) { film_.reset(); return false; }
    texture_.reset(SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, film_->width(),
                                     film_->height()));
    // Created at (x, y) = (10, 10), then centred by WM_CREATE (seg7:6477 -> seg6:0000).
    box_ = SDL_Rect{(canvasWidth - film_->width()) / 2, (canvasHeight - film_->height()) / 2, film_->width(),
                    film_->height()};
    loops_ = loops > 0 ? loops : 1;
    shown_ = 1;
    nextFrame_ = now + Uint32(film_->frameMs());
    upload();
    return true;
}

void FilmView::update(Uint32 now) {
    if (!film_) return;
    while (film_ && SDL_TICKS_PASSED(now, nextFrame_)) {
        if (shown_ >= film_->frames() * loops_) { film_.reset(); return; } // seg7:6619: all loops played
        if (!film_->decodeNext()) { film_.reset(); return; }
        ++shown_;
        nextFrame_ += Uint32(film_->frameMs());
        upload();
    }
}

void FilmView::upload() {
    if (!film_ || !texture_) return;
    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(texture_.get(), nullptr, &pixels, &pitch) != 0) return;
    const auto& palette = film_->palette();
    const auto& src = film_->pixels();
    for (int y = 0; y < film_->height(); ++y) {
        auto* row = reinterpret_cast<Uint32*>(static_cast<uint8_t*>(pixels) + y * pitch);
        for (int x = 0; x < film_->width(); ++x) {
            const size_t c = size_t(src[size_t(y) * size_t(film_->width()) + size_t(x)]) * 3;
            row[x] = 0xFF000000u | Uint32(palette[c]) << 16 | Uint32(palette[c + 1]) << 8 | palette[c + 2];
        }
    }
    SDL_UnlockTexture(texture_.get());
}

void FilmView::draw() {
    if (film_ && texture_) SDL_RenderCopy(renderer_, texture_.get(), nullptr, &box_);
}

} // namespace dtvr
