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

void EstatesView::open(int town) {
    town_ = town;
    tab_ = EstatesTab::None;
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

EstatesView::Action EstatesView::handle(const SDL_Event& event, const std::vector<EstateRow>& rows) {
    Action action;
    if (!isOpen()) return action;
    const int count = int(rows.size());
    auto rowAction = [&](int row) {
        selected_ = row;
        if (tab_ != EstatesTab::None && !rows[size_t(row)].disabled) {
            action.kind = Action::Kind::Row;
            action.entry = rows[size_t(row)].entry;
        }
    };
    if (event.type == SDL_MOUSEMOTION) {
        const int row = rowAt(event.motion.x, event.motion.y, count);
        if (row >= 0) selected_ = row;
    } else if (event.type == SDL_MOUSEWHEEL) {
        scroll_ = std::max(0, std::min(std::max(0, count - kVisibleRows), scroll_ - event.wheel.y));
    } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        action.kind = Action::Kind::Close;
    } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const SDL_Rect b = box();
        const int x = event.button.x - b.x, y = event.button.y - b.y;
        if (x >= 0 && x < b.w && y >= 0 && y < kRow) { // header tabs
            if (x >= kTabX[2]) action.kind = Action::Kind::Cards;
            else { // seg1:543E / 5475: the tab selects the first visible row
                tab_ = x >= kTabX[1] ? EstatesTab::Sell : EstatesTab::Buy;
                selected_ = count > 0 ? std::min(scroll_, count - 1) : -1;
                action.kind = Action::Kind::Tab;
            }
        } else if (const int row = rowAt(event.button.x, event.button.y, count); row >= 0) {
            rowAction(row);
        }
    } else if (event.type == SDL_KEYDOWN && count > 0) {
        switch (event.key.keysym.sym) {
        case SDLK_DOWN: selected_ = selected_ < 0 ? 0 : std::min(count - 1, selected_ + 1); keepVisible(count); break;
        case SDLK_UP: selected_ = selected_ <= 0 ? 0 : selected_ - 1; keepVisible(count); break;
        case SDLK_RETURN: case SDLK_KP_ENTER: if (selected_ >= 0 && selected_ < count) rowAction(selected_); break;
        case SDLK_ESCAPE: action.kind = Action::Kind::Close; break;
        default: break;
        }
    } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        action.kind = Action::Kind::Close;
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
    for (int i = 0; i < 3; ++i) {
        const auto it = texts_.find(321 + i); // LoadString 0x141-0x143: Kaufen, Verkaufen, Karte
        const std::string label = it == texts_.end() ? std::string() : it->second;
        const bool active = (i == 0 && tab_ == EstatesTab::Buy) || (i == 1 && tab_ == EstatesTab::Sell);
        text_.draw(renderer_, label, ScreenRect{b.x + kTabX[i], b.y + 10, 140, kRow},
                   TextStyle{20, false, false, false, false, 0, active ? kYellow : kBrown});
    }
    // Rows seg1:65A7 / 6163: strip GR_Z (selected GR_ZI), owner icon at x 0, texts in
    // Arial 18 weight 400 from x 0x54 + 10, y + 10.
    for (int k = 0; k < kVisibleRows && scroll_ + k < int(rows.size()); ++k) {
        const int index = scroll_ + k;
        const EstateRow& row = rows[size_t(index)];
        const int y = (k + 1) * kRow;
        const bool selected = index == selected_;
        blit(selected ? "GR_ZI" : "GR_Z", 0, y);
        std::string icon = "0N";
        if (row.owner >= 0 && row.owner < 8)
            icon = std::string(1, ownerLetters[size_t(row.owner)]) + (row.wholeTown ? "_M" : "_N");
        blit(icon, 0, y);
        const SDL_Color color = row.disabled ? kGray : selected ? kWhite : kBrown;
        TextStyle left{18, false, false, false, false, 0, color};
        TextStyle right = left;
        right.right = true;
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
}

} // namespace dtvr
