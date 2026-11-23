// SPDX-License-Identifier: GPL-3.0-only
// Overview zoom/crop and input from rally.exe seg1:A95E / AB7B. Synthetic art only.
#include "overview_view.hpp"

#include <chrono>
#include <cstdio>
#include <stdexcept>

using namespace dtvr;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static SDL_Color pattern(int x, int y) {
    return {Uint8(x / 4), Uint8(y / 4), Uint8(((x / 32) ^ (y / 32)) * 13), 255};
}

struct Assets {
    fs::path root = fs::temp_directory_path() / ("dtvr-overview-input-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Assets() {
        if (!fs::create_directory(root)) throw std::runtime_error("Cannot create synthetic asset directory");
    }
    ~Assets() { std::error_code error; fs::remove_all(root, error); }
    void makeMap(const ScreenLayout& layout) {
        const int size = layout.map.w - 16;
        Surface map(SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
        if (!map) throw std::runtime_error(SDL_GetError());
        for (int y = 0; y < size; ++y) {
            auto* row = reinterpret_cast<Uint32*>(static_cast<Uint8*>(map->pixels) + y * map->pitch);
            for (int x = 0; x < size; ++x) {
                const auto color = pattern(x, y);
                row[x] = SDL_MapRGBA(map->format, color.r, color.g, color.b, color.a);
            }
        }
        const fs::path directory = root / "butt" / std::to_string(layout.artDir);
        fs::create_directories(directory);
        if (SDL_SaveBMP(map.get(), (directory / "map.bmp").c_str()) != 0) throw std::runtime_error(SDL_GetError());
    }
};

static bool key(OverviewView& view, SDL_Keycode code) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = code;
    return view.handle(event);
}

static bool pointer(OverviewView& view, Uint32 type, int x, int y, Uint8 button = SDL_BUTTON_LEFT,
                    Uint8 clicks = 1) {
    SDL_Event event{};
    event.type = type;
    if (type == SDL_MOUSEMOTION) {
        event.motion.x = x; event.motion.y = y;
    } else {
        event.button.x = x; event.button.y = y;
        event.button.button = button; event.button.clicks = clicks;
    }
    return view.handle(event);
}

static Surface read(SDL_Renderer* renderer, const ScreenLayout& layout) {
    Surface image(SDL_CreateRGBSurfaceWithFormat(0, layout.width, layout.height, 32, SDL_PIXELFORMAT_ARGB8888),
                  SDL_FreeSurface);
    if (!image || SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, image->pixels, image->pitch) != 0)
        throw std::runtime_error(SDL_GetError());
    return image;
}

static SDL_Color pixel(const SDL_Surface& image, int x, int y) {
    const auto* row = reinterpret_cast<const Uint32*>(static_cast<const Uint8*>(image.pixels) + y * image.pitch);
    SDL_Color color{};
    SDL_GetRGBA(row[x], image.format, &color.r, &color.g, &color.b, &color.a);
    return color;
}

