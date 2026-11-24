// SPDX-License-Identifier: GPL-3.0-only
// The monthly statement STAT (seg1:972F, round R1) in the game: where the player change
// waits for it, and the announcement of what a suspended change found on its way.
// Evidence in docs/formats.md "Monthly statement".
#include "game.hpp"

namespace dtvr {

void Game::announceChange(const TurnChange& change) {
    for (int p : change.satOut) messages_.push_back(playerName(p) + " setzt aus");
    for (const auto& [text, p] : change.texts) {
        notice(text, {playerArg(p)});
        if (text >= 3251 && text < 3256) notices_.back().picture = Notice::Picture::Drago; // qp.bmp (seg2:C040)
        if (text >= 1000 && text < 1010) sound(5, 15);      // Zucker: Sfx08 (seg2:095B)
        else if (text >= 1010 && text < 1020) sound(5, 20); // Schlafen: Schlaf01 (seg2:09F9)
    }
}

void Game::suspendTurn(TurnChange change) {
    // A sit-out on the way (seg2:0915 / 09B3 / B74D) shows its modal message before the
    // loop reaches the month change, so before BILANZ or STAT.
    announceChange(change);
    change.texts.clear();
    change.satOut.clear();
    balanceTurn_ = change;
    phase_ = change.statement ? Phase::Statement : Phase::Balance;
}

bool Game::openStatement(int player, bool master) {
    ++statementSerial_;
    statementPlayer_ = player; // DS:94E2: the new player (seg2:074E) or the winner
    if (!statementWindow_) return false;
    statementMaster_ = master;
    if (master) phase_ = Phase::Statement; // the victory (seg7:71C4) follows its close
    return true;
}

bool Game::statementFinish() { // seg1:972F returns when STAT is destroyed
    if (phase_ != Phase::Statement) return false;
    if (statementMaster_) { // seg2:12A8 -> seg7:71C4
        statementMaster_ = false;
        phase_ = Phase::GameOver;
        return true;
    }
    const TurnChange before = balanceTurn_;
    TurnChange rest = nextPlayer(players_, current_, calendar_, rng_, turnHooks(), before.resumePlayer, true);
    rest.monthsAdvanced += before.monthsAdvanced;
    rest.yearsEnded += before.yearsEnded;
    if (rest.suspended) {
        suspendTurn(rest);
        return true;
    }
    completeTurn(rest);
    return true;
}

} // namespace dtvr
