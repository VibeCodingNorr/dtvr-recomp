// SPDX-License-Identifier: GPL-3.0-only
#include "intro_view.hpp"

#include "sbs.hpp"
#include "scene.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWindowW = 0x1F0, kWindowH = 0x178; // CreateWindow seg7:0053
constexpr int kWidth = 0x1E0, kHeight = 0x168;    // the DIB (seg7:2176)
constexpr int kVoiceChannel = 0;
constexpr Uint32 kPassMs = 20, kFadeMs = 15, kWipeMs = 20; // seg6:0CDF(20), seg5:3597(15), seg8:1440(20)
constexpr int kSubtitleLift = 22;                          // seg7:0476: seg6:160B(hwnd, 0, -22)

// DS:1240: the SBS files of the intro (loaded in WM_CREATE, seg7:2211).
const char* const kFiles[] = {"zoom",  "paar2",    "redet", "tuer",     "leute2", "bild",  "kopf",  "lampe",
                              "leute1", "paar1", "red_foot", "fate", "fate_hut", "wall1", "wall2", "sm_rlyy"};

std::vector<uint8_t> readBytes(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

// seg5:3483: every channel of entries 1..0xFE grows by 0x1E, from 0xE1 on it is 0xFF.
bool towardsWhite(SDL_Color& c) {
    bool changed = false;
    for (Uint8* v : {&c.r, &c.g, &c.b}) {
        if (*v == 0xFF) continue;
        *v = *v < 0xE1 ? Uint8(*v + 0x1E) : 0xFF;
        changed = true;
    }
    return changed;
}

// seg5:3623: back from white by 0x1E per step until the target is reached.
bool towardsTarget(SDL_Color& c, const SDL_Color& target) {
    bool changed = false;
    auto stepDown = [&changed](Uint8& v, Uint8 t) {
        if (v == t) return;
        v = v - t > 0x1E ? Uint8(v - 0x1E) : t;
        changed = true;
    };
    stepDown(c.r, target.r);
    stepDown(c.g, target.g);
    stepDown(c.b, target.b);
    return changed;
}

} // namespace

IntroView::IntroView(SDL_Renderer* renderer, const fs::path& root, int canvasWidth, int canvasHeight)
    : renderer_(renderer), root_(root), canvasWidth_(canvasWidth), canvasHeight_(canvasHeight) {}

SDL_Rect IntroView::window() const {
    return SDL_Rect{(canvasWidth_ - kWindowW) / 2, (canvasHeight_ - kWindowH) / 2 - (subtitlesOn_ ? kSubtitleLift : 0),
                    kWindowW, kWindowH};
}

bool IntroView::loadBackdrop(const fs::path& file) {
    if (file.empty()) return false;
    Surface s = loadBmp(file);
    if (!s || s->format->BitsPerPixel != 8 || s->w != kWidth || s->h != kHeight) return false;
    SDL_SetColorKey(s.get(), SDL_FALSE, 0);
    SDL_SetSurfacePalette(s.get(), palette_ ? palette_ : s->format->palette);
    backdrop_ = std::move(s);
    return true;
}

