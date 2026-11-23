// SPDX-License-Identifier: GPL-3.0-only
// Tests for the hotseat game flow assembled from the recovered rules.
#include <memory>
#include "estates.hpp"
#include "game.hpp"
#include "save.hpp"

#include <cstdio>
#include <stdexcept>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// A ring of 13 fields: town "London" (kind 6) at 0, town "Rom" (kind 6) at 12,
// blue fields elsewhere. Fields are linked left/right, 12 wraps to 0.
static Board line(std::vector<Town>& towns) {
    Board b;
    b.fields.resize(13);
    for (int i = 0; i < 13; ++i) {
        Field& f = b.fields[size_t(i)];
        f.x = uint8_t(i); f.y = 0; f.kind = 1;
        f.next = {Field::kNone, int16_t((i + 1) % 13), Field::kNone, int16_t((i + 12) % 13)};
    }
    b.fields[0].kind = 6;
    b.fields[12].kind = 6;
    towns.resize(2);
    towns[0].x = 0; towns[0].name = "London";
    towns[1].x = 12; towns[1].name = "Rom";
    return b;
}

int main() {
    std::vector<Town> towns;
    const Board board = line(towns);
    bool threw = false;
    try { Game g(board, towns, 0, 1, 1); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);

    // A single human gets a computer opponent in another slot (seg2:021F).
    {
        Game solo(board, towns, 1, 0, 4);
        CHECK(activePlayerCount(solo.players()) == 2 && solo.players()[0].status == 1);
        int computers = 0;
        for (int p = 1; p < kPlayerSlots; ++p) computers += solo.isComputer(p);
        CHECK(computers == 1);
    }
    // seg2:6426: with a slot window the move starts only when it has closed
    // (seg2:66B9 -> 61B7); seg2:69AD: a human's Würfeln is taken back before that
    // (rounds 129 / 131).
    {
        auto rngOf = [](const Game& g) { // the "rng" record of the save format
            const std::string s = SaveCodec::encode(g);
            const size_t at = s.find("\nrng ");
            return uint32_t(std::stoul(s.substr(at + 5)));
        };
        Game ref(board, towns, 2, 0, 1), g(board, towns, 2, 0, 1);
        ref.setRandomEvents(false);
        g.setRandomEvents(false);
        g.setSpinPresentation(true);
        CHECK(!g.rollCancellable() && !g.cancelRoll(false) && !g.finishSpin());
        const std::string before = SaveCodec::encode(g);
        CHECK(g.roll() && g.spinPending() && g.phase() == Game::Phase::Roll && g.rollCancellable());
        CHECK(!g.roll() && !g.playCard(0) && !g.step(Direction::Right)); // the window is modal
        // Before the lever: nothing has been drawn; the state is the one before.
        CHECK(g.cancelRoll(false) && g.phase() == Game::Phase::Roll && !g.spinPending() && !g.cancelRoll(false));
        CHECK(SaveCodec::encode(g) == before);
        // The same reel again; the move starts when the window has closed.
        CHECK(g.roll() && ref.roll() && g.finishSpin() && !g.spinPending() && !g.rollCancellable());
        CHECK(g.phase() == Game::Phase::Move && g.allowance() == ref.allowance());
        // After the lever: the reel's draw stays consumed.
        Game h(board, towns, 2, 0, 1);
        h.setRandomEvents(false);
        h.setSpinPresentation(true);
        CHECK(h.roll() && h.cancelRoll(true) && h.phase() == Game::Phase::Roll);
        CHECK(rngOf(h) == rngOf(ref)); // ref has drawn its one reel as well
        // Without a window (tests, headless play) the move starts at once.
        CHECK(ref.phase() == Game::Phase::Move && !ref.spinPending() && !ref.rollCancellable());
    }

    // seg4:01EA -> seg2:0BC3: with a game window a human accepts the field with Return;
    // the vehicle waits on its last field and can still turn back (round 137).
    {
        Game g(board, towns, 2, 0, 1);
        g.setRandomEvents(false);
        g.setArrivalConfirm(true);
        CHECK(g.roll() && !g.arrivalDue() && !g.confirmArrival());
        const int n = g.allowance();
        for (int i = 0; i < n; ++i) CHECK(g.step(Direction::Right));
        CHECK(g.phase() == Game::Phase::Move && g.arrivalDue() && g.cash(0) == kStartingCash);
        if (n > 1) { // turning back undoes the last step, forward again
            CHECK(g.step(Direction::Left) && !g.arrivalDue() && g.step(Direction::Right) && g.arrivalDue());
        }
        CHECK(g.confirmArrival() && g.phase() == Game::Phase::Arrived && g.cash(0) > kStartingCash); // blue field
        CHECK(!g.arrivalDue() && !g.confirmArrival());
    }

    Game game(board, towns, 2, 0, 1);
    game.setRandomEvents(false);
    CHECK(game.origin() == 0 && game.destination() == 12 && game.currentPlayer() == 0);
    CHECK(game.phase() == Game::Phase::Roll);
    CHECK(!game.step(Direction::Right) && !game.endTurn());

    CHECK(game.roll() && game.phase() == Game::Phase::Move && !game.roll());
    const int rolled = game.allowance();
    CHECK(rolled >= 1 && rolled <= 6);
    CHECK(!game.step(Direction::Up) && !game.step(Direction::Down)); // no link
    // Step forward, back (undo), forward again until the allowance is used.
    CHECK(game.step(Direction::Right) && game.stepsTaken() == 1);
    if (rolled > 1) {
        CHECK(game.step(Direction::Left) && game.stepsTaken() == 0 && game.playerField(0) == 0);
        for (int i = 0; i < rolled; ++i) CHECK(game.step(Direction::Right));
    }
    CHECK(game.phase() == Game::Phase::Arrived && game.playerField(0) == rolled);
    CHECK(game.cash(0) > kStartingCash);                         // blue field
    CHECK(!game.step(Direction::Right));

    CHECK(game.endTurn() && game.currentPlayer() == 1 && game.phase() == Game::Phase::Roll);
    CHECK(game.calendar().month == 0);
    CHECK(game.roll());
    for (int i = 0; i < game.allowance(); ++i) game.step(Direction::Right);
    CHECK(game.endTurn() && game.currentPlayer() == 0 && game.calendar().month == 1);

    // Keep driving right around the ring; landing exactly on the destination with
    // 0 stages remaining ends the game (300,000 reward).
    for (int guard = 0; guard < 5000 && game.phase() != Game::Phase::GameOver; ++guard) {
        if (game.phase() == Game::Phase::Arrived) game.endTurn();
        else if (game.phase() == Game::Phase::Roll) game.roll();
        else CHECK(game.step(Direction::Right));
    }
    CHECK(game.phase() == Game::Phase::GameOver);
    CHECK(game.cash(game.currentPlayer()) >= kStartingCash + 300000.0);

    // With one stage left the first arrival continues with a new destination; it is
    // neither the old stage start nor the reached town (round 49: seg7:3755 chooses
    // before DS:A10B moves to the reached field). A third destination town (field 6)
    // keeps a candidate.
    Board threeTowns = board;
    threeTowns.fields[6].kind = 6;
    std::vector<Town> threeTownList = towns;
    threeTownList.push_back(Town{});
    threeTownList.back().x = 6;
    threeTownList.back().name = "Paris";
    Game stages(threeTowns, threeTownList, 1, 1, 3, 1, nullptr, nullptr, false); // solo variant
    stages.setRandomEvents(false);
    const int firstOrigin = stages.origin(), firstGoal = stages.destination();
    for (int guard = 0; guard < 5000 && stages.remainingStages() == 1; ++guard) {
        if (stages.phase() == Game::Phase::Arrived) stages.endTurn();
        else if (stages.phase() == Game::Phase::Roll) stages.roll();
        else stages.step(Direction::Right);
    }
    CHECK(stages.remainingStages() == 0 && stages.phase() != Game::Phase::GameOver);
    CHECK(firstOrigin == 0 && stages.origin() == firstGoal);
    CHECK(stages.destination() >= 0 && stages.destination() != firstOrigin && stages.destination() != firstGoal);

    // Cards: a movement card from the starting hand replaces the spin.
    Game cardGame(board, towns, 1, 0, 1, 8, nullptr, nullptr, false);
    cardGame.setRandomEvents(false);
    int moveSlot = -1, otherSlot = -1;
    for (int i = 0; i < 8; ++i) {
        const int type = cardGame.deck().typeOfSlot(cardGame.hand(0)[size_t(i)]);
        if (type < 0) continue;
        const CardMove m = cardMove(type);
        if ((m.reels || m.fields) && moveSlot < 0) moveSlot = i;
        if (cardKind(type) == CardKind::Unsupported && otherSlot < 0) otherSlot = i;
    }
    CHECK(cardGame.deck().remaining() == 199 - 8);
    if (otherSlot >= 0) CHECK(!cardGame.playCard(otherSlot) && cardGame.phase() == Game::Phase::Roll);
    if (moveSlot >= 0) {
        const int type = cardGame.deck().typeOfSlot(cardGame.hand(0)[size_t(moveSlot)]);
        CHECK(cardGame.playCard(moveSlot) && cardGame.phase() == Game::Phase::Move);
        CHECK(cardGame.hand(0)[size_t(moveSlot)] == Deck::kTaken);
        const CardMove m = cardMove(type);
        if (m.fields) CHECK(cardGame.allowance() == m.fields);
        else CHECK(cardGame.allowance() >= m.reels && cardGame.allowance() <= m.reels * (m.maximum + 1));
        CHECK(!cardGame.roll() && !cardGame.playCard(moveSlot));
    }
    CHECK(!cardGame.playCard(-1) && !cardGame.playCard(8));

    // Yellow fields hand out the top card of the deck.
    Board yellow = board;
    for (int i = 1; i < 12; ++i) yellow.fields[size_t(i)].kind = 3;
    Game yellowGame(yellow, towns, 1, 0, 2, 1, nullptr, nullptr, false);
    yellowGame.setRandomEvents(false);
    auto held = [&] { int n = 0; for (auto s : yellowGame.hand(0)) n += s != Deck::kTaken; return n; };
    CHECK(held() == 1);
    const int top = yellowGame.deck().peek();
    yellowGame.roll();
    while (yellowGame.phase() == Game::Phase::Move) yellowGame.step(Direction::Right);
    CHECK(held() == 2 && yellowGame.deck().remaining() == 199 - 2);
    bool hasTop = false;
    for (auto s : yellowGame.hand(0)) hasTop |= s == top;
    CHECK(hasTop);

    // Town purchase: only after arriving in a town, entry must exist and be free.
    std::vector<Town> shopTowns = towns;
    shopTowns[0].entries[0].name = "Kiosk"; shopTowns[0].entries[0].value = 1000; shopTowns[0].entries[0].words[0] = 5;
    Game shop(board, shopTowns, 1, 99, 1, 1, nullptr, nullptr, false);
    shop.setRandomEvents(false);
    CHECK(shop.townAt(0) == 0 && shop.townAt(5) == -1 && !shop.buyProperty(0));
    for (int guard = 0; guard < 5000; ++guard) {
        if (shop.phase() == Game::Phase::Arrived && shop.playerField(0) == 0) break;
        if (shop.phase() == Game::Phase::Arrived) shop.endTurn();
        else if (shop.phase() == Game::Phase::Roll) shop.roll();
        else shop.step(Direction::Right);
    }
    CHECK(shop.phase() == Game::Phase::Arrived && shop.playerField(0) == 0);
    {
        const double before = shop.cash(0);
        CHECK(!shop.buyProperty(1) && shop.buyProperty(0) && shop.cash(0) == before - 1000);
        CHECK(!shop.buyProperty(0));
        CHECK(shop.economy().town(0).entries[0].owner == 0);
        // Statistik (round 34): the owned entry with value * 0.7; category filter.
        const auto stats = statisticsRows(shop, 0, 0, 0);
        CHECK(stats.size() == 1 && stats[0].name == "Kiosk" && stats[0].town == 0 && stats[0].price == 700);
        CHECK(stats[0].yieldPercent == 5 * 1.2 && !stats[0].disabled && stats[0].townName == oemToUtf8(shopTowns[0].name));
        CHECK(statisticsRows(shop, 0, shopTowns[0].entries[0].words[1] + 1, 0).empty());
        CHECK(statisticsRows(shop, 1, 0, 0).empty());
    }

    // Ranking (round 46, seg1:8EE1): cash + property value, slot order on a tie.
    {
        Game rank(board, towns, 3, 1, 4, 1, nullptr, nullptr, false);
        const auto r = rank.ranking();
        CHECK(r.size() == 3 && r[0] == 0 && r[1] == 1 && r[2] == 2); // all equal at the start
    }

    // Money and status cards with targets (two players: target is automatic).
    {
        std::vector<PlayerSlot> three(8);
        three[0].status = three[2].status = three[5].status = 1;
        CHECK(resolveTarget(three, 0, 2) == 2 && resolveTarget(three, 0, 0) == -1);
        CHECK(resolveTarget(three, 0, 1) == -1 && resolveTarget(three, 0, -1) == -1);
        three[5].status = 0;
        CHECK(resolveTarget(three, 0, -1) == 2);                   // two players: the other
        three[2].status = 0;
        CHECK(resolveTarget(three, 0, -1) == 0);                   // one player: first active
    }
    CHECK(cardKind(9) == CardKind::Fiskus && cardKind(16) == CardKind::Schlafen);
    CHECK(cardKind(36) == CardKind::Unsupported && cardKind(26) == CardKind::Risiko && cardStatusFlag(CardKind::Zucker) == 0x02);
    CHECK(cardNeedsTarget(CardKind::Fiskus) && !cardNeedsTarget(CardKind::Konto));

    // Mitspieler (round 86, seg2:5E93): a human who rolled hands the turn to the
    // computer, which finishes it; switching back makes the slot human again.
    {
        Game swap(board, towns, 2, 0, 5, 1, nullptr, nullptr, false);
        swap.setRandomEvents(false);
        CHECK(swap.roll() && swap.phase() == Game::Phase::Move && swap.currentPlayer() == 0);
        CHECK(swap.setComputer(0, true) && swap.isComputer(0));
        for (int guard = 0; guard < 50 && swap.currentPlayer() == 0; ++guard) swap.runComputerTurn();
        CHECK(swap.currentPlayer() == 1 && swap.phase() == Game::Phase::Roll);
        CHECK(swap.setComputer(0, false) && !swap.isComputer(0));
    }

    // Year balance closing (round 92, seg8:95E5): pos_e / pos_f; letter 4 gives 5,
    // letter 5 gives 4, otherwise one more draw rand() % 2 + 4.
    {
        Game year(board, towns, 2, 40, 7, 1, nullptr, nullptr, false);
        year.setComputer(0, true);
        year.setComputer(1, true);
        for (int guard = 0; guard < 20000 && year.balanceSerial() == 0; ++guard) year.runComputerTurn();
        CHECK(year.balanceSerial() == 1 && !year.balanceEntries().empty());
        if (!year.balanceEntries().empty()) {
            const int next = (year.balanceEntries().back().letter + 1) % 7;
            const int c = year.balanceClosing();
            CHECK(c == 4 || c == 5);
            if (next == 4) CHECK(c == 5);
            if (next == 5) CHECK(c == 4);
        }
    }

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("game_test: OK");
    // Name codes of slot 7 (round 112, seg2:6332): case-sensitive, applied after the deal.
    {
        std::array<uint8_t, kPlayerSlots> slots{};
        slots[0] = 1;
        slots[7] = 1;
        auto make = [&](const std::string& name) {
            auto g = std::make_unique<Game>(board, towns, slots, 99, 5, 1, nullptr, nullptr);
            std::array<std::string, kPlayerSlots> names;
            names[7] = name;
            g->setPlayerNames(names);
            g->applyNameCodes();
            return g;
        };
        auto plain = make("Armino Gesserti");
        const double start = plain->cash(7);
        const int cards = plain->handCount(7);
        CHECK(make("armin gessert")->cash(7) == start);
        auto armin = make("Armin Gessert");
        CHECK(armin->cash(7) == 500000.0 && armin->handCount(7) == cards);
        auto birthday = make("130663");
        CHECK(birthday->cash(7) == 1000000.0 && birthday->playerNames()[7] == "Armin Gessert");
        CHECK(cards == 0 ? birthday->handCount(7) == 0 : birthday->handCount(7) == 8); // the walk cascades
        CHECK(birthday->cash(0) == start && birthday->handCount(0) == cards);
    }

    return failures != 0;
}
