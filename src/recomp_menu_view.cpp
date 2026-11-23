// SPDX-License-Identifier: GPL-3.0-only
#include "recomp_menu_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

// Our own palette: dark wood, warm text and a gold accent.
constexpr SDL_Color kGold{214, 170, 80, 255};
constexpr SDL_Color kText{238, 226, 200, 255};
constexpr SDL_Color kMuted{170, 150, 118, 255};
constexpr SDL_Color kDark{34, 22, 12, 255};
constexpr SDL_Color kWarn{244, 150, 64, 255};

void fill(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color c, Uint8 alpha) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, alpha);
    SDL_RenderFillRect(r, &rect);
}

void frame(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color c, int width) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int i = 0; i < width; ++i) {
        const SDL_Rect f{rect.x + i, rect.y + i, rect.w - 2 * i, rect.h - 2 * i};
        SDL_RenderDrawRect(r, &f);
    }
}

bool inside(const SDL_Rect& r, int x, int y) {
    const SDL_Point p{x, y};
    return SDL_PointInRect(&p, &r) == SDL_TRUE;
}

} // namespace

RecompMenuView::RecompMenuView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout,
                               TextRenderer& text, std::string version)
    : renderer_(renderer), root_(root), layout_(layout), text_(text), version_(std::move(version)) {}

SDL_Rect RecompMenuView::sliderBar(const SDL_Rect& row) const {
    const int w = px(200), h = px(12);
    return {row.x + row.w - px(84) - w, row.y + (row.h - h) / 2, w, h};
}

int RecompMenuView::px(double v) const { return int(std::lround(v * layout_.height / 768.0)); }

RecompMenuView::Geometry RecompMenuView::geometry() const {
    Geometry g;
    g.scale = layout_.height / 768.0;
    const int w = std::min(layout_.width - px(48), px(960)), h = std::min(layout_.height - px(48), px(620));
    g.panel = {(layout_.width - w) / 2, (layout_.height - h) / 2, w, h};
    g.header = {g.panel.x, g.panel.y, w, px(72)};
    g.footer = {g.panel.x, g.panel.y + h - px(76), w, px(76)};
    const int bodyY = g.header.y + g.header.h, bodyH = g.footer.y - bodyY;
    g.sidebar = {g.panel.x, bodyY, px(230), bodyH};
    g.content = {g.sidebar.x + g.sidebar.w + px(28), bodyY + px(18), w - g.sidebar.w - px(56), bodyH - px(18)};
    for (size_t i = 0; i < menu_.pages().size(); ++i)
        g.pages.push_back({g.sidebar.x + px(14), g.sidebar.y + px(18) + int(i) * px(52), g.sidebar.w - px(28), px(44)});
    const auto& items = menu_.pages()[size_t(menu_.page())].items;
    for (size_t i = 0; i < items.size(); ++i)
        g.rows.push_back({g.content.x, g.content.y + px(52) + int(i) * px(46), g.content.w, px(40)});
    return g;
}

RecompMenu::Action RecompMenuView::handle(const SDL_Event& event) {
    using Input = RecompMenu::Input;
    if (!menu_.isOpen()) return RecompMenu::Action::None;
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_UP: case SDLK_KP_8: return menu_.input(Input::Up);
        case SDLK_DOWN: case SDLK_KP_2: return menu_.input(Input::Down);
        case SDLK_LEFT: case SDLK_KP_4: return menu_.input(Input::Left);
        case SDLK_RIGHT: case SDLK_KP_6: return menu_.input(Input::Right);
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: return menu_.input(Input::Accept);
        case SDLK_ESCAPE: case SDLK_BACKSPACE:
            if (event.key.repeat) return RecompMenu::Action::None; // a held key must not reopen the game at once
            return menu_.input(Input::Back);
        default: return RecompMenu::Action::None;
        }
    }
    const Geometry g = geometry();
    if (event.type == SDL_MOUSEMOTION) {
        for (size_t i = 0; i < g.rows.size(); ++i)
            if (inside(g.rows[i], event.motion.x, event.motion.y)) menu_.focusItem(int(i));
        return RecompMenu::Action::None;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) return menu_.input(Input::Back);
    if (event.type != SDL_MOUSEBUTTONDOWN || event.button.button != SDL_BUTTON_LEFT) return RecompMenu::Action::None;
    const int x = event.button.x, y = event.button.y;
    for (size_t i = 0; i < g.pages.size(); ++i)
        if (inside(g.pages[i], x, y)) { menu_.focusPage(int(i)); return RecompMenu::Action::None; }
    for (size_t i = 0; i < g.rows.size(); ++i) {
        if (!inside(g.rows[i], x, y)) continue;
        menu_.focusItem(int(i));
        if (!menu_.inItems() || menu_.row() != int(i)) return RecompMenu::Action::None; // an info row
        const auto& item = menu_.pages()[size_t(menu_.page())].items[i];
        if (item.kind == RecompMenu::Kind::Slider) { // a click on the bar sets the value
            const SDL_Rect bar = sliderBar(g.rows[i]);
            if (x < bar.x - px(8)) return menu_.step(-1);
            return menu_.setSlider((x - bar.x) * 100 / std::max(1, bar.w));
        }
        // A choice: the left half of its value steps back, the right half forward.
        if (item.kind == RecompMenu::Kind::Choice && x < g.rows[i].x + g.rows[i].w - px(130)) return menu_.step(-1);
        return menu_.activate();
    }
    return RecompMenu::Action::None;
}

