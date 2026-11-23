// SPDX-License-Identifier: GPL-3.0-only
// The slot machine window SLOT (seg2:6426 / 6855) over the map: the housing
// bmp/slot<N>.bmp, reels spinning with drehung1-4 and stopping one after another
// on walze_<v>h / walze_<v>, and the running sum as count_<d> (seg2:7C0A).
// Cosmetic presentation: the result comes from the game (round 23).
#pragma once

#include "layout.hpp"
#include "sdl_assets.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace dtvr {

class SlotView {
public:
    SlotView(SDL_Renderer* renderer, const std::filesystem::path& root);
    // Starts the animation for a new spin (reel values 1..6, one to five reels).
    // `computer`: the window slides out by itself after the count (seg2:664E);
    // otherwise it waits for key().
    void start(const std::vector<int>& reels, Uint32 now, bool computer);
    // Enter / Space (seg2:6B4A, for every player): first the lever is pulled (seg2:6A09,
    // returns 1), while the reels turn they stop at once (seg2:6AA2), after the
    // count the window slides out (seg2:6B08; both return 2); 0 when unused.
    int key(Uint32 now);
    bool active(Uint32 now) const;
    // The window has slid in and the reels turn (seg2:6597 returned).
    bool running(Uint32 now) const { return !reels_.empty() && reelStart() != kNever && now >= reelStart(); }
    // All reels show their final face (the count-up starts, seg2:7C83).
    bool stopped(Uint32 now) const;
    // Draws the window horizontally centred in the map window: created at y = -height
    // (seg2:6528), slid down 16 x 16 pixels (seg2:6717) before the reels run and up
    // again at the end (seg2:67B8), one step per 20 ms (seg6:0CDF(20)).
    void draw(const ScreenRect& map, Uint32 now);

    static constexpr Uint32 kTickMs = 50; // assumption: the 50 ms game timer (seg5:01EB)
    static constexpr int kSlideSteps = 16, kSlideStep = 16; // seg2:675D: 0x100 >> 4 steps of 0x10
    static constexpr Uint32 kSlideMs = 20;                  // seg6:0CDF(0x14): TimerCount milliseconds
    static constexpr Uint32 kSlideTime = kSlideSteps * kSlideMs;
    // seg2:6657: more than 0x1E passes of the loop seg2:65C4 in state 4; a pass is
    // the idle pass of seg2:1C37, taken as 20 ms (assumption: seg6:0CDF(20)).
    static constexpr Uint32 kComputerHold = 31 * 20;
    static constexpr Uint32 kNever = 0xFFFFFFFFu;
    static constexpr int kLeverPullTicks = 9 * 2;      // DS:2292: 9 frames, delay 2
    // Ticks until reel i shows its final face: (i + 3) * 2 turns of four frames at
    // two ticks each (DS:2398), then walze_<v>h for four ticks (DS:23BE ff.).
    static int stopTick(int reel) { return (reel + 3) * 2 * 4 * 2 + 4; }

private:
    SDL_Texture* frame(int reels, const std::string& name);
    SDL_Texture* housing(int reels);

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    std::map<std::string, std::vector<uint8_t>> pixels_;   // slot0.sbs frames (upper-case names)
    std::map<std::string, std::pair<int, int>> sizes_;
    std::map<std::string, Texture> textures_;               // per housing palette
    std::vector<int> reels_;
    Uint32 slidIn_ = 0;         // the window has slid in
    Uint32 lever_ = kNever;     // the lever was pulled (state 1 -> 2)
    Uint32 reelStart() const;
    bool computer_ = false;
    Uint32 stoppedEarly_ = 0;   // a human's key stopped the reels (0: not)
    Uint32 released_ = kNever;  // a human's key after the count
    Uint32 stopTime() const;
    Uint32 releaseTime() const;
    int slideOffset(Uint32 now) const; // pixels the window has moved down from y = -height
};

} // namespace dtvr
