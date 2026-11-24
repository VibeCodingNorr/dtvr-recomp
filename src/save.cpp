// SPDX-License-Identifier: GPL-3.0-only
#include "save.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace dtvr {
namespace {

constexpr const char* kMagic = "DTVR-RECOMP-SAVE";
constexpr const char* kLegacyMagic = "DTVR-RECOMB-SAVE"; // the project's former name; read only

void fnv(uint64_t& h, uint64_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        h ^= (v >> (8 * i)) & 0xFF;
        h *= 0x100000001B3ull;
    }
}

// Doubles as hexadecimal floating point: exact round trip.
std::string hexDouble(double v) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%a", v);
    return buffer;
}

bool parseDouble(const std::string& token, double& out) {
    char* end = nullptr;
    out = std::strtod(token.c_str(), &end);
    return end && *end == '\0' && !token.empty();
}

// Reads whitespace-separated integers / doubles from one record.
class Record {
public:
    explicit Record(const std::string& rest) : in_(rest) {}
    bool integer(long long& v, long long lo, long long hi) {
        std::string t;
        if (!(in_ >> t)) return ok_ = false;
        char* end = nullptr;
        v = std::strtoll(t.c_str(), &end, 10);
        if (!end || *end || v < lo || v > hi) ok_ = false;
        return ok_;
    }
    template <typename T> bool get(T& out, long long lo, long long hi) {
        long long v = 0;
        if (!integer(v, lo, hi)) return false;
        out = T(v);
        return true;
    }
    bool real(double& v) {
        std::string t;
        if (!(in_ >> t) || !parseDouble(t, v)) ok_ = false;
        return ok_;
    }
    bool more() {
        in_ >> std::ws;
        return !in_.eof();
    }
    bool done() { return ok_ && !more(); }

private:
    std::istringstream in_;
    bool ok_ = true;
};

} // namespace

uint64_t dataFingerprint(const Board& board, const std::vector<Town>& towns) {
    uint64_t h = 0xCBF29CE484222325ull;
    fnv(h, board.fields.size(), 4);
    for (const Field& f : board.fields) {
        fnv(h, f.x, 1);
        fnv(h, f.y, 1);
        for (int16_t n : f.next) fnv(h, uint16_t(n), 2);
        fnv(h, f.kind, 2);
        fnv(h, f.flags, 2);
    }
    fnv(h, towns.size(), 4);
    for (const Town& t : towns) {
        fnv(h, t.x, 2);
        fnv(h, t.y, 2);
        for (char c : t.name) fnv(h, uint8_t(c), 1);
        for (const TownEntry& e : t.entries) {
            for (char c : e.name) fnv(h, uint8_t(c), 1);
            fnv(h, e.value, 4);
            for (uint16_t w : e.words) fnv(h, w, 2);
        }
    }
    return h;
}