static bool same(SDL_Color a, SDL_Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
static bool same(SDL_Rect a, SDL_Rect b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }

int main(int argc, char** argv) {
    Assets assets;
    // Software rendering over an owned surface needs no window or video device.
    if (SDL_Init(0) != 0) throw std::runtime_error(SDL_GetError());
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    Board board;
    board.fields.resize(2);
    board.fields[0].kind = board.fields[1].kind = 6;
    board.fields[1].x = 80;
    board.fields[0].next[1] = 1;
    board.fields[1].next[3] = 0;
    std::vector<Town> towns(2);
    towns[0].name = "London";
    towns[1].name = "Rom";
    towns[1].x = 80;
    Game game(board, towns, 2, 2, 1);
    for (int width : {640, 800, 1024}) {
        const auto layout = layoutForCanvas(width);
        assets.makeMap(layout);
        Surface canvas(SDL_CreateRGBSurfaceWithFormat(0, layout.width, layout.height, 32, SDL_PIXELFORMAT_ARGB8888),
                       SDL_FreeSurface);
        if (!canvas) throw std::runtime_error(SDL_GetError());
        std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(SDL_CreateSoftwareRenderer(canvas.get()),
                                                                            SDL_DestroyRenderer);
        if (!renderer) throw std::runtime_error(SDL_GetError());
        Surface flagImage(SDL_CreateRGBSurfaceWithFormat(0, 2, 2, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
        if (!flagImage) throw std::runtime_error(SDL_GetError());
        SDL_FillRect(flagImage.get(), nullptr, SDL_MapRGBA(flagImage->format, 240, 240, 240, 255));
        Texture flag(SDL_CreateTextureFromSurface(renderer.get(), flagImage.get()), SDL_DestroyTexture);
        if (!flag) throw std::runtime_error(SDL_GetError());
        OverviewView view(renderer.get(), assets.root, layout, board,
                          [&](const std::string& name) -> SDL_Texture* {
                              return name == "D_FL" || name == "F_FL" ? flag.get() : nullptr;
                          });
        auto checkCrop = [&](bool zoomed, int column, int row, const char* shot = nullptr) {
            SDL_SetRenderDrawColor(renderer.get(), 17, 19, 23, 255);
            SDL_RenderClear(renderer.get());
            view.draw(game);
            auto image = read(renderer.get(), layout);
            const int sx = zoomed ? column * (layout.map.w / 4 - 4) + 100 : 200;
            const int sy = zoomed ? row * (layout.map.h / 4 - 4) + 100 : 200;
            // The paint starts at (0, 0) of the client, inside the 8-pixel border (seg1:AA80).
            const int cx = layout.map.x + 8, cy = layout.map.y + 8;
            CHECK(same(pixel(*image, cx + 200, cy + 200), pattern(sx, sy)));
            CHECK(same(pixel(*image, 0, 0), SDL_Color{17, 19, 23, 255}));
            if (zoomed && column == 0 && row == 0) {
                // The two-pixel driver flag and underlying green start marker
                // must be enlarged together with the bitmap, not drawn afterward.
                CHECK(same(pixel(*image, cx + 2, cy + 2), SDL_Color{240, 240, 240, 255}));
                CHECK(same(pixel(*image, cx + 4, cy + 4), SDL_Color{0, 255, 0, 255}));
            }
            if (shot && argc == 2) {
                fs::create_directories(argv[1]);
                const fs::path file = fs::path(argv[1]) / (std::to_string(width) + "-" + shot + ".bmp");
                CHECK(SDL_SaveBMP(image.get(), file.c_str()) == 0);
            }
        };
        CHECK(!key(view, SDLK_RETURN));
        view.open();
        for (auto code : {SDLK_a, SDLK_SPACE, SDLK_TAB, SDLK_F8, SDLK_LEFT, SDLK_UP, SDLK_DOWN, SDLK_RIGHT}) {
            CHECK(!key(view, code));
            CHECK(view.isOpen());
        }
        checkCrop(false, 0, 0, "full");
        CHECK(!key(view, SDLK_RETURN) && view.isOpen());
        CHECK(!key(view, SDLK_UP) && !key(view, SDLK_LEFT));
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                checkCrop(true, column, row, row == 2 && column == 2 ? "zoom" : nullptr);
                key(view, SDLK_RIGHT);
            }
            checkCrop(true, 2, row); // repeated Right stays in the last column
            if (row < 2) { key(view, SDLK_LEFT); key(view, SDLK_LEFT); key(view, SDLK_DOWN); }
        }
        key(view, SDLK_DOWN);
        checkCrop(true, 2, 2);
        key(view, SDLK_RETURN);
        checkCrop(false, 0, 0);
        key(view, SDLK_KP_ENTER);
        checkCrop(true, 0, 0); // zoom restarts at the first cell
        pointer(view, SDL_MOUSEBUTTONDOWN, 200, 200, SDL_BUTTON_LEFT, 2);
        checkCrop(false, 0, 0);
        pointer(view, SDL_MOUSEBUTTONDOWN, 200, 200); // ordinary click zooms in
        checkCrop(true, 0, 0);
        pointer(view, SDL_MOUSEBUTTONDOWN, 200, 200); // mouse steering cross
        pointer(view, SDL_MOUSEMOTION, 225, 200);
        pointer(view, SDL_MOUSEBUTTONUP, 225, 200); // posts Right
        checkCrop(true, 1, 0);
        pointer(view, SDL_MOUSEBUTTONDOWN, 200, 200);
        pointer(view, SDL_MOUSEBUTTONDOWN, 200, 200, SDL_BUTTON_RIGHT); // closes cross only
        CHECK(view.isOpen());
        key(view, SDLK_RIGHT);
        checkCrop(true, 2, 0);
        pointer(view, SDL_MOUSEBUTTONDOWN, 200, 200);
        pointer(view, SDL_MOUSEBUTTONUP, 200, 200); // centre posts Return
        checkCrop(false, 0, 0);
        CHECK(!pointer(view, SDL_MOUSEBUTTONDOWN, 200, 200, SDL_BUTTON_MIDDLE) && view.isOpen());

        // Rendering must preserve a caller's logical canvas, scale, viewport, clip and target.
        Texture target(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                         layout.width, layout.height), SDL_DestroyTexture);
        CHECK(target && SDL_SetRenderTarget(renderer.get(), target.get()) == 0);
        SDL_RenderSetLogicalSize(renderer.get(), layout.width, layout.height);
        SDL_RenderSetScale(renderer.get(), 1.25f, 0.75f);
        SDL_Rect wanted{3, 5, 200, 180}, clip{7, 9, 80, 70};
        SDL_RenderSetViewport(renderer.get(), &wanted);
        SDL_RenderSetClipRect(renderer.get(), &clip);
        SDL_Rect before{}, beforeClip{}, after{}, afterClip{};
        SDL_RenderGetViewport(renderer.get(), &before);
        SDL_RenderGetClipRect(renderer.get(), &beforeClip);
        view.draw(game);
        SDL_RenderGetViewport(renderer.get(), &after);
        SDL_RenderGetClipRect(renderer.get(), &afterClip);
        float sx = 0, sy = 0;
        int lw = 0, lh = 0;
        SDL_RenderGetScale(renderer.get(), &sx, &sy);
        SDL_RenderGetLogicalSize(renderer.get(), &lw, &lh);
        CHECK(SDL_GetRenderTarget(renderer.get()) == target.get());
        CHECK(same(before, after) && same(beforeClip, afterClip));
        CHECK(SDL_RenderIsClipEnabled(renderer.get()) && sx == 1.25f && sy == 0.75f);
        CHECK(lw == layout.width && lh == layout.height);
        SDL_SetRenderTarget(renderer.get(), nullptr);
        SDL_RenderSetLogicalSize(renderer.get(), 0, 0);
        SDL_RenderSetScale(renderer.get(), 1, 1);
        SDL_RenderSetViewport(renderer.get(), nullptr);
        SDL_RenderSetClipRect(renderer.get(), nullptr);

        CHECK(key(view, SDLK_ESCAPE) && !view.isOpen());
        view.open(); CHECK(key(view, SDLK_BACKSPACE) && !view.isOpen());
        view.open(); CHECK(pointer(view, SDL_MOUSEBUTTONDOWN, 200, 200, SDL_BUTTON_RIGHT) && !view.isOpen());
        view.open(); checkCrop(false, 0, 0);
    }
    SDL_Quit();
    return failures ? 1 : 0;
}
