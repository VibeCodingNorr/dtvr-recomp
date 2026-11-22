// SPDX-License-Identifier: GPL-3.0-only
#include "estates_view.hpp"

#include "panel.hpp"

#include <algorithm>
#include <cstdio>

namespace dtvr {
namespace {

constexpr int kWidth = 0x22E, kHeight = 0x1A4, kRow = 0x2A; // CreateWindow seg1:4CB6, rows of 42
constexpr int kVisibleRows = kHeight / kRow - 1;            // below the header strip
constexpr int kTabX[3] = {18, 18 + 0x8E, 18 + 0x8E + 0xB0};  // seg1:7375, 73F5, 747B
constexpr SDL_Color kBrown{67, 27, 0, 255}, kGray{123, 95, 27, 255}, kWhite{255, 255, 255, 255},
    kYellow{255, 247, 0, 255}; // palette 0x45, 0x43, 0xFF, 0x2C

} // namespace

EstatesView::EstatesView(SDL_Renderer* renderer, const ScreenLayout& layout,
                         const std::map<int, std::string>& texts, TextRenderer& text, SpriteLookup sprite)
    : renderer_(renderer), layout_(layout), texts_(texts), text_(text), sprite_(std::move(sprite)) {}

void EstatesView::openStatistics(int player) {
    open(-1);
    statistics_ = true;
    focus_ = 0; // seg1:4FC9
    player_ = player;
    category_ = 0; // seg1:4F59
    sort_ = 0;
}

void EstatesView::open(int town) {
    statistics_ = false;
    town_ = town;
    tab_ = EstatesTab::None;
    focus_ = 0; // seg1:4CEB
    selected_ = -1;
    scroll_ = 0;
}

SDL_Rect EstatesView::box() const {
    return SDL_Rect{(layout_.width - kWidth) / 2, (layout_.height - kHeight) / 2, kWidth, kHeight};
}

int EstatesView::rowAt(int x, int y, int count) const {
    const SDL_Rect b = box();
    if (x < b.x || x >= b.x + b.w || y < b.y + kRow || y >= b.y + b.h) return -1;
    const int row = (y - b.y) / kRow - 1 + scroll_;
    return row < count ? row : -1;
}

void EstatesView::keepVisible(int count) {
    // seg1:494F: past the eighth visible row the list jumps so that the selection is
    // on top (at most to count - 7); above the top it jumps back by seven.
    if (selected_ < 0) return;
    if (scroll_ + 8 <= selected_) scroll_ = std::min(selected_, count - 7);
    else if (selected_ < scroll_) scroll_ = std::max(0, selected_ - 7);
    scroll_ = std::max(0, scroll_);
}

void EstatesView::afterAction(int count) {
    // seg1:55A5 / 5646: a purchase or sale that succeeded sends VK_DOWN to the dialog.
    if (tab_ == EstatesTab::None || count <= 0) return;
    selected_ = ((selected_ < 0 ? 0 : selected_) + 1) % count;
    keepVisible(count);
}

EstatesView::Action EstatesView::handle(const SDL_Event& event, const std::vector<EstateRow>& rows) {
    Action action;
    if (!isOpen()) return action;
    const int count = int(rows.size());
    auto rowAction = [&](int row) {
        selected_ = row;
        // seg1:5583: the purchase (sale) is tried for any row; a refused one only sounds.
        if (tab_ != EstatesTab::None) {
            action.kind = Action::Kind::Row;
            action.entry = rows[size_t(row)].entry;
        }
    };
    // seg1:537F: Escape / Backspace (and the right button, which sends VK_BACK,
    // seg1:5365) leave an active tab with Plopp02 and give its header item the
    // focus again; without a tab they close the dialog (the stop then ends the turn).
    auto cancel = [&] {
        if (tab_ != EstatesTab::None && !statistics_) {
            focus_ = tab_ == EstatesTab::Buy ? 0 : 1; // DS:3FB6 = DS:3FBC
            tab_ = EstatesTab::None;
            selected_ = -1;
            action.kind = Action::Kind::Tab;
        } else {
            action.kind = Action::Kind::Close;
        }
    };
    auto selectTab = [&](EstatesTab tab) { // seg1:543E / 5475: the tab selects the first visible row
        tab_ = tab;
        focus_ = -1;
        selected_ = count > 0 ? std::min(scroll_, count - 1) : -1;
        action.kind = Action::Kind::Tab;
    };
    auto stepList = [&](int& value, int modulo, int by) { // seg1:56CA / 5710 / 59EF / 5A40
        value = (value + by + modulo) % modulo;
        scroll_ = 0;
        selected_ = -1;
        action.kind = Action::Kind::Tab; // Plopp02
    };
    // Enter on the header item with the focus (seg1:5406; statistics seg1:5676).
    auto activate = [&] {
        if (statistics_) {
            if (focus_ == 1) stepList(category_, 7, 1);
            else if (focus_ == 2) stepList(sort_, 3, 1);
            else if (focus_ == 3) { action.kind = Action::Kind::Close; action.sound = true; }
            // focus 0 (seg1:5687): the sale mode 0x12 is not rebuilt yet
        } else if (focus_ == 0) selectTab(EstatesTab::Buy);
        else if (focus_ == 1) selectTab(EstatesTab::Sell);
        else if (focus_ == 2) action.kind = Action::Kind::Cards;
        else if (focus_ == 3) { action.kind = Action::Kind::Close; action.sound = true; }
    };
    // No WM_MOUSEMOVE handler (seg1:5D4A). The wheel is a recomb addition where the
    // original has arrow buttons to page: the statistics (PFEIL1 / PFEIL2).
    if (event.type == SDL_MOUSEWHEEL && statistics_) {
        scroll_ = std::max(0, std::min(std::max(0, count - kVisibleRows), scroll_ - event.wheel.y));
    } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        cancel();
    } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) { // seg1:5DA2
        const SDL_Rect b = box();
        const int x = event.button.x - b.x, y = event.button.y - b.y;
        // Header rectangles (PtInRect, right edge excluded): the dialog and the statistics.
        static constexpr int kItems[2][4][2] = {{{0, 0x9F}, {0xA0, 0x128}, {0x129, 0x190}, {0x191, 0x22E}},
                                                {{0, 0x81}, {0x82, 0xD1}, {0xD2, 0x1AE}, {0x1AF, 0x22E}}};
        if (y >= 0 && y < 0x2A) {
            if (tab_ == EstatesTab::None) // seg1:5FB3: only without an active tab
                for (int i = 0; i < 4; ++i)
                    if (x >= kItems[statistics_][i][0] && x < kItems[statistics_][i][1]) {
                        focus_ = i; // seg1:5FD9, then Enter
                        activate();
                        break;
                    }
        } else if (const int row = rowAt(event.button.x, event.button.y, count); row >= 0) {
            if (!statistics_ && tab_ != EstatesTab::None) rowAction(row); // seg1:6037: only with a tab
        }
    } else if (event.type == SDL_KEYDOWN) { // seg1:5BE5
        const SDL_Keycode k = event.key.keysym.sym;
        const bool enter = k == SDLK_RETURN || k == SDLK_KP_ENTER;
        if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE) {
            cancel();
        } else if (tab_ != EstatesTab::None) { // seg1:5942 / 5A7F: up / down wrap around
            if (count > 0 && (k == SDLK_UP || k == SDLK_DOWN)) {
                const int from = selected_ < 0 ? 0 : selected_;
                selected_ = (from + count + (k == SDLK_UP ? -1 : 1)) % count;
                keepVisible(count);
            }
            if (enter && selected_ >= 0 && selected_ < count) rowAction(selected_);
        } else { // the header: left / right move the focus over four items (seg1:58EF / 591B)
            if (k == SDLK_LEFT) focus_ = focus_ > 0 ? focus_ - 1 : 3;
            if (k == SDLK_RIGHT) focus_ = focus_ >= 3 || focus_ < 0 ? 0 : focus_ + 1;
            if (enter) activate();
            if (statistics_ && (k == SDLK_UP || k == SDLK_DOWN)) { // seg1:5995 / 5AD5
                const int dir = k == SDLK_UP ? -1 : 1;
                if (focus_ == 0) { // a page of seven
                    if (dir < 0) scroll_ = scroll_ > 7 ? scroll_ - 7 : 0;
                    else if (count > 0) scroll_ = count - 8 > scroll_ ? scroll_ + 7 : std::max(0, count - kVisibleRows);
                    action.kind = Action::Kind::Tab;
                } else if (focus_ == 1) stepList(category_, 7, dir);
                else if (focus_ == 2) stepList(sort_, 3, dir);
            }
        }
    }
    return action;
}

