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
    if (randomEvents_) applyEvent(chooseEvent(active, events_.turns, calendar_.month, rng_)); // seg8:180B
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
    case EventKind::Rain: placeObstacle(kRain, 3); return;     // seg8:42B0
    case EventKind::Snow: placeObstacle(kSnow, 3); return;     // seg8:535E
    case EventKind::Rockfall: {                                // seg8:5A76
        const Cell c = nextRockfallCell(places_);
        const LinkRef link = linkAtCell(board_, c.x, c.y);
        const int n = activePlayerCount(players_);
        obstacles_.add({kRockfall, 4 * n, c.x, c.y, link.field, link.dir});
        rng_.random(2);
        messages_.push_back("Ereignis Steinschlag bei (" + std::to_string(c.x) + "," + std::to_string(c.y) + ")");
        return;
    }
    case EventKind::Storm:
    case EventKind::Drought:
    case EventKind::Flood: {
        std::vector<uint8_t> owned(towns_.size());
        for (size_t t = 0; t < towns_.size(); ++t) owned[t] = economy_.townHasOwner(int(t));
        const int category = kind == EventKind::Storm ? 4 : kind == EventKind::Drought ? 3 : 1;
        const int town = pickDamageTown(towns_, owned, category, rng_);  // seg8:6C77
        rng_.random(2);
        if (town >= 0) damageTown(town, 37); // Versicherung
        return;
    }
    case EventKind::Nessie: {
        const int town = townAtCell(towns_, nextNessiePlace(places_));
        rng_.random(2);
        if (town >= 0) damageTown(town, 39); // only the Nessie card protects
        return;
    }
    case EventKind::Earthquake: {
        const int town = townAtCell(towns_, nextQuakePlace(places_));
        rng_.random(2);
        if (town >= 0) damageTown(town, 37);
        return;
    }
    case EventKind::Volcano: {
        const int town = townAtCell(towns_, nextVolcanoPlace(places_));
        rng_.random(2);
        rng_.random(2);
        if (town >= 0) damageTown(town, 37);
        return;
    }
    default:
        messages_.push_back("Ereignis " + eventName(kind) + " (Wirkung noch nicht nachgebaut)");
        return;
    }
}

void Game::placeObstacle(uint16_t type, int durationFactor) {
    for (int attempt = 0; attempt < 10; ++attempt) { // seg8:4460: up to ten tries
        const ObstacleSpot spot = pickObstacleSpot(board_, players_, current_, destination_, routeMask(), rng_,
                                                   map_, props_);
        if (spot.field < 0) continue;
        rng_.random(2);
        obstacles_.add({type, durationFactor * activePlayerCount(players_), spot.x, spot.y, spot.field, spot.dir});
        messages_.push_back("Ereignis " + std::string(type == kRain ? "Regen" : "Schnee") + ": Strasse bei ("
                            + std::to_string(spot.x) + "," + std::to_string(spot.y) + ") gesperrt");
        return;
    }
    messages_.push_back("Ereignis ohne passende Strasse");
}

int Game::findCard(int player, int type) const {
    for (int s = 0; s < 8; ++s)
        if (deck_.typeOfSlot(hands_[size_t(player)][size_t(s)]) == type) return s;
    return -1;
}

void Game::damageTown(int town, int protectionCard) {
    std::array<bool, 8> insured{};
    for (int p = 0; p < kPlayerSlots; ++p) insured[size_t(p)] = findCard(p, protectionCard) >= 0;
    const auto used = economy_.damageTown(town, insured);
    for (int p = 0; p < kPlayerSlots; ++p)
        if (used[size_t(p)]) hands_[size_t(p)][size_t(findCard(p, protectionCard))] = Deck::kTaken; // seg2:3C62
    messages_.push_back("Schaden in " + oemToUtf8(towns_[size_t(town)].name)
                        + ": unversicherte Grundstuecke gehen verloren");
}

} // namespace dtvr
