// SPDX-License-Identifier: GPL-3.0-only
#include "risiko_view.hpp"

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWindowW = 0xF6, kWindowH = 0x9E; // CreateWindow seg7:67F7
constexpr int kFrameW = 0xE6, kFrameH = 0x8E;   // the risiko.sbs frames (seg7:6E87)
constexpr int kSplitX = 0x64, kButtonTop = 0x50; // seg7:6ED3 / 6EB5: pressed parts of r-button

} // namespace

RisikoView::RisikoView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout)
    : renderer_(renderer), root_(root), layout_(layout) {}

void RisikoView::load() {
    if (loaded_) return;
    loaded_ = true;
    Surface palette = loadBmp(findCaseInsensitive(root_, fs::path("butt") / "ziel.bmp"));
    const SDL_Palette* pal = palette && palette->format->palette ? palette->format->palette : nullptr;
    loadSbsTextures(renderer_, findCaseInsensitive(root_, fs::path("sbs") / "risiko.sbs"), pal, frames_); // DS:54C2
}

SDL_Rect RisikoView::frame() const {
    // Window centred by seg6:0000 (seg7:6D42); the frame fills its client area.
    const int wx = (layout_.width - kWindowW) / 2, wy = (layout_.height - kWindowH) / 2;
    return SDL_Rect{wx + (kWindowW - kFrameW) / 2, wy + (kWindowH - kFrameH) / 2, kFrameW, kFrameH};
}

int RisikoView::buttonAt(int x, int y) const {
    const SDL_Rect f = frame();
    x -= f.x;
    y -= f.y;
    if (y < 0x50 || y > 0x8C) return 0; // seg7:6C83
    if (x >= 4 && x <= 0x50) return 1;
    if (x >= 0x98 && x <= 0xE2) return 2;
    return 0;
}

void RisikoView::draw(int light, Uint32 now) {
    load();
    const int wx = (layout_.width - kWindowW) / 2, wy = (layout_.height - kWindowH) / 2;
    const SDL_Rect window{wx, wy, kWindowW, kWindowH};
    drawWindowBorder(renderer_, borderSprites_, window, BorderSize::Thin); // seg6:1A68 (seg7:7138)
    const SDL_Rect f = frame(); // fills the client area: 0xF6 x 0x9E minus the border
    const char* name = light == 0 ? "R-LIGHT0" : light == 2 ? "R-LIGHT2" : light == 3 ? "R-LIGHT3"
                     : light == 4 ? "R-LIGHT4" : "R-NORMAL"; // seg7:6E54
    if (const auto it = frames_.find(name); it != frames_.end()) SDL_RenderCopy(renderer_, it->second.get(), nullptr, &f);
    if (pressed_ && SDL_TICKS_PASSED(now, pressedUntil_)) pressed_ = 0;
    const auto button = frames_.find("R-BUTTON");
    if (!pressed_ || button == frames_.end()) return;
    const SDL_Rect src = pressed_ == 1 ? SDL_Rect{0, kButtonTop, kSplitX, kFrameH - kButtonTop}
                                       : SDL_Rect{kSplitX, kButtonTop, kFrameW - kSplitX, kFrameH - kButtonTop};
    const SDL_Rect dst{f.x + src.x, f.y + src.y, src.w, src.h};
    SDL_RenderCopy(renderer_, button->second.get(), &src, &dst);
}

} // namespace dtvr
