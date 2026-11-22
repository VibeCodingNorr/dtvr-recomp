// SPDX-License-Identifier: GPL-3.0-only
#include "game.hpp"

#include <cstdio>
#include <stdexcept>

namespace dtvr {
namespace {

std::string money(double v) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%+.0f", v);
    return buffer;
}

} // namespace

Game::Game(const Board& board, const std::vector<Town>& towns, int humans, int rounds, uint16_t seed)
    : board_(board), towns_(towns), rng_(seed) {
    if (humans < 1 || humans > kPlayerSlots) throw std::invalid_argument("1 bis 8 Spieler");
    players_.resize(kPlayerSlots);
    money_.assign(kPlayerSlots, 0.0);
    origin_ = startField(board_, towns_);
    if (origin_ < 0) throw std::runtime_error("Startstadt LONDON nicht gefunden");
    for (int i = 0; i < humans; ++i) {
        players_[size_t(i)].status = 1;
        players_[size_t(i)].field = origin_;
    }
    remaining_ = rounds;
    current_ = firstActivePlayer(players_);
    // New game: route mask 0x4000 (seg4:0E0B); first destination drawn from the start.
    destination_ = chooseDestination(board_, players_, origin_, origin_, 0x4000, rng_);
    messages_.push_back("Start in " + townName(origin_) + ", Ziel " + townName(destination_));
}

std::string Game::townName(int field) const {
    if (field < 0 || size_t(field) >= board_.fields.size()) return "?";
    const Field& f = board_.fields[size_t(field)];
    for (const Town& t : towns_)
        if (t.x == f.x && t.y == f.y) return oemToUtf8(t.name);
    return "Feld " + std::to_string(field);
}

bool Game::roll() {
    messages_.clear();
    if (phase_ != Phase::Roll) return false;
    int sum = 0;
    for (int v : spinReels(rng_, 1, 5)) sum += v;
    counter_ = MoveCounter(sum);
    phase_ = Phase::Move;
    messages_.push_back("Spieler " + std::to_string(current_ + 1) + " wuerfelt " + std::to_string(sum));
    return true;
}

bool Game::step(Direction d) {
    messages_.clear();
    if (phase_ != Phase::Move || d == Direction::None) return false;
    PlayerSlot& p = players_[size_t(current_)];
    const int16_t next = board_.fields[size_t(p.field)].next[size_t(d) - 1];
    if (next == Field::kNone) return false;
    if (!counter_.step(d, true)) return false;
    p.field = next;
    if (counter_.taken() == counter_.allowance()) arrive();
    return true;
}

void Game::arrive() {
    // seg2:0D38 / dispatch seg2:160C.
    phase_ = Phase::Arrived;
    const int field = players_[size_t(current_)].field;
    const Field& f = board_.fields[size_t(field)];
    const std::string who = "Spieler " + std::to_string(current_ + 1);
    switch (f.kind) {
    case 1:
    case 2: {
        const auto e = moneyField(f.kind, calendar_.month, calendar_.inflation, rng_);
        money_[size_t(current_)] += e.amount;
        messages_.push_back(who + (f.kind == 1 ? " erhaelt " : " bezahlt ") + money(e.amount)
                            + " (Text " + std::to_string(e.textId) + ")");
        break;
    }
    case 3: messages_.push_back(who + ": Ereigniskarte (noch nicht nachgebaut)"); break;
    case 5: messages_.push_back(who + ": Kartenfeld (noch nicht nachgebaut)"); break;
    case 4:
    case 6:
        if (field == destination_) {
            const auto r = reachDestination(remaining_, calendar_.inflation);
            money_[size_t(current_)] += r.reward;
            messages_.push_back(who + " erreicht das Ziel " + townName(field) + ": " + money(r.reward));
            if (r.gameOver) {
                phase_ = Phase::GameOver;
                messages_.push_back("Spielende.");
                return;
            }
            remaining_ = r.remaining;
            origin_ = field;
            destination_ = chooseDestination(board_, players_, origin_, destination_, 0x4000, rng_);
            messages_.push_back(std::string(r.lastStageNext ? "Letzte Etappe" : "Neue Etappe")
                                + ", Ziel " + townName(destination_));
        } else {
            messages_.push_back(who + " in " + townName(field) + " (Stadtdialog noch nicht nachgebaut)");
        }
        break;
    default: break;
    }
}

bool Game::endTurn() {
    messages_.clear();
    if (phase_ != Phase::Arrived) return false;
    const auto change = nextPlayer(players_, current_, calendar_, rng_);
    for (int p : change.satOut) messages_.push_back("Spieler " + std::to_string(p + 1) + " setzt aus");
    if (change.yearsEnded) messages_.push_back("Neues Jahr, Inflation " + std::to_string(calendar_.inflation));
    if (change.player < 0) { phase_ = Phase::GameOver; return true; }
    current_ = change.player;
    phase_ = Phase::Roll;
    messages_.push_back("Spieler " + std::to_string(current_ + 1) + " ist dran (Monat "
                        + std::to_string(calendar_.month % 12 + 1) + ")");
    return true;
}

} // namespace dtvr
