// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "rules.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dtvr {

// One entry of the 14-byte card definition table at DS:2EB2 in rally.exe:
// category byte, number of copies, a double value and a far pointer to the
// handler. Names follow the card pictures (bcards.sbs frame "1CARD_NN" with
// NN = type + 1) and the help texts 11001 ff. of rally_d.dat.
struct CardType {
    uint8_t category;     // 1, 2 or 4 (meaning open)
    uint8_t copies;       // copies in the deck
    double value;         // usage open (candidate: price / computer evaluation)
    uint8_t handlerSegment;
    uint16_t handlerOffset;
    const char* name;
};
constexpr int kCardTypes = 41;
const std::array<CardType, kCardTypes>& cardTypes();

// What playing a card does to the move, where rebuilt from its handler.
struct CardMove {
    int reels = 0;      // > 0: spin this many reels with maximum `maximum`
    int maximum = 5;
    int fields = 0;     // > 0: allowance set directly (seg2:7F14)
};
// Movement cards: 2-5 Walzen post WM_COMMAND 0x2711-0x2714, "1 oder 2" posts
// 0x2715 (one reel, maximum 1), "N Felder" call seg2:7F14(N). Other cards: nullopt-like
// result with all zero.
CardMove cardMove(int type);

// Rebuilt card effects (round 9). Money cards end the turn (their handlers call
// nextPlayer, seg2:06FC); status cards set a flag on the target and then post
// WM_COMMAND 0x2710, i.e. a normal spin with one reel (seg4:0A27).
enum class CardKind {
    Movement, Fiskus, Konto, Schulden, Zero, // movement and money cards
    Zucker, Blockieren, Schlafen,            // status cards
    Weihnachten, HauAb,                      // Dr. Drago / gift cards (round 10)
    Erdbeben, Naegel, Punch, Konferenz, Flugzeug, Verdoppeln, // round 13
    Property, Tschuess,                      // round 14 (cards 1-8 in the town dialog)
    Unsupported,
};
CardKind cardKind(int type);
bool cardNeedsTarget(CardKind kind);        // target chosen by seg2:51EA
uint16_t cardStatusFlag(CardKind kind);      // bit set in the target's +0x40 flags

// The event card deck (seg3:0000, seg3:01AD, seg3:0206, seg3:032D). Slots hold
// card types; `order` holds slot indices or 0xFF once taken. Taken cards never
// return to the deck.
class Deck {
public:
    static constexpr uint8_t kTaken = 0xFF;
    Deck();                                  // one slot per copy, in type order
    void shuffle(MsRandom& rng);             // n * 10 swaps of rand() % n pairs
    int size() const { return int(slots_.size()); }
    int pointer() const { return pointer_; }
    // seg3:032D(0): slot at the pointer without taking it (kTaken if empty).
    int peek() const { return order_[size_t(pointer_)]; }
    // seg3:032D(1): take the slot at the pointer and advance cyclically.
    int take();
    int typeOfSlot(int slot) const;          // seg3:0DCE; -1 for kTaken
    int remaining() const;

private:
    std::vector<uint8_t> slots_;   // DS:9EDA, card type per slot (7-byte records in the original)
    std::vector<uint8_t> order_;   // DS:9572
    int pointer_ = 0;              // DS:94FA
};

// A player's hand: 8 slots at player +0x24, 0xFF = empty (seg3:0286).
using Hand = std::array<uint8_t, 8>;
Hand emptyHand();
bool addToHand(Hand& hand, int slot); // false if all 8 slots are used (text 403)

// [Setup] defaultCards: default 1, 0 becomes 3, more than 8 becomes 8 (seg2:02C8).
int startingCards(int iniValue);

} // namespace dtvr
