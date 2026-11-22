// SPDX-License-Identifier: GPL-3.0-only
// Tests for the event card deck recovered from rally.exe.
#include "cards.hpp"

#include <algorithm>
#include <cstdio>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main() {
    int copies = 0;
    for (const auto& t : cardTypes()) copies += t.copies;
    CHECK(copies == 199);
    CHECK(std::string(cardTypes()[16].name) == "Schlafen" && cardTypes()[26].handlerSegment == 7);

    CHECK(cardMove(22).reels == 2 && cardMove(25).reels == 5 && cardMove(25).maximum == 5);
    CHECK(cardMove(28).reels == 1 && cardMove(28).maximum == 1);
    CHECK(cardMove(29).fields == 1 && cardMove(34).fields == 6);
    CHECK(cardMove(0).reels == 0 && cardMove(0).fields == 0);

    Deck deck;
    CHECK(deck.size() == 199 && deck.remaining() == 199);
    CHECK(deck.typeOfSlot(0) == 0 && deck.typeOfSlot(2) == 1 && deck.typeOfSlot(198) == 40);
    CHECK(deck.typeOfSlot(-1) == -1 && deck.typeOfSlot(199) == -1);
    CHECK(deck.peek() == 0);

    // Shuffle uses rand() % n twice per swap, n * 10 swaps.
    MsRandom a(1), b(1);
    deck.shuffle(a);
    for (int i = 0; i < 199 * 20; ++i) b.rand();
    CHECK(a.state() == b.state());
    std::vector<int> seen;
    for (int i = 0; i < 199; ++i) seen.push_back(deck.peek()), deck.take();
    std::sort(seen.begin(), seen.end());
    for (int i = 0; i < 199; ++i) CHECK(seen[size_t(i)] == i);   // a permutation
    CHECK(deck.remaining() == 0 && deck.take() == Deck::kTaken && deck.peek() == Deck::kTaken);

    Deck small;
    const int first = small.take();
    CHECK(first == 0 && small.pointer() == 1 && small.remaining() == 198);

    Hand hand = emptyHand();
    for (int i = 0; i < 8; ++i) CHECK(addToHand(hand, i));
    CHECK(!addToHand(hand, 9));

    CHECK(startingCards(0) == 3 && startingCards(1) == 1 && startingCards(8) == 8 && startingCards(12) == 8);

    // Returned cards fill the deck cyclically from DS:9C94 = 0.
    {
        Deck d;
        const int a = d.take(), b = d.take();
        CHECK(d.remaining() == d.size() - 2 && d.slotAt(0) == Deck::kTaken);
        d.giveBack(b);
        CHECK(d.slotAt(0) == b && d.returnPointer() == 1 && d.remaining() == d.size() - 1);
        d.giveBack(a);
        CHECK(d.slotAt(1) == a && d.remaining() == d.size());
        // Shop offers: one per type, the first remaining position; 41 types initially.
        auto offers = cardShopOffers(d);
        CHECK(offers.size() == size_t(kCardTypes) && offers[0].type == 0 && offers[1].type == 1);
        CHECK(d.typeOfSlot(d.slotAt(offers[5].position)) == 5);
        // Swap to the draw position, then draw it.
        d.swapWithPointer(offers[5].position);
        CHECK(d.typeOfSlot(d.take()) == 5);
        CHECK(cardBuyPrice(1) == 150000 && cardSellPrice(1) == 75000 && cardBuyPrice(99) == 0);
    }

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("cards_test: OK");
    return failures != 0;
}
