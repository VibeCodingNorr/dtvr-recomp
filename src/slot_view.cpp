// SPDX-License-Identifier: GPL-3.0-only
#include "slot_view.hpp"

#include "sbs.hpp"

#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

std::string upper(std::string s) {
    for (char& c : s) c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

SlotView::SlotView(SDL_Renderer* renderer, const fs::path& root) : renderer_(renderer), root_(root) {
    loadFrames("sbs/slot0.sbs"); // loaded at start (DS:02B1)
}

void SlotView::loadFrames(const fs::path& relative) {
    const fs::path file = findCaseInsensitive(root_, relative);
    if (file.empty()) return;
    for (const SbsFrame& f : parseSbs(readFile(file)).frames) {
        pixels_[upper(f.name)] = f.pixels;
        sizes_[upper(f.name)] = {f.width, f.height};
    }
}

SDL_Texture* SlotView::housing(int reels) {
    const std::string key = "housing" + std::to_string(reels);
    if (auto it = textures_.find(key); it != textures_.end()) return it->second.get();
    Texture t(nullptr, SDL_DestroyTexture);
    const fs::path path = findCaseInsensitive(root_, "bmp/slot" + std::to_string(reels) + ".bmp");
    if (!path.empty()) {
        auto surface = loadBmp(path);
        t.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
    }
    SDL_Texture* raw = t.get();
    textures_.emplace(key, std::move(t));
    return raw;
}

SDL_Texture* SlotView::frame(int reels, const std::string& name) {
    // Frames are drawn into the housing DIB, so they use the housing's palette
    // (seg2:6C63 sets colours 10..153 from it).
    const std::string key = std::to_string(reels) + ":" + upper(name);
    if (auto it = textures_.find(key); it != textures_.end()) return it->second.get();
    Texture t(nullptr, SDL_DestroyTexture);
    const auto px = pixels_.find(upper(name));
    const fs::path path = findCaseInsensitive(root_, "bmp/slot" + std::to_string(reels) + ".bmp");
    if (px != pixels_.end() && !path.empty()) {
        const auto palette = loadBmp(path);
        const auto [w, h] = sizes_[upper(name)];
        Surface s(SDL_CreateRGBSurfaceWithFormat(0, w, h, 8, SDL_PIXELFORMAT_INDEX8), SDL_FreeSurface);
        if (s && palette->format->palette) {
            SDL_SetSurfacePalette(s.get(), palette->format->palette);
            for (int y = 0; y < h; ++y)
                std::copy_n(&px->second[size_t(y) * size_t(w)], w, static_cast<uint8_t*>(s->pixels) + y * s->pitch);
            SDL_SetColorKey(s.get(), SDL_TRUE, 0);
            t.reset(SDL_CreateTextureFromSurface(renderer_, s.get()));
        }
    }
    SDL_Texture* raw = t.get();
    textures_.emplace(key, std::move(t));
    return raw;
}

void SlotView::start(const std::vector<int>& reels, Uint32 now, bool computer) {
    reels_ = reels;
    if (reels_.size() > 5) reels_.resize(5);
    // seg2:6426: "sbs\slot%i.sbs" (DS:2E8E) with the gear frames; only slot3-5 exist.
    if (reels_.size() >= 3) loadFrames("sbs/slot" + std::to_string(reels_.size()) + ".sbs");
    slidIn_ = now + kSlideTime; // seg2:6597: the window slides in first
    computer_ = computer;
    // State 1: a computer's loop posts the key at once (seg2:65B2), a human pulls the
    // lever with Enter or Space (seg2:6A09).
    lever_ = computer ? slidIn_ : kNever;
    stopKeyPass_ = -1;
    released_ = kNever;
    recompute();
}

void SlotView::play(SlotRecords& records, int last) const {
    records.pullLever();
    for (int p = 0; p <= last; ++p) {
        if (p == stopKeyPass_) records.stopReels(); // the key is handled before the pass's advance
        records.pass();
    }
}

void SlotView::recompute() {
    startPass_ = stopPass_ = -1;
    if (lever_ == kNever || reels_.empty()) return;
    SlotRecords records(reels_);
    records.pullLever();
    for (int p = 0; p < 5000 && stopPass_ < 0; ++p) {
        if (p == stopKeyPass_) records.stopReels();
        records.pass();
        if (startPass_ < 0 && records.reelsStarted()) startPass_ = p;
        if (records.state() == 4) stopPass_ = p;
    }
}

Uint32 SlotView::reelStart() const {
    // seg2:7671 runs when the lever's DS:2292 ends; DS:2338 swings it back meanwhile.
    return startPass_ < 0 ? kNever : lever_ + Uint32(startPass_) * kTickMs;
}

Uint32 SlotView::stopTime() const {
    return stopPass_ < 0 ? kNever : lever_ + Uint32(stopPass_) * kTickMs;
}

Uint32 SlotView::releaseTime() const {
    // seg2:664E: a computer's loop posts the key after 30 passes in state 4; a human
    // presses Enter or Space (seg2:6B08 -> 67B8).
    if (stopTime() == kNever) return kNever;
    return computer_ ? std::min(stopTime() + kComputerHold, released_) : released_;
}

bool SlotView::stopped(Uint32 now) const {
    if (reels_.empty()) return true;
    return now >= stopTime();
}

bool SlotView::active(Uint32 now) const {
    if (reels_.empty()) return false;
    const Uint32 out = releaseTime();
    return out == kNever || now < out + kSlideTime;
}

int SlotView::key(Uint32 now) {
    // The procedure does not ask who plays: a key also shortens a computer's spin.
    if (reels_.empty() || now < slidIn_) return 0;
    if (lever_ == kNever) { lever_ = now; recompute(); return 1; } // state 1: the lever (seg2:6A09)
    if (!stopped(now)) {
        // seg2:6AA2: state 2 ends the reels' turns (state 3), except while the lever
        // definition DS:2292 still plays; a further key in state 3 does nothing.
        const int pass = passAt(now);
        if (stopKeyPass_ >= 0 || pass <= startPass_) return 0;
        stopKeyPass_ = pass;
        recompute();
        return 3;
    }
    if (released_ == kNever) { released_ = now; return 2; }       // state 4: slide out
    return 0;
}

int SlotView::slideOffset(Uint32 now) const {
    const Uint32 in = slidIn_ - kSlideTime;
    if (now < slidIn_) // seg2:6777: each step first moves, then waits
        return kSlideStep * std::min(kSlideSteps, int((now - in) / kSlideMs) + 1);
    const Uint32 out = releaseTime();
    if (out == kNever || now < out) return kSlideStep * kSlideSteps;
    return kSlideStep * std::max(0, kSlideSteps - (int((now - out) / kSlideMs) + 1)); // seg2:6817
}

void SlotView::draw(const ScreenRect& map, Uint32 now) {
    if (!active(now)) return;
    const int n = int(reels_.size());
    SDL_Texture* box = housing(n);
    if (!box) return;
    int w = 0, h = 0;
    SDL_QueryTexture(box, nullptr, nullptr, &w, &h);
    const int x0 = map.x + map.w / 2 - w / 2, y0 = map.y - h + slideOffset(now); // seg2:6528 / 6717
    const SDL_Rect target{x0, y0, w, h};
    const SDL_Rect clip{map.x, map.y, map.w, map.h}; // a child of the map window
    SDL_RenderSetClipRect(renderer_, &clip);
    SDL_RenderCopy(renderer_, box, nullptr, &target);
    auto blit = [&](const std::string& name, int x, int y) {
        if (SDL_Texture* t = frame(n, name)) {
            int fw = 0, fh = 0;
            SDL_QueryTexture(t, nullptr, nullptr, &fw, &fh);
            const SDL_Rect r{x0 + x, y0 + y, fw, fh};
            SDL_RenderCopy(renderer_, t, nullptr, &r);
        }
    };
    // The records from the lever pull to this pass (seg2:6DCB), drawn into the housing.
    if (lever_ != kNever && now >= lever_) {
        SlotRecords records(reels_);
        play(records, passAt(now));
        for (const SlotFrame& f : records.frames()) blit(f.name, f.x, f.y);
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
}

} // namespace dtvr
