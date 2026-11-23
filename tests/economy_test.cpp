// SPDX-License-Identifier: GPL-3.0-only
// Tests for the property economy recovered from rally.exe.
#include "economy.hpp"

#include <cstdio>
#include <stdexcept>

using namespace dtvr;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// Two towns: town 0 with entries 0 (value 100000, factor 4) and 1 (value 20000,
// factor 2); town 1 with one entry (value 50000, factor 1).
static std::vector<Town> sampleTowns() {
    std::vector<Town> towns(2);
    towns[0].name = "London";
    towns[0].entries[0].name = "Cafe";   towns[0].entries[0].value = 100000; towns[0].entries[0].words[0] = 4;
    towns[0].entries[1].name = "Hotel";  towns[0].entries[1].value = 20000;  towns[0].entries[1].words[0] = 2;
    towns[1].name = "Rom";
    towns[1].entries[0].name = "Museum"; towns[1].entries[0].value = 50000;  towns[1].entries[0].words[0] = 1;
    return towns;
}

int main() {
    const auto towns = sampleTowns();
    bool threw = false;
    try { Economy e(towns, 0); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);

    Economy eco(towns, 3);
    CHECK(eco.money(0).cash == 30000.0);
    CHECK(eco.entryExists(0, 1) && !eco.entryExists(0, 2) && !eco.entryExists(5, 0) && !eco.entryExists(0, 10));
    CHECK(eco.price(0, 0, BuyMode::Normal) == 100000 && eco.price(0, 0, BuyMode::Discount75) == 75000);
    CHECK(eco.price(0, 0, BuyMode::Discount50) == 50000 && eco.price(0, 0, BuyMode::Discount10) == 10000);

    // Normal purchase needs enough cash; discount modes are cheaper.
    CHECK(!eco.buy(0, 0, 0, BuyMode::Normal));
    CHECK(eco.buy(0, 0, 1, BuyMode::Normal) && eco.money(0).cash == 10000);
    CHECK(!eco.buy(1, 0, 1, BuyMode::Normal));                    // already owned
    CHECK(eco.buy(0, 0, 0, BuyMode::Discount10) && eco.money(0).cash == 0);
    CHECK(eco.town(0).wholeOwner == 0);                           // all entries of town 0

    // Monthly income: value * 0.001 * factor, doubled for a whole town.
    eco.settleMonth();
    const double expected = (100000 * 0.001 * 4 + 20000 * 0.001 * 2) * 2.0;
    CHECK(eco.money(0).cash == expected && eco.money(0).monthlyIncome == expected);
    CHECK(eco.money(0).propertyValue == 120000);

    // Credit: no cash check, debt recorded.
    CHECK(eco.buy(1, 1, 0, BuyMode::Credit) && eco.money(1).cash == 30000 && eco.money(1).creditDebt == 50000);

    // Rent: player 2 rents player 0's hotel for 12 months; the whole-town bonus ends.
    CHECK(!eco.buy(2, 0, 2, BuyMode::Rent) && !eco.buy(0, 0, 1, BuyMode::Rent));
    CHECK(eco.buy(2, 0, 1, BuyMode::Rent));
    CHECK(eco.town(0).entries[1].owner == 2 && eco.town(0).entries[1].landlord == 0);
    CHECK(eco.town(0).entries[1].monthsLeft == 12 && eco.town(0).wholeOwner == kNoOwner);
    const double before = eco.money(2).cash;
    eco.settleMonth();
    CHECK(eco.money(2).cash == before + 20000 * 0.001 * 2);       // renter earns
    CHECK(eco.money(0).propertyValue == 120000);                  // landlord keeps the value
    for (int m = 0; m < 11; ++m) eco.settleMonth();
    CHECK(eco.town(0).entries[1].owner == 0 && eco.town(0).entries[1].landlord == kNoOwner);

    // Take over and buy from another player.
    CHECK(eco.buy(2, 1, 0, BuyMode::Take) && eco.town(1).entries[0].owner == 2);
    const double sellerBefore = eco.money(2).cash;
    eco.addMoney(1, 100000);
    CHECK(eco.buy(1, 1, 0, BuyMode::BuyFromPlayer));
    CHECK(eco.money(2).cash == sellerBefore + 50000 && eco.town(1).entries[0].owner == 1);
    CHECK(!eco.buy(1, 1, 0, BuyMode::BuyFromPlayer));             // own entry
    // Round 147, seg1:806C -> 8176: with Abkaufen a free entry is a normal purchase at
    // its value; too little cash refuses it.
    {
        Economy free(towns, 2);
        const double start = free.money(0).cash;
        free.addMoney(0, 50000 - start - 1);
        CHECK(!free.buy(0, 1, 0, BuyMode::BuyFromPlayer) && free.town(1).entries[0].owner == kNoOwner);
        free.addMoney(0, 1);
        CHECK(free.buy(0, 1, 0, BuyMode::BuyFromPlayer) && free.town(1).entries[0].owner == 0);
        CHECK(free.money(0).cash == 0 && free.money(1).cash == start);
    }
    CHECK(!eco.buy(-1, 0, 0, BuyMode::Normal) && !eco.buy(3, 0, 0, BuyMode::Normal));

    // Selling: own, not rented entries; x1.5 with the card.
    Economy sale(towns, 2);
    sale.addMoney(0, 100000);
    CHECK(sale.buy(0, 0, 1, BuyMode::Normal));
    CHECK(!sale.sell(1, 0, 1, false) && !sale.sell(0, 0, 0, false));
    const double c0 = sale.money(0).cash;
    CHECK(sale.sell(0, 0, 1, true) && sale.money(0).cash == c0 + 30000 && sale.town(0).entries[1].owner == kNoOwner);
    CHECK(sale.buy(0, 0, 1, BuyMode::Normal) && sale.sell(0, 0, 1, false) && sale.money(0).cash == c0 + 30000);
    CHECK(sale.buy(1, 0, 1, BuyMode::Normal) && sale.buy(0, 0, 1, BuyMode::Rent));
    CHECK(!sale.sell(0, 0, 1, false) && !sale.sell(1, 0, 1, false)); // rented out

    // Credit charged at the year end, then cleared.
    Economy credit(towns, 2);
    CHECK(credit.buy(1, 1, 0, BuyMode::Credit));
    credit.chargeCredits({0, 1});
    CHECK(credit.money(1).cash == 30000 - 50000 && credit.money(1).creditDebt == 50000);
    credit.clearCredits();
    CHECK(credit.money(1).creditDebt == 0);

    // Money cards.
    Economy cards(towns, 3);
    cards.addMoney(0, 60000);                                     // 90000, 30000, 30000
    CHECK(cards.balanceAccounts({0, 1, 2}) == 50000 && cards.money(1).cash == 50000);
    CHECK(cards.balanceAccounts({1}) == 50000);                   // one player: unchanged
    CHECK(cards.fiskus(2) == 5000 && cards.money(2).cash == 45000);
    CHECK(!cards.passDebt(0, 1));                                 // only with negative cash
    cards.addMoney(0, -80000);                                    // -30000
    CHECK(cards.passDebt(0, 1) && cards.money(0).cash == 0 && cards.money(1).cash == 20000);
    cards.zero(1);
    CHECK(cards.money(1).cash == 0);

    if (failures) std::fprintf(stderr, "%d Fehler\n", failures);
    else std::puts("economy_test: OK");
    return failures != 0;
}