bool IntroView::start(SoundPlayer* sound, SubtitleView* subtitles, bool subtitlesOn, Uint32 now) {
    active_ = false;
    sound_ = sound;
    subtitles_ = subtitles;
    subtitlesOn_ = subtitlesOn;
    const fs::path butt("butt");
    Surface club = loadBmp(findCaseInsensitive(root_, butt / "club.bmp")); // DS:1388
    if (!club || !club->format->palette || club->w != kWidth || club->h != kHeight) return false;
    if (palette_) SDL_FreePalette(palette_);
    palette_ = SDL_AllocPalette(256);
    SDL_SetPaletteColors(palette_, club->format->palette->colors, 0, club->format->palette->ncolors);
    if (!loadBackdrop(findCaseInsensitive(root_, butt / "club.bmp"))) return false;
    frames_.clear();
    files_.clear();
    for (const char* file : kFiles) { // "%s\sbs\intro\%s" (DS:1394)
        const fs::path path = findCaseInsensitive(root_, butt / "sbs" / "intro" / (std::string(file) + ".sbs"));
        if (path.empty()) continue;
        SbsFile sbs;
        try { sbs = parseSbs(readBytes(path)); } catch (const std::exception&) { continue; }
        for (const SbsFrame& f : sbs.frames) {
            Surface s(SDL_CreateRGBSurfaceWithFormat(0, f.width, f.height, 8, SDL_PIXELFORMAT_INDEX8), SDL_FreeSurface);
            if (!s) continue;
            SDL_SetSurfacePalette(s.get(), palette_);
            for (int y = 0; y < f.height; ++y)
                std::copy_n(&f.pixels[size_t(y) * f.width], f.width, static_cast<uint8_t*>(s->pixels) + y * s->pitch);
            files_[file].push_back(upperAscii(f.name));
            frames_.insert_or_assign(upperAscii(f.name), std::move(s));
        }
    }
    scene_.reset(SDL_CreateRGBSurfaceWithFormat(0, kWidth, kHeight, 8, SDL_PIXELFORMAT_INDEX8));
    if (!scene_) return false;
    SDL_SetSurfacePalette(scene_.get(), palette_);
    sync_ = loadVoiceSync(root_);
    script_ = IntroScript(&sync_);
    rand_ = MsRandom(1); // assumption: the intro runs before any game seeds rand
    effect_ = Effect::None;
    shown_.clear();
    active_ = true;
    firstPass_ = false;
    for (const IntroEvent& e : script_.start()) apply(e, now);
    compose();
    lastPass_ = now;
    return true;
}

void IntroView::stop() {
    if (!active_) return;
    active_ = false;
    if (subtitles_) subtitles_->close(); // seg7:1CC3
    if (sound_) sound_->stop(kVoiceChannel);
}

bool IntroView::handle(const SDL_Event& event) {
    if (!active_) return false;
    if (event.type == SDL_KEYDOWN) {
        const SDL_Keycode k = event.key.keysym.sym;
        if (firstPass_ && (k == SDLK_ESCAPE || k == SDLK_SPACE || k == SDLK_RETURN || k == SDLK_KP_ENTER)) stop();
        return true;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN) { // seg7:25F6: WM_LBUTTONDOWN only (seg7:214B)
        if (firstPass_ && event.button.button == SDL_BUTTON_LEFT) stop();
        return true;
    }
    return false;
}

void IntroView::stamp(const std::string& frame, int x, int y) {
    const auto it = frames_.find(upperAscii(frame));
    if (it != frames_.end() && backdrop_) blitKeyed(it->second.get(), backdrop_.get(), x, y);
}

void IntroView::apply(const IntroEvent& e, Uint32 now) {
    switch (e.kind) {
    case IntroEvent::Voice: {
        const fs::path wav = findCaseInsensitive(root_, fs::path("butt") / "d" / "voice" / "intro" / (e.name + ".wav"));
        if (sound_) sound_->playVoice(kVoiceChannel, wav);
        if (subtitles_ && effect_ == Effect::None) subtitles_->voice(e.name + ".wav", window(), now);
        break;
    }
    case IntroEvent::StampFile:
        for (const std::string& name : files_[e.name]) stamp(name, e.x, e.y);
        break;
    case IntroEvent::StampFrame: stamp(e.name, e.x, e.y); break;
    case IntroEvent::Flash:
        if (subtitles_) subtitles_->close(); // seg7:1A21: no subtitles from here on
        subtitles_ = nullptr;
        effect_ = Effect::FadeOut;
        effectStep_ = 0;
        effectAt_ = now;
        break;
    case IntroEvent::Wipe:
        next_ = loadBmp(findCaseInsensitive(root_, fs::path("butt") / "d" / (e.name + ".bmp"))); // DS:1376
        if (!next_ || next_->format->BitsPerPixel != 8) { stop(); break; }                     // seg7:1BA4
        SDL_SetSurfacePalette(next_.get(), palette_);
        wipe_ = wipeMode4(kWidth, kHeight, rand_);
        effect_ = Effect::Wipe;
        effectStep_ = 0;
        effectAt_ = now;
        break;
    case IntroEvent::End: stop(); break;
    }
}