void EstatesView::draw(const std::vector<EstateRow>& rows, const std::array<char, 8>& ownerLetters) {
    if (!isOpen()) return;
    const SDL_Rect b = box();
    auto blit = [&](const std::string& name, int x, int y) {
        SDL_Texture* t = sprite_(name);
        if (!t) return;
        int w = 0, h = 0;
        SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
        const SDL_Rect dst{b.x + x, b.y + y, w, h};
        SDL_RenderCopy(renderer_, t, nullptr, &dst);
    };
    // Header seg1:7275: KO_Z, tabs in Arial 20 weight 400 at y + 10; the active tab
    // (DS:3FBC) in colour 0x2C.
    blit("KO_Z", 0, 0);
    for (int i = 0; i < 3 && !statistics_; ++i) {
        const auto it = texts_.find(321 + i); // LoadString 0x141-0x143: Kaufen, Verkaufen, Karte
        const std::string label = it == texts_.end() ? std::string() : it->second;
        // seg1:738C: with a header focus the focused item is white, the others 0x45;
        // otherwise the active tab is yellow (pressed items 0x43 are not tracked).
        const bool active = (i == 0 && tab_ == EstatesTab::Buy) || (i == 1 && tab_ == EstatesTab::Sell);
        const SDL_Color colour = focus_ >= 0 ? (i == focus_ ? kWhite : kBrown) : active ? kYellow : kBrown;
        text_.draw(renderer_, label, ScreenRect{b.x + kTabX[i], b.y + 10, 140, kRow},
                   TextStyle{20, false, false, false, false, 0, colour});
    }
    // Rows seg1:65A7 / 6163: strip GR_Z (selected GR_ZI), owner icon at x 0, texts in
    // Arial 18 weight 400 from x 0x54 + 10, y + 10.
    for (int k = 0; k < kVisibleRows && scroll_ + k < int(rows.size()); ++k) {
        const int index = scroll_ + k;
        const EstateRow& row = rows[size_t(index)];
        const int y = (k + 1) * kRow;
        const bool selected = index == selected_;
        if (statistics_) blit(selected ? "ZW_ZI" : "ZW_Z", 0, y); // seg1:6617: bit 0x10
        else blit(selected ? "GR_ZI" : "GR_Z", 0, y);
        std::string icon = "0N";
        if (row.owner >= 0 && row.owner < 8)
            icon = std::string(1, ownerLetters[size_t(row.owner)]) + (row.wholeTown ? "_M" : "_N");
        blit(icon, 0, y);
        const SDL_Color color = row.disabled ? kGray : selected ? kWhite : kBrown;
        TextStyle left{statistics_ ? 17 : 18, false, false, false, false, 0, color};
        TextStyle right = left;
        right.right = true;
        if (statistics_) { // seg1:6EC5: name, town, value * 0.7, income, yield
            const int ty = b.y + y + 10;
            text_.draw(renderer_, row.name, ScreenRect{b.x + 0x2A + 2, ty, 0xC8 - 0x2C, kRow}, left);
            text_.draw(renderer_, row.townName, ScreenRect{b.x + 0xC8, ty, 0x153 - 0xC8, kRow}, left);
            text_.draw(renderer_, formatMoney(row.price), ScreenRect{b.x + 0x153, ty, 0x53, kRow}, right);
            text_.draw(renderer_, formatMoney(row.income), ScreenRect{b.x + 0x1AE, ty, 0x3C, kRow}, right);
            char yield[16];
            std::snprintf(yield, sizeof yield, "%.1f%%", row.yieldPercent); // DS:402E
            text_.draw(renderer_, yield, ScreenRect{b.x + 0x1F0, ty, 0x3A, kRow}, right);
            continue;
        }
        int x = 0x54 + 10;
        text_.draw(renderer_, row.name, ScreenRect{b.x + x, b.y + y + 10, 200, kRow}, left);
        x += 200;
        text_.draw(renderer_, formatMoney(row.price), ScreenRect{b.x + x, b.y + y + 10, 0x54, kRow}, right);
        x += 80;
        text_.draw(renderer_, formatMoney(row.income), ScreenRect{b.x + x, b.y + y + 10, 0x5E, kRow}, right);
        x += 90;
        char percent[16];
        std::snprintf(percent, sizeof percent, "%.1f%%", row.yieldPercent); // DS:4027
        text_.draw(renderer_, percent, ScreenRect{b.x + x, b.y + y + 10, 0x54, kRow}, right);
    }
    if (statistics_) drawStatisticsHeader();
}

