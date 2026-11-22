// SPDX-License-Identifier: GPL-3.0-only
#include "text.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <tuple>
#include <vector>

#ifdef DTVR_HAVE_TTF
#include <SDL_ttf.h>
#endif

namespace fs = std::filesystem;

namespace dtvr {

#ifdef DTVR_HAVE_TTF
namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

// Font files by face: regular, bold, italic, bold italic.
struct FontSet { std::array<fs::path, 4> files; std::string name; };

FontSet findFonts() {
    std::vector<fs::path> dirs = {"/usr/share/fonts", "/usr/local/share/fonts"};
    if (const char* home = std::getenv("HOME")) dirs.push_back(fs::path(home) / ".local/share/fonts");
    std::map<std::string, fs::path> found;
    for (const auto& dir : dirs) {
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) continue;
        for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec))
            if (!ec && it->is_regular_file(ec)) found.emplace(lower(it->path().filename().string()), it->path());
    }
    const std::array<std::array<const char*, 4>, 2> candidates = {{
        {"arial.ttf", "arialbd.ttf", "ariali.ttf", "arialbi.ttf"},
        {"liberationsans-regular.ttf", "liberationsans-bold.ttf", "liberationsans-italic.ttf",
         "liberationsans-bolditalic.ttf"}}};
    const char* names[2] = {"Arial", "Liberation Sans"};
    for (size_t c = 0; c < candidates.size(); ++c) {
        FontSet set;
        set.name = names[c];
        bool complete = true;
        for (size_t i = 0; i < 4; ++i) {
            auto it = found.find(candidates[c][i]);
            if (it == found.end()) { complete = false; break; }
            set.files[i] = it->second;
        }
        if (complete) return set;
    }
    return {};
}

} // namespace

struct TextRenderer::Impl {
    FontSet fonts;
    std::map<std::tuple<int, int>, TTF_Font*> open; // (face, pixel height)
    ~Impl() {
        for (auto& entry : open) TTF_CloseFont(entry.second);
        TTF_Quit();
    }
    TTF_Font* font(int face, int height) {
        auto key = std::make_tuple(face, height);
        if (auto it = open.find(key); it != open.end()) return it->second;
        TTF_Font* f = TTF_OpenFont(fonts.files[size_t(face)].c_str(), height);
        open.emplace(key, f);
        return f;
    }
};

TextRenderer::TextRenderer() {
    if (TTF_Init() != 0) return;
    impl_ = std::make_unique<Impl>();
    impl_->fonts = findFonts();
    available_ = !impl_->fonts.files[0].empty();
    description_ = available_ ? impl_->fonts.name : "keine Schrift (Arial oder Liberation Sans) gefunden";
}

TextRenderer::~TextRenderer() = default;

void TextRenderer::draw(SDL_Renderer* renderer, const std::string& text, const ScreenRect& rect,
                        const TextStyle& style) {
    if (!available_ || text.empty()) return;
    TTF_Font* font = impl_->font((style.bold ? 1 : 0) + (style.italic ? 2 : 0), style.pixelHeight);
    if (!font) return;
    auto blit = [&](SDL_Color colour, int dx, int dy) {
        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), colour);
        if (!surface) return;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        const int x = style.center ? rect.x + (rect.w - surface->w) / 2 : rect.x;
        const SDL_Rect target{x + dx, rect.y + dy, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &target);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    };
    if (style.shadow) blit(SDL_Color{0, 0, 0, 255}, -style.shadow, -style.shadow);
    blit(style.color, 0, 0);
}
#else
struct TextRenderer::Impl {};
TextRenderer::TextRenderer() : description_("ohne SDL2_ttf gebaut: kein Text") {}
TextRenderer::~TextRenderer() = default;
void TextRenderer::draw(SDL_Renderer*, const std::string&, const ScreenRect&, const TextStyle&) {}
#endif

} // namespace dtvr
