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

// Sans: Arial or Liberation Sans; serif: Times New Roman or Liberation Serif.
FontSet findFonts(bool serif) {
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
    const std::array<std::array<const char*, 4>, 2> sans = {{
        {"arial.ttf", "arialbd.ttf", "ariali.ttf", "arialbi.ttf"},
        {"liberationsans-regular.ttf", "liberationsans-bold.ttf", "liberationsans-italic.ttf",
         "liberationsans-bolditalic.ttf"}}};
    const std::array<std::array<const char*, 4>, 2> roman = {{
        {"times.ttf", "timesbd.ttf", "timesi.ttf", "timesbi.ttf"},
        {"liberationserif-regular.ttf", "liberationserif-bold.ttf", "liberationserif-italic.ttf",
         "liberationserif-bolditalic.ttf"}}};
    const auto& candidates = serif ? roman : sans;
    const char* names[2] = {serif ? "Times New Roman" : "Arial", serif ? "Liberation Serif" : "Liberation Sans"};
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
    FontSet fonts, serif;
    std::map<std::tuple<int, int>, TTF_Font*> open; // (face, pixel height)
    ~Impl() {
        for (auto& entry : open) TTF_CloseFont(entry.second);
        TTF_Quit();
    }
    TTF_Font* font(int face, int height) {
        auto key = std::make_tuple(face, height);
        if (auto it = open.find(key); it != open.end()) return it->second;
        const FontSet& set = face >= 4 && !serif.files[0].empty() ? serif : fonts;
        TTF_Font* f = TTF_OpenFont(set.files[size_t(face & 3)].c_str(), height);
        open.emplace(key, f);
        return f;
    }
};

TextRenderer::TextRenderer() {
    if (TTF_Init() != 0) return;
    impl_ = std::make_unique<Impl>();
    impl_->fonts = findFonts(false);
    impl_->serif = findFonts(true); // without it the sans face stands in
    available_ = !impl_->fonts.files[0].empty();
    description_ = available_ ? impl_->fonts.name : "keine Schrift (Arial oder Liberation Sans) gefunden";
}

TextRenderer::~TextRenderer() = default;

void TextRenderer::draw(SDL_Renderer* renderer, const std::string& text, const ScreenRect& rect,
                        const TextStyle& style) {
    if (!available_ || text.empty()) return;
    TTF_Font* font = impl_->font((style.serif ? 4 : 0) + (style.bold ? 1 : 0) + (style.italic ? 2 : 0), style.pixelHeight);
    if (!font) return;
    // Screen: rasterise at the renderer's scale (SDL_RenderSetLogicalSize), place by the
    // canvas-size extent so that positions match the other modes.
    float sx = 1, sy = 1;
    SDL_RenderGetScale(renderer, &sx, &sy);
    const float scale = std::min(sx, sy);
    TTF_Font* fine = nullptr;
    if (smoothing_ == Screen && scale > 1.01f) {
        const int height = int(style.pixelHeight * scale + 0.5f);
        fine = impl_->font((style.serif ? 4 : 0) + (style.bold ? 1 : 0) + (style.italic ? 2 : 0), height);
    }
    int width = 0, lineHeight = 0;
    TTF_SizeUTF8(font, text.c_str(), &width, &lineHeight);
    auto blit = [&](SDL_Color colour, int dx, int dy) {
        SDL_Surface* surface = smoothing_ == Original ? TTF_RenderUTF8_Solid(font, text.c_str(), colour)
                               : TTF_RenderUTF8_Blended(fine ? fine : font, text.c_str(), colour);
        if (!surface) return;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        const int x = style.center ? rect.x + (rect.w - width) / 2 : style.right ? rect.x + rect.w - width : rect.x;
        const SDL_FRect target{float(x + dx), float(rect.y + dy), fine ? surface->w / scale : float(surface->w),
                               fine ? surface->h / scale : float(surface->h)};
        SDL_RenderCopyF(renderer, texture, nullptr, &target);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    };
    if (style.shadow) blit(SDL_Color{0, 0, 0, 255}, -style.shadow, -style.shadow);
    blit(style.color, 0, 0);
}
SDL_Point TextRenderer::measure(const std::string& text, const TextStyle& style) {
    SDL_Point size{0, style.pixelHeight};
    if (!available_) return size;
    TTF_Font* font = impl_->font((style.serif ? 4 : 0) + (style.bold ? 1 : 0) + (style.italic ? 2 : 0), style.pixelHeight);
    if (font) TTF_SizeUTF8(font, text.c_str(), &size.x, &size.y);
    return size;
}

void TextRenderer::drawWrapped(SDL_Renderer* renderer, const std::string& text, const ScreenRect& rect,
                               const TextStyle& style) {
    if (!available_ || text.empty()) return;
    TTF_Font* font = impl_->font((style.serif ? 4 : 0) + (style.bold ? 1 : 0) + (style.italic ? 2 : 0), style.pixelHeight);
    if (!font) return;
    std::vector<std::string> lines;
    std::string line, word;
    auto fits = [&](const std::string& s) { int w = 0, h = 0; TTF_SizeUTF8(font, s.c_str(), &w, &h); return w <= rect.w; };
    auto flushWord = [&] {
        if (word.empty()) return;
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (line.empty() || fits(candidate)) line = candidate;
        else { lines.push_back(line); line = word; }
        word.clear();
    };
    for (char c : text) {
        if (c == ' ') flushWord();
        else if (c == '\n') { flushWord(); lines.push_back(line); line.clear(); }
        else if (c != '\r') word += c;
    }
    flushWord();
    if (!line.empty()) lines.push_back(line);
    const int step = TTF_FontLineSkip(font);
    ScreenRect r = rect;
    for (const auto& l : lines) {
        draw(renderer, l, r, style);
        r.y += step;
    }
}
#else
struct TextRenderer::Impl {};
void TextRenderer::drawWrapped(SDL_Renderer*, const std::string&, const ScreenRect&, const TextStyle&) {}
TextRenderer::TextRenderer() : description_("ohne SDL2_ttf gebaut: kein Text") {}
TextRenderer::~TextRenderer() = default;
void TextRenderer::draw(SDL_Renderer*, const std::string&, const ScreenRect&, const TextStyle&) {}
SDL_Point TextRenderer::measure(const std::string&, const TextStyle& style) { return SDL_Point{0, style.pixelHeight}; }
#endif

} // namespace dtvr
