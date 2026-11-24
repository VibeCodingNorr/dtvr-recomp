// SPDX-License-Identifier: GPL-3.0-only
// The year balance's closing part (seg8:925C..9871) pass by pass with scripted voices.
#include "balance_closing.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace dtvr;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}

struct FakeIo : BalancePlayers::Io {
    int pass = 0, voiceStart = 0, voiceLength = 0, talk = 0;
    int nextLength = 80, resultLength = 40, hebelLength = 78, letter = 5;
    bool humans = false;
    int closes = 0;
    struct Event { int pass; std::string what; };
    std::vector<Event> events;
    void begin(int length, const std::string& what) {
        voiceStart = pass;
        voiceLength = length;
        events.push_back({pass, what});
    }
    bool voicePlaying() override { return pass - voiceStart < voiceLength; }
    bool talking() override { return voicePlaying() && pass - voiceStart < talk; }
    bool noHuman() override { return !humans; }
    void stopVoice() override { voiceLength = 0; events.push_back({pass, "stop"}); }
    void playVoice(const std::string& name) override { begin(name == "next_a_a" ? nextLength : resultLength, name); }
    void playHebel() override { begin(hebelLength, "hebel"); }
    int announce(size_t) override { return 0; }
    int close() override { ++closes; return letter; }
    int at(const std::string& what) const {
        for (const auto& e : events)
            if (e.what == what) return e.pass;
        return -1;
    }
};

} // namespace

int main() {
    {
        BalanceClosing c;
        FakeIo io;
        io.talk = 30;
        c.start(io);
        check(io.at("stop") == 0 && io.at("next_a_a") == 0, "seg8:925C stops the voice, Next_a_a starts");
        check(c.master().def == &kBilanzHold && c.leverShown() && !c.graph(), "DS:4938 and the lever (seg8:92A7, 932B)");
        int ended = -1;
        for (; io.pass < 1000 && ended < 0; ++io.pass) {
            if (c.pass(io)) ended = io.pass;
            if (io.pass == 20)
                check(std::string(c.master().name()) == "HEBEL02" && c.master().countdown == 1, "held on frame 0 while talking");
            if (io.pass == 100) check(!c.leverShown() && c.master().def == &kBilanzPull, "the pull clears the lever");
            if (io.pass == 200) // pos_ plays: DS:4938 runs (seg8:95BF), no hold in state 2
                check(c.master().def == &kBilanzHold && c.state() == 2 && !c.leverShown(), "DS:4938 while pos_ plays");
            if (io.pass == 240)
                check(c.master().def == &kBilanzPullFollow && std::string(c.master().name()) == "HEBEL03" && c.leverShown(),
                      "DS:4968 and the lever after pos_ (seg8:9673, 96F3)");
        }
        check(io.at("hebel") == 80 + 35, "Hebel01 at pull pass 35");
        check(c.graph(), "DS:8D5E = 0xA with Hebel01 (seg8:954C)");
        check(io.at("pos_f") == 115 + 78 && io.closes == 1, "pos_ after Hebel01 and frame > 13, one draw");
        check(ended == 193 + 500, "500 passes without a human from pos_ (seg8:9656, 9857)");
    }
    {
        // With a human the loop never ends by itself; with letter 4 the voice is pos_e.
        BalanceClosing c;
        FakeIo io;
        io.humans = true;
        io.letter = 4;
        c.start(io);
        bool ended = false;
        for (; io.pass < 2000; ++io.pass) ended = ended || c.pass(io);
        check(!ended && io.at("pos_e") > 0, "humans: no timeout");
    }
    {
        // A long Hebel01: pos_ waits for it even past the pull's end (follow-up DS:4968).
        BalanceClosing c;
        FakeIo io;
        io.hebelLength = 200;
        c.start(io);
        for (; io.pass < 400; ++io.pass) c.pass(io);
        check(io.at("pos_f") == 115 + 200, "pos_ after a long Hebel01 on DS:4968");
    }
    if (failures) return 1;
    std::puts("balance closing: ok");
    return 0;
}
