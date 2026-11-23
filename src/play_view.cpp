// SPDX-License-Identifier: GPL-3.0-only
#include "play_view.hpp"

#include "save.hpp"

#include <algorithm>

namespace dtvr {

PlayView::PlayView(const Board& board, const std::vector<Town>& towns, std::unique_ptr<WorldMap> map,
                   std::unique_ptr<TileProperties> props, const std::array<uint8_t, kPlayerSlots>& slots,
                   int rounds, uint16_t seed)
    : board_(board), towns_(towns), map_(std::move(map)), props_(std::move(props)),
      game_(std::make_unique<Game>(board, towns, slots, rounds, seed, 1, map_.get(), props_.get())), rounds_(rounds) {}

bool PlayView::newGame(const std::array<uint8_t, kPlayerSlots>& slots,
                       const std::array<std::string, kPlayerSlots>& names, uint16_t seed, std::string& error,
                       int rounds) {
    if (rounds > 0) rounds_ = rounds;
    Game fresh(board_, towns_, slots, rounds_, seed, 1, map_.get(), props_.get());
    fresh.setPlayerNames(names);
    fresh.applyNameCodes(); // seg4:0E71 -> seg2:6332, after the deal
    if (!SaveCodec::decode(*game_, SaveCodec::encode(fresh), error)) return false;
    game_->setPlayerNames(fresh.playerNames()); // with the renamed code of seg2:640F
    game_->takeNotices(); // nothing of the old game is announced any more
    game_->adoptNotices(fresh);
    cardTarget_ = pendingNails_ = -1;
    offerCursor_ = 0;
    nextTick_ = 0;
    return true;
}

std::string PlayView::title() const {
    static const char* phases[] = {"wuerfeln (W)", "fahren (Pfeile)", "Zug beenden (Enter)", "Spielende",
                                   "Risiko (Leertaste druecken, Ruecktaste aufhoeren)"};
    const Game& g = *game_;
    const int p = g.currentPlayer();
    std::string s = "DTVR Recomp | " + g.playerName(p) + " | Monat "
        + std::to_string(g.calendar().month % 12 + 1) + " | Schritte " + std::to_string(g.stepsTaken()) + "/"
        + std::to_string(g.allowance()) + " | Ziel " + g.townName(g.destination()) + " | Etappen "
        + std::to_string(g.remainingStages()) + " | Geld " + std::to_string(int64_t(g.cash(p)));
    if (g.eastOpen()) s += " | Osten offen";
    if (g.drago().holder >= 0) s += " | Dr. Drago bei " + std::to_string(g.drago().holder + 1);
    if (cardTarget_ >= 0) s += " | Ziel-Spieler " + std::to_string(cardTarget_ + 1);
    if (g.investmentOffer() != EventKind::None) s += " | " + eventName(g.investmentOffer()) + ": I / N";
    if (pendingNails_ >= 0) s += " | Naegel: Richtung waehlen";
    return s + " | " + phases[int(g.phase())];
}

void PlayView::printLog(std::ostream& out) const {
    const Game& g = *game_;
    for (const auto& m : g.messages()) out << m << "\n";
    const int p = g.currentPlayer();
    const int town = g.townAt(g.playerField(p));
    if (g.phase() == Game::Phase::Arrived && town >= 0) {
        const int pm = g.propertyMode();
        const BuyMode mode = pm >= 3 && pm <= 9 ? BuyMode(pm) : BuyMode::Normal;
        if (pm != 0xFF) out << "Kaufmodus: " << cardTypes()[size_t(pm - 1)].name << "\n";
        for (int e = 0; e < 10; ++e) { // town dialog subset: keys 1-9, 0 buy, with Shift sell
            if (!g.economy().entryExists(town, e)) continue;
            const auto owner = g.economy().town(town).entries[size_t(e)].owner;
            out << "  [" << (e + 1) % 10 << "] " << oemToUtf8(towns_[size_t(town)].entries[size_t(e)].name) << " "
                << int64_t(g.economy().price(town, e, mode)) << " "
                << (owner == kNoOwner ? std::string("frei") : "Spieler " + std::to_string(owner + 1)) << "\n";
        }
    }
    if (g.atCardField()) { // card shop: PgUp / PgDn choose, B buys, Shift+1-8 sells
        const auto offers = g.cardOffers();
        out << "Kartenfeld (Bild auf/ab waehlen, B kaufen, Shift+1-8 verkaufen):\n";
        for (size_t i = 0; i < offers.size(); ++i)
            out << (int(i) == offerCursor_ ? " >" : "  ") << cardTypes()[size_t(offers[i].type)].name << " "
                << int64_t(cardBuyPrice(offers[i].type)) << (i % 4 == 3 ? "\n" : "");
        out << "\n";
    }
    if (g.phase() == Game::Phase::Roll || (g.phase() == Game::Phase::Arrived && (town >= 0 || g.atCardField()))) {
        out << "Hand " << g.playerName(p) << (g.phase() == Game::Phase::Arrived ? " (Strg+Ziffer):" : ":");
        for (int i = 0; i < 8; ++i)
            if (g.hand(p)[size_t(i)] != Deck::kTaken) out << " [" << i + 1 << "] " << g.cardName(g.hand(p)[size_t(i)]);
        out << "\n";
    }
}

bool PlayView::handleKey(SDL_Keycode key, Uint16 mod) {
    Game& g = *game_;
    auto direction = [&](Direction d) {
        if (pendingNails_ >= 0) { g.playCard(pendingNails_, int(d)); pendingNails_ = -1; }
        else g.step(d);
        return true;
    };
    switch (key) {
    case SDLK_SPACE: g.risikoPress(); return true;
    case SDLK_BACKSPACE: g.risikoStop(); return true;
    case SDLK_w: g.roll(); return true;
    case SDLK_RETURN: g.endTurn(); return true;
    case SDLK_i: g.invest(std::max(1000, int(g.cash(g.currentPlayer()) / 2000) * 1000)); return true;
    case SDLK_n: g.declineInvestment(); return true;
    case SDLK_PAGEUP: case SDLK_PAGEDOWN: {
        const int n = int(g.cardOffers().size());
        if (n > 0) offerCursor_ = (offerCursor_ + (key == SDLK_PAGEUP ? n - 1 : 1)) % n;
        return true;
    }
    case SDLK_b: {
        const auto offers = g.cardOffers();
        if (offerCursor_ < int(offers.size())) g.buyCard(offers[size_t(offerCursor_)].type);
        return true;
    }
    case SDLK_t:
        for (int i = 1; i <= kPlayerSlots; ++i) { // next active player other than the current one
            const int candidate = ((cardTarget_ < 0 ? g.currentPlayer() : cardTarget_) + i) % kPlayerSlots;
            if (g.players()[size_t(candidate)].status && candidate != g.currentPlayer()) { cardTarget_ = candidate; break; }
        }
        return true;
    case SDLK_UP: return direction(Direction::Up);
    case SDLK_DOWN: return direction(Direction::Down);
    case SDLK_RIGHT: return direction(Direction::Right);
    case SDLK_LEFT: return direction(Direction::Left);
    default: break;
    }
    if ((key >= SDLK_1 && key <= SDLK_9) || key == SDLK_0) {
        const int digit = key == SDLK_0 ? 9 : int(key - SDLK_1);
        if (g.yellowCardPending()) {
            if (digit < 8) g.discardForCard(digit);       // full hand on a yellow field
        } else if (g.atCardField()) {
            if ((mod & KMOD_SHIFT) && digit < 8) g.sellCard(digit);
        } else if (g.phase() == Game::Phase::Arrived) {
            if (mod & KMOD_CTRL) { if (digit < 8) g.playCard(digit, cardTarget_); }
            else (mod & KMOD_SHIFT) ? g.sellProperty(digit) : g.buyProperty(digit);
        } else if (digit < 8) {
            const CardKind kind = cardKind(g.deck().typeOfSlot(g.hand(g.currentPlayer())[size_t(digit)]));
            if (kind == CardKind::Naegel) pendingNails_ = digit;          // direction follows
            else if (kind == CardKind::Flugzeug) g.playCard(digit, g.townAt(g.destination())); // viewer: fly to the goal
            else if (kind == CardKind::Tschuess) // without T: the next other player
                g.playCard(digit, cardTarget_ >= 0 ? cardTarget_ : (g.currentPlayer() + 1) % activePlayerCount(g.players()));
            else g.playCard(digit, cardTarget_);
        }
        return true;
    }
    return false;
}

bool PlayView::update(Uint32 now, bool stepwise) {
    Game& g = *game_;
    // Computer slots; also a turn handed over during the move (Mitspieler, seg2:5E93).
    const bool computerDue = g.computerControls() && (g.phase() == Game::Phase::Roll || g.phase() == Game::Phase::Move
                                                      || g.phase() == Game::Phase::Arrived);
    if (stepwise && (g.computerTurnRunning() || computerDue)) return g.computerAdvance();
    if (computerDue) return g.runComputerTurn();
    if (g.phase() != Game::Phase::Risiko) { nextTick_ = 0; return false; }
    if (int(now - risikoHold_) < 0) { nextTick_ = 0; return false; }
    if (!nextTick_) nextTick_ = now + Uint32(g.risikoTickMs());
    if (now < nextTick_) return false;
    nextTick_ = now + Uint32(g.risikoTickMs());
    return g.risikoTick();
}

void PlayView::draw(SDL_Renderer* renderer, float x, float y, float zoom) const {
    const Game& g = *game_;
    const float cell = WorldMap::kTile * zoom;
    const float r = std::max(4.f, cell * 0.6f);
    auto mark = [&](int index, SDL_Color c, float size) {
        if (index < 0) return;
        const auto& f = board_.fields[size_t(index)];
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
        for (int t = 0; t < 2; ++t) {
            const SDL_FRect box{x + (f.x + 0.5f) * cell - size - t, y + (f.y + 0.5f) * cell - size - t,
                                2 * (size + t), 2 * (size + t)};
            SDL_RenderDrawRectF(renderer, &box);
        }
    };
    static const SDL_Color colours[8] = {{255, 60, 60, 255}, {60, 160, 255, 255}, {60, 220, 60, 255},
        {255, 160, 0, 255}, {200, 60, 255, 255}, {0, 230, 230, 255}, {255, 255, 255, 255}, {120, 120, 120, 255}};
    mark(g.origin(), {255, 255, 255, 255}, r * 1.6f);
    for (const auto& o : g.obstacles().slots()) { // road obstacles; investments have no cell
        if (!o.type || o.field < 0) continue;
        const SDL_FRect box{x + o.x * cell, y + o.y * cell, cell, cell};
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRectF(renderer, &box);
    }
    if (g.drago().holder >= 0) mark(g.playerField(g.drago().holder), {200, 0, 255, 255}, r * 2.4f);
    mark(g.destination(), {255, 230, 0, 255}, r * 2.0f);
    for (int p = 0; p < kPlayerSlots; ++p) {
        if (!g.players()[size_t(p)].status) continue;
        const auto& f = board_.fields[size_t(g.playerField(p))];
        const float size = (p == g.currentPlayer() ? 0.9f : 0.5f) * r;
        const SDL_FRect box{x + (f.x + 0.5f) * cell - size + p, y + (f.y + 0.5f) * cell - size + p, 2 * size, 2 * size};
        SDL_SetRenderDrawColor(renderer, colours[p].r, colours[p].g, colours[p].b, 255);
        SDL_RenderFillRectF(renderer, &box);
    }
}

} // namespace dtvr
