// SPDX-License-Identifier: GPL-3.0-only
#include "sight_view.hpp"

#include "ne_strings.hpp"

#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 0x200, kHeight = 0xE8; // CreateWindow seg6:4EDE

Texture loadTexture(SDL_Renderer* renderer, const fs::path& file) {
    Texture t(nullptr, SDL_DestroyTexture);
    if (file.empty()) return t;
    auto surface = loadBmp(file);
    t.reset(SDL_CreateTextureFromSurface(renderer, surface.get()));
    return t;
}

} // namespace

bool SightView::open(int cellX, int cellY) {
    const std::string name = std::to_string(cellX) + "_" + std::to_string(cellY); // "%d_%d" (DS:18A1 / 18B7)
    picture_ = loadTexture(renderer_, findCaseInsensitive(root_, fs::path("butt") / "views" / (name + ".bmp")));
    if (!picture_) return false;
    if (!paper_) paper_ = loadTexture(renderer_, findCaseInsensitive(root_, fs::path("butt") / "unipaper.bmp"));
    body_.clear();
    if (const fs::path txt = findCaseInsensitive(root_, fs::path("butt") / "d" / "views" / (name + ".txt")); !txt.empty()) {
        std::ifstream in(txt, std::ios::binary);
        body_ = ansiToUtf8(std::string(std::istreambuf_iterator<char>(in), {}));
    }
    open_ = true;
    return true;
}

bool SightView::handle(const SDL_Event& event) {
    if (!open_) return false;
    // seg6:518B: WM_KEYDOWN (0x100) and WM_LBUTTONDOWN (0x201) destroy the window;
    // other buttons go to DefWindowProc. The window is modal (seg6:0FB4).
    if (event.type == SDL_KEYDOWN || (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT))
        open_ = false;
    return event.type == SDL_KEYDOWN || event.type == SDL_KEYUP || event.type == SDL_MOUSEBUTTONDOWN
           || event.type == SDL_MOUSEBUTTONUP;
}

void SightView::draw() {
    if (!open_) return;
    // Child of the canvas DS:40 created at (0, 0) (seg6:4ED8); WM_CREATE centres it
    // with seg6:0000 (seg6:4F86).
    const SDL_Rect box{(canvasWidth_ - kWidth) / 2, (canvasHeight_ - kHeight) / 2, kWidth, kHeight};
    if (paper_) {
        const SDL_Rect src{0, 0, kWidth, kHeight};
        SDL_RenderCopy(renderer_, paper_.get(), &src, &box);
    }
    int pw = 0, ph = 0;
    SDL_QueryTexture(picture_.get(), nullptr, nullptr, &pw, &ph);
    const SDL_Rect pic{box.x + 10, box.y + 10, pw, ph}; // seg6:507D: at (10, 10)
    SDL_RenderCopy(renderer_, picture_.get(), nullptr, &pic);
    // seg6:5094: client rect, left + 200, inflated by (-10, -5); Arial 15 weight 600,
    // colour 0x45, DT_WORDBREAK | DT_NOCLIP (0x110).
    const ScreenRect area{box.x + 210, box.y + 5, kWidth - 220, kHeight - 10};
    text_.drawWrapped(renderer_, body_, area, TextStyle{15, true, false, false, false, 0, SDL_Color{67, 27, 0, 255}});
}

} // namespace dtvr
