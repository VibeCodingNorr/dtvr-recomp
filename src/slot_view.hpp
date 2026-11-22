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
    void start(const std::vector<int>& reels, Uint32 now);
    bool active(Uint32 now) const;
    // The window has slid in and the reels turn (seg2:6597 returned).
    bool running(Uint32 now) const { return !reels_.empty() && now >= started_; }
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
    Uint32 started_ = 0;  // the reels start after the slide-in
    int slideOffset(Uint32 now) const; // pixels the window has moved down from y = -height
};

} // namespace dtvr
