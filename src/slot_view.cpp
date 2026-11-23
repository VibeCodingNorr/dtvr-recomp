// SPDX-License-Identifier: GPL-3.0-only
#include "slot_view.hpp"

#include "sbs.hpp"

#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

// seg2:7674: reel frame positions (x, y) in the housing by number of reels.
constexpr int kReelX[5][5] = {{150}, {108, 240}, {122, 198, 274}, {166, 200, 252, 286}, {174, 222, 270, 246, 196}};
constexpr int kReelY[5][5] = {{146}, {204, 204}, {178, 162, 178}, {212, 160, 160, 212}, {154, 118, 154, 204, 204}};
// seg2:7C1E: tens and units of the sum display.
constexpr int kCountX[5][2] = {{144, 166}, {168, 190}, {192, 214}, {220, 242}, {216, 238}};
constexpr int kCountY[5] = {210, 204, 208, 212, 164};

std::string upper(std::string s) {
    for (char& c : s) c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

SlotView::SlotView(SDL_Renderer* renderer, const fs::path& root) : renderer_(renderer), root_(root) {
    const fs::path file = findCaseInsensitive(root_, "sbs/slot0.sbs"); // loaded at start (DS:02B1)
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
    started_ = now + kSlideTime; // seg2:6597: the window slides in first
    computer_ = computer;
    stoppedEarly_ = 0;
    released_ = kNever;
}

Uint32 SlotView::stopTime() const {
    const Uint32 regular = started_ + Uint32(stopTick(int(reels_.size()) - 1)) * kTickMs;
    return stoppedEarly_ != 0 && stoppedEarly_ < regular ? stoppedEarly_ : regular;
}

Uint32 SlotView::releaseTime() const {
    // seg2:664E: a computer's loop posts the key after 30 passes in state 4; a human
    // presses Enter or Space (seg2:6B08 -> 67B8).
    return computer_ ? stopTime() + kComputerHold : released_;
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

bool SlotView::key(Uint32 now) {
    if (reels_.empty() || computer_ || now < started_) return false;
    if (!stopped(now)) { // seg2:6AA2: state 2 ends the reels' runs at once (state 3)
        stoppedEarly_ = now;
        return true;
    }
    if (released_ == kNever) { released_ = now; return true; } // state 4: slide out
    return false;
}

int SlotView::slideOffset(Uint32 now) const {
    const Uint32 in = started_ - kSlideTime;
    if (now < started_) // seg2:6777: each step first moves, then waits
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
    const int tick = now < started_ ? 0 : int((now - started_) / kTickMs);
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        const int stop = stopTick(i);
        const int v = std::clamp(reels_[size_t(i)], 0, 6);
        std::string name;
        const bool early = stoppedEarly_ != 0 && now >= stoppedEarly_; // seg2:6AA2
        if (early) { name = "walze_" + std::to_string(v); sum += reels_[size_t(i)]; }
        else if (tick < stop - 4) name = "drehung" + std::to_string(tick / 2 % 4 + 1);
        else if (tick < stop) name = "walze_" + std::to_string(v) + "h";
        else { name = "walze_" + std::to_string(v); sum += reels_[size_t(i)]; }
        blit(name, kReelX[n - 1][i], kReelY[n - 1][i]);
    }
    if (tick >= stopTick(0) || (stoppedEarly_ != 0 && now >= stoppedEarly_)) { // seg2:7C0A updates the sum
        blit("count_" + std::to_string(sum / 10 % 10), kCountX[n - 1][0], kCountY[n - 1]);
        blit("count_" + std::to_string(sum % 10), kCountX[n - 1][1], kCountY[n - 1]);
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
}

} // namespace dtvr
