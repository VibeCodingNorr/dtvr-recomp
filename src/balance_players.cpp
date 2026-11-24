// SPDX-License-Identifier: GPL-3.0-only
#include "balance_players.hpp"

namespace dtvr {

const SceneRecord kBilanzSpeaking{79, 155, 0, 3, {"rd014", "rd012", "rd011", "rd012", "rd014", "rd012", "rd011", "rd012",
                                                 "rd011", "rd012", "rd013", "rd012", "rd013", "rd012", "rd014", "rd012"}};
const SceneRecord kBilanzFoot{74, 276, 0, 999, {"red_foot"}};
const SceneRecord kBilanzLever{150, 184, 0, 999, {"HEBEL"}};
const SceneRecord kBilanzPull{80, 159, 0, 3, {"HEBEL05", "HEBEL06", "HEBEL07", "HEBEL08", "HEBEL08", "HEBEL08", "HEBEL08",
                                              "HEBEL08", "HEBEL09", "HEBEL10", "HEBEL11", "HEBEL12", "HEBEL13", "HEBEL13",
                                              "HEBEL13", "HEBEL13", "HEBEL13", "HEBEL13", "HEBEL13", "HEBEL13", "HEBEL13",
                                              "HEBEL13", "HEBEL13"}};
const SceneRecord kBilanzPullFollow{80, 159, 0, 9999, {"HEBEL03", "HEBEL03", "HEBEL03"}};
const SceneRecord kBilanzHold{80, 159, 0, 3, {"HEBEL02", "HEBEL03", "HEBEL02", "HEBEL03", "HEBEL04"}};

void ShowRecord::step() {
    if (!def) return;                    // seg2:6E1B: an empty record is skipped
    const int old = countdown--;         // seg2:6E2B
    if (old > 0) return;
    countdown = def->delay;              // seg2:6E52
    ++frame;                             // seg2:6E5F
    if (int(def->frames.size()) > frame) return; // seg2:6E7C
    frame = 0;
    if (repeats > 0) { --repeats; return; } // seg2:6E9B
    def = follow;                        // seg2:6EEC: the follow-up, or the record ends (6F42)
    follow = nullptr;
}

const char* ShowRecord::name() const {
    if (!def || frame < 0 || size_t(frame) >= def->frames.size()) return "";
    return def->frames[size_t(frame)];
}

void BalancePlayers::start(const std::vector<Game::BalanceEntry>& entries) {
    entries_ = entries;
    // seg8:8A48 ff.: DS:4800 at (0x4F, 0x9B), repetitions 999, frame 0, countdown 0.
    master_ = ShowRecord{&kBilanzSpeaking, nullptr, 0, 999, 0, kBilanzSpeaking.x, kBilanzSpeaking.y};
    lever_ = true;     // seg8:8B49: record 2 = DS:4AE8
    click_ = false;    // seg8:88F2
    state_ = 0;        // seg8:8C9C
    counter_ = 0;      // seg8:8982
    board_ = -1;       // seg8:88F8
    next_ = 0;         // seg8:8CA1: the first slot in play (the entries are those slots)
}

bool BalancePlayers::pass(Io& io) {
    if (counter_ < 0x7530) ++counter_; // seg8:8CC5
    const auto phase = [this] { return state_ % 3; };
    // seg8:8D03: a click while the opening voice plays stops it.
    if (state_ == 0 && io.voicePlaying() && click_) {
        click_ = false;
        io.stopVoice();
    }
    // seg8:8D2B: while the voice talks (phase 0 or 1) record 0 is held on frame 0.
    if (io.talking() && (phase() == 1 || phase() == 0)) {
        master_.countdown = 2;
        master_.frame = 0;
    }
    // seg8:8D65: phase 0 with the voice ended: DS:4938 waits on frame 4, else frame 0.
    if (phase() == 0 && !io.voicePlaying()) {
        master_.frame = master_.def == &kBilanzHold ? 4 : 0;
        master_.countdown = 2;
    }
    // seg8:8DC8: the show master goes on after a click, at once for the first player,
    // or after 300 passes without a human.
    if (phase() == 0 && !io.voicePlaying() && (click_ || state_ == 0 || (counter_ > 300 && io.noHuman()))) {
        click_ = false;                               // seg8:8E1A
        if (next_ >= entries_.size()) return true;    // seg8:8E20: slot 8, the closing part
        Game::BalanceEntry& e = entries_[next_];
        const int v = io.announce(next_);             // seg8:8E38
        if (v >= 0) e.variant = v;
        if (e.variant < 0) e.variant = 0;
        // seg8:8E55: Next_<A + rand() % 4>_<A + state / 3> (DS:7D15)
        io.playVoice(std::string("next_") + char('a' + e.variant) + "_" + char('a' + state_ / 3));
        ++state_;
        counter_ = 0;
    }
    // seg8:8E85: phase 1 and Next_ ended: the lever pull DS:4A30, follow-up DS:4968.
    if (phase() == 1 && !io.voicePlaying()) {
        ++state_;
        counter_ = 0;
        master_ = ShowRecord{&kBilanzPull, &kBilanzPullFollow, 0, 0, 0, 0x50, 0x9F};
        lever_ = false; // seg8:8F3D: record 2 cleared
    }
    // seg8:8F47: lever frame 9 with countdown 1: Hebel01 through the voice path.
    if (phase() == 2 && master_.def == &kBilanzPull && master_.frame == 9 && master_.countdown == 1) {
        io.playHebel();
        board_ = entries_[next_].player; // seg8:8FC4: DS:8D5E = slot, InvalidateRect / UpdateWindow
    }
    // seg8:8FE2: Hebel01 ended and lever frame > 13 (or its follow-up): pos_ / neg_.
    if (phase() == 2 && !io.voicePlaying() &&
        ((master_.def == &kBilanzPull && master_.frame > 13) || master_.def == &kBilanzPullFollow)) {
        master_.def = &kBilanzHold; // seg8:904B: frame 0, countdown 0, repetitions 999
        master_.countdown = 0;
        master_.frame = 0;
        master_.repeats = 999;
        const Game::BalanceEntry& e = entries_[next_];
        // seg8:9075: pos_ (DS:7D49) or neg_ (DS:7D67) with the letter [bp-0x7E], which
        // then advances (seg8:913D); Game::balanceEntries holds both per entry.
        io.playVoice(std::string(e.positive ? "pos_" : "neg_") + char('a' + e.letter));
        io.spoken(next_);
        ++state_;
        counter_ = 0;
        ++next_; // seg8:9168: the next slot in play
    }
    master_.step(); // seg8:91C6: seg2:6DCB, then seg2:6F69 shows
    return false;
}

} // namespace dtvr