std::string SaveCodec::encode(const Game& g) {
    std::ostringstream o;
    o << kMagic << ' ' << kSaveFormat << '\n';
    o << "rules " << kRulesRevision << '\n';
    char fp[32];
    std::snprintf(fp, sizeof fp, "%016llx", static_cast<unsigned long long>(dataFingerprint(g.board_, g.towns_)));
    o << "data " << fp << '\n';
    o << "rng " << g.rng_.state() << '\n';
    for (int p = 0; p < kPlayerSlots; ++p) {
        const PlayerSlot& s = g.players_[size_t(p)];
        o << "player " << p << ' ' << int(s.status) << ' ' << s.flags << ' ' << s.field << '\n';
    }
    for (size_t t = 0; t < g.economy_.townCount(); ++t) {
        const TownState& ts = g.economy_.town(int(t));
        o << "town " << t << ' ' << int(ts.wholeOwner);
        for (const PropertyState& e : ts.entries)
            o << ' ' << int(e.owner) << ' ' << hexDouble(e.burden) << ' ' << int(e.landlord) << ' ' << int(e.monthsLeft);
        o << '\n';
    }
    for (size_t p = 0; p < g.economy_.playerCount(); ++p) {
        const PlayerMoney& m = g.economy_.money(int(p));
        o << "money " << p << ' ' << hexDouble(m.cash) << ' ' << hexDouble(m.creditDebt) << ' '
          << hexDouble(m.monthlyIncome) << ' ' << hexDouble(m.propertyValue) << '\n';
    }
    for (size_t p = 0; p < g.economy_.playerCount(); ++p) { // format 6: +0x66 and +0x6A
        const PlayerMoney& m = g.economy_.money(int(p));
        size_t count = m.worth.size();
        while (count > 0 && m.worth[count - 1] == 0) --count;
        o << "worth " << p << ' ' << m.yearRent << ' ' << count;
        for (size_t i = 0; i < count; ++i) o << ' ' << m.worth[i];
        o << '\n';
    }
    o << "drago " << g.drago_.holder << ' ' << g.drago_.form << ' ' << g.drago_.countdown << '\n';
    o << "events " << g.events_.turns << ' ' << g.events_.countdown << ' ' << g.events_.hold << '\n';
    o << "places " << g.places_.nessie << ' ' << g.places_.quake << ' ' << g.places_.volcano << ' '
      << g.places_.rockfall << '\n';
    const auto& slots = g.obstacles_.slots();
    for (size_t i = 0; i < slots.size(); ++i) {
        const Obstacle& ob = slots[i];
        if (!ob.type) continue;
        o << "obstacle " << i << ' ' << ob.type << ' ' << ob.duration << ' ' << ob.x << ' ' << ob.y << ' '
          << ob.field << ' ' << ob.dir << ' ' << ob.player << ' ' << hexDouble(ob.amount) << '\n';
    }
    o << "state " << int(g.randomEvents_) << ' ' << int(g.offer_) << ' ' << g.eventPlayer_ << ' '
      << int(g.doubleNext_) << ' ' << g.propertyMode_ << ' ' << int(g.eastOpen_) << ' '
      << int(g.yellowPending_) << ' ' << g.loadedSeason_ << ' ' << hexDouble(g.westValue_) << '\n';
    o << "facing";
    for (int d : g.lastDirection_) o << ' ' << d;
    o << '\n';
    o << "deck " << g.deck_.pointer() << ' ' << g.deck_.returnPointer();
    for (uint8_t s : g.deck_.order()) o << ' ' << int(s);
    o << '\n';
    for (int p = 0; p < kPlayerSlots; ++p) {
        o << "hand " << p;
        for (uint8_t s : g.hands_[size_t(p)]) o << ' ' << int(s);
        o << '\n';
    }
    o << "calendar " << g.calendar_.month << ' ' << g.calendar_.year << ' ' << hexDouble(g.calendar_.inflation) << '\n';
    o << "counter " << g.counter_.allowance();
    for (Direction d : g.counter_.history()) o << ' ' << int(d);
    o << '\n';
    o << "risiko " << g.risiko_.level << ' ' << g.risiko_.light << ' ' << g.risiko_.ticks << ' ' << g.risiko_.steps
      << '\n';
    o << "demo " << int(g.demoEnd_) << '\n';
    o << "extensions " << int(g.computerExtraCards_) << '\n';
    o << "stages " << g.totalStages_ << '\n';
    o << "turn " << int(g.phase_) << ' ' << g.current_ << ' ' << g.origin_ << ' ' << g.destination_ << ' '
      << g.remaining_ << '\n';
    o << "end\n";
    return o.str();
}

