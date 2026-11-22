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
    // All reels show their final face (the count-up starts, seg2:7C83).
    bool stopped(Uint32 now) const;
    // Draws the window centred at the bottom of the map window (seg2:6528).
    void draw(const ScreenRect& map, Uint32 now);

    static constexpr Uint32 kTickMs = 50; // assumption: the 50 ms game timer (seg5:01EB)
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
    Uint32 started_ = 0;
};

} // namespace dtvr
