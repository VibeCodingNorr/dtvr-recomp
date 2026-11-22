// SPDX-License-Identifier: GPL-3.0-only
#include "message_view.hpp"

#include "panel.hpp"

namespace fs = std::filesystem;

namespace dtvr {

MessageView::MessageView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout,
                         const std::map<int, std::string>& texts, TextRenderer& text)
    : renderer_(renderer), root_(root), layout_(layout), texts_(texts), text_(text) {}

SDL_Texture* MessageView::bitmap(const fs::path& relative, int& w, int& h) {
    const std::string key = relative.generic_string();
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        Texture t(nullptr, SDL_DestroyTexture);
        const fs::path path = findCaseInsensitive(root_, relative);
        if (!path.empty()) {
            auto surface = loadBmp(path);
            t.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        }
        it = cache_.emplace(key, std::move(t)).first;
    }
    w = h = 0;
    if (it->second) SDL_QueryTexture(it->second.get(), nullptr, nullptr, &w, &h);
    return it->second.get();
}

void MessageView::draw(const Game& game) {
    if (queue_.empty()) return;
    const std::string dir = std::to_string(layout_.artDir);
    int pw = 0, ph = 0, sw = 0, sh = 0;
    SDL_Texture* paper = bitmap(fs::path("butt") / dir / "p1.bmp", pw, ph);
    SDL_Texture* showmaster = bitmap(fs::path("bmp") / dir / "sm.bmp", sw, sh);
    // seg6:5213: width from element 5, height = showmaster + 0x24; centred on the
    // canvas (seg6:5852 -> seg6:0000).
    const int w = layout_.elements[5][0], h = sh + 0x24;
    const SDL_Rect box{std::max(0, (layout_.width - w) / 2), std::max(0, (layout_.height - h) / 2), w, h};
    if (paper && pw > 0 && ph > 0) // the paper fills the client area (tiled where it is smaller)
        for (int y = 0; y < h; y += ph)
            for (int x = 0; x < w; x += pw) {
                const SDL_Rect src{0, 0, std::min(pw, w - x), std::min(ph, h - y)};
                const SDL_Rect dst{box.x + x, box.y + y, src.w, src.h};
                SDL_RenderCopy(renderer_, paper, &src, &dst);
            }
    if (showmaster) {
        const SDL_Rect dst{box.x + 10, box.y + 10, sw, sh};
        SDL_RenderCopy(renderer_, showmaster, nullptr, &dst);
    }
    // seg6:5B59: client rectangle inflated by -10, left edge moved past the showmaster;
    // Arial weight 600 at {18, 16, 14}, palette colour 0x45, DT_WORDBREAK | DT_NOCLIP.
    const Notice& n = queue_.front();
    const auto it = texts_.find(n.text);
    const std::string message = it == texts_.end() ? "Text " + std::to_string(n.text) : formatNotice(it->second, n, game);
    const int res = layout_.width == 1024 ? 0 : layout_.width == 800 ? 1 : 2;
    static constexpr int kSize[3] = {18, 16, 14};
    TextStyle style{kSize[res], true, false, false, 0, SDL_Color{67, 27, 0, 255}};
    const ScreenRect textRect{box.x + 10 + sw, box.y + 10, w - 20 - sw, h - 20};
    text_.drawWrapped(renderer_, message, textRect, style);
}

} // namespace dtvr
