// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <vector>

namespace dtvr {

class Game;

// What a click on an enabled card in the card window leads to (no SDL). The card
// window procedure sends every enabled card through the same dispatcher, whatever
// the mask: seg4:0A8D calls seg3:0E11(slot + 0x78, 1) for mask 1 (town dialog,
// seg4:0AA8) and mask 2 (before spinning, seg4:0AE6). The dispatcher uses the card
// up (seg3:0E94 -> seg2:3C62) and calls the card's handler from DS:2EBC
// (seg3:0EDB); nothing there buys a town property.
struct CardWindowChoice {
    enum class Step {
        None,         // a target card without any possible target: nothing happens
        Discard,      // seg3:1057: a full hand on a yellow field drops the card
        ChooseTarget, // seg2:51EA: SELPLAYER with `candidates`
        Play,         // Game::playCard(slot, target) at once
        Nails,        // seg3:1C96: the direction window
        Flight,       // seg3:31D5: the booking window
    };
    Step step = Step::None;
    int target = -1;             // Play: the argument for Game::playCard
    std::vector<int> candidates; // ChooseTarget: the players SELPLAYER offers
};

CardWindowChoice cardWindowChoice(const Game& game, int slot);

} // namespace dtvr
