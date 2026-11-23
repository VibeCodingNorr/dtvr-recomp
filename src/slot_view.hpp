// SPDX-License-Identifier: GPL-3.0-only
// The slot machine window SLOT (seg2:6426 / 6855) over the map: the housing
// bmp/slot<N>.bmp and the animation records of src/slot_records.hpp (lever, reels,
// sum display and the gears of three to five reels), played pass by pass from the
// lever pull. Cosmetic presentation: the result comes from the game (round 23).
#pragma once

#include "layout.hpp"
#include "sdl_assets.hpp"
#include "slot_records.hpp"

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
    // Enter / Space (seg2:6B4A, for every player): 1 the lever is pulled (seg2:6A09),
    // 3 the turning reels end their turn (seg2:6AA2; both play sound 2 on channel 1),
    // 2 after the count the window slides out (seg2:6B08); 0 when unused.
    int key(Uint32 now);
    // seg2:69AD: Escape / Backspace (and the right button) close the window in state 1
    // or 2 (DS:A780 <= 2: before the lever, or turning without a stop key). The game
    // decides whether the spin may be taken back (Würfeln of a human).
    bool cancellable(Uint32 now) const;
    void close() { reels_.clear(); } // DestroyWindow at once, no slide (seg2:69F6)
    bool active(Uint32 now) const;
    // The lever's callback seg2:7671 has set the reels turning.
    bool running(Uint32 now) const { return !reels_.empty() && reelStart() != kNever && now >= reelStart(); }
    // The last reel has stopped and the sum is shown (state 4, seg2:7EAC).
    bool stopped(Uint32 now) const;
    // Draws the window horizontally centred in the map window: created at y = -height
    // (seg2:6528), slid down 16 x 16 pixels (seg2:6717) before the reels run and up
    // again at the end (seg2:67B8), one step per 20 ms (seg6:0CDF(20)).
    void draw(const ScreenRect& map, Uint32 now);

    // One tick is one pass of the loop seg2:65C4, which advances the records once
    // (seg2:65F4 -> 6DCB) and ends in the idle pass seg2:1C37 with seg6:0CDF(20): at
    // least 20 ms by TOOLHELP.TimerCount (round 117; earlier taken as 50 ms).
    static constexpr Uint32 kTickMs = 20;
    static constexpr int kSlideSteps = 16, kSlideStep = 16; // seg2:675D: 0x100 >> 4 steps of 0x10
    static constexpr Uint32 kSlideMs = 20;                  // seg6:0CDF(0x14): TimerCount milliseconds
    static constexpr Uint32 kSlideTime = kSlideSteps * kSlideMs;
    // seg2:6657: more than 0x1E passes of the loop seg2:65C4 in state 4.
    static constexpr Uint32 kComputerHold = 31 * kTickMs;
    static constexpr Uint32 kNever = 0xFFFFFFFFu;

private:
    SDL_Texture* frame(int reels, const std::string& name);
    SDL_Texture* housing(int reels);
    void loadFrames(const std::filesystem::path& relative); // into pixels_ / sizes_
    int passAt(Uint32 now) const { return int((now - lever_) / kTickMs); } // the lever's pass is 0
    // Plays the records from the lever pull up to pass `last`, with the stop key.
    void play(SlotRecords& records, int last) const;
    void recompute(); // startPass_ / stopPass_ for the current spin

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    std::map<std::string, std::vector<uint8_t>> pixels_;   // slot0 / slot3-5.sbs frames (upper-case names)
    std::map<std::string, std::pair<int, int>> sizes_;
    std::map<std::string, Texture> textures_;               // per housing palette
    std::vector<int> reels_;
    Uint32 slidIn_ = 0;         // the window has slid in
    Uint32 lever_ = kNever;     // the lever was pulled (state 1 -> 2): pass 0
    int stopKeyPass_ = -1;      // the pass of a key in state 2 (seg2:6AA2), -1 none
    int startPass_ = -1, stopPass_ = -1; // seg2:7671 ran / state 4 reached
    bool computer_ = false;
    Uint32 released_ = kNever;  // a human's key after the count
    Uint32 reelStart() const;
    Uint32 stopTime() const;
    Uint32 releaseTime() const;
    int slideOffset(Uint32 now) const; // pixels the window has moved down from y = -height
};

} // namespace dtvr
