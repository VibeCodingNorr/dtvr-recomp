// SPDX-License-Identifier: GPL-3.0-only
// Input behavior traced from rally.exe seg8:B24E, B335, B8F6 and BA3B.
#include "save_view.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static SaveView::Result key(SaveView& view, SDL_Keycode code) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = code;
    return view.handle(event);
}

static SaveView::Result click(SaveView& view, int x, int y, Uint8 button = SDL_BUTTON_LEFT, Uint8 count = 1) {
    const SDL_Rect box = view.box();
    SDL_Event event{};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = button;
    event.button.clicks = count;
    event.button.x = box.x + x;
    event.button.y = box.y + y;
    return view.handle(event);
}

static void hover(SaveView& view, int x, int y) {
    const SDL_Rect box = view.box();
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.x = box.x + x;
    event.motion.y = box.y + y;
    CHECK(view.handle(event) == SaveView::Result::None);
}

int main() {
    TextRenderer text;
    const std::map<int, std::string> texts;
    for (int width : {640, 800, 1024}) {
        SaveView view(nullptr, layoutForCanvas(width), texts, text,
                      [](const std::string&) -> SDL_Texture* { return nullptr; });
        using Result = SaveView::Result;
        CHECK(key(view, SDLK_RETURN) == Result::None);
        view.open(true);
        CHECK(view.saving() && view.slot() == 0);
        CHECK(key(view, SDLK_RETURN) == Result::Confirm);
        for (int i = 0; i < 12; ++i) key(view, SDLK_DOWN);
        CHECK(view.slot() == 8);
        view.close();
        view.open(false);
        CHECK(!view.saving() && view.slot() == 0);
        key(view, SDLK_UP);
        CHECK(view.slot() == 0);

        // Every row is 41 clickable pixels followed by a one-pixel separator.
        for (int slot = 0; slot < 9; ++slot) {
            const int top = (slot + 1) * 42;
            CHECK(click(view, 0, top) == Result::None);
            CHECK(view.slot() == slot);
            CHECK(click(view, 557, top + 40) == Result::None);
            CHECK(view.slot() == slot);
            key(view, slot == 0 ? SDLK_DOWN : SDLK_UP);
            const int previous = view.slot();
            CHECK(click(view, 100, top + 41) == Result::None);
            CHECK(view.slot() == previous);
        }
        const int selected = view.slot();
        CHECK(click(view, 100, 41) == Result::None && view.slot() == selected);
        CHECK(click(view, -1, 100) == Result::None && view.slot() == selected);
        CHECK(click(view, 558, 100) == Result::None && view.slot() == selected);
        CHECK(click(view, 100, 100, SDL_BUTTON_MIDDLE) == Result::None && view.slot() == selected);

        for (int y : {420, 461}) {
            CHECK(click(view, 0, y) == Result::Close);
            CHECK(click(view, 149, y) == Result::Close);
            CHECK(click(view, 150, y) == Result::None);
            CHECK(click(view, 278, y) == Result::None);
            CHECK(click(view, 457, y) == Result::None);
            CHECK(click(view, 458, y) == Result::Confirm);
            CHECK(click(view, 557, y) == Result::Confirm);
        }
        CHECK(click(view, 500, 462) == Result::None);
        hover(view, 20, 430);
        CHECK(key(view, SDLK_RETURN) == Result::Confirm);
        key(view, SDLK_LEFT);
        hover(view, 500, 430);
        CHECK(key(view, SDLK_RETURN) == Result::Close);
        CHECK(click(view, 500, 430) == Result::Close); // Enter respects selected Nein
        CHECK(click(view, 100, 50, SDL_BUTTON_LEFT, 2) == Result::Close);
        key(view, SDLK_RIGHT);
        CHECK(click(view, 100, 50) == Result::None && view.slot() == 0);
        CHECK(click(view, 100, 50, SDL_BUTTON_LEFT, 2) == Result::Confirm);
        CHECK(click(view, 200, 10, SDL_BUTTON_LEFT, 2) == Result::Confirm);
        CHECK(click(view, 0, 0, SDL_BUTTON_RIGHT) == Result::Close);
        CHECK(key(view, SDLK_ESCAPE) == Result::Close);
        CHECK(key(view, SDLK_BACKSPACE) == Result::Close);
        key(view, SDLK_LEFT);
        view.close();
        view.open(true);
        CHECK(view.slot() == 0 && key(view, SDLK_RETURN) == Result::Confirm);
    }
    return failures ? 1 : 0;
}
