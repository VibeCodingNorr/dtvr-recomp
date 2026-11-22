// SPDX-License-Identifier: GPL-3.0-only
#include "cards.hpp"

#include <numeric>

namespace dtvr {

const std::array<CardType, kCardTypes>& cardTypes() {
    // Values from DS:2EB2 (41 records, terminated by 0xFF); handler segment/offset
    // from the data segment relocations.
    static const std::array<CardType, kCardTypes> table = {{
        {2, 2, 50000, 3, 0x1156, "Weihnachten"},  {1, 4, 150000, 3, 0x134D, "Verkaufen"},
        {1, 4, 100000, 3, 0x1379, "Kaufen"},      {1, 3, 150000, 3, 0x13A5, "Gold"},
        {1, 2, 500000, 3, 0x13D1, "Platin"},      {1, 4, 30000, 3, 0x13FD, "Mieten"},
        {1, 4, 100000, 3, 0x1429, "Nimm"},        {1, 6, 50000, 3, 0x1455, "Abkaufen"},
        {1, 2, 100000, 3, 0x1481, "Kredit"},      {2, 2, 100000, 3, 0x14AD, "Fiskus"},
        {2, 3, 100000, 3, 0x15B0, "Konto"},       {2, 3, 50000, 3, 0x1790, "Schulden"},
        {2, 3, 50000, 3, 0x18EA, "Zero"},         {2, 6, 20000, 3, 0x1994, "Steuerlos"},
        {2, 5, 30000, 3, 0x19E5, "Zucker"},       {2, 7, 20000, 3, 0x1AB9, "Blockieren"},
        {2, 5, 30000, 3, 0x1B0A, "Schlafen"},     {2, 3, 50000, 3, 0x1BDE, "Erdbeben"},
        {2, 8, 15000, 3, 0x1C96, "Naegel"},       {2, 6, 50000, 3, 0x28ED, "Tschuess"},
        {2, 6, 30000, 3, 0x2D17, "Punch"},        {2, 3, 100000, 3, 0x2F5F, "Konferenz"},
        {2, 9, 20000, 3, 0x30F1, "2 Walzen"},     {2, 8, 30000, 3, 0x312A, "3 Walzen"},
        {2, 5, 40000, 3, 0x3163, "4 Walzen"},     {2, 4, 50000, 3, 0x319C, "5 Walzen"},
        {2, 6, 15000, 7, 0x674D, "Risiko"},       {2, 3, 50000, 3, 0x31D5, "Flugzeug"},
        {2, 8, 10000, 3, 0x39D0, "1 oder 2"},     {2, 7, 10000, 3, 0x3A09, "1 Feld"},
        {2, 7, 10000, 3, 0x3ADC, "2 Felder"},     {2, 6, 10000, 3, 0x3BAF, "3 Felder"},
        {2, 6, 15000, 3, 0x3C82, "4 Felder"},     {2, 5, 15000, 3, 0x3D55, "5 Felder"},
        {2, 4, 15000, 3, 0x3E28, "6 Felder"},     {2, 6, 75000, 3, 0x3EFB, "Hau ab"},
        {4, 4, 20000, 3, 0x3F47, "Schneepflug"},  {4, 4, 50000, 3, 0x3F64, "Versicherung"},
        {4, 5, 20000, 3, 0x3F81, "Naegel weg"},   {4, 5, 50000, 3, 0x3F9E, "Nessie"},
        {2, 6, 75000, 3, 0x3FBB, "Verdoppeln"},
    }};
    return table;
}

CardMove cardMove(int type) {
    CardMove m;
    if (type >= 22 && type <= 25) m.reels = type - 20;           // 2..5 Walzen
    else if (type == 28) { m.reels = 1; m.maximum = 1; }          // 1 oder 2
    else if (type >= 29 && type <= 34) m.fields = type - 28;      // 1..6 Felder
    return m;
}

CardKind cardKind(int type) {
    const CardMove m = cardMove(type);
    if (m.reels || m.fields) return CardKind::Movement;
    switch (type) {
    case 9: return CardKind::Fiskus;      // handler seg3:14AD
    case 10: return CardKind::Konto;      // seg3:15B0
    case 11: return CardKind::Schulden;   // seg3:1790
    case 12: return CardKind::Zero;       // seg3:18EA
    case 14: return CardKind::Zucker;     // seg3:19E5, flag 0x02
    case 15: return CardKind::Blockieren; // seg3:1AB9, flag 0x08
    case 16: return CardKind::Schlafen;   // seg3:1B0A, flag 0x04
    case 0: return CardKind::Weihnachten; // seg3:1156
    case 35: return CardKind::HauAb;      // seg3:3EFB
    case 17: return CardKind::Erdbeben;   // seg3:1BDE
    case 18: return CardKind::Naegel;     // seg3:1C96
    case 20: return CardKind::Punch;      // seg3:2D17
    case 21: return CardKind::Konferenz;  // seg3:2F5F
    case 27: return CardKind::Flugzeug;   // seg3:31D5
    case 40: return CardKind::Verdoppeln; // seg3:3FBB
    case 19: return CardKind::Tschuess;   // seg3:28ED
    case 13: return CardKind::Steuerlos;  // seg3:1994
    case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
        return CardKind::Property;        // seg3:134D-1481: DS:3FC6 = type + 1
    default: return CardKind::Unsupported;
    }
}

bool cardNeedsTarget(CardKind kind) {
    return kind == CardKind::Fiskus || kind == CardKind::Schulden || kind == CardKind::Zucker
        || kind == CardKind::Blockieren || kind == CardKind::Schlafen || kind == CardKind::Steuerlos;
}

uint16_t cardStatusFlag(CardKind kind) {
    switch (kind) {
    case CardKind::Zucker: return 0x02;
    case CardKind::Blockieren: return 0x08;
    case CardKind::Schlafen: return 0x04;
    case CardKind::Steuerlos: return 0x20; // seg3:19C4
    default: return 0;
    }
}

Deck::Deck() {
    for (int t = 0; t < kCardTypes; ++t)
        for (int c = 0; c < cardTypes()[size_t(t)].copies; ++c) slots_.push_back(uint8_t(t));
    order_.resize(slots_.size());
    std::iota(order_.begin(), order_.end(), uint8_t(0));
}

void Deck::shuffle(MsRandom& rng) {
    const int n = size();
    if (n <= 0) return;
    for (int i = 0; i < n * 10; ++i) {
        const int a = rng.rand() % n;
        const int b = rng.rand() % n;
        std::swap(order_[size_t(a)], order_[size_t(b)]);
    }
}

int Deck::take() {
    const int slot = order_[size_t(pointer_)];
    if (slot == kTaken) return kTaken;
    order_[size_t(pointer_)] = kTaken;
    pointer_ = pointer_ >= size() - 1 ? 0 : pointer_ + 1;
    return slot;
}

void Deck::giveBack(int slot) {
    if (slot < 0 || slot >= size()) return;
    order_[size_t(returnPointer_)] = uint8_t(slot);
    returnPointer_ = returnPointer_ >= size() - 1 ? 0 : returnPointer_ + 1;
}

void Deck::swapWithPointer(int position) {
    if (position < 0 || position >= size()) return;
    std::swap(order_[size_t(position)], order_[size_t(pointer_)]);
}

std::vector<CardOffer> cardShopOffers(const Deck& deck) {
    std::vector<CardOffer> offers;
    for (int t = 0; t < kCardTypes; ++t)
        for (int i = 0; i < deck.size(); ++i)
            if (deck.slotAt(i) != Deck::kTaken && deck.typeOfSlot(deck.slotAt(i)) == t) {
                offers.push_back({i, t});
                break;
            }
    return offers;
}

double cardBuyPrice(int type) {
    return type < 0 || type >= kCardTypes ? 0 : cardTypes()[size_t(type)].value;
}

double cardSellPrice(int type) { return cardBuyPrice(type) * 0.5; }

int Deck::typeOfSlot(int slot) const {
    if (slot < 0 || slot >= size()) return -1;
    return slots_[size_t(slot)];
}

int Deck::remaining() const {
    int n = 0;
    for (uint8_t s : order_) n += s != kTaken;
    return n;
}

Hand emptyHand() {
    Hand h;
    h.fill(Deck::kTaken);
    return h;
}

bool addToHand(Hand& hand, int slot) {
    for (auto& s : hand)
        if (s == Deck::kTaken) { s = uint8_t(slot); return true; }
    return false;
}

int startingCards(int iniValue) {
    if (iniValue <= 0) return 3;
    return iniValue > 8 ? 8 : iniValue;
}

} // namespace dtvr
