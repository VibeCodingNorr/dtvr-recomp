// SPDX-License-Identifier: GPL-3.0-only
// The loading logo of rally.exe: seg6:0A4D("BB.bmp", 0) loads butt/<n>/BB.bmp and the
// PICT window is centred on the canvas by seg6:0000; the canvas paints bk.bmp.
#include "logo_view.hpp"

#include <chrono>
#include <cstdio>
#include <stdexcept>

using namespace dtvr;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

struct Assets {
    fs::path root = fs::temp_directory_path() / ("dtvr-logo-input-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Assets() {
        if (!fs::create_directory(root)) throw std::runtime_error("Cannot create synthetic asset directory");
    }
    ~Assets() { std::error_code error; fs::remove_all(root, error); }
    // An 8-bit bitmap of one colour, as the original files are.
    void bitmap(const fs::path& relative, int w, int h, SDL_Color color) {
        Surface image(SDL_CreateRGBSurfaceWithFormat(0, w, h, 8, SDL_PIXELFORMAT_INDEX8), SDL_FreeSurface);
        if (!image) throw std::runtime_error(SDL_GetError());
        SDL_SetPaletteColors(image->format->palette, &color, 1, 1);
        SDL_FillRect(image.get(), nullptr, 1);
        fs::create_directories((root / relative).parent_path());
        if (SDL_SaveBMP(image.get(), (root / relative).c_str()) != 0) throw std::runtime_error(SDL_GetError());
    }
};

static SDL_Color pixel(SDL_Renderer* renderer, int x, int y) {
    Uint32 value = 0;
    const SDL_Rect r{x, y, 1, 1};
    if (SDL_RenderReadPixels(renderer, &r, SDL_PIXELFORMAT_ARGB8888, &value, 4) != 0) throw std::runtime_error(SDL_GetError());
    return {Uint8(value >> 16), Uint8(value >> 8), Uint8(value), 255};
}

static bool same(SDL_Color a, SDL_Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

int main() {
    // seg6:0000: (parent - window) / 2 towards zero, clamped at 0.
    const ScreenRect r640 = centredOnCanvas(640, 480, 175, 313);
    CHECK(r640.x == 232 && r640.y == 83 && r640.w == 175 && r640.h == 313);
    const ScreenRect r800 = centredOnCanvas(800, 600, 219, 391);
    CHECK(r800.x == 290 && r800.y == 104);
    const ScreenRect r1024 = centredOnCanvas(1024, 768, 280, 500);
    CHECK(r1024.x == 372 && r1024.y == 134);
    const ScreenRect big = centredOnCanvas(100, 100, 300, 101);
    CHECK(big.x == 0 && big.y == 0);
    CHECK(isModifierKey(SDLK_LALT) && isModifierKey(SDLK_RSHIFT) && isModifierKey(SDLK_LCTRL) && isModifierKey(SDLK_RGUI));
    CHECK(!isModifierKey(SDLK_RETURN) && !isModifierKey(SDLK_ESCAPE) && !isModifierKey(SDLK_SPACE));
    CHECK(pictPath(6, "BB.bmp") == fs::path("butt") / "6" / "BB.bmp");

    Assets assets;
    if (SDL_Init(0) != 0) throw std::runtime_error(SDL_GetError());
    const SDL_Color wood{120, 60, 20, 255}, blue{20, 40, 200, 255};
    for (int width : {640, 800, 1024}) {
        const ScreenLayout layout = layoutForCanvas(width);
        const fs::path dir = fs::path("butt") / std::to_string(layout.artDir);
        const int w = width == 640 ? 175 : width == 800 ? 219 : 280, h = width == 640 ? 313 : width == 800 ? 391 : 500;
        Surface canvas(SDL_CreateRGBSurfaceWithFormat(0, layout.width, layout.height, 32, SDL_PIXELFORMAT_ARGB8888),
                       SDL_FreeSurface);
        if (!canvas) throw std::runtime_error(SDL_GetError());
        std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(SDL_CreateSoftwareRenderer(canvas.get()),
                                                                            SDL_DestroyRenderer);
        if (!renderer) throw std::runtime_error(SDL_GetError());
        {
            LogoView missing(renderer.get(), assets.root, layout);
            CHECK(!missing.show() && !missing.shown()); // no file: no window (seg6:0AA7)
        }
        assets.bitmap(dir / "BK.BMP", layout.width, layout.height, wood); // looked up without regard to case
        assets.bitmap(dir / "bb.bmp", w, h, blue);
        LogoView logo(renderer.get(), assets.root, layout);
        CHECK(logo.show() && logo.shown());
        SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
        SDL_RenderClear(renderer.get());
        logo.draw();
        const ScreenRect r = centredOnCanvas(layout.width, layout.height, w, h);
        CHECK(same(pixel(renderer.get(), r.x, r.y), blue));
        CHECK(same(pixel(renderer.get(), r.x + w - 1, r.y + h - 1), blue));
        CHECK(same(pixel(renderer.get(), r.x - 1, r.y), wood));
        CHECK(same(pixel(renderer.get(), r.x + w, r.y + h - 1), wood));
        CHECK(same(pixel(renderer.get(), 0, 0), wood));
        logo.hide(); // seg6:0B37
        CHECK(!logo.shown());
        SDL_RenderClear(renderer.get());
        logo.draw(); // nothing once removed
        CHECK(same(pixel(renderer.get(), r.x, r.y), SDL_Color{0, 0, 0, 255}));
    }
    SDL_Quit();
    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("logo_view_test: OK");
    return failures != 0;
}
