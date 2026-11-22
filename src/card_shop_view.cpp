// SPDX-License-Identifier: GPL-3.0-only
#include "card_shop_view.hpp"

#include "cards.hpp"
#include "panel.hpp"
#include "scene.hpp"

#include <cstdio>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 0x22E, kHeight = 0x1CE, kRow = 0x2A; // CreateWindow seg3:456B
constexpr int kGridX = 0xF, kGridY = 0x33, kStepX = 0x3C, kStepY = 0x4A, kColumns = 9; // seg3:4E62 / 4FA6
constexpr SDL_Color kBrown{67, 27, 0, 255}, kGray{123, 95, 27, 255}, kWhite{255, 255, 255, 255},
    kYellow{255, 247, 0, 255};

} // namespace

CardShopView::CardShopView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout,
                           const std::map<int, std::string>& texts, TextRenderer& text)
    : renderer_(renderer), root_(root), layout_(layout), texts_(texts), text_(text) {}

std::string CardShopView::text(int id) const {
    const auto it = texts_.find(id);
    return it == texts_.end() ? std::string() : it->second;
}

void CardShopView::open(const Game& game) {
    if (frames_.empty()) { // %s\%c\bcards.sbs (DS:3261) and the strips of ls.sbs
        Surface palette = loadBmp(findCaseInsensitive(root_, fs::path("butt") / "ziel.bmp"));
        const SDL_Palette* pal = palette && palette->format->palette ? palette->format->palette : nullptr;
        loadSbsTextures(renderer_, findCaseInsensitive(root_, fs::path("butt") / "d" / "bcards.sbs"), pal, frames_, 0xFF);
        loadSbsTextures(renderer_, findCaseInsensitive(root_, fs::path("sbs") / "ls.sbs"), pal, frames_);
    }
    (void)game;
    mode_ = 0;
    choice_ = 0;
    selected_ = -1;
    open_ = true;
}

SDL_Rect CardShopView::box() const {
    return SDL_Rect{(layout_.width - kWidth) / 2, (layout_.height - kHeight) / 2, kWidth, kHeight};
}

std::vector<CardShopView::Item> CardShopView::items(const Game& game) const {
    std::vector<Item> list;
    // seg3:5813(choice): the header previews its choice (seg3:4A79 / 4AEF): Kaufen
    // the offers, Verkaufen the hand, Beenden nothing (seg3:5997).
    if (mode_ == 0 && choice_ == 2) return list;
    if (mode_ == 2 || (mode_ == 0 && choice_ == 1)) { // seg3:5813(1): the own hand slots
        const int p = game.currentPlayer();
        for (int s = 0; s < 8; ++s) {
            const int type = game.deck().typeOfSlot(game.hand(p)[size_t(s)]);
            if (type >= 0) list.push_back({type, s});
        }
    } else { // seg3:5813(0): the first deck position of every card type
        for (const CardOffer& o : cardShopOffers(game.deck())) list.push_back({o.type, o.position});
    }
    return list;
}

CardShopView::Action CardShopView::enter(const Game& game) {
    Action a;
    if (mode_ == 0) { // seg3:4701
        a.sound = 2;
        if (choice_ == 0) { mode_ = 1; selected_ = 0; }
        else if (choice_ == 1) {
            if (game.handCount(game.currentPlayer()) == 0) { a.sound = -1; return a; } // DS:3FBA
            mode_ = 2;
            selected_ = 0;
        } else a.kind = Action::Kind::Close;
        return a;
    }
    const auto list = items(game);
    if (selected_ < 0 || selected_ >= int(list.size())) return a;
    a.kind = mode_ == 1 ? Action::Kind::Buy : Action::Kind::Sell; // seg3:47F1 / 4912
    a.value = mode_ == 1 ? list[size_t(selected_)].type : list[size_t(selected_)].value;
    return a;
}

void CardShopView::afterSale(const Game& game) {
    const int count = int(items(game).size());
    if (selected_ >= count) selected_ = count - 1; // seg3:49EB
    if (game.handCount(game.currentPlayer()) == 0) { // seg3:4A1B: Backspace (seg3:46D6)
        mode_ = 0;
        selected_ = -1;
    }
}

