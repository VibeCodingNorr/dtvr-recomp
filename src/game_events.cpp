// SPDX-License-Identifier: GPL-3.0-only
// Event trigger and event effects in the game (round 11, docs/formats.md).
#include "game.hpp"

#include <cstdio>

namespace dtvr {

void Game::eventTick() {
    // seg8:1624, called inside nextPlayer after the month bookkeeping.
    ++events_.turns;                                   // DS:A0FE
    if (events_.hold > 0) --events_.hold;              // DS:A111
    const int season = seasonForMonth(calendar_.month);
    if (season != loadedSeason_) {                     // seg5:186A reloads tiles: no event
        loadedSeason_ = season;
        messages_.push_back("Neue Jahreszeit (map/" + std::to_string(season) + ")");
        return;
    }
    if (events_.countdown > 0) { --events_.countdown; return; } // DS:A108
    const int active = activePlayerCount(players_);
    events_.countdown = nextEventCountdown(active, calendar_.year, rng_); // seg8:1B91
    if (!eastOpen_) {
        double owned = 0;
        for (int p = 0; p < kPlayerSlots; ++p) owned += economy_.money(p).propertyValue; // all slots
        if (eastShouldOpen(westValue_, owned, active, calendar_.year)) {
            applyEvent(EventKind::OpenEast);
            return;
        }
    }
    applyEvent(chooseEvent(active, events_.turns, calendar_.month, rng_)); // seg8:180B
}

void Game::applyEvent(EventKind kind) {
    switch (kind) {
    case EventKind::OpenEast: // seg8:69F0: barriers replaced, DS:A10F = 0
        eastOpen_ = true;
        messages_.push_back("Ereignis: Osteuropa oeffnet seine Grenzen!");
        return;
    case EventKind::Robbers: { // seg8:1E70
        std::vector<double> cash(kPlayerSlots);
        std::vector<bool> sea(kPlayerSlots, false);
        for (int p = 0; p < kPlayerSlots; ++p) {
            cash[size_t(p)] = economy_.money(p).cash;
            const int field = players_[size_t(p)].field;
            if (map_ && props_ && field >= 0) {
                const Field& f = board_.fields[size_t(field)];
                sea[size_t(p)] = props_->cell(*map_, f.x + 2, f.y) & TileProperties::kSeaRoute;
            }
        }
        const int victim = robberyVictim(players_, board_, cash, calendar_.inflation, sea);
        if (victim < 0) { messages_.push_back("Ereignis Raeuber: niemand ist reich genug"); return; }
        const double loss = economy_.money(victim).cash * 0.25; // DS:8766
        economy_.addMoney(victim, -loss);
        rng_.random(2); // text 5021 + random(2)
        char text[96];
        std::snprintf(text, sizeof text, "Ereignis Raeuber: Spieler %d verliert %.0f", victim + 1, loss);
        messages_.push_back(text);
        return;
    }
    default:
        messages_.push_back("Ereignis " + eventName(kind) + " (Wirkung noch nicht nachgebaut)");
        return;
    }
}

} // namespace dtvr
