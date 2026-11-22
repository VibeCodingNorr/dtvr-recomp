// SPDX-License-Identifier: GPL-3.0-only
// Blue/red arrival messages: rally.exe seg2:0EF1-0F0D and 1081-109D.
#include "game.hpp"
#include "panel.hpp"
#include "save.hpp"

#include <cstdio>
#include <sstream>
#include <stdexcept>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static uint32_t rngState(const Game& game) {
    std::istringstream save(SaveCodec::encode(game));
    std::string line;
    while (std::getline(save, line)) {
        if (line.rfind("rng ", 0) != 0) continue;
        std::istringstream value(line.substr(4));
        uint32_t state = 0;
        if (value >> state) return state;
    }
    throw std::runtime_error("RNG state absent from native save");
}

static Board moneyBoard(int kind) {
    Board board;
    board.fields.resize(80);
    for (int i = 0; i < 80; ++i) {
        auto& field = board.fields[size_t(i)];
        field.x = uint8_t(i);
        field.kind = uint16_t(kind);
        field.next[1] = int16_t((i + 1) % 80);
        field.next[3] = int16_t((i + 79) % 80);
    }
    board.fields[0].kind = board.fields[79].kind = 6;
    return board;
}

int main() {
    std::vector<Town> towns(2);
    towns[0].name = "London";
    towns[1].name = "Rom";
    towns[1].x = 79;
    std::array<std::string, kPlayerSlots> names{};
    names[0] = "Testfahrer";
    for (int kind : {1, 2}) {
        const Board board = moneyBoard(kind);
        for (int seed = 1; seed <= 128; ++seed) {
            Game game(board, towns, 2, 2, uint16_t(seed));
            game.setPlayerNames(names);
            game.setRandomEvents(false);
            CHECK(game.takeNotices().empty());
            CHECK(game.takeSounds().empty());
            const double before = game.cash(0);
            MsRandom reference;
            reference.setState(rngState(game));
            const int steps = reference.random(5) + 1;
            const auto expected = moneyField(kind, game.calendar().month, game.calendar().inflation, reference);
            const double displayAmount = kind == 1 ? expected.amount : -expected.amount;

            CHECK(game.roll() && game.allowance() == steps);
            for (int i = 0; i < steps; ++i) {
                CHECK(game.step(Direction::Right));
                if (i + 1 < steps) {
                    CHECK(game.phase() == Game::Phase::Move);
                    CHECK(game.takeNotices().empty()); // passing a money field does not open a dialog
                    const auto driving = game.takeSounds();
                    CHECK(driving.size() == 1);
                    if (driving.size() == 1) CHECK(driving[0].channel == 1 && driving[0].sound == 0);
                }
            }
            CHECK(game.phase() == Game::Phase::Arrived && game.currentPlayer() == 0);
            CHECK(game.cash(0) == before + expected.amount);
            CHECK(rngState(game) == reference.state()); // no extra draw to create a dialog
            const auto notices = game.takeNotices();
            CHECK(notices.size() == 1);
            if (notices.size() == 1) {
                const auto& notice = notices.front();
                CHECK(notice.text == expected.textId);
                CHECK(notice.args.size() == 2);
                if (notice.args.size() == 2) {
                    CHECK(notice.args[0].kind == NoticeArg::Kind::Player && notice.args[0].player == 0);
                    CHECK(notice.args[1].kind == NoticeArg::Kind::Money && notice.args[1].money == displayAmount);
                }
                CHECK(notice.buttons == 0 && notice.picture == Notice::Picture::Showmaster);
                CHECK(formatNotice("driver=%Fs;sum=%Fs", notice, game)
                      == "driver=Testfahrer;sum=" + formatMoney(displayAmount));
            }
            const auto sounds = game.takeSounds();
            CHECK(sounds.size() == 2);
            if (sounds.size() == 2) {
                CHECK(sounds[0].channel == 1 && sounds[0].sound == 9); // final driving step
                CHECK(sounds[1].channel == 3 && sounds[1].sound == (kind == 1 ? 14 : 1));
                CHECK(sounds[1].loops == 0);
            }
            CHECK(!game.step(Direction::Right));
            CHECK(game.takeNotices().empty() && game.takeSounds().empty());
        }
    }
    return failures ? 1 : 0;
}
