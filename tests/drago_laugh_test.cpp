// SPDX-License-Identifier: GPL-3.0-only
// Dr. Drago's laugh seg2:BB48 starts with his message, not after it has been closed:
// every action with his picture calls it right before seg6:51EA (seg2:BB24, BC8B, BD84,
// BE78, BF8C), the breakdown at the turn change too (seg2:C067 before C07A). Weihnachten's
// Hebel05 (seg3:1257) also precedes its message seg3:126F.
#include "game.hpp"
#include "save.hpp"

#include <cstdio>
#include <memory>
#include <sstream>
#include <stdexcept>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static std::string replaceLine(const std::string& save, const std::string& key, const std::string& line) {
    std::istringstream in(save);
    std::string out, l;
    while (std::getline(in, l)) out += (l.rfind(key, 0) == 0 ? line : l) + "\n";
    return out;
}

static uint32_t rngState(const Game& game) {
    std::istringstream save(SaveCodec::encode(game));
    std::string line;
    while (std::getline(save, line))
        if (line.rfind("rng ", 0) == 0) return uint32_t(std::stoul(line.substr(4)));
    throw std::runtime_error("RNG state absent from native save");
}

static Board moneyBoard() {
    Board board;
    board.fields.resize(80);
    for (int i = 0; i < 80; ++i) {
        auto& field = board.fields[size_t(i)];
        field.x = uint8_t(i);
        field.kind = 2;
        field.next[1] = int16_t((i + 1) % 80);
        field.next[3] = int16_t((i + 79) % 80);
    }
    board.fields[0].kind = board.fields[79].kind = 6;
    return board;
}

static bool isLaugh(const SoundCue& c) { return c.channel == kDragoLaughChannel && (c.sound == 3 || c.sound == 29); }

int main() {
    // The rules: the breakdown text random(4), then the laugh random(1) (seg2:C05F, C067).
    {
        int seen3 = 0, seen29 = 0;
        for (uint32_t seed = 1; seed <= 64; ++seed) {
            std::vector<PlayerSlot> turn(kPlayerSlots);
            turn[0].status = turn[1].status = turn[2].status = 1;
            turn[1].flags = kFlagDragoBlock;
            Calendar cal;
            MsRandom rng(seed), copy(seed);
            std::vector<SitOutText> shown;
            TurnHooks hooks;
            hooks.dragoHolder = 1;
            hooks.announce = [&](const SitOutText& t) { shown.push_back(t); };
            const auto tc = nextPlayer(turn, 0, cal, rng, hooks);
            const int text = 3251 + copy.random(4);
            const int laugh = copy.random(1) == 0 ? 3 : 29;
            CHECK(tc.player == 2 && shown.size() == 1);
            if (shown.size() == 1) CHECK(shown[0].text == text && shown[0].player == 1 && shown[0].sound == laugh);
            CHECK(rng.state() == copy.state());
            (laugh == 3 ? seen3 : seen29)++;
        }
        CHECK(seen3 > 0 && seen29 > 0);
    }

    // The game: Dr. Drago acts after a red field (seg2:1095 -> B58C). His laugh is emitted
    // before his notice, so the window plays it when the message opens.
    std::vector<Town> towns(2);
    towns[0].name = "London";
    towns[1].name = "Rom";
    towns[1].x = 79;
    const Board board = moneyBoard();
    int acted = 0, spends = 0, blocks = 0;
    for (int form = 0; form <= 2; ++form) {
        for (int seed = 1; seed <= 400; ++seed) {
            Game game(board, towns, 2, 2, uint16_t(seed));
            game.setRandomEvents(false);
            game.startDrago(form);
            if (game.drago().holder != 0 || game.currentPlayer() != 0) continue;
            game.takeNotices();
            game.takeSounds();
            MsRandom ref;
            ref.setState(rngState(game));
            if (!game.roll()) continue;
            const int steps = ref.random(5) + 1;
            for (int i = 0; i < steps; ++i) game.step(Direction::Right);
            if (game.phase() != Game::Phase::Arrived) continue;
            const auto notices = game.takeNotices();
            const auto sounds = game.takeSounds();
            const Notice* drago = nullptr;
            for (const Notice& n : notices) if (n.picture == Notice::Picture::Drago) drago = &n;
            int laughs = 0;
            for (const SoundCue& c : sounds) {
                if (!isLaugh(c)) continue;
                ++laughs;
                CHECK(drago != nullptr);
                if (drago) CHECK(c.afterNotice == drago->seq); // plays as the message opens
            }
            if (drago) {
                ++acted;
                CHECK(laughs == 1);
            }
            const bool block = notices.size() == 2 && notices.back().text == 3351;
            if (block) {
                ++blocks;
                CHECK(laughs == 0); // seg2:BFB0 has no laugh
            }
            // Exact draw order for the spending action (seg2:BAD4, BAE6, BB5E).
            const auto field = moneyField(2, game.calendar().month, game.calendar().inflation, ref);
            (void)field;
            if (chooseDragoAction(form, ref) == DragoAction::Spend) {
                ++spends;
                const int text = 3151 + ref.random(19);
                dragoAmount(game.calendar().month, form, game.calendar().inflation, ref);
                const int laugh = dragoLaugh(ref);
                CHECK(drago && drago->text == text);
                CHECK(laughs == 1);
                for (const SoundCue& c : sounds) if (isLaugh(c)) CHECK(c.sound == laugh);
                CHECK(rngState(game) == ref.state());
            }
        }
    }
    CHECK(acted > 20 && spends > 0 && blocks > 0);

    // Weihnachten in December with a small hand: Hebel05 before the message (seg3:1257).
    {
        int checked = 0;
        for (uint16_t seed = 1; seed < 3000 && checked < 3; ++seed) {
            auto g = std::make_unique<Game>(board, towns, 2, 2, seed, 1, nullptr, nullptr, false);
            g->setRandomEvents(false);
            if (g->currentPlayer() != 0 || cardKind(g->deck().typeOfSlot(g->hand(0)[0])) != CardKind::Weihnachten)
                continue;
            std::string error;
            const std::string december = replaceLine(SaveCodec::encode(*g), "calendar ", "calendar 11 0 0x1p+0");
            CHECK(SaveCodec::decode(*g, december, error));
            CHECK(g->calendar().month == 11);
            g->takeNotices();
            g->takeSounds();
            CHECK(g->playCard(0));
            const auto notices = g->takeNotices();
            const auto sounds = g->takeSounds();
            CHECK(notices.size() >= 1 && sounds.size() >= 1);
            if (!notices.empty() && !sounds.empty()) {
                CHECK(notices[0].text >= 13006 && notices[0].text <= 13008);
                CHECK(sounds[0].channel == 2 && sounds[0].sound == 19);
                CHECK(sounds[0].afterNotice == notices[0].seq);
            }
            ++checked;
        }
        CHECK(checked == 3);
    }

    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
