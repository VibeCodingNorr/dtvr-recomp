// SPDX-License-Identifier: GPL-3.0-only
#include "music_player.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;
using namespace dtvr;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<class Work, class Done> void until(Work work, Done done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done()) {
        check(std::chrono::steady_clock::now() < deadline, "audio worker timed out");
        work();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void controls() {
    MusicPack pack;
    pack.id = "test";
    for (const char* cue : kMusicCues) pack.tracks.emplace(cue, MusicTrack{{}, 60});
    MusicPlayer player({pack}, {});
    player.interrupt("i");
    check(player.cue() == "i", "intro");
    player.nextBackground();
    check(player.cue() == "g0", "first playlist entry");
    player.stream().stop();
    player.tick();
    check(player.cue() == "g0", "stop is not natural completion");
    player.interrupt("e1");
    player.returnToBackground();
    check(player.cue() == "g0", "explicit return to background");
    player.turn(6);
    check(player.cue() == "g1", "six player changes advance background");
    player.turn(6);
    check(player.cue() == "g1", "same counter cannot retrigger");
    player.setEnabled(false);
    player.tick();
    check(player.cue().empty(), "disable clears current music");
    player.setEnabled(true);
    check(player.cue() == "g2", "enabling selects a new background");
    player.nextBackground(); player.nextBackground(); player.nextBackground(); player.nextBackground();
    check(player.cue() == "g0", "playlist wraps");
    check(musicForNotice(5021) == "e2" && musicForNotice(5483) == "e8", "event mapping");
    check(musicForNotice(5141).empty(), "no invented drought/e10 cue");
    check(validMusicPackId("1-gm") && !validMusicPackId("../bank"), "pack identity");
}

void decoding(const fs::path& root) {
    std::vector<std::string> issues;
    auto packs = discoverMusicPacks(root, issues);
    check(packs.size() == 2 && issues.size() == 4, "reject incomplete, linked, corrupt and unsafe packs");
    const auto pack = readMusicPack(root / "tone");
    const auto track = pack.tracks.at("g0");
    MusicStream stream;
    stream.load(track);
    std::array<int16_t, 512> buffer{};
    bool audible = false;
    until([&] {
        stream.render(buffer.data(), buffer.size());
        audible |= std::any_of(buffer.begin(), buffer.end(), [](auto n) { return n != 0; });
    }, [&] { return stream.status().position > 0.025; });
    check(audible, "decoded generated tone must contain PCM");
    stream.pause(true);
    const double before = stream.status().position;
    for (int i = 0; i < 20; ++i) stream.render(buffer.data(), buffer.size());
    check(stream.status().position == before, "pause preserves exact position");
    check(std::all_of(buffer.begin(), buffer.end(), [](auto n) { return n == 0; }), "pause produces silence");
    stream.pause(false);
    stream.volume(0);
    until([&] {
        stream.render(buffer.data(), buffer.size());
        check(std::all_of(buffer.begin(), buffer.end(), [](auto n) { return n == 0; }), "mute emits silence");
    }, [&] { return stream.status().finished; });
    check(std::abs(stream.status().position - 0.12) < 0.0001, "completion uses nominal end without synth tail");
    stream.stop();
    check(!stream.status().finished, "stopping cannot emit completion");
    stream.volume(100);
    stream.load(track, 0.04);
    until([&] { stream.render(buffer.data(), buffer.size()); }, [&] { return stream.status().finished; });
    check(stream.status().error.empty(), "seek/resume decoding");
    stream.load({root / "nonexistent.ogg", 1});
    until([] {}, [&] { return !stream.status().error.empty(); });
    stream.stop();
    stream.load(track);
    until([&] { stream.render(buffer.data(), buffer.size()); }, [&] { return stream.status().finished; });
    check(stream.status().error.empty(), "decoder recovers after an error/cancellation");

    MusicPlayer player(packs, {"tone", 70});
    player.interrupt("g0");
    player.choosePack("dry");
    check(player.activePack() == "tone", "pack change is deferred");
    until([&] { player.stream().render(buffer.data(), buffer.size()); }, [&] { return player.stream().status().finished; });
    player.tick();
    check(player.activePack() == "dry" && player.cue() == "g0", "natural completion applies selected pack");
    player.interrupt("e1");
    player.setFocused(false);
    const double suspended = player.stream().status().position;
    for (int i = 0; i < 10; ++i) { player.stream().render(buffer.data(), buffer.size()); player.tick(); }
    check(player.cue() == "e1" && player.stream().status().position == suspended, "focus pause retains interrupted cue");
    player.setFocused(true);
    player.returnToBackground();
    check(player.cue() == "g0", "resume remembered background");

    const auto config = root.parent_path() / "preferences/music.ini";
    std::string error;
    check(saveMusicPreferences(config, {"tone", 35}, error), "write local preferences");
    const auto restored = loadMusicPreferences(config);
    check(restored.pack == "tone" && restored.volume == 35, "restore local preferences");
}

int main(int argc, char** argv) {
    try {
        controls();
        if (argc > 1) decoding(argv[1]);
        std::cout << "Music control/stream checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
