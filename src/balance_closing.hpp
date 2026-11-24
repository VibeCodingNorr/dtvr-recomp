// SPDX-License-Identifier: GPL-3.0-only
// The closing part of the year balance BILANZ (no SDL): seg8:925C..9871, one pass at a
// time, with the show master's record 0 and the lever record 2 stepped as seg2:6DCB.
// Evidence in docs/formats.md "Year balance" (round 150).
#pragma once

#include "balance_players.hpp"

namespace dtvr {

class BalanceClosing {
public:
    // seg8:925C..93CA: seg1:38B7 stops the voice, Next_a_a starts (DS:7D85), state 0,
    // pass counter 0 (DS:A6F2 = 1 is the caller's); record 0 = DS:4938 at (0x50, 0x9F),
    // repetitions 999, frame 0, countdown 0; record 2 = the lever DS:4AE8 again.
    void start(BalancePlayers::Io& io);
    // One pass of seg8:93CF..9842 (without the input and the end tests of DS:A6F2 and
    // DS:8D5C). Returns true when the end test seg8:9857 ends the show: 500 passes since
    // the last state change and seg2:5C5A (no human).
    bool pass(BalancePlayers::Io& io);

    const ShowRecord& master() const { return master_; }
    bool leverShown() const { return lever_; }
    bool graph() const { return graph_; }    // DS:8D5E = 0xA since the closing Hebel01 (seg8:954C)
    int state() const { return state_; }     // [bp-0x6E]
    int counter() const { return counter_; } // [bp-0x6C]

private:
    ShowRecord master_;
    bool lever_ = true, graph_ = false;
    int state_ = 0, counter_ = 0;
};

} // namespace dtvr
