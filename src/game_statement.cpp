// SPDX-License-Identifier: GPL-3.0-only
// The monthly statement STAT (seg1:972F, round R1) in the game: where the player change
// waits for it, and the announcement of what a suspended change found on its way.
// Evidence in docs/formats.md "Monthly statement".
#include "game.hpp"

namespace dtvr {

void Game::announceSitOut(const SitOutText& t) {
    const int text = t.text, player = t.player;
    messages_.push_back(playerName(player) + " setzt aus");
    // The sound starts before the modal message opens (seg2:095B before 096F, 09F9 before 0A0D,
    // Dr. Drago's laugh seg2:C067 before C07A).
    if (text >= 1000 && text < 1010) sound(5, 15);      // Zucker: Sfx08 (seg2:095B)
    else if (text >= 1010 && text < 1020) sound(5, 20); // Schlafen: Schlaf01 (seg2:09F9)
    else if (t.sound >= 0) sound(kDragoLaughChannel, t.sound);
    notice(text, {playerArg(player)});
    if (text >= 3251 && text < 3256) notices_.back().picture = Notice::Picture::Drago; // qp.bmp (seg2:C040)
}

void Game::announceChange(const TurnChange& change) {
    // The game's TurnHooks::announce shows each text when nextPlayer reaches it
    // (seg2:096F / 0A0D before seg2:0977 / 0A15, round R3b); this covers a change
    // computed without that hook.
    for (const SitOutText& t : change.texts) announceSitOut(t);
}

void Game::suspendTurn(TurnChange change) {
    // A sit-out on the way (seg2:0915 / 09B3 / B74D) has shown its modal message before
    // the loop reached the month change, so before BILANZ or STAT.
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
