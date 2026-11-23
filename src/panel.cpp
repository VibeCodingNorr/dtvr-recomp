// SPDX-License-Identifier: GPL-3.0-only
#include "panel.hpp"

#include <cstdio>

namespace dtvr {

std::string formatMoney(double value) {
    char raw[32];
    std::snprintf(raw, sizeof raw, "$%li", long(value)); // seg10:368A truncates
    const std::string s = raw;
    std::string out;
    size_t from;
    if (value < 0) { out = "-$"; from = 2; } else { out = "$"; from = 1; }
    int group = int((s.size() - from) % 3);
    if (group == 0) group = 3;
    for (size_t i = from; i < s.size(); ++i) {
        out += s[i];
        if (--group == 0 && i + 1 < s.size()) { out += '.'; group = 3; }
    }
    return out;
}

ScreenRect panelSlot(const ScreenLayout& layout, int slot) {
    // DS:3936, rows for 1024, 800 and 640 (seg6:12FA order 0, 1, 2).
    static constexpr int kSlots[3][8][4] = {
        {{0, 12, 255, 255}, {0, 41, 255, 80}, {20, 82, 255, 255}, {150, 82, 198, 106},
         {20, 111, 255, 255}, {150, 111, 198, 135}, {20, 149, 255, 175}, {160, 153, 186, 169}},
        {{0, 8, 255, 255}, {0, 30, 255, 50}, {12, 66, 255, 255}, {124, 66, 162, 85},
         {12, 88, 255, 255}, {124, 88, 162, 107}, {12, 119, 255, 137}, {132, 122, 152, 113}},
        {{0, 5, 255, 255}, {0, 25, 255, 50}, {8, 55, 255, 255}, {97, 55, 129, 71},
         {8, 77, 255, 255}, {97, 77, 129, 93}, {8, 102, 255, 251}, {104, 105, 120, 114}}};
    const int res = layout.resolution();
    const ScreenRect client = layout.playerInfo();
    const int* s = kSlots[res][slot & 7];
    const int left = s[0] == 255 ? 0 : s[0], top = s[1] == 255 ? 0 : s[1];
    const int right = s[2] == 255 ? client.w : s[2], bottom = s[3] == 255 ? client.h : s[3];
    return {left, top, right - left, bottom - top};
}

PlayerInfo playerInfo(const Game& game, const Board& board, const std::vector<Town>& towns,
                      const std::map<int, std::string>& texts, int labelField) {
    auto text = [&](int id) { auto it = texts.find(id); return it == texts.end() ? std::string() : it->second; };
    auto townName = [&](int field) {
        if (field < 0 || size_t(field) >= board.fields.size()) return std::string();
        const Field& f = board.fields[size_t(field)];
        for (const Town& t : towns)
            if (t.x == f.x && t.y == f.y) return oemToUtf8(t.name);
        return std::string();
    };
    PlayerInfo info;
    const int p = game.currentPlayer();
    info.monthLine = text(301 + game.calendar().month % 12) + " (" + std::to_string(game.calendar().year + 1) + ")";
    info.money = formatMoney(game.cash(p));
    info.destination = townName(game.destination());
    const int field = game.playerField(p);
    const auto dist = fieldDistances(board, game.destination(), game.routeMaskForDisplay());
    int d = field >= 0 && dist[size_t(field)] >= 0 ? dist[size_t(field)] : 99;
    if (d > 99) d = 99;
    info.distance = {d / 10, d % 10};
    info.stepsLabel = text(201);
    const int left = game.phase() == Game::Phase::Move ? game.allowance() - game.stepsTaken() : 0;
    info.steps = {left / 10 % 10, left % 10};
    if (labelField >= 0 && size_t(labelField) < board.fields.size()) { // seg2:5ABC
        const int kind = board.fields[size_t(labelField)].kind;
        info.fieldKind = (kind == 4 || kind == 6) ? townName(labelField) : text(202 + kind);
        if (info.fieldKind.size() > 19) info.fieldKind.resize(19);
        info.fieldIcon = kind == 1 ? "BLAU" : kind == 2 ? "ROT" : kind == 3 ? "GELB" : kind == 5 ? "LILA" : "";
    }
    return info;
}

std::string formatNotice(const std::string& pattern, const Notice& notice, const Game& game) {
    std::string out;
    size_t next = 0;
    auto arg = [&]() -> std::string {
        if (next >= notice.args.size()) return {};
        const NoticeArg& a = notice.args[next++];
        switch (a.kind) {
        case NoticeArg::Kind::Player: return game.playerName(a.player);
        case NoticeArg::Kind::Money: return formatMoney(a.money);
        case NoticeArg::Kind::Text: return a.text;
        }
        return {};
    };
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] != '%') { out += pattern[i]; continue; }
        if (pattern.compare(i, 3, "%Fs") == 0) { out += arg(); i += 2; }
        else if (pattern.compare(i, 2, "%s") == 0 || pattern.compare(i, 2, "%d") == 0) { out += arg(); i += 1; }
        else if (pattern.compare(i, 2, "%%") == 0) { out += '%'; i += 1; }
        else out += '%';
    }
    return out;
}

} // namespace dtvr
