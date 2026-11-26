// SPDX-License-Identifier: GPL-3.0-only
#include "card_window_choice.hpp"

#include "game.hpp"

namespace dtvr {

CardWindowChoice cardWindowChoice(const Game& g, int slot) {
    using Step = CardWindowChoice::Step;
    CardWindowChoice choice;
    if (slot < 0 || slot >= 8) return choice;
    if (g.yellowCardPending()) { choice.step = Step::Discard; return choice; } // seg3:1057
    const CardKind kind = cardKind(g.deck().typeOfSlot(g.hand(g.currentPlayer())[size_t(slot)]));
    if (cardNeedsTarget(kind) || kind == CardKind::Tschuess) {
        // seg2:51EA: the player is asked when more than one target is possible
        // (excluded: the player; Tschüss excludes nobody, argument 0xFF).
        for (int p = 0; p < kPlayerSlots; ++p)
            if (g.players()[size_t(p)].status && (kind == CardKind::Tschuess || p != g.currentPlayer()))
                choice.candidates.push_back(p);
        if (choice.candidates.size() > 1) choice.step = Step::ChooseTarget;
        else if (choice.candidates.size() == 1) { choice.step = Step::Play; choice.target = choice.candidates.front(); }
        return choice;
    }
    if (kind == CardKind::Naegel && g.nailsInWater()) { // seg3:1CCF: into the water, no window
        choice.step = Step::Play;
        choice.target = 0;
        return choice;
    }
    if (kind == CardKind::Naegel) { choice.step = Step::Nails; return choice; }
    if (kind == CardKind::Flugzeug) { choice.step = Step::Flight; return choice; }
    choice.step = Step::Play; // every other card goes straight to its handler (seg3:0EDB)
    return choice;
}

} // namespace dtvr
