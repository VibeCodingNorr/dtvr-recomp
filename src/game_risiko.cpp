// SPDX-License-Identifier: GPL-3.0-only
// Card Risiko (round 19): seg7:674D with its window procedure at seg7:6BF0.
// Evidence in docs/formats.md, "Risiko and Nebel".
#include "game.hpp"

namespace dtvr {

void Game::startRisiko() {
    // seg2:6426(1, 5, 0): one reel; the result becomes DS:A778.
    int sum = 0;
    sum = spin(1, 5);
    risiko_.steps = sum;
    phase_ = Phase::Risiko;
    messages_.push_back("Risiko: " + std::to_string(sum) + " Felder - druecken, wenn das Licht leuchtet");
    startRisikoLevel(2);
}

void Game::startRisikoLevel(int level) {
    // seg7:686E: the light starts lit with the level; the tick counter starts at
    // rand() % 2 (seg10:1DDE is rand()) after the first pause.
    risiko_.level = level;
    risiko_.light = level;
    risiko_.ticks = rng_.rand() % 2;
}

int Game::risikoTickMs() const {
    // seg7:6984: (475 - 75 * level) / 10 pauses of seg6:0CDF(10); the first pause of
    // a level is (400 - 75 * level) / 10 (seg7:6888). One pause unit is taken as 10 ms.
    return phase_ == Phase::Risiko ? (475 - 75 * risiko_.level) / 10 * 10 : 0;
}

bool Game::risikoTick() {
    messages_.clear();
    if (phase_ != Phase::Risiko) return false;
    risiko_.light = risiko_.light ? 0 : risiko_.level;          // seg7:68E0
    bool press = false;
    if (computerControls() && risiko_.ticks > 7) {              // seg7:6927
        press = rng_.rand() % 100 < 25 || risiko_.light != 0;   // seg7:692F: a guess in the dark sometimes
        if (press) risiko_.ticks = 0;
    }
    ++risiko_.ticks;
    if (press) {
        const bool acting = aiActing_;
        aiActing_ = true;
        risikoPress();
        aiActing_ = acting;
    }
    return true;
}

bool Game::risikoPress() {
    if (phase_ != Phase::Risiko || (computerControls() && !aiActing_)) return false;
    messages_.clear();
    const int light = risiko_.light;
    sound(2, light ? 24 : 23); // seg7:6A46: 0x18 while lit, 0x17 in the dark
    // seg7:6ABE: seg2:7F14(light * steps / (light - 1)); a dark light gives 0.
    risiko_.steps = light ? light * risiko_.steps / (light - 1) : 0;
    if (!light) {
        messages_.push_back("Risiko: im Dunkeln gedrueckt - alle Felder verloren (Text 41002)");
        notice(41002, {playerArg(current_)}); // seg7:6AE3
        finishRisiko();
        return true;
    }
    messages_.push_back("Risiko: " + std::to_string(risiko_.steps) + " Felder");
    if (risiko_.level >= 4) {                                   // seg7:6B3D: level 5 reached
        messages_.push_back("Risiko: alles gewonnen, viermal so viel (Text 41003)");
        notice(41003, {playerArg(current_)}); // seg7:6B52
        finishRisiko();
        return true;
    }
    startRisikoLevel(risiko_.level + 1);
    return true;
}

bool Game::risikoStop() {
    if (phase_ != Phase::Risiko || (computerControls() && !aiActing_)) return false;
    messages_.clear();
    messages_.push_back("Risiko: aufgehoert mit " + std::to_string(risiko_.steps) + " Feldern (Text 41001)");
    notice(41001, {playerArg(current_)}); // seg7:69DE
    finishRisiko();
    return true;
}

void Game::finishRisiko() {
    // seg7:6BB9: no steps left -> nextPlayer, otherwise drive (seg2:61B7).
    const int steps = risiko_.steps;
    risiko_ = RisikoState{};
    if (steps == 0) {
        phase_ = Phase::Roll;
        advanceTurn();
        return;
    }
    counter_ = MoveCounter(steps);
    phase_ = Phase::Move;
}

} // namespace dtvr
