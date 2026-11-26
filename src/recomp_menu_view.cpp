// SPDX-License-Identifier: GPL-3.0-only
#include "recomp_menu_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

// Our own palette: dark wood, warm text and a gold accent.
constexpr SDL_Color kGold{214, 170, 80, 255};
constexpr SDL_Color kText{238, 226, 200, 255};
constexpr SDL_Color kMuted{170, 150, 118, 255};
constexpr SDL_Color kDark{34, 22, 12, 255};

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
    const RecompMenu::Page& page = menu_.pages()[size_t(menu_.page())];
    if (page.table()) { // compact rows under a heading row; RecompMenu::kTableRows at once
        g.tableHead = {g.content.x, g.content.y + px(44), g.content.w, px(26)};
        const int n = int(page.items.size()), shown = RecompMenu::kTableRows;
        if (menu_.inItems()) tableTop_ = std::clamp(tableTop_, menu_.row() - shown + 1, menu_.row());
        tableTop_ = std::clamp(tableTop_, 0, std::max(0, n - shown));
        for (int i = 0; i < n; ++i) {
            const int k = i - tableTop_;
            g.rows.push_back(k >= 0 && k < shown ? SDL_Rect{g.content.x, g.content.y + px(76) + k * px(31), g.content.w, px(29)}
                                                 : SDL_Rect{0, 0, 0, 0});
        }
        return g;
    }
    for (size_t i = 0; i < page.items.size(); ++i)
        g.rows.push_back({g.content.x, g.content.y + px(52) + int(i) * px(46), g.content.w, px(40)});
    return g;
}

