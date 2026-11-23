// SPDX-License-Identifier: GPL-3.0-only
// The window INTRO of rally.exe (seg7:0000, procedure seg7:20E8; round 89): club.bmp
// with the people and the speakers of the SBS files under butt/sbs/intro, driven by
// IntroScript once per 20 ms, then the lightning (clubfoto.bmp) and the newspaper.
// The scene is composed in software as an 8-bit surface, so the palette fades of
// seg5:3483 / 3623 apply to it exactly.
#pragma once

#include "window_border.hpp"

#include "intro_script.hpp"
#include "rules.hpp"
#include "sdl_assets.hpp"
#include "sound.hpp"
#include "subtitle_view.hpp"

#include <SDL.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace dtvr {

class IntroView {
public:
    // B1-B8 of assorted.sbs for the 8-pixel border of seg6:1A68 (round 112).
    void setSprites(SpriteLookup sprites) { borderSprites_ = std::move(sprites); }
    IntroView(SDL_Renderer* renderer, const std::filesystem::path& root, char language, const int& canvasWidth, const int& canvasHeight);
    ~IntroView() { if (palette_) SDL_FreePalette(palette_); }
    IntroView(const IntroView&) = delete;
    IntroView& operator=(const IntroView&) = delete;
    // False when club.bmp is missing. `subtitles` may be null. The wipes draw from
    // `random`, the program's generator, which seg5:0029 has already seeded with the
    // time before the intro starts (seg5:0188); it must outlive the intro.
    bool start(SoundPlayer* sound, SubtitleView* subtitles, bool subtitlesOn, MsRandom& random, Uint32 now);
    bool active() const { return active_; }
    void stop();
    // Esc, Space, Enter or a click close it once the first pass ran (seg7:2118).
    bool handle(const SDL_Event& event);
    void update(Uint32 now);
    void draw();
    SDL_Rect window() const; // canvas coordinates of the INTRO window

private:
    SpriteLookup borderSprites_;
    enum class Effect { None, FadeOut, FadeIn, Wipe };
    void apply(const IntroEvent& e, Uint32 now);
    void compose();                   // backdrop + the records of the last pass
    void stamp(const std::string& frame, int x, int y);
    bool loadBackdrop(const std::filesystem::path& file);
    void showPalette(const SDL_Palette* palette);

    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    char language_; // the lan.inf letter DS:A0F0 (seg5:028C)
    const int& canvasWidth_; // the shared layout's size (live widescreen switch)
    const int& canvasHeight_;
    SoundPlayer* sound_ = nullptr;
    SubtitleView* subtitles_ = nullptr;
    bool subtitlesOn_ = false;
    IntroScript script_{nullptr};
    std::vector<VoiceSyncRecord> sync_;
    std::map<std::string, Surface> frames_;                 // upper-case name -> 8-bit frame
    std::map<std::string, std::vector<std::string>> files_; // file -> frame names in file order
    Surface backdrop_{nullptr, SDL_FreeSurface};            // DS:A0A8
    Surface scene_{nullptr, SDL_FreeSurface};
    Surface next_{nullptr, SDL_FreeSurface};                // the newspaper during the wipe
    SDL_Palette* palette_ = nullptr;                        // the bitmaps' colours
    Texture texture_{nullptr, SDL_DestroyTexture};
    std::vector<IntroFrame> shown_;
    std::vector<WipeCell> wipe_;
    MsRandom* rand_ = nullptr; // the session's generator (DS:838E)
    Effect effect_ = Effect::None;
    int effectStep_ = 0;
    Uint32 effectAt_ = 0, lastPass_ = 0;
    bool active_ = false, firstPass_ = false;
};

} // namespace dtvr
