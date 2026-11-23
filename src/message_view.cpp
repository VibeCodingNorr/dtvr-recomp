// SPDX-License-Identifier: GPL-3.0-only
#include "message_view.hpp"

#include "panel.hpp"

#include <cstdio>

namespace fs = std::filesystem;

namespace dtvr {

MessageView::MessageView(SDL_Renderer* renderer, const fs::path& root, char language, const ScreenLayout& layout,
                         const std::map<int, std::string>& texts, TextRenderer& text)
    : renderer_(renderer), root_(root), language_(language), layout_(layout), texts_(texts), text_(text) {}

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

int MessageView::buttonSize() const {
    const int res = layout_.resolution();
    static constexpr int kSize[3] = {18, 16, 14};
    return kSize[res] + 5; // seg6:5505: text size + 5 for the buttons
}

MessageView::Geometry MessageView::geometry() {
    Geometry g;
    const std::string dir = std::to_string(layout_.artDir);
    const Notice& n = queue_.front();
    if (n.cardPicture >= 0) { // DS:1A6C: "%s\%d\%c\%dcard_%.2d.bmp" instead of the showmaster
        char name[32];
        std::snprintf(name, sizeof name, "%dcard_%02d.bmp", layout_.artDir, n.cardPicture + 1);
        g.picture = bitmap(fs::path("bmp") / dir / languageDir(language_) / name, g.pw, g.ph);
    }
    if (!g.picture && n.picture == Notice::Picture::Drago) g.picture = bitmap(fs::path("bmp") / dir / "qp.bmp", g.pw, g.ph);
    if (!g.picture && n.picture == Notice::Picture::Professor) g.picture = bitmap(fs::path("butt") / "prof.bmp", g.pw, g.ph);
    if (!g.picture && n.picture == Notice::Picture::Football) g.picture = bitmap(fs::path("butt") / "fuss.bmp", g.pw, g.ph);
    if (!g.picture) g.picture = bitmap(fs::path("bmp") / dir / "sm.bmp", g.pw, g.ph);
    // seg6:5213: width from element 5, height = picture + 0x24; centred on the canvas.
    const int w = layout_.elements[5][0], h = g.ph + 0x24;
    g.box = SDL_Rect{std::max(0, (layout_.width - w) / 2), std::max(0, (layout_.height - h) / 2), w, h};
    g.client = borderClient(g.box, BorderSize::Thin); // picture + 0x24 - 16: 10 pixels above and below
    if (n.buttons == 4) {
        // seg6:54C4: client rectangle inflated by -10, left past the picture + 10, then
        // inflated by (-10, 0); seg6:639C puts Ja (text 115) bottom left and Nein (116)
        // bottom right of it.
        const SDL_Rect r{g.client.x + 30 + g.pw, g.client.y + 10, g.client.w - 50 - g.pw, g.client.h - 20};
        const TextStyle style{buttonSize(), true, false, false, false, 0, SDL_Color{}};
        auto text = [&](int id) { const auto it = texts_.find(id); return it == texts_.end() ? std::string() : it->second; };
        const SDL_Point yes = text_.measure(text(115), style), no = text_.measure(text(116), style);
        g.yes = SDL_Rect{r.x, r.y + r.h - yes.y, yes.x, yes.y};
        g.no = SDL_Rect{r.x + r.w - no.x, r.y + r.h - no.y, no.x, no.y};
    }
    return g;
}

int MessageView::handle(const SDL_Event& event) {
    if (!question()) return 0;
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) { // seg6:54BB
        const Geometry g = geometry();
        const SDL_Point at{event.button.x, event.button.y};
        if (SDL_PointInRect(&at, &g.yes)) return 6;
        if (SDL_PointInRect(&at, &g.no)) return 7;
        return 0;
    }
    if (event.type != SDL_KEYDOWN) return 0;
    switch (event.key.keysym.sym) { // seg6:579F
    case SDLK_LEFT: choice_ = 6; return 0;
    case SDLK_RIGHT: choice_ = 7; return 0;
    case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: return choice_;
    case SDLK_ESCAPE: case SDLK_BACKSPACE: return 7; // VK_RIGHT to itself, then closed
    default: return 0;
    }
}

void MessageView::draw(const Game& game) {
    if (queue_.empty()) return;
    const std::string dir = std::to_string(layout_.artDir);
    int pw = 0, ph = 0;
    SDL_Texture* paper = bitmap(fs::path("butt") / dir / "p1.bmp", pw, ph);
    const Geometry g = geometry();
    SDL_Texture* showmaster = g.picture;
    const SDL_Rect box = g.client;
    const int sw = g.pw, sh = g.ph, w = box.w, h = box.h;
    drawWindowBorder(renderer_, sprites_, g.box, BorderSize::Thin);
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
    // seg6:5B59: client rectangle inflated by -10, left edge moved by the picture width
    // + 10 (seg6:5B6F), so the text starts 10 pixels right of the picture;
    // Arial weight 600 at {18, 16, 14}, palette colour 0x45, DT_WORDBREAK | DT_NOCLIP.
    const Notice& n = queue_.front();
    const auto it = texts_.find(n.text);
    const std::string message = it == texts_.end() ? "Text " + std::to_string(n.text) : formatNotice(it->second, n, game);
    const int res = layout_.resolution();
    static constexpr int kSize[3] = {18, 16, 14};
    TextStyle style{kSize[res], true, false, false, false, 0, SDL_Color{67, 27, 0, 255}};
    const ScreenRect textRect{box.x + 10 + sw + 10, box.y + 10, w - 30 - sw, h - 20};
    text_.drawWrapped(renderer_, message, textRect, style);
    if (n.buttons == 4) { // seg6:5CC1: the chosen answer in colour 0xFF, the other in 0x45
        auto button = [&](int id, const SDL_Rect& r, bool chosen) {
            const auto t = texts_.find(id);
            if (t == texts_.end()) return;
            const SDL_Color c = chosen ? SDL_Color{255, 255, 255, 255} : SDL_Color{67, 27, 0, 255};
            text_.draw(renderer_, t->second, ScreenRect{r.x, r.y, r.w, r.h},
                       TextStyle{buttonSize(), true, false, false, false, 0, c});
        };
        button(115, g.yes, choice_ == 6);
        button(116, g.no, choice_ == 7);
    }
}

} // namespace dtvr