RecompMenu::Action RecompMenuView::handle(const SDL_Event& event) {
    using Input = RecompMenu::Input;
    if (!menu_.isOpen()) return RecompMenu::Action::None;
    if (menu_.capturing()) { // waiting for a pad button: Escape / Backspace or a click cancel
        if (event.type == SDL_KEYDOWN && !event.key.repeat
            && (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE))
            return menu_.input(Input::Back);
        if (event.type == SDL_MOUSEBUTTONDOWN) menu_.cancelCapture("Abgebrochen, die Belegung bleibt.");
        return RecompMenu::Action::None;
    }
    if (event.type == SDL_MOUSEWHEEL && menu_.pages()[size_t(menu_.page())].table() && menu_.inItems()) {
        if (event.wheel.y > 0) return menu_.input(Input::Up); // the table scrolls with the focus
        if (event.wheel.y < 0) return menu_.input(Input::Down);
        return RecompMenu::Action::None;
    }
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_UP: case SDLK_KP_8: return menu_.input(Input::Up);
        case SDLK_DOWN: case SDLK_KP_2: return menu_.input(Input::Down);
        case SDLK_LEFT: case SDLK_KP_4: return menu_.input(Input::Left);
        case SDLK_RIGHT: case SDLK_KP_6: return menu_.input(Input::Right);
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: return menu_.input(Input::Accept);
        case SDLK_TAB: // the pad's LB / RB arrive as Shift+Tab / Tab: the neighbour page
            return menu_.input((event.key.keysym.mod & KMOD_SHIFT) ? Input::PrevPage : Input::NextPage);
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

// Columns: the action (38 %), the keyboard (34 %) and the pad (the rest), gold headings.
// One font size for all cells: the largest down to two thirds at which every cell fits
// its column (the canvas of 640 is narrower than the panel's 768-line proportions).
// Binding rows, the stick choice and the reset take the focus like other entries.
void RecompMenuView::drawTable(const Geometry& g, const RecompMenu::Page& page) {
    const SDL_Rect& head = g.tableHead;
    const int pad = px(14);
    const int x0 = head.x + pad, x1 = head.x + head.w * 38 / 100, x2 = head.x + head.w * 72 / 100;
    const int xs[] = {x0, x1, x2}, ws[] = {x1 - x0 - pad, x2 - x1 - pad, head.x + head.w - x2 - pad};
    TextStyle style{px(17), true, false, false, false, 0, kText};
    const auto fits = [&] {
        for (const RecompMenu::Item& item : page.items) {
            const std::string cells[] = {item.label, item.value, menu_.padText(item)};
            for (int c = 0; c < 3; ++c)
                if (text_.measure(cells[c], style).x > ws[c]) return false;
        }
        return true;
    };
    for (const int smallest = style.pixelHeight * 2 / 3; style.pixelHeight > smallest && !fits();) --style.pixelHeight;
    const int lineH = style.pixelHeight + px(4);
    TextStyle heading = style;
    heading.color = kGold;
    for (size_t c = 0; c < page.columns.size() && c < 3; ++c)
        text_.draw(renderer_, page.columns[c], ScreenRect{xs[c], head.y + (head.h - lineH) / 2, ws[c], lineH}, heading);
    fill(renderer_, SDL_Rect{head.x, head.y + head.h, head.w, std::max(1, px(2))}, kGold, 140);
    int first = -1, last = -1;
    for (size_t i = 0; i < g.rows.size() && i < page.items.size(); ++i) {
        const SDL_Rect& row = g.rows[i];
        if (row.w == 0) continue; // outside the table's window
        if (first < 0) first = int(i);
        last = int(i);
        const RecompMenu::Item& item = page.items[i];
        const bool focused = menu_.inItems() && int(i) == menu_.row();
        if (focused) {
            fill(renderer_, row, SDL_Color{255, 236, 190, 255}, 34);
            fill(renderer_, SDL_Rect{row.x, row.y, std::max(3, px(5)), row.h}, kGold, 255);
        } else if (i % 2 == 0) {
            fill(renderer_, row, SDL_Color{0, 0, 0, 255}, 40);
        }
        const bool waiting = item.kind == RecompMenu::Kind::Binding && menu_.captureAction() == item.padAction;
        const std::string cells[] = {item.label, item.value, menu_.padText(item)}; // value: the keyboard
        for (int c = 0; c < 3; ++c) {
            TextStyle cell = style;
            if (c == 0) cell.color = item.kind == RecompMenu::Kind::Info ? kMuted : kText;
            if (c == 2 && (waiting || item.kind == RecompMenu::Kind::Choice)) cell.color = kGold;
            text_.draw(renderer_, cells[c], ScreenRect{xs[c], row.y + (row.h - lineH) / 2, ws[c], lineH}, cell);
        }
        if (item.kind == RecompMenu::Kind::Command)
            text_.draw(renderer_, "›", ScreenRect{row.x, row.y + (row.h - lineH) / 2, row.w - pad, lineH},
                       TextStyle{style.pixelHeight, true, false, false, true, 0, focused ? kGold : kMuted});
    }
    // More rows above / below the window: marks at the right of the heading row.
    const std::string more = std::string(first > 0 ? "▲" : "") + (last >= 0 && last + 1 < int(page.items.size()) ? "▼" : "");
    if (!more.empty())
        text_.draw(renderer_, more, ScreenRect{head.x, head.y + (head.h - lineH) / 2, head.w - px(4), lineH},
                   TextStyle{style.pixelHeight, true, false, false, true, 0, kGold});
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
    text_.draw(renderer_, "Recomp " + version_, ScreenRect{g.header.x, g.header.y + px(30), g.header.w - px(28), px(20)}, small);
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
    if (page.table()) drawTable(g, page);
    for (size_t i = 0; i < g.rows.size() && !page.table(); ++i) {
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
        case RecompMenu::Kind::Info: case RecompMenu::Kind::Binding:
            text_.draw(renderer_, value, ScreenRect{row.x, textY, row.w - px(14), px(22)},
                       TextStyle{px(18), true, false, false, true, 0, kText});
            break;
        case RecompMenu::Kind::Command:
            text_.draw(renderer_, "›", ScreenRect{row.x, textY - px(2), row.w - px(18), px(24)},
                       TextStyle{px(24), true, false, false, true, 0, focused ? kGold : kMuted});
            break;
        }
        if (item.original)
            text_.draw(renderer_, "ORIGINAL", ScreenRect{row.x, row.y + (row.h - px(14)) / 2, tagRight - row.x, px(14)},
                       TextStyle{px(12), true, false, false, true, 0, kMuted});
    }

    // Footer: the focused entry's explanation and the keys.
    fill(renderer_, SDL_Rect{g.panel.x + px(20), g.footer.y, g.panel.w - px(40), 1}, kGold, 120);
    std::string hint = page.hint.empty() ? "Wähle eine Seite." : page.hint;
    if (menu_.inItems() && !page.items.empty()) {
        const RecompMenu::Item& focusedItem = page.items[size_t(menu_.row())];
        hint = focusedItem.hint;
        // The sound set's state (pending switch, missing sets) replaces its explanation.
        if (focusedItem.field == RecompMenu::Field::MusicPack && !menu_.music().note.empty()) hint = menu_.music().note;
    }
    if (page.title == "Audio" && menu_.music().ids.empty() && !menu_.music().note.empty() && !menu_.inItems())
        hint = menu_.music().note;
    if (page.table() && menu_.inItems() && !menu_.padNote().empty()) hint = menu_.padNote(); // the last remapping
    if (menu_.capturing())
        hint = std::string("Neue Taste am Gamepad f\u00fcr \u201e") + padActionLabel(menu_.captureAction())
               + "\u201c dr\u00fccken \u2026 Esc oder Mausklick bricht ab ("
               + std::to_string(menu_.captureSecondsLeft(SDL_GetTicks())) + " s).";
    text_.drawWrapped(renderer_, hint, ScreenRect{g.footer.x + px(28), g.footer.y + px(8), g.footer.w - px(56), px(40)},
                      TextStyle{px(16), true, false, false, false, 0, kText});
    // Keyboard first, the pad's button after the slash (the Steuerung page has the rest).
    text_.draw(renderer_, menu_.keyLine(),
               ScreenRect{g.footer.x + px(28), g.footer.y + px(52), g.footer.w - px(56), px(18)},
               TextStyle{px(14), true, false, false, false, 0, kMuted});
    SDL_SetRenderDrawBlendMode(renderer_, blend);
}

} // namespace dtvr
