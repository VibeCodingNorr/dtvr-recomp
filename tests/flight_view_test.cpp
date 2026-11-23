// SPDX-License-Identifier: GPL-3.0-only
// Flugzeug input from rally.exe seg3:339B, 346D, 34B9, 3507 and 3971.
#include "flight_view.hpp"

#include <chrono>
#include <cstdio>
#include <stdexcept>

using namespace dtvr;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static bool idle(const FlightView::Result& result) {
    return result.town.empty() && !result.closed && !result.cancel;
}

static FlightView::Result key(FlightView& view, SDL_Keycode code) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = code;
    return view.handle(event);
}

static FlightView::Result pointer(FlightView& view, const ScreenLayout& layout, int x, int y,
                                 Uint32 type = SDL_MOUSEBUTTONDOWN, Uint8 button = SDL_BUTTON_LEFT) {
    // Client coordinates of WM_LBUTTONDOWN: inside the 8-pixel border of seg6:1A68 (round 112).
    const int left = (layout.width - 0x114) / 2 + 8, top = (layout.height - 0x100) / 2 + 8;
    SDL_Event event{};
    event.type = type;
    if (type == SDL_MOUSEMOTION) {
        event.motion.x = left + x;
        event.motion.y = top + y;
    } else {
        event.button.x = left + x;
        event.button.y = top + y;
        event.button.button = button;
    }
    return view.handle(event);
}

// No assets are needed: keep lookup inside an owned, empty external directory.
struct EmptyAssets {
    fs::path root = fs::temp_directory_path() / ("dtvr-flight-input-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    EmptyAssets() {
        if (!fs::create_directory(root)) throw std::runtime_error("Cannot create empty asset directory");
    }
    ~EmptyAssets() { std::error_code error; fs::remove(root, error); }
};

int main() {
    EmptyAssets assets;
    TextRenderer text;
    std::map<int, std::string> texts;
    for (int row = 0; row < 8; ++row) texts[5602 + row] = "Town" + std::to_string(row);
    for (int width : {640, 800, 1024}) {
        const ScreenLayout layout = layoutForCanvas(width);
        FlightView view(nullptr, assets.root, layout, texts, text);
        CHECK(idle(key(view, SDLK_RETURN)));
        for (bool east : {false, true}) {
            for (bool last : {false, true}) {
                const int count = east ? 8 : 7;
                view.open(east, last);
                CHECK(key(view, SDLK_RETURN).town == "Town0");
                CHECK(idle(key(view, SDLK_UP)));
                CHECK(key(view, SDLK_RETURN).town == "Town0"); // no wrap at the top
                for (int i = 0; i < 12; ++i) CHECK(idle(key(view, SDLK_DOWN)));
                CHECK(key(view, SDLK_RETURN).town == "Town" + std::to_string(count - 1));
                CHECK(idle(key(view, SDLK_DOWN)));
                CHECK(key(view, SDLK_RETURN).town == "Town" + std::to_string(count - 1));
                for (int i = 0; i < 12; ++i) CHECK(idle(key(view, SDLK_UP)));
                CHECK(key(view, SDLK_RETURN).town == "Town0");

                // Hovering and unsupported inputs neither select nor cancel.
                CHECK(idle(pointer(view, layout, 20, 50 + 3 * 24, SDL_MOUSEMOTION)));
                CHECK(key(view, SDLK_RETURN).town == "Town0");
                CHECK(idle(key(view, SDLK_ESCAPE)));
                CHECK(idle(key(view, SDLK_BACKSPACE)));
                CHECK(idle(pointer(view, layout, 20, 100, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT)));
                CHECK(idle(pointer(view, layout, 20, 100, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_MIDDLE)));
                CHECK(view.isOpen() && key(view, SDLK_RETURN).town == "Town0");

                // Every row is the half-open rectangle [0,240) x [50+24i,74+24i).
                for (int row = 0; row < count; ++row) {
                    const auto result = pointer(view, layout, 0, 50 + row * 24);
                    if (row == 1 && last) {
                        CHECK(result.closed && result.town.empty() && !result.cancel);
                        CHECK(key(view, SDLK_RETURN).town == "Town0"); // refused click retains selection
                    } else {
                        CHECK(result.town == "Town" + std::to_string(row));
                        CHECK(key(view, SDLK_RETURN).town == result.town);
                        CHECK(pointer(view, layout, 239, 50 + row * 24 + 23).town == result.town);
                    }
                }
                CHECK(idle(pointer(view, layout, -1, 50)));
                CHECK(idle(pointer(view, layout, 240, 50)));
                CHECK(idle(pointer(view, layout, 0, 49)));
                CHECK(idle(pointer(view, layout, 0, 50 + count * 24)));

                view.open(east, last);
                CHECK(idle(key(view, SDLK_DOWN)));
                const auto brussels = key(view, SDLK_RETURN);
                if (last) CHECK(brussels.closed && brussels.town.empty() && !brussels.cancel);
                else CHECK(brussels.town == "Town1" && !brussels.closed && !brussels.cancel);
                CHECK(idle(key(view, SDLK_DOWN)));
                CHECK(key(view, SDLK_KP_ENTER).town == "Town2");
                view.close();
                CHECK(idle(key(view, SDLK_RETURN)));
                CHECK(idle(pointer(view, layout, 0, 50)));
                view.open(east, last);
                CHECK(key(view, SDLK_RETURN).town == "Town0");
                view.close();
            }
        }
    }
    return failures ? 1 : 0;
}
