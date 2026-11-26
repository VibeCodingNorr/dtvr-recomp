// SPDX-License-Identifier: GPL-3.0-only
// The record tables and passes of the finish ZIEL and the victory SIEG (no SDL).
#include "finale_scenes.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace dtvr;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}

uint8_t at(const PaintImage& i, int x, int y) { return i.pixels[size_t(y) * size_t(i.width) + size_t(x)]; }

} // namespace

int main() {
    // seg2:6DCB: countdown 0 advances at once; every frame then stays delay + 1 steps,
    // the index wraps at the frame count and uses up a repetition.
    {
        static const SceneRecord def{0, 0, 0, 2, {"a", "b", "c"}};
        SceneTrack t;
        t.def = &def;
        t.repeats = 1;
        t.step();
        check(t.index == 1 && t.countdown == 2, "countdown 0 advances in the first step");
        t.step();
        t.step();
        check(t.index == 1, "frame kept delay + 1 steps");
        t.step();
        check(t.index == 2, "next frame after delay + 1 steps");
        for (int i = 0; i < 3; ++i) t.step();
        check(t.index == 0 && t.repeats == 0 && t.def, "wrap uses a repetition");
        for (int i = 0; i < 9; ++i) t.step();
        check(t.def == nullptr && !t.paint().active, "no follow-up: the record ends");
    }
    // A start index beyond the count (pfahl: first 2, one frame) wraps in the first step.
    {
        FinishScene s;
        check(s.track(14).index == 2 && s.track(14).paint().frame.empty(), "pfahl starts out of range");
        s.pass();
        check(s.track(14).index == 0 && s.track(14).paint().frame == "pfahl", "pfahl after the first step");
    }
    // ZIEL's +0xD flags (seg7:2CA4..34FF): plain copies for floete, genie, happy and paar.
    {
        const FinishScene s;
        const bool keyed[16] = {true, true, false, true, false, false, false, true,
                                true, true, true,  true, true,  true,  true,  true};
        bool ok = true;
        for (size_t i = 0; i < FinishScene::kRecords; ++i) ok = ok && s.track(i).keyed == keyed[i];
        check(ok, "finish keyed flags");
        check(s.track(FinishScene::kCar).x == 460 && s.track(FinishScene::kCar).y == 244, "car start");
        const std::vector<std::string> f = FinishScene::files('M');
        check(f.size() == 16 && f[8] == "mzielcar.sbs" && f[15] == "lampe.sbs", "finish files with the car in slot 8");
        const std::vector<SceneFill>& fill = FinishScene::fills();
        check(fill.size() == 6 && fill[3].slot == 4 && fill[3].x == 0x19 && fill[3].y == 0xD6, "finish fills");
        // The fills sit at the records' own positions.
        bool same = true;
        for (const SceneFill& x : fill) same = same && s.track(x.slot).x == x.x && s.track(x.slot).y == x.y;
        check(same, "fills at the record positions");
    }
    // seg7:3546 / 3564: 2 pixels per pass; the pass in which x < -160 draws nothing.
    {
        FinishScene s;
        int passes = 0;
        while (!s.carGone() && passes < 1000) { s.pass(); ++passes; }
        check(passes == 311 && s.track(FinishScene::kCar).x == 460 - 2 * 311, "car leaves after 311 passes");
        check(s.passes() == 311, "pass counter");
    }
    // Duo starts at +0xC 1 and shows frame 2 in the first pass (round 145 rule).
    {
        FinishScene s;
        s.pass();
        check(s.records()[1].frame == "duo02", "duo first pass");
        for (int i = 0; i < 5; ++i) s.pass();
        check(s.records()[1].frame == "duo03", "duo after delay + 1 passes");
    }
    // SIEG: countdowns of seg7:7516 ff.; the winner (countdown 1) shows frame 0 first.
    {
        VictoryScene s(3, 150); // M
        check(s.track(VictoryScene::kWinner).x == 0xE4 - 39 && s.track(VictoryScene::kWinner).y == 0xCA - 150,
              "winner position");
        check(!s.track(7).paint().active && !s.track(8).paint().active, "slots 7 and 8 empty");
        check(!s.track(0).keyed && !s.track(1).keyed && !s.track(2).keyed && s.track(3).keyed && s.track(9).keyed
                  && s.track(10).keyed,
              "victory keyed flags");
        s.pass(false, false, true);
        check(s.records()[VictoryScene::kWinner].frame == "MW01", "winner holds frame 0 in pass 1");
        check(s.records()[0].frame == "ml01", "many-l (countdown 0) advances in pass 1");
        check(s.records()[1].frame == "mr00", "many-r (countdown 1) holds in pass 1");
        s.pass(false, false, true);
        check(s.records()[VictoryScene::kWinner].frame == "MW02", "winner advances in pass 2");
        check(s.records()[1].frame == "mr01", "many-r advances in pass 2");
        const std::vector<std::string> f = VictoryScene::files({3, 0, 7});
        check(f.size() == 11 && f[7] == "ziel/mzielcar.sbs" && f[8] == "sieg/m_winner.sbs" && f[10] == "sieg/v_winner.sbs",
              "victory files");
        check(VictoryScene::files({5}).size() == 9, "one place: no second and third file");
        int x = 0, y = 0;
        VictoryScene::placePosition(1, 51, 92, x, y);
        check(x == 0xB9 - 25 && y == 0xDC - 92, "second place position");
        VictoryScene::placePosition(2, 56, 103, x, y);
        check(x == 0x10F - 28 && y == 0xE3 - 103, "third place position");
        check(VictoryScene::placeFrame(7) == "VW00", "place frame name");
    }
    // seg7:7BFF..7C46: the speaker by the voice.
    {
        VictoryScene s(0, 100);
        const size_t k = VictoryScene::kSpeaker;
        s.pass(false, false, true);
        check(s.track(k).index == 1, "speaker advances while the voice plays outside a talk interval");
        s.pass(true, false, true);
        check(s.track(k).index == 0 && s.track(k).countdown == 1, "talk interval: frame 0, countdown 2 then stepped");
        s.pass(false, false, false);
        check(s.track(k).index == 1 && s.track(k).countdown == 1, "voice over: frame 1");
        s.pass(true, true, true);
        check(s.track(k).index == 1, "the cue overrides the talk interval");
        for (int i = 0; i < 100; ++i) s.pass(false, false, false);
        check(s.records()[k].frame == "rd012", "speaker held on rd012 after the voice");
    }
    // ZIEL's car with the painter: the old position is restored, nothing trails.
    {
        FinishScene s;
        PaintFrames frames;
        frames["CAR1"] = frames["CAR2"] = frames["CAR3"] = PaintImage(4, 2, 9);
        RecordPainter painter(FinishScene::kRecords);
        PaintImage backdrop(0x1E0, 0x168, 1), dib = backdrop, screen = backdrop;
        for (int i = 0; i < 3; ++i) {
            s.pass();
            painter.forget(FinishScene::kCar);
            painter.restore(backdrop, dib);
            painter.show(s.records(), frames, dib);
            painter.present(dib, screen);
        }
        const int x = s.track(FinishScene::kCar).x;
        check(at(screen, x, 244) == 9 && at(screen, x + 3, 244) == 9, "car drawn at its new place");
        check(at(screen, x + 4, 244) == 1 && at(screen, x + 5, 244) == 1, "old place restored on screen");
    }
    // forget(): a record with an unchanged frame name is drawn again.
    {
        RecordPainter painter(1);
        PaintFrames frames{{"A", PaintImage(1, 1, 5)}};
        PaintImage backdrop(4, 1, 1), dib = backdrop, screen = backdrop;
        painter.show({{true, "a", 0, 0, false}}, frames, dib);
        painter.present(dib, screen);
        dib.pixels[0] = 1;
        painter.show({{true, "a", 0, 0, false}}, frames, dib);
        check(dib.pixels[0] == 1, "same name: not drawn again");
        painter.forget(0);
        painter.forget(7); // out of range: ignored
        painter.show({{true, "a", 0, 0, false}}, frames, dib);
        check(dib.pixels[0] == 5, "forgotten name: drawn again");
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