void RecompMenuView::drawToggle(const SDL_Rect& row, bool on, bool focused, double scale) {
    const int w = int(52 * scale), h = int(24 * scale);
    const SDL_Rect pill{row.x + row.w - w - int(12 * scale), row.y + (row.h - h) / 2, w, h};
    fill(renderer_, pill, on ? kGold : SDL_Color{78, 64, 52, 255}, 255);
    frame(renderer_, pill, focused ? kText : SDL_Color{20, 12, 6, 255}, 1);
    const int knob = h - int(6 * scale);
    const SDL_Rect k{on ? pill.x + pill.w - knob - int(3 * scale) : pill.x + int(3 * scale), pill.y + int(3 * scale), knob, knob};
    fill(renderer_, k, on ? kDark : kMuted, 255);
}

void RecompMenuView::draw() {
    if (!menu_.isOpen()) return;
    if (!woodTried_) {
        woodTried_ = true;
        const fs::path p = findCaseInsensitive(root_, fs::path("butt") / std::to_string(layout_.artDir) / "bk.bmp");
        if (!p.empty()) {
            auto surface = loadBmp(p);
            wood_.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        }
    }
    SDL_BlendMode blend;
    SDL_GetRenderDrawBlendMode(renderer_, &blend);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    const Geometry g = geometry();
    const double s = g.scale;
    const SDL_Rect canvas{0, 0, layout_.width, layout_.height};
    fill(renderer_, canvas, SDL_Color{0, 0, 0, 255}, 170);
    fill(renderer_, SDL_Rect{g.panel.x + px(10), g.panel.y + px(10), g.panel.w, g.panel.h}, SDL_Color{0, 0, 0, 255}, 120);
    if (wood_) {
        SDL_RenderCopy(renderer_, wood_.get(), nullptr, &g.panel);
        fill(renderer_, g.panel, SDL_Color{12, 8, 5, 255}, 185);
    } else {
        fill(renderer_, g.panel, SDL_Color{40, 27, 18, 255}, 255);
    }
    frame(renderer_, g.panel, kGold, std::max(2, px(3)));

    // Header: the game's name and the port's version.
    TextStyle title{px(30), true, true, false, false, std::max(1, px(2)), kGold};
    text_.draw(renderer_, "Die total verrückte Rallye", ScreenRect{g.header.x + px(28), g.header.y + px(18), g.header.w / 2, px(34)}, title);
    TextStyle small{px(15), true, false, false, true, 0, kMuted};
    text_.draw(renderer_, "Recomb " + version_, ScreenRect{g.header.x, g.header.y + px(30), g.header.w - px(28), px(20)}, small);
    fill(renderer_, SDL_Rect{g.panel.x + px(20), g.header.y + g.header.h - 2, g.panel.w - px(40), 2}, kGold, 160);

    // Page list.
    fill(renderer_, g.sidebar, SDL_Color{0, 0, 0, 255}, 70);
    for (size_t i = 0; i < g.pages.size(); ++i) {
        const bool selected = int(i) == menu_.page();
        if (selected) {
            fill(renderer_, g.pages[i], kGold, menu_.inItems() ? 150 : 235);
            if (!menu_.inItems()) frame(renderer_, g.pages[i], kText, std::max(1, px(2)));
        }
        TextStyle style{px(21), true, false, false, false, 0, selected ? kDark : kText};
        text_.draw(renderer_, menu_.pages()[i].title,
                   ScreenRect{g.pages[i].x + px(16), g.pages[i].y + (g.pages[i].h - px(24)) / 2, g.pages[i].w - px(20), px(24)}, style);
    }

    // Entries of the page.
    const RecompMenu::Page& page = menu_.pages()[size_t(menu_.page())];
    text_.draw(renderer_, page.title, ScreenRect{g.content.x, g.content.y, g.content.w, px(30)},
               TextStyle{px(26), true, false, false, false, 1, kText});
    for (size_t i = 0; i < g.rows.size(); ++i) {
        const RecompMenu::Item& item = page.items[i];
        const SDL_Rect& row = g.rows[i];
        const bool focused = menu_.inItems() && int(i) == menu_.row();
        if (focused) {
            fill(renderer_, row, SDL_Color{255, 236, 190, 255}, 34);
            fill(renderer_, SDL_Rect{row.x, row.y, std::max(3, px(5)), row.h}, kGold, 255);
        } else if (i % 2 == 0) {
            fill(renderer_, row, SDL_Color{0, 0, 0, 255}, 40);
        }
        const int textY = row.y + (row.h - px(22)) / 2;
        const SDL_Color labelColour = item.kind == RecompMenu::Kind::Info ? kMuted : kText;
        text_.draw(renderer_, item.label, ScreenRect{row.x + px(18), textY, row.w / 2, px(22)},
                   TextStyle{px(19), true, false, false, false, 0, labelColour});
        int tagRight = row.x + row.w - px(150);
        const std::string value = menu_.valueText(item);
        switch (item.kind) {
        case RecompMenu::Kind::Toggle:
            drawToggle(row, value == "An", focused, s);
            tagRight = row.x + row.w - px(80);
            break;
        case RecompMenu::Kind::Choice:
            text_.draw(renderer_, "‹  " + value + "  ›", ScreenRect{row.x, textY, row.w - px(14), px(22)},
                       TextStyle{px(18), true, false, false, true, 0, kGold});
            tagRight = row.x + row.w - px(14) - text_.measure("‹  " + value + "  ›", TextStyle{px(18)}).x - px(16);
            break;
        case RecompMenu::Kind::Slider: {
            const SDL_Rect bar = sliderBar(row);
            const int v = std::clamp(std::atoi(value.c_str()), 0, 100);
            fill(renderer_, bar, SDL_Color{78, 64, 52, 255}, 255);
            fill(renderer_, SDL_Rect{bar.x, bar.y, bar.w * v / 100, bar.h}, kGold, 255);
            frame(renderer_, bar, focused ? kText : SDL_Color{20, 12, 6, 255}, 1);
            text_.draw(renderer_, value, ScreenRect{row.x, textY, row.w - px(14), px(22)},
                       TextStyle{px(18), true, false, false, true, 0, kGold});
            tagRight = bar.x - px(12);
            break;
        }
        case RecompMenu::Kind::Info:
            text_.draw(renderer_, value, ScreenRect{row.x, textY, row.w - px(14), px(22)},
                       TextStyle{px(18), true, false, false, true, 0, kText});
            break;
        case RecompMenu::Kind::Command:
            text_.draw(renderer_, "›", ScreenRect{row.x, textY - px(2), row.w - px(18), px(24)},
                       TextStyle{px(24), true, false, false, true, 0, focused ? kGold : kMuted});
            break;
        }
        std::string tag = item.original ? "ORIGINAL" : "";
        SDL_Color tagColour = kMuted;
        if (item.restart) {
            tag = menu_.restartPending() ? "NEUSTART NÖTIG" : "AB NEUSTART";
            tagColour = menu_.restartPending() ? kWarn : kMuted;
        }
        if (!tag.empty())
            text_.draw(renderer_, tag, ScreenRect{row.x, row.y + (row.h - px(14)) / 2, tagRight - row.x, px(14)},
                       TextStyle{px(12), true, false, false, true, 0, tagColour});
    }

    // Footer: the focused entry's explanation and the keys.
    fill(renderer_, SDL_Rect{g.panel.x + px(20), g.footer.y, g.panel.w - px(40), 1}, kGold, 120);
    std::string hint = "Wähle eine Seite.";
    if (menu_.inItems() && !page.items.empty()) {
        const RecompMenu::Item& focusedItem = page.items[size_t(menu_.row())];
        hint = focusedItem.hint;
        // The sound set's state (pending switch, missing sets) replaces its explanation.
        if (focusedItem.field == RecompMenu::Field::MusicPack && !menu_.music().note.empty()) hint = menu_.music().note;
    }
    if (page.title == "Audio" && menu_.music().ids.empty() && !menu_.music().note.empty() && !menu_.inItems())
        hint = menu_.music().note;
    text_.drawWrapped(renderer_, hint, ScreenRect{g.footer.x + px(28), g.footer.y + px(8), g.footer.w - px(56), px(40)},
                      TextStyle{px(16), true, false, false, false, 0, kText});
    text_.draw(renderer_, "↑↓ Auswählen    ←→ Ändern    Enter Bestätigen    Esc Zurück",
               ScreenRect{g.footer.x + px(28), g.footer.y + px(52), g.footer.w - px(56), px(18)},
               TextStyle{px(14), true, false, false, false, 0, kMuted});
    SDL_SetRenderDrawBlendMode(renderer_, blend);
}

} // namespace dtvr