CardShopView::Action CardShopView::handle(const SDL_Event& event, const Game& game) {
    Action a;
    if (!open_) return a;
    const auto list = items(game);
    const SDL_Rect b = box();
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int x = event.button.x - b.x, y = event.button.y - b.y;
        if (x < 0 || x >= b.w || y < 0 || y >= b.h) return a;
        // WM_LBUTTONDBLCLK posts Enter, even outside a card/header hit rectangle
        // (seg3:4674). Ordinary clicks only select a card (seg3:5148-5166).
        if (event.button.clicks == 2) return enter(game);
        if (mode_ == 0 && y >= 0 && y < kRow) { // seg3:5051: the header ranges post Enter
            if (x < 0x6E) choice_ = 0;
            else if (x >= 0xB4 && x < 0x140) choice_ = 1;
            else if (x >= 0x1CC && x < kWidth) choice_ = 2;
            else return a;
            return enter(game);
        }
        for (int i = 0; i < int(list.size()) && mode_ != 0; ++i) { // seg3:510C: choose a card
            const int cx = kGridX + (i % kColumns) * kStepX, cy = kGridY + (i / kColumns) * kStepY;
            if (x >= cx && x < cx + 44 && y >= cy && y < cy + 63) {
                selected_ = i;
                return a;
            }
        }
        return a;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        SDL_Event esc{};
        esc.type = SDL_KEYDOWN;
        esc.key.keysym.sym = SDLK_ESCAPE;
        return handle(esc, game);
    }
    if (event.type != SDL_KEYDOWN) return a;
    switch (event.key.keysym.sym) {
    case SDLK_ESCAPE: case SDLK_BACKSPACE: // seg3:46A2
        if (mode_ == 0) { a.kind = Action::Kind::Close; a.sound = 2; }
        else { mode_ = 0; selected_ = -1; }
        return a;
    case SDLK_RETURN: case SDLK_KP_ENTER: return enter(game);
    case SDLK_LEFT: // seg3:4A41 / 4A96
        if (mode_ == 0) { if (choice_ > 0) --choice_; } // seg3:4A4A: no wrap from Kaufen
        else if (selected_ > 0) --selected_;
        return a;
    case SDLK_RIGHT:
        if (mode_ == 0) choice_ = choice_ == 2 ? 0 : choice_ + 1;
        else if (selected_ + 1 < int(list.size())) ++selected_;
        return a;
    case SDLK_UP: // seg3:4B42: move up one complete grid row
        if (mode_ != 0 && selected_ >= kColumns) selected_ -= kColumns;
        return a;
    case SDLK_DOWN: // seg3:4B79: move down only if that column has a card
        if (mode_ != 0 && selected_ >= 0 && selected_ + kColumns < int(list.size())) selected_ += kColumns;
        return a;
    default: return a;
    }
}

void CardShopView::draw(const Game& game) {
    if (!open_) return;
    const SDL_Rect b = box();
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, &b);
    auto strip = [&](const char* name, int y) { drawSceneFrame(renderer_, frames_, name, b.x, b.y + y); };
    // Header seg3:5234: KO_ZI while choosing, else KO_Z; texts in Arial 20 weight 400.
    strip(mode_ == 0 ? "KO_ZI" : "KO_Z", 0);
    const bool handEmpty = game.handCount(game.currentPlayer()) == 0;
    auto colour = [&](int item) {
        if (item == 1 && handEmpty) return kGray;               // DS:3FBA
        if (mode_ == 0) return item == choice_ ? kWhite : kBrown;
        return item + 1 == mode_ ? kYellow : kBrown;            // active tab: 0x2C
    };
    TextStyle style{20, false, false, false, false, 0, colour(0)};
    text_.draw(renderer_, text(321), ScreenRect{b.x + 18, b.y + 10, 200, kRow}, style);
    style.color = colour(1);
    style.center = true;
    text_.draw(renderer_, text(322), ScreenRect{b.x, b.y + 10, 0x229, kRow}, style);
    style.center = false;
    style.right = true;
    style.color = colour(2);
    text_.draw(renderer_, text(371), ScreenRect{b.x, b.y + 10, 0x229, kRow}, style);
    // Rows seg3:4D7A: nine strips K1 (choosing) or K2 from y 42.
    for (int i = 0; i < 9; ++i) strip(mode_ == 0 ? "K1" : "K2", (i + 1) * kRow);
    const auto list = items(game);
    for (int i = 0; i < int(list.size()); ++i) { // seg3:4E71: 1card_<type + 1>
        const int x = b.x + kGridX + (i % kColumns) * kStepX, y = b.y + kGridY + (i / kColumns) * kStepY;
        char name[24];
        std::snprintf(name, sizeof name, "1card_%02d", list[size_t(i)].type + 1);
        drawSceneFrame(renderer_, frames_, name, x, y);
        if (i == selected_ && mode_ != 0) { // red PS_INSIDEFRAME pen, width 2
            SDL_SetRenderDrawColor(renderer_, 255, 0, 0, 255);
            for (int w = 0; w < 2; ++w) {
                const SDL_Rect r{x + w, y + w, 44 - 2 * w, 63 - 2 * w};
                SDL_RenderDrawRect(renderer_, &r);
            }
        }
    }
    // Info line seg3:553C at y 420: name (12000 + type), "   ", price; cash on the right.
    strip("KO_Z", 10 * kRow);
    if (mode_ != 0 && selected_ >= 0 && selected_ < int(list.size())) {
        const int type = list[size_t(selected_)].type;
        const double value = cardTypes()[size_t(type)].value * (mode_ == 2 ? 0.5 : 1.0); // DS:8646
        TextStyle info{20, false, false, false, false, 0, kBrown};
        text_.draw(renderer_, text(12000 + type) + "   " + formatMoney(value),
                   ScreenRect{b.x + 10, b.y + 0x1AC, 0x224 - 10, kRow}, info);
        info.right = true;
        text_.draw(renderer_, formatMoney(game.cash(game.currentPlayer())), ScreenRect{b.x, b.y + 0x1AC, 0x224, kRow}, info);
    }
}

} // namespace dtvr
