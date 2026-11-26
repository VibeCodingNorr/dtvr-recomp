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
    // seg1:02A9 seeds rand() once (round 143): a game built from a running generator
    // is the game of that state, and a game after the intro's draws continues them.
    {
        const std::array<uint8_t, kPlayerSlots> slots{1, 2, 0, 0, 0, 0, 0, 0};
        Game bySeed(board, towns, slots, 0, uint16_t(9)), byState(board, towns, slots, 0, MsRandom(9));
        CHECK(SaveCodec::encode(bySeed) == SaveCodec::encode(byState));
        MsRandom session(9);
        for (int i = 0; i < 40; ++i) session.rand(); // e.g. the intro's wipe cells
        Game after(board, towns, slots, 0, session);
        CHECK(SaveCodec::encode(after) != SaveCodec::encode(bySeed));
        MsRandom again(9);
        for (int i = 0; i < 40; ++i) again.rand();
        CHECK(SaveCodec::encode(Game(board, towns, slots, 0, again)) == SaveCodec::encode(after));
    }
    // seg8:6FBF: a computer's investment offer waits for the human's answer in the
    // game window (the window ignores the timeout 0x7EF, round 138).
    {
        const std::array<uint8_t, kPlayerSlots> slots{2, 1, 0, 0, 0, 0, 0, 0};
        Game w(board, towns, slots, 0, 1), plain(board, towns, slots, 0, 1);
        for (Game* g : {&w, &plain}) { g->setRandomEvents(false); g->triggerEvent(EventKind::Professor); }
        w.setInvestmentWindow(true);
        CHECK(w.currentPlayer() == 0 && w.isComputer(0) && w.investmentOffer() == EventKind::Professor);
        CHECK(!w.computerAdvance() && w.phase() == Game::Phase::Roll && w.investmentOffer() == EventKind::Professor);
        const double before = w.cash(0);
        CHECK(w.invest(5000) && w.cash(0) == before - 5000 && w.investmentOffer() == EventKind::None);
        CHECK(w.computerAdvance()); // the computer's turn goes on
        CHECK(plain.computerAdvance() && plain.investmentOffer() == EventKind::None); // no window: declined
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
        CHECK(g.roll() && !g.arrivalDue() && !g.pressReturn());
        const int n = g.allowance();
        for (int i = 0; i < n; ++i) CHECK(g.step(Direction::Right));
        CHECK(g.phase() == Game::Phase::Move && g.arrivalDue() && g.cash(0) == kStartingCash);
        if (n > 1) { // turning back undoes the last step, forward again
            CHECK(g.step(Direction::Left) && !g.arrivalDue() && g.step(Direction::Right) && g.arrivalDue());
        }
        CHECK(g.pressReturn() && g.phase() == Game::Phase::Arrived && g.cash(0) > kStartingCash); // blue field
        CHECK(!g.arrivalDue() && !g.pressReturn());
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

    // With one stage left the first arrival continues with the last stage. seg7:3755
    // still draws (round 49: before DS:A10B moves to the reached field), but the last
    // stage's destination is Brussels (seg7:4B89, round R3a), here field 6.
    Board threeTowns = board;
    threeTowns.fields[6].kind = 6;
    std::vector<Town> threeTownList = towns;
    threeTownList.push_back(Town{});
    threeTownList.back().x = 6;
    threeTownList.back().name = "Br\x81ssel";
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
    CHECK(stages.destination() == 6 && stages.announceMode() == 3);

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

    // Mitspieler (round R7, seg4:0804 -> seg2:5E93): only at the turn start (MAIN1 opens
    // from ONFIELD only); a computer then plays the whole turn from its card choice
    // (seg5:6BF2: state 1), exactly like a slot that was a computer from the start.
    {
        Game swap(board, towns, 2, 0, 5, 1, nullptr, nullptr, false), born(board, towns, 2, 0, 5, 1, nullptr, nullptr, false);
        swap.setRandomEvents(false);
        born.setRandomEvents(false);
        CHECK(!swap.switchDriver(2) && !swap.switchDriver(-1));     // empty slot, no slot
        CHECK(swap.switchDriver(0) && swap.isComputer(0));
        CHECK(born.setComputer(0, true));
        for (int guard = 0; guard < 50 && swap.currentPlayer() == 0; ++guard) swap.runComputerTurn();
        for (int guard = 0; guard < 50 && born.currentPlayer() == 0; ++guard) born.runComputerTurn();
        CHECK(swap.currentPlayer() == 1 && swap.phase() == Game::Phase::Roll);
        CHECK(swap.sessionRandom().state() == born.sessionRandom().state());
        CHECK(swap.playerField(0) == born.playerField(0) && swap.cash(0) == born.cash(0));
        CHECK(swap.roll() && swap.phase() == Game::Phase::Move);
        CHECK(!swap.switchDriver(0) && !swap.switchDriver(1));       // after the spin: no MAIN1
        CHECK(swap.isComputer(0) && !swap.isComputer(1));
    }
    {
        // Back to human at the computer's own turn start: the state machine is reset
        // (seg5:6CE8) and nothing is played for the slot any more.
        Game back(board, towns, 2, 0, 5, 1, nullptr, nullptr, false);
        back.setRandomEvents(false);
        CHECK(back.setComputer(0, true) && back.switchDriver(0) && !back.isComputer(0));
        CHECK(!back.runComputerTurn() && back.currentPlayer() == 0 && back.phase() == Game::Phase::Roll);
    }

    // The year balance as a phase (round 144): with a window the player change waits;
    // watching the whole show draws what the headless game draws at once, ending it
    // early skips the remaining announcements' and the closing lever's draws.
    {
        auto toBalance = [&](Game& g, bool window) {
            g.setComputer(0, true);
            g.setComputer(1, true);
            g.setBalanceWindow(window);
            for (int guard = 0; guard < 20000 && g.balanceSerial() == 0; ++guard) g.runComputerTurn();
        };
        Game headless(board, towns, 2, 40, 7, 1, nullptr, nullptr, false), watched(board, towns, 2, 40, 7, 1, nullptr, nullptr, false),
            skipped(board, towns, 2, 40, 7, 1, nullptr, nullptr, false),
            interrupted(board, towns, 2, 40, 7, 1, nullptr, nullptr, false);
        toBalance(headless, false);
        toBalance(watched, true);
        toBalance(skipped, true);
        toBalance(interrupted, true);
        CHECK(headless.phase() != Game::Phase::Balance && watched.phase() == Game::Phase::Balance);
        CHECK(watched.balanceEntries().size() == 2 && watched.balanceEntries()[0].variant == -1);
        CHECK(!watched.roll() && !watched.endTurn()); // nothing else runs while the show is on
        CHECK(!watched.balanceSpeak()); // nothing announced yet
        while (watched.balanceAnnounce()) CHECK(watched.balanceSpeak());
        CHECK(!watched.balanceSpeak() && watched.balanceSpoken() == 2);
        CHECK(watched.balanceClose() && !watched.balanceClose());
        for (size_t i = 0; i < 2; ++i)
            CHECK(watched.balanceEntries()[i].variant == headless.balanceEntries()[i].variant);
        CHECK(watched.balanceClosing() == headless.balanceClosing());
        const uint32_t afterShow = watched.sessionRandom().state();
        CHECK(watched.balanceFinish() && watched.phase() != Game::Phase::Balance && watched.currentPlayer() >= 0);
        CHECK(!watched.balanceFinish());
        // Escape after the first announcement: player 2's draw and the closing draw are skipped.
        CHECK(skipped.balanceAnnounce());
        const uint32_t afterOne = skipped.sessionRandom().state();
        CHECK(afterOne != afterShow && skipped.balanceEntries()[1].variant == -1);
        CHECK(skipped.balanceFinish() && skipped.phase() != Game::Phase::Balance);
        // Round 151: Escape after player 2's Next_ but before his pos_ / neg_: the closing
        // letter is the one after player 1 (seg8:913D), so the 4 / 5 rule of seg8:95E5..9613
        // and the draw at seg8:9605 follow the spoken results, not the announcements.
        CHECK(interrupted.balanceAnnounce() && interrupted.balanceSpeak() && interrupted.balanceAnnounce());
        CHECK(interrupted.balanceAnnounced() == 2 && interrupted.balanceSpoken() == 1);
        const uint32_t beforeClose = interrupted.sessionRandom().state();
        CHECK(interrupted.balanceClose());
        const int letter = (interrupted.balanceChoice() + 1) % 7;
        const bool drew = interrupted.sessionRandom().state() != beforeClose;
        if (letter == 4) CHECK(interrupted.balanceClosing() == 5 && !drew);
        else if (letter == 5) CHECK(interrupted.balanceClosing() == 4 && !drew);
        else CHECK(drew && (interrupted.balanceClosing() == 4 || interrupted.balanceClosing() == 5));
        CHECK(interrupted.balanceFinish());
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
