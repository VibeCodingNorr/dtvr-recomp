// SPDX-License-Identifier: GPL-3.0-only
#include "balance_closing.hpp"

namespace dtvr {

void BalanceClosing::start(BalancePlayers::Io& io) {
    io.stopVoice();             // seg8:925C
    io.playVoice("next_a_a");   // seg8:9261..9292 (DS:7D85)
    state_ = 0;                 // seg8:9297
    counter_ = 0;               // seg8:929C
    master_ = ShowRecord{&kBilanzHold, nullptr, 0, 999, 0, 0x50, 0x9F}; // seg8:92A7..9323
    lever_ = true;              // seg8:932B..93B2 (countdown 0x190, repetitions 999)
    graph_ = false;
}

bool BalanceClosing::pass(BalancePlayers::Io& io) {
    // seg8:93CF sets the lever's countdown to 0x190 in every pass: its one frame stays.
    if (counter_ < 0x7530) ++counter_; // seg8:93DA
    // seg8:9408: while Next_a_a talks or after it ended, state 0 holds record 0 on frame 0.
    if ((io.talking() || !io.voicePlaying()) && state_ == 0) {
        master_.countdown = 2;
        master_.frame = 0;
    }
    // seg8:943B: Next_a_a played out: the lever pull DS:4A30, follow-up DS:4968.
    if (state_ == 0 && !io.voicePlaying()) {
        state_ = 1;
        counter_ = 0;
        master_ = ShowRecord{&kBilanzPull, &kBilanzPullFollow, 0, 0, 0, 0x50, 0x9F};
        lever_ = false; // seg8:94D7: record 2 cleared
    }
    // seg8:94E7: lever frame 9 with countdown 1: Hebel01 (DS:7DA5) through the voice path.
    if (state_ == 1 && master_.def == &kBilanzPull && master_.frame == 9 && master_.countdown == 1) {
        io.playHebel();
        graph_ = true; // seg8:954C: DS:8D5E = 0xA, InvalidateRect / UpdateWindow
    }
    // seg8:956A: Hebel01 ended and lever frame > 13 (or the follow-up): the closing voice.
    if (state_ == 1 && !io.voicePlaying() &&
        ((master_.def == &kBilanzPull && master_.frame > 13) || master_.def == &kBilanzPullFollow)) {
        master_.def = &kBilanzHold; // seg8:95BF: frame 0, countdown 0, repetitions 999
        master_.countdown = 0;
        master_.frame = 0;
        master_.repeats = 999;
        int letter = io.close(); // seg8:95E5..9613: the letter after the last player, 4 <-> 5 or rand() % 2 + 4
        if (letter < 0) letter = 4;
        io.playVoice(std::string("pos_") + char('a' + letter)); // DS:7DB7, lower case (seg8:9619)
        state_ = 2;   // seg8:9653
        counter_ = 0; // seg8:9656
    }
    // seg8:965B: pos_ ended: record 0 = DS:4968 and the lever record again, in every pass.
    if (state_ == 2 && !io.voicePlaying()) {
        master_ = ShowRecord{&kBilanzPullFollow, nullptr, 0, 999, 0, 0x50, 0x9F}; // seg8:9673..96EB
        lever_ = true;                                                             // seg8:96F3..977A
    }
    master_.step(); // seg8:97C1
    return counter_ >= 500 && io.noHuman(); // seg8:9857 (DS:A6F2 and DS:8D5C are the caller's)
}

} // namespace dtvr
