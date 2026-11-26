// SPDX-License-Identifier: GPL-3.0-only
// Event trigger and event effects in the game (round 11, docs/formats.md).
#include "game.hpp"

#include <cstdio>

namespace dtvr {

void Game::eventTick(int player) {
    // seg8:1624, called inside nextPlayer after the month bookkeeping.
    eventPlayer_ = player;
    ++events_.turns;                                   // DS:A0FE
    if (events_.hold > 0) --events_.hold;              // DS:A111
    // seg5:186A(DS:A100, 0): with Colorcycle off the directory is 1 (seg5:1929-1953).
    const int season = tileDirectory();
    if (season != loadedSeason_) {                     // seg5:186A reloads tiles: no event
        loadedSeason_ = season;
        ++seasonChanges_; // the window's blind wipe (seg8:1667-16A4)
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

int Game::tileDirectory() const { return colorcycle_ ? seasonForMonth(calendar_.month) : 1; }

void Game::setColorcycle(bool on) {
    colorcycle_ = on;
    loadedSeason_ = tileDirectory(); // seg5:186A(DS:A100, 0) at OPTION's close, result unused
}

void Game::applyEvent(EventKind kind) {
    switch (kind) {
    case EventKind::OpenEast: // seg8:69F0: barriers replaced, DS:A10F = 0
        eastOpen_ = true;
        westValue_ = westPropertyValue(board_, towns_, 0); // seg8:6C4C: DS:A113 = seg1:A15B(), mask 0
        notice(5001 + rng_.random(2)); // seg8:6C13 (round 25: this draw was missing before)
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
        const Field& at = board_.fields[size_t(players_[size_t(victim)].field)]; // seg8:2238 -> seg2:3216
        eventNotice(5021 + rng_.random(2), {moneyArg(loss)}, at.x, at.y);
        char text[96];
        std::snprintf(text, sizeof text, "%.0f", loss);
        messages_.push_back("Ereignis Raeuber: " + playerName(victim) + " verliert " + text);
        return;
    }
    case EventKind::Professor:
    case EventKind::FootballClub: // seg8:6FBF: offered to the player whose turn starts
        if (economy_.money(eventPlayer_).cash > 10000.0) {         // DS:8776
            offer_ = kind;
            // The offer window draws its text on creation (seg8:7551): 5321 / 5401 + random(2).
            notice((kind == EventKind::Professor ? 5321 : 5401) + rng_.random(2), {playerArg(eventPlayer_)});
            messages_.push_back("Ereignis " + eventName(kind) + ": " + playerName(eventPlayer_)
                                + " kann investieren (I)");
        } else {
            messages_.push_back("Ereignis " + eventName(kind) + ": " + playerName(eventPlayer_)
                                + " hat nicht genug Geld");
        }
        return;
    case EventKind::Rain: placeObstacle(kRain, 3); return;     // seg8:42B0
    case EventKind::Snow: placeObstacle(kSnow, 3); return;     // seg8:535E
    case EventKind::Rockfall: {                                // seg8:5A76
        const Cell c = nextRockfallCell(places_);
        const LinkRef link = linkAtCell(board_, c.x, c.y);
        const int n = activePlayerCount(players_);
        obstacles_.add({kRockfall, 4 * n, c.x, c.y, link.field, link.dir});
        eventNotice(5081 + rng_.random(2), {}, c.x, c.y); // seg8:5DA1
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
        if (town < 0) return; // seg8:1C60 / 2553 / 4B3C: no town, no animation, no text
        const int corner = kind == EventKind::Flood ? rng_.random(3) : 0; // seg8:4BF5 -> seg8:517B: scene start
        // seg8:1CED / 25A4 / 4BC3: the town's cell.
        const int tx = town >= 0 ? towns_[size_t(town)].x : -1, ty = town >= 0 ? towns_[size_t(town)].y : -1;
        eventNotice((kind == EventKind::Storm ? 5041 : kind == EventKind::Drought ? 5141 : 5241) + rng_.random(2),
                    {townArg(town)}, tx, ty, corner);
        if (town >= 0) damageTown(town, 37); // Versicherung
        return;
    }
    case EventKind::Nessie: {
        const Cell place = nextNessiePlace(places_);
        const int town = townAtCell(towns_, place);
        eventNotice(5101 + rng_.random(2), {townArg(town)}, place.x, place.y, places_.nessie); // seg8:3259, DS:680C
        if (town >= 0) damageTown(town, 39); // only the Nessie card protects
        return;
    }
    case EventKind::Earthquake: {
        const Cell place = nextQuakePlace(places_);
        const int town = townAtCell(towns_, place);
        eventNotice(5201 + rng_.random(2), {townArg(town)}, place.x, place.y); // seg8:4023
        if (town >= 0) damageTown(town, 37);
        return;
    }
    case EventKind::Volcano: {
        const Cell place = nextVolcanoPlace(places_);
        const int town = townAtCell(towns_, place);
        eventNotice((places_.volcano == 0 ? 5281 : 5301) + rng_.random(2), {}, place.x, place.y); // DS:69F0, seg8:3AA0
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
        const ObstacleSpot spot = pickObstacleSpot(board_, players_, eventPlayer_, destination_, routeMask(), rng_,
                                                   map_, props_);
        if (spot.field < 0) continue;
        const int corner = rng_.random(3); // seg8:451B / 562C -> seg8:517B: corner the cloud starts from
        eventNotice((type == kRain ? 5121 : 5481) + rng_.random(2), {}, spot.x, spot.y, corner); // seg8:44E9 / 55FA
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
        if (used[size_t(p)]) discard(p, findCard(p, protectionCard)); // seg8:4F6C -> seg2:3C62
    messages_.push_back("Schaden in " + oemToUtf8(towns_[size_t(town)].name)
                        + ": unversicherte Grundstuecke gehen verloren");
}

} // namespace dtvr
