// SPDX-License-Identifier: GPL-3.0-only
#include "events.hpp"

#include <cstdlib>

namespace dtvr {
namespace {

using E = EventKind;
// Far-pointer tables built on the stack at seg8:181F-1ADC.
constexpr std::array<std::array<EventKind, 16>, 4> kTables = {{
    {E::Rain, E::Rain, E::Rain, E::Rain, E::Storm, E::Storm, E::Storm, E::Storm,
     E::Snow, E::Snow, E::Snow, E::Snow, E::Rockfall, E::Rockfall, E::ProfessorGood, E::ProfessorGood},
    {E::Rain, E::Rain, E::Robbers, E::Robbers, E::Robbers, E::Rockfall, E::Rockfall, E::Snow,
     E::Snow, E::ProfessorGood, E::ProfessorGood, E::ProfessorBad, E::Flood, E::Flood, E::Earthquake, E::Earthquake},
    {E::Rain, E::Rain, E::Robbers, E::Robbers, E::Rockfall, E::Rockfall, E::Snow, E::Snow,
     E::Drought, E::Drought, E::Flood, E::Flood, E::Earthquake, E::Nessie, E::Storm, E::Storm},
    {E::Rain, E::Rockfall, E::Snow, E::Robbers, E::Robbers, E::Storm, E::Drought, E::ProfessorGood,
     E::Flood, E::Flood, E::Earthquake, E::Earthquake, E::Nessie, E::Nessie, E::Robbers, E::Volcano},
}};

} // namespace

std::string eventName(EventKind kind) {
    switch (kind) {
    case E::None: return "-";
    case E::OpenEast: return "Osteuropa oeffnet sich";
    case E::Rain: return "Regen";
    case E::Storm: return "Sturm";
    case E::Snow: return "Schnee";
    case E::Rockfall: return "Steinschlag";
    case E::Robbers: return "Raeuber";
    case E::Flood: return "Hochwasser";
    case E::Earthquake: return "Erdbeben";
    case E::Drought: return "Duerre";
    case E::Nessie: return "Nessie";
    case E::Volcano: return "Vulkan";
    case E::ProfessorGood: return "Professor (seg8:6FBF(1))";
    case E::ProfessorBad: return "Professor (seg8:6FBF(0))";
    }
    return "?";
}

int nextEventCountdown(int activePlayers, int year, MsRandom& rng) {
    const int base = (10 - (year >> 5)) * activePlayers;
    return base + rng.random(activePlayers);
}

EventKind chooseEvent(int activePlayers, int turns, int month, MsRandom& rng) {
    const int n = activePlayers > 0 ? activePlayers : 1;
    size_t table = 3;
    if (120 / n > turns) table = 0;
    else if (500 / n > month) table = 1;
    else if (900 / n > month) table = 2;
    return kTables[table][size_t(rng.random(15))];
}

bool eastShouldOpen(double westValue, double ownedValue, int activePlayers, int year) {
    static constexpr double factors[9] = {0, 10, 12, 14, 16, 18, 20, 22, 24}; // DS:870E..874E
    const int n = activePlayers < 0 ? 0 : activePlayers > 8 ? 8 : activePlayers;
    const double threshold = factors[n] * 0.01 * westValue;                // DS:8756
    return threshold < ownedValue || year > 40;
}

double westPropertyValue(const Board& board, const std::vector<Town>& towns, uint16_t mask) {
    double sum = 0;
    for (const Field& f : board.fields) {
        if ((f.flags & mask) || (f.kind != 4 && f.kind != 6)) continue;
        for (const Town& t : towns)
            if (t.x == f.x && t.y == f.y) {
                for (const auto& e : t.entries) sum += e.value;
                break;
            }
    }
    return sum;
}

std::vector<bool> barrierLinks(const Board& board, const WorldMap& map, const TileProperties& props) {
    static constexpr int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    std::vector<bool> barred(board.fields.size() * 4, false);
    for (size_t i = 0; i < board.fields.size(); ++i) {
        const Field& f = board.fields[i];
        for (int k = 0; k < 4; ++k) {
            const int16_t n = f.next[size_t(k)];
            if (n == Field::kNone) continue;
            const Field& g = board.fields[size_t(n)];
            const int steps = std::abs(g.x - f.x) + std::abs(g.y - f.y);
            for (int s = 1; s < steps; ++s)
                if (props.cell(map, f.x + dx[k] * s, f.y + dy[k] * s) & 0x20) barred[i * 4 + size_t(k)] = true;
        }
    }
    return barred;
}

int robberyVictim(const std::vector<PlayerSlot>& players, const Board& board,
                  const std::vector<double>& cash, double inflation, const std::vector<bool>& seaTwoRight) {
    for (int p = 0; p < kPlayerSlots && size_t(p) < players.size(); ++p) {
        if (!players[size_t(p)].status) continue;
        if (!(cash[size_t(p)] > 500000.0 * inflation)) continue;       // DS:875E
        const int field = players[size_t(p)].field;
        if (field < 0 || board.fields[size_t(field)].next[1] == Field::kNone) continue;
        if (size_t(p) < seaTwoRight.size() && seaTwoRight[size_t(p)]) continue;
        return p;
    }
    return -1;
}

} // namespace dtvr
