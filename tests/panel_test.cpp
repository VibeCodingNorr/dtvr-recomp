// SPDX-License-Identifier: GPL-3.0-only
// Player info panel model (seg5:61C5, seg6:137F, DS:3936).
#include "panel.hpp"

#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    CHECK(formatMoney(30000) == "$30.000" && formatMoney(0) == "$0" && formatMoney(999) == "$999");
    CHECK(formatMoney(1000) == "$1.000" && formatMoney(1234567) == "$1.234.567");
    CHECK(formatMoney(-50) == "-$50" && formatMoney(-1234567.9) == "-$1.234.567");

    const ScreenLayout l = layoutForCanvas(640);
    const ScreenRect s0 = panelSlot(l, 0), s3 = panelSlot(l, 3), s6 = panelSlot(l, 6);
    CHECK(s0.x == 0 && s0.y == 5 && s0.w == 138 && s0.h == 130);   // 255 -> panel edge 138 x 135
    CHECK(s3.x == 97 && s3.y == 55 && s3.w == 32 && s3.h == 16);
    CHECK(s6.x == 8 && s6.y == 102 && s6.h == 149);
    const ScreenRect big = panelSlot(layoutForCanvas(1024), 5);
    CHECK(big.x == 150 && big.y == 111 && big.w == 48 && big.h == 24);

    // Contents from a game: a row of fields, London (start) and Rom (destination).
    Board b;
    b.fields.resize(5);
    for (int i = 0; i < 5; ++i) {
        b.fields[size_t(i)].x = uint8_t(i * 3);
        b.fields[size_t(i)].kind = 1;
        if (i < 4) b.fields[size_t(i)].next[1] = int16_t(i + 1);
        if (i > 0) b.fields[size_t(i)].next[3] = int16_t(i - 1);
    }
    b.fields[0].kind = 6;
    b.fields[4].kind = 6;
    std::vector<Town> towns(2);
    towns[0].name = "London";
    towns[1].x = 12; towns[1].name = "Rom";
    Game game(b, towns, 1, 0, 1, 1, nullptr, nullptr, false);
    const std::map<int, std::string> texts{{201, "Z\xc3\xbcge"}, {301, "Januar"}, {203, "Gewinn-Feld"}};
    PlayerInfo pi = playerInfo(game, b, towns, texts, -1);
    CHECK(pi.monthLine == "Januar (1)" && pi.money == "$30.000" && pi.destination == "Rom");
    CHECK(pi.distance[0] == 0 && pi.distance[1] == 4 && pi.stepsLabel == "Z\xc3\xbcge" && pi.fieldKind.empty());
    CHECK(game.roll());
    pi = playerInfo(game, b, towns, texts, -1);
    CHECK(pi.steps[1] == game.allowance() % 10);
    for (int i = 0; i < 10 && game.phase() == Game::Phase::Move; ++i)  // the row ends at field 4
        if (!game.step(Direction::Right)) game.step(Direction::Left);
    pi = playerInfo(game, b, towns, texts, 2); // DS:B632 of the drive, whatever the phase
    CHECK(pi.fieldKind == "Gewinn-Feld" && pi.fieldIcon == "BLAU");
    pi = playerInfo(game, b, towns, texts, 0);
    CHECK(pi.fieldKind == "London" && pi.fieldIcon.empty());

    // Notices: the solo opponent (texts 961-963) and the yellow field card (4201-4214).
    {
        Game solo(b, towns, 1, 0, 2);
        solo.setPlayerNames({"Anna", "Bert", "Carl", "Dora", "Emil", "Fred", "Gina", "Hugo"});
        auto notes = solo.takeNotices();
        CHECK(notes.size() == 1 && notes[0].text >= 961 && notes[0].text <= 963 && notes[0].args.size() == 2);
        CHECK(solo.takeNotices().empty());
        const std::string text = formatNotice("%Fs bekommt %Fs als Mitspieler (100%%), %d", notes[0], solo);
        CHECK(text.rfind("Anna bekommt ", 0) == 0 && text.find(" als Mitspieler (100%), ") != std::string::npos);
        Notice money{5021, {}};
        NoticeArg m;
        m.kind = NoticeArg::Kind::Money;
        m.money = 7500;
        money.args.push_back(m);
        CHECK(formatNotice("Du verlierst %Fs!", money, solo) == "Du verlierst $7.500!");
    }

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("panel_test: OK");
    return failures != 0;
}
