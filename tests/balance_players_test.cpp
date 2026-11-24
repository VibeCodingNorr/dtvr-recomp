// SPDX-License-Identifier: GPL-3.0-only
// The year balance's players' part (seg8:8CC5..9258) pass by pass with scripted voices.
#include "balance_players.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace dtvr;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}

// Voices last a fixed number of passes; talking is reported for the first `talk` of them.
struct FakeIo : BalancePlayers::Io {
    int pass = 0, voiceStart = 0, voiceLength = 0, talk = 0;
    int nextLength = 60, resultLength = 40, hebelLength = 78;
    bool humans = true;
    struct Event { int pass; std::string what; };
    std::vector<Event> events;
    std::vector<size_t> announced;

    void begin(int length, const std::string& what) {
        voiceStart = pass;
        voiceLength = length;
        events.push_back({pass, what});
    }
    bool voicePlaying() override { return pass - voiceStart < voiceLength; }
    bool talking() override { return voicePlaying() && pass - voiceStart < talk; }
    bool noHuman() override { return !humans; }
    void stopVoice() override { voiceLength = 0; events.push_back({pass, "stop"}); }
    void playVoice(const std::string& name) override {
        begin(name.rfind("next_", 0) == 0 ? nextLength : resultLength, name);
    }
    void playHebel() override { begin(hebelLength, "hebel"); }
    int announce(size_t index) override {
        announced.push_back(index);
        return int(index % 4);
    }
    int at(const std::string& what) const {
        for (const auto& e : events)
            if (e.what == what) return e.pass;
        return -1;
    }
};

std::vector<Game::BalanceEntry> entries(int n) {
    std::vector<Game::BalanceEntry> out;
    for (int i = 0; i < n; ++i) out.push_back({i, 0, i % 2 == 0, (3 + i) % 7});
    return out;
}

// Runs passes until the closing part or `limit`; returns the pass that reported it (-1: none).
int run(BalancePlayers& b, FakeIo& io, int limit, int clickAt = -1) {
    for (; io.pass < limit; ++io.pass) {
        if (io.pass == clickAt) b.click();
        if (b.pass(io)) return io.pass;
    }
    return -1;
}

} // namespace

int main() {
    // seg2:6DCB on DS:4A30: frame 9 with countdown 1 at the start of pass 35, frame 14
    // from pass 53, the follow-up DS:4968 from the step of pass 88 (rounds 141 / 145).
    {
        ShowRecord r{&kBilanzPull, &kBilanzPullFollow, 0, 0, 0, 0x50, 0x9F};
        int hebel = -1, past13 = -1, follow = -1;
        for (int p = 0; p < 100; ++p) {
            if (hebel < 0 && r.def == &kBilanzPull && r.frame == 9 && r.countdown == 1) hebel = p;
            if (past13 < 0 && r.def == &kBilanzPull && r.frame > 13) past13 = p;
            r.step();
            if (follow < 0 && r.def == &kBilanzPullFollow) follow = p;
        }
        check(hebel == 35, "lever frame 9 / countdown 1 at pass 35");
        check(past13 == 53, "lever frame > 13 from pass 53");
        check(follow == 88 && std::string(r.name()) == "HEBEL03", "follow-up HEBEL03 after one run");
        ShowRecord hold{&kBilanzHold, nullptr, 0, 0, 4, 80, 159};
        hold.step();
        check(hold.def == nullptr, "a record without repetitions or follow-up ends");
    }

    // Two players, no clicks needed for the first; a human clicks for the second.
    {
        BalancePlayers b;
        FakeIo io;
        io.begin(100, "bilanz_c"); // the opening voice plays before the loop
        io.talk = 30;
        b.start(entries(2));
        check(b.master().def == &kBilanzSpeaking && b.leverShown(), "starts with DS:4800 and the lever");
        int closing = run(b, io, 400);
        check(closing < 0, "waits for a click after the last player");
        // The opening voice ends in pass 100: Next_ starts in that pass (seg8:8E55).
        check(io.at("next_a_a") == 100, "first Next_ at once after the opening voice");
        check(io.announced.size() == 1 && io.announced[0] == 0, "one draw for the first player");
        // Next_ ends in pass 160: the lever is pulled in that pass, record 2 goes.
        check(io.at("hebel") == 160 + 35, "Hebel01 at lever pass 35");
        // Hebel01 lasts 78 passes: pos_ in the first pass >= 53 after it ended.
        check(io.at("pos_d") == 195 + 78, "pos_ after Hebel01 and lever frame > 13");
        check(!b.leverShown(), "the lever record stays cleared");
        check(b.master().def == &kBilanzHold && b.state() == 3, "DS:4938 waits in state 3");
        check(std::string(b.master().name()) == "HEBEL04", "waits on frame 4 after the voice");
        // The second player needs a click.
        closing = run(b, io, 1200, 401);
        check(io.at("next_b_b") == 401, "the click lets the show master go on");
        check(io.announced.size() == 2, "the second draw at the click");
        check(io.at("neg_e") > 401, "neg_ for the second player");
        check(closing < 0 && b.next() == 2, "after the last player the show waits again");
        closing = run(b, io, 1300, 1250);
        check(closing == 1250, "a click after the last player starts the closing part");
        check(io.announced.size() == 2, "no draw for the closing part here");
    }

    // A click during the opening voice stops it; Next_ follows in the same pass.
    {
        BalancePlayers b;
        FakeIo io;
        io.begin(500, "bilanz_a");
        b.start(entries(1));
        run(b, io, 30, 20);
        check(io.at("stop") == 20 && io.at("next_a_a") == 20, "click stops the opening voice");
        check(!b.clickPending(), "the click is used up");
    }

    // A click while Next_ plays is kept and used when the next wait begins.
    {
        BalancePlayers b;
        FakeIo io;
        io.begin(10, "bilanz_a");
        b.start(entries(2));
        run(b, io, 600, 30);
        const int second = io.at("next_b_b");
        check(second > 0 && second == io.at("pos_d") + io.resultLength, "a kept click goes on as the voice ends");
    }

    // Without humans: 300 passes counted from the start of pos_ / neg_ (seg8:9163, 8E05).
    {
        BalancePlayers b;
        FakeIo io;
        io.humans = false;
        io.begin(10, "bilanz_b");
        b.start(entries(2));
        run(b, io, 1500);
        const int result = io.at("pos_d");
        check(io.at("next_b_b") == result + 301, "the computer show goes on after 300 passes");
    }

    // Record 0 is held on frame 0 while the voice talks (seg8:8D2B).
    {
        BalancePlayers b;
        FakeIo io;
        io.begin(50, "bilanz_a");
        io.talk = 50;
        b.start(entries(1));
        for (; io.pass < 40; ++io.pass) b.pass(io);
        check(std::string(b.master().name()) == "rd014" && b.master().countdown == 1, "held on frame 0 while talking");
    }

    // No player in play: the opening voice leads straight to the closing part.
    {
        BalancePlayers b;
        FakeIo io;
        io.begin(5, "bilanz_a");
        b.start({});
        check(run(b, io, 50) == 5 && io.announced.empty(), "no players: closing part at once");
    }

    if (failures) return 1;
    std::puts("balance players: ok");
    return 0;
}
