// SPDX-License-Identifier: GPL-3.0-only
// The window SUBTITLE of rally.exe (seg6:64A5 / 6596 / 6554, procedure seg6:661F):
// with [Setup] Subtitle a strip below the intro, ZIEL, Sieg and BILANZ windows shows
// the language text of the voice playing, timed by dat/<lang>/sync.dat.
#pragma once

#include "sdl_assets.hpp"
#include "text.hpp"
#include "voice_sync.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace dtvr {

class SubtitleView {
public:
    SubtitleView(SDL_Renderer* renderer, const std::filesystem::path& root, TextRenderer& text,
                 const std::map<int, std::string>& texts);
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool enabled() const { return enabled_; }
    // A voice starts below the window `owner` (canvas coordinates, GetWindowRect).
    void voice(const std::string& file, const SDL_Rect& owner, Uint32 now);
    void close(); // seg6:6554
    void update(Uint32 now);
    void draw();

private:
    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    TextRenderer& text_;
    const std::map<int, std::string>& texts_;
    std::vector<VoiceSyncRecord> table_;
    Texture paper_{nullptr, SDL_DestroyTexture};
    SubtitleTrack track_;
    SDL_Rect rect_{0, 0, 0, 0};
    bool enabled_ = false, open_ = false;
};

} // namespace dtvr