void IntroView::compose() {
    if (!scene_ || !backdrop_) return;
    SDL_BlitSurface(backdrop_.get(), nullptr, scene_.get(), nullptr);
    for (const IntroFrame& f : shown_) {
        const auto it = frames_.find(upperAscii(f.name));
        if (it != frames_.end()) blitKeyed(it->second.get(), scene_.get(), f.x, f.y);
    }
    if (effect_ == Effect::Wipe && next_) { // the cells revealed so far (seg8:1357)
        const size_t shown = std::min(wipe_.size(), size_t(effectStep_) * 16);
        for (size_t i = 0; i < shown; ++i) {
            SDL_Rect r{wipe_[i].left, wipe_[i].top, wipe_[i].right - wipe_[i].left, wipe_[i].bottom - wipe_[i].top};
            SDL_Rect to = r;
            SDL_BlitSurface(next_.get(), &r, scene_.get(), &to);
        }
    }
    showPalette(palette_);
}

void IntroView::showPalette(const SDL_Palette* palette) {
    SDL_SetSurfacePalette(scene_.get(), const_cast<SDL_Palette*>(palette));
    texture_.reset(SDL_CreateTextureFromSurface(renderer_, scene_.get()));
    SDL_SetSurfacePalette(scene_.get(), palette_);
}

void IntroView::update(Uint32 now) {
    if (!active_) return;
    if (subtitles_) subtitles_->update(now);
    if (effect_ == Effect::FadeOut || effect_ == Effect::FadeIn) {
        const int due = int((now - effectAt_) / kFadeMs);
        if (due <= effectStep_) return;
        effectStep_ = due;
        // Step k of the palette loop (entries 1..0xFE); the loop ends with the step
        // that changes nothing any more (seg5:359F).
        SDL_Palette* p = SDL_AllocPalette(256);
        SDL_SetPaletteColors(p, palette_->colors, 0, palette_->ncolors);
        bool changed = false;
        for (int i = 1; i < 0xFF; ++i) {
            SDL_Color c = effect_ == Effect::FadeOut ? palette_->colors[i] : SDL_Color{0xFF, 0xFF, 0xFF, 0xFF};
            for (int s = 1; s <= effectStep_; ++s) {
                const bool c2 = effect_ == Effect::FadeOut ? towardsWhite(c) : towardsTarget(c, palette_->colors[i]);
                if (s == effectStep_ && c2) changed = true;
            }
            p->colors[i] = c;
        }
        showPalette(p);
        SDL_FreePalette(p);
        if (changed) return;
        if (effect_ == Effect::FadeOut) { // seg7:1A8F: club photo behind the white palette
            if (!loadBackdrop(findCaseInsensitive(root_, fs::path("butt") / "clubfoto.bmp"))) { stop(); return; }
            shown_.clear();
            compose();
            effect_ = Effect::FadeIn;
            effectStep_ = 0;
            effectAt_ = now;
            SDL_Palette* white = SDL_AllocPalette(256);
            SDL_SetPaletteColors(white, palette_->colors, 0, palette_->ncolors);
            for (int i = 1; i < 0xFF; ++i) white->colors[i] = SDL_Color{0xFF, 0xFF, 0xFF, 0xFF};
            showPalette(white);
            SDL_FreePalette(white);
            return;
        }
        effect_ = Effect::None;
        lastPass_ = now;
        return;
    }
    if (effect_ == Effect::Wipe) {
        const int due = int((now - effectAt_) / kWipeMs) + 1; // a wait precedes every 16th cell
        if (due <= effectStep_) return;
        effectStep_ = std::min(due, 25);
        compose();
        if (effectStep_ < 25) return;
        backdrop_ = std::move(next_); // DS:A0A8 = the newspaper
        effect_ = Effect::None;
        compose();
        lastPass_ = now;
        return;
    }
    if (now - lastPass_ > 10 * kPassMs) lastPass_ = now - kPassMs; // no long catch-up after a stall
    while (active_ && effect_ == Effect::None && now - lastPass_ >= kPassMs) {
        lastPass_ += kPassMs;
        const IntroPass p = script_.pass(lastPass_);
        for (const IntroEvent& e : p.events) apply(e, now);
        if (p.draw) shown_ = p.frames;
        if (effect_ == Effect::None) compose();
        firstPass_ = true; // DS:1198
    }
}

void IntroView::draw() {
    if (!active_ || !texture_) return;
    const SDL_Rect w = window();
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, &w);
    drawWindowBorder(renderer_, borderSprites_, w, BorderSize::Thin); // seg7:259D
    const SDL_Rect area{w.x + (kWindowW - kWidth) / 2, w.y + (kWindowH - kHeight) / 2, kWidth, kHeight};
    SDL_RenderCopy(renderer_, texture_.get(), nullptr, &area);
}

} // namespace dtvr