void EstatesView::drawStatisticsHeader() {
    // seg1:7506: text 375 (Ansehen) at x 28, then PFEIL1 at (x - 15, 8) before the
    // category (361 + DS:8C48) at x 160 and the sort order (368 + DS:8C4A) at x 266
    // (German offsets 0x84 / 0x6A, seg1:766F / 77C4).
    const SDL_Rect b = box();
    auto label = [&](int id) {
        const auto it = texts_.find(id);
        return it == texts_.end() ? std::string() : it->second;
    };
    auto arrow = [&](int x) {
        if (SDL_Texture* t = sprite_("PFEIL1")) {
            int w = 0, h = 0;
            SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
            const SDL_Rect dst{b.x + x - 15, b.y + 8, w, h};
            SDL_RenderCopy(renderer_, t, nullptr, &dst);
        }
    };
    // seg1:7606 ff.: the item with the focus (DS:3FB6) white, the others 0x45; the
    // arrows' pfeil1 / pfeil2 choice by the focus (seg1:752A) is not traced in full.
    auto style = [&](int item) {
        return TextStyle{20, false, false, false, false, 0, item == focus_ ? kWhite : kBrown};
    };
    text_.draw(renderer_, label(375), ScreenRect{b.x + 28, b.y + 10, 120, kRow}, style(0));
    arrow(160);
    text_.draw(renderer_, label(361 + category_), ScreenRect{b.x + 160, b.y + 10, 100, kRow}, style(1));
    arrow(266);
    text_.draw(renderer_, label(368 + sort_), ScreenRect{b.x + 266, b.y + 10, 280, kRow}, style(2));
}

} // namespace dtvr
