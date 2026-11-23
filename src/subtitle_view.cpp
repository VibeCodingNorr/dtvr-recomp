// SPDX-License-Identifier: GPL-3.0-only
#include "subtitle_view.hpp"


namespace fs = std::filesystem;

namespace dtvr {

SubtitleView::SubtitleView(SDL_Renderer* renderer, const fs::path& root, TextRenderer& text,
                           const std::map<int, std::string>& texts)
    : renderer_(renderer), root_(root), text_(text), texts_(texts), table_(loadVoiceSync(root)) {}

void SubtitleView::voice(const std::string& file, const SDL_Rect& owner, Uint32 now) {
    if (!enabled_) return;
    // seg7:04AC: top = bottom - 0x10, bottom = top + 0x3C, left / right of the owner.
    rect_ = SDL_Rect{owner.x, owner.y + owner.h - 0x10, owner.w, 0x3C};
    if (!paper_) {
        if (const fs::path p = findCaseInsensitive(root_, fs::path("butt") / "unipaper.bmp"); !p.empty()) {
            auto surface = loadBmp(p);
            paper_.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        }
    }
    if (!open_) track_.stop(); // a new window starts with an empty loop state
    open_ = true;
    track_.start(findVoiceSync(table_, file), now);
}

void SubtitleView::close() {
    open_ = false;
    track_.stop();
}

void SubtitleView::update(Uint32 now) {
    if (open_) track_.poll(now);
}

void SubtitleView::draw() {
    if (!open_) return;
    // The 8-pixel border of seg6:1A68 (seg6:6991); seg6:678B: unipaper.bmp stretched
    // over the client area.
    drawWindowBorder(renderer_, borderSprites_, rect_, BorderSize::Thin);
    const SDL_Rect client = borderClient(rect_, BorderSize::Thin);
    if (paper_) SDL_RenderCopy(renderer_, paper_.get(), nullptr, &client);
    const auto it = texts_.find(track_.shown());
    if (track_.shown() == 0 || it == texts_.end()) return;
    // seg6:68A6: font seg6:04F9(FONTSIZE 14, weight 600); client rect inflated by
    // (-6, -6); DrawText 0x110 in PALETTEINDEX(0) without shadow.
    const ScreenRect area{client.x + 6, client.y + 6, client.w - 12, client.h - 12};
    text_.drawWrapped(renderer_, it->second, area, TextStyle{14, true, false, false, false, 0, SDL_Color{0, 0, 0, 255}});
}

} // namespace dtvr