bool SaveCodec::decode(Game& g, const std::string& text, std::string& error, Random random) {
    const long long fields = (long long)g.board_.fields.size();
    std::istringstream in(text);
    std::string line;
    int version = 0;
    auto fail = [&](const std::string& why) { error = why; return false; };
    if (!std::getline(in, line)) return fail("leere Datei");
    {
        std::istringstream head(line);
        std::string magic;
        int format = 0;
        if (!(head >> magic >> format) || (magic != kMagic && magic != kLegacyMagic)) return fail("kein DTVR-Recomp-Spielstand");
        if (format < 1 || format > kSaveFormat)
            return fail("Spielstandformat " + std::to_string(format) + " wird nicht unterstuetzt");
        version = format;
    }
    // Everything is parsed into copies first and applied only when complete.
    uint32_t rng = 0;
    std::vector<PlayerSlot> players(kPlayerSlots);
    std::vector<TownState> towns(g.economy_.townCount());
    std::vector<PlayerMoney> money(g.economy_.playerCount());
    DragoState drago;
    EventClock events;
    EventPlaces places;
    std::array<Obstacle, ObstacleTable::kSlots> obstacles{};
    int randomEvents = 1, offer = 0, eventPlayer = 0, doubleNext = 0, propertyMode = 0xFF, eastOpen = 0;
    int yellow = 0, season = 0;
    double west = 0;
    std::array<int, 8> facing{};
    int deckPointer = 0, deckReturn = 0;
    std::vector<uint8_t> order;
    std::vector<Hand> hands(kPlayerSlots, emptyHand());
    Calendar calendar;
    int allowance = 0;
    std::vector<Direction> history;
    int phase = 0, current = 0, origin = -1, destination = -1, remaining = 0;
    Game::RisikoState risiko;
    int demoEnd = 0;
    int extraCards = 0; // format 5: recomp extensions (0 in older saves)
    int totalStages = -1;
    std::map<std::string, int> seen;
    bool ended = false;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const size_t space = line.find(' ');
        const std::string key = line.substr(0, space);
        Record r(space == std::string::npos ? "" : line.substr(space + 1));
        ++seen[key];
        bool ok = true;
        if (key == "end") { ended = true; break; }
        else if (key == "rules") {
            int rules = 0;
            ok = r.get(rules, 0, 1 << 30);
            if (ok && rules > kRulesRevision) return fail("Spielstand aus neuerer Regelversion " + std::to_string(rules));
        } else if (key == "data") {
            std::string rest = line.substr(space + 1);
            char fp[32];
            std::snprintf(fp, sizeof fp, "%016llx", static_cast<unsigned long long>(dataFingerprint(g.board_, g.towns_)));
            if (rest != fp) return fail("Spielstand passt nicht zu diesen Originaldaten");
            continue;
        } else if (key == "rng") ok = r.get(rng, 0, 0xFFFFFFFFll);
        else if (key == "player") {
            int p = 0;
            ok = r.get(p, 0, kPlayerSlots - 1) && r.get(players[size_t(p)].status, 0, 2)
                 && r.get(players[size_t(p)].flags, 0, 0xFFFF) && r.get(players[size_t(p)].field, -1, fields - 1);
        } else if (key == "town") {
            long long t = 0;
            ok = r.integer(t, 0, (long long)towns.size() - 1);
            if (ok) {
                TownState& ts = towns[size_t(t)];
                ok = r.get(ts.wholeOwner, 0, 0xFF);
                for (PropertyState& e : ts.entries)
                    ok = ok && r.get(e.owner, 0, 0xFF) && r.real(e.burden) && r.get(e.landlord, 0, 0xFF)
                         && r.get(e.monthsLeft, 0, 0xFF);
            }
        } else if (key == "money") {
            long long p = 0;
            ok = r.integer(p, 0, (long long)money.size() - 1);
            if (ok) {
                PlayerMoney& m = money[size_t(p)];
                ok = r.real(m.cash) && r.real(m.creditDebt) && r.real(m.monthlyIncome) && r.real(m.propertyValue);
            }
        } else if (key == "worth") {
            long long p = 0, count = 0;
            ok = r.integer(p, 0, (long long)money.size() - 1);
            if (ok) {
                PlayerMoney& m = money[size_t(p)];
                ok = r.get(m.yearRent, INT32_MIN, INT32_MAX) && r.integer(count, 0, kWorthYears);
                for (long long i = 0; ok && i < count; ++i) ok = r.get(m.worth[size_t(i)], INT32_MIN, INT32_MAX);
            }
        } else if (key == "drago") ok = r.get(drago.holder, -1, kPlayerSlots - 1) && r.get(drago.form, 0, 2)
                                       && r.get(drago.countdown, -1000000, 1000000);
        else if (key == "events") ok = r.get(events.turns, 0, 1ll << 31) && r.get(events.countdown, -1000000, 1000000)
                                       && r.get(events.hold, -1000000, 1000000);
        else if (key == "places") ok = r.get(places.nessie, 0, 1000) && r.get(places.quake, 0, 1000)
                                       && r.get(places.volcano, 0, 1000) && r.get(places.rockfall, 0, 1000);
        else if (key == "obstacle") {
            int i = 0;
            ok = r.get(i, 0, ObstacleTable::kSlots - 1);
            if (ok) {
                Obstacle& ob = obstacles[size_t(i)];
                ok = r.get(ob.type, 1, 0xFFFF) && r.get(ob.duration, -1000000, 1000000) && r.get(ob.x, 0, 0xFFFF)
                     && r.get(ob.y, 0, 0xFFFF) && r.get(ob.field, -1, fields - 1) && r.get(ob.dir, 0, 4)
                     && r.get(ob.player, -1, kPlayerSlots - 1) && r.real(ob.amount);
            }
        } else if (key == "state") {
            ok = r.get(randomEvents, 0, 1) && r.get(offer, 0, int(EventKind::FootballClub))
                 && r.get(eventPlayer, 0, kPlayerSlots - 1) && r.get(doubleNext, 0, 1) && r.get(propertyMode, 0, 0xFF)
                 && r.get(eastOpen, 0, 1) && r.get(yellow, 0, 1) && r.get(season, 0, 3) && r.real(west);
        } else if (key == "facing") {
            for (int& d : facing) ok = ok && r.get(d, 1, 4);
        } else if (key == "deck") {
            const long long n = g.deck_.size();
            ok = r.get(deckPointer, 0, n - 1) && r.get(deckReturn, 0, n - 1);
            order.assign(size_t(n), Deck::kTaken);
            for (uint8_t& s : order) ok = ok && r.get(s, 0, 0xFF);
        } else if (key == "hand") {
            int p = 0;
            ok = r.get(p, 0, kPlayerSlots - 1);
            for (uint8_t& s : hands[size_t(p)]) ok = ok && r.get(s, 0, 0xFF);
        } else if (key == "calendar") ok = r.get(calendar.month, 0, 1 << 24) && r.get(calendar.year, 0, 1 << 24)
                                         && r.real(calendar.inflation);
        else if (key == "counter") {
            ok = r.get(allowance, 0, 1000);
            long long d = 0;
            history.clear();
            while (ok && r.more()) {
                ok = r.integer(d, 1, 4);
                history.push_back(Direction(d));
            }
        } else if (key == "risiko") {
            ok = r.get(risiko.level, 0, 4) && r.get(risiko.light, 0, 4) && r.get(risiko.ticks, 0, 1000)
                 && r.get(risiko.steps, 0, 1000);
        } else if (key == "demo") {
            ok = r.get(demoEnd, 0, 1);
        } else if (key == "extensions") {
            ok = r.get(extraCards, 0, 1);
        } else if (key == "stages") {
            ok = r.get(totalStages, 0, 1 << 20);
        } else if (key == "turn") {
            ok = r.get(phase, 0, int(Game::Phase::Risiko)) && r.get(current, 0, kPlayerSlots - 1)
                 && r.get(origin, -1, fields - 1) && r.get(destination, -1, fields - 1) && r.get(remaining, 0, 1 << 20);
        } else return fail("unbekannter Eintrag '" + key + "'");
        if (!ok || !r.done()) return fail("fehlerhafter Eintrag '" + key + "'");
    }
    if (!ended) return fail("Spielstand unvollstaendig");
    for (const char* k : {"rules", "data", "rng", "drago", "events", "places", "state", "facing", "deck", "calendar",
                          "counter", "turn"})
        if (seen[k] != 1) return fail(std::string("Eintrag '") + k + "' fehlt oder ist doppelt");
    if (version >= 2 && seen["risiko"] != 1) return fail("Eintrag 'risiko' fehlt oder ist doppelt");
    if (version >= 3 && seen["demo"] != 1) return fail("Eintrag 'demo' fehlt oder ist doppelt");
    if (version >= 4 && seen["stages"] != 1) return fail("Eintrag 'stages' fehlt oder ist doppelt");
    if (version >= 5 && seen["extensions"] != 1) return fail("Eintrag 'extensions' fehlt oder ist doppelt");
    if (version >= 6 ? seen["worth"] != int(money.size()) : seen["worth"] != 0)
        return fail("Eintraege 'worth' unvollstaendig");
    if ((Game::Phase(phase) == Game::Phase::Risiko) != (risiko.level != 0)) return fail("Risiko-Zustand passt nicht zur Phase");
    if (seen["player"] != kPlayerSlots || seen["hand"] != kPlayerSlots || seen["town"] != int(towns.size())
        || seen["money"] != int(money.size()))
        return fail("Spieler-, Hand-, Stadt- oder Geldeintraege unvollstaendig");
    if (!players[size_t(current)].status) return fail("aktueller Spieler ist nicht im Spiel");
    if (int(history.size()) > allowance) return fail("mehr Schritte als erlaubt");
    for (const Hand& h : hands)
        for (uint8_t s : h)
            if (s != Deck::kTaken && s >= g.deck_.size()) return fail("ungueltige Handkarte");
    // Validate the deck on a copy so a failure leaves the game untouched.
    Deck deck = g.deck_;
    if (!deck.restore(order, deckPointer, deckReturn)) return fail("ungueltiger Kartenstapel");
    if (towns.size() != g.economy_.townCount() || money.size() != g.economy_.playerCount())
        return fail("Wirtschaftsdaten passen nicht");

    if (random == Random::Restore) {
        g.rng_ = MsRandom();
        g.rng_.setState(rng);
    }
    g.players_ = players;
    g.economy_.restore(towns, money);
    g.drago_ = drago;
    g.events_ = events;
    g.places_ = places;
    g.obstacles_.restore(obstacles);
    g.randomEvents_ = randomEvents;
    g.offer_ = EventKind(offer);
    g.eventPlayer_ = eventPlayer;
    g.doubleNext_ = doubleNext;
    g.propertyMode_ = propertyMode;
    g.eastOpen_ = eastOpen;
    g.yellowPending_ = yellow;
    g.loadedSeason_ = season;
    g.westValue_ = west;
    g.lastDirection_ = facing;
    g.deck_ = deck;
    g.hands_ = hands;
    g.calendar_ = calendar;
    g.counter_ = MoveCounter(allowance, history);
    g.phase_ = Game::Phase(phase);
    g.current_ = current;
    g.origin_ = origin;
    g.destination_ = destination;
    g.remaining_ = remaining;
    g.risiko_ = risiko;
    g.demoEnd_ = demoEnd;
    g.computerExtraCards_ = extraCards != 0;
    g.totalStages_ = totalStages >= 0 ? totalStages : remaining; // formats 1-3: the stages left
    g.messages_.clear();
    g.pendingAllowance_ = -1; // a spin waiting for its window is not part of a state
    g.rollUndo_.clear();
    return true;
}

bool writeSaveFile(const std::string& path, const std::string& text, std::string& error) {
    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out || !(out << text) || !out.flush()) {
            error = "Spielstand kann nicht geschrieben werden: " + temp;
            std::remove(temp.c_str());
            return false;
        }
    }
    if (std::rename(temp.c_str(), path.c_str()) != 0) {
        error = "Spielstand kann nicht ersetzt werden: " + path;
        std::remove(temp.c_str());
        return false;
    }
    return true;
}

bool readSaveFile(const std::string& path, std::string& text, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "Spielstand nicht gefunden: " + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    text = buffer.str();
    return true;
}

} // namespace dtvr
