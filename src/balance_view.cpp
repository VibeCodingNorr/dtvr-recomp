// SPDX-License-Identifier: GPL-3.0-only
#include "balance_view.hpp"

#include "scene.hpp"

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 480, kHeight = 360, kWindowW = 0x1F0, kWindowH = 0x178;
constexpr int kVoiceChannel = 0;

// Definitions of seg8:8A48 ff. (positions of records 0-2).
const SceneRecord kSpeaking{79, 155, 0, 3, {"rd014", "rd012", "rd011", "rd012", "rd014", "rd012", "rd011", "rd012",
                                           "rd011", "rd012", "rd013", "rd012", "rd013", "rd012", "rd014", "rd012"}}; // DS:4800
const SceneRecord kFoot{74, 276, 0, 999, {"red_foot"}};  // DS:4798
const SceneRecord kLever{150, 184, 0, 999, {"HEBEL"}};   // DS:4AE8
const SceneRecord kPull{80, 159, 0, 3, {"HEBEL05", "HEBEL06", "HEBEL07", "HEBEL08", "HEBEL08", "HEBEL08", "HEBEL08",
                                        "HEBEL08", "HEBEL09", "HEBEL10", "HEBEL11", "HEBEL12", "HEBEL13", "HEBEL13",
                                        "HEBEL13", "HEBEL13", "HEBEL13", "HEBEL13", "HEBEL13", "HEBEL13", "HEBEL13",
                                        "HEBEL13", "HEBEL13"}};         // DS:4A30
const SceneRecord kHold{80, 159, 0, 3, {"HEBEL02", "HEBEL03", "HEBEL02", "HEBEL03", "HEBEL04"}}; // DS:4938

} // namespace

BalanceView::BalanceView(SDL_Renderer* renderer, const fs::path& root, int canvasWidth, int canvasHeight)
    : renderer_(renderer), root_(root), canvasWidth_(canvasWidth), canvasHeight_(canvasHeight) {}

bool BalanceView::start(int intro, const std::vector<Game::BalanceEntry>& entries, int closing, bool humans,
                        SoundPlayer* sound, Uint32 now) {
    active_ = false;
    const fs::path butt("butt");
    Surface scene = loadBmp(findCaseInsensitive(root_, butt / "bil_leer.bmp")); // DS:7CDA
    if (!scene || !scene->format->palette) return false;                    // seg8:89C0: no window
    background_.reset(SDL_CreateTextureFromSurface(renderer_, scene.get()));
    frames_.clear();
    for (const char* file : {"ziel/hebel.sbs", "intro/red_foot.sbs", "intro/redet.sbs"}) // DS:7C4A ff.
        loadSbsTextures(renderer_, findCaseInsensitive(root_, butt / "sbs" / file), scene->format->palette, frames_, 0xFF);
    entries_ = entries;
    intro_ = intro;
    closing_ = closing;
    humans_ = humans;
    skipped_ = false;
    sound_ = sound;
    index_ = 0;
    clicked_ = false;
    started_ = now;
    active_ = background_ != nullptr;
    if (active_) enter(Stage::Intro, now);
    return active_;
}

void BalanceView::voice(const std::string& name) {
    if (sound_) sound_->playVoice(kVoiceChannel, findCaseInsensitive(root_, fs::path("butt") / "d" / "voice" / "bilanz" / (name + ".wav")));
    if (subtitles_) // seg6:64A5 below this window (0x1F0 x 0x178, centred)
        subtitles_->voice(name + ".wav", SDL_Rect{(canvasWidth_ - kWindowW) / 2, (canvasHeight_ - kWindowH) / 2, kWindowW, kWindowH},
                          SDL_GetTicks());
}

bool BalanceView::voiceDone(Uint32 now, Uint32 minimum) const {
    if (skipped_) return true;
    if (now - stageStart_ < minimum) return false;
    if (now - stageStart_ > 20000) return true; // a stalled audio device must not hold the scene
    return !sound_ || !sound_->busy(kVoiceChannel);
}

void BalanceView::click() {
    if (!active_) return;
    if (closingPart()) { active_ = false; return; } // seg8:986C
    if (stage_ == Stage::Intro && !voiceDone(SDL_GetTicks(), 0)) { // seg8:8D17: stop the opening voice
        if (sound_) sound_->stop(kVoiceChannel);
        skipped_ = true;
        return;
    }
    clicked_ = true; // kept until the show master goes on (seg8:8E1A)
}

void BalanceView::escape(Uint32 now) {
    if (!active_) return;
    if (closingPart()) { active_ = false; return; } // seg8:984E
    if (sound_) sound_->stop(kVoiceChannel);        // seg8:925C: seg1:38B7
    enter(Stage::Closing, now);
}

void BalanceView::enter(Stage stage, Uint32 now) {
    stage_ = stage;
    stageStart_ = now;
    skipped_ = false;
    const char a = 'a';
    switch (stage) {
    case Stage::Intro: voice(std::string("bilanz_") + char(a + intro_)); break; // seg8:8C74
    case Stage::Pull: if (sound_) sound_->play(2, 4); break;                    // Hebel01 (DS:7D37)
    case Stage::Next: // seg8:8E55: Next_<A + rand() % 4>_<A + player index>
        voice(std::string("next_") + char(a + entries_[index_].variant) + "_" + char(a + int(index_)));
        break;
    case Stage::Result: // seg8:90FF / 912A
        voice(std::string(entries_[index_].positive ? "pos_" : "neg_") + char(a + entries_[index_].letter));
        break;
    case Stage::Wait: break;
    case Stage::Closing: clicked_ = false; voice("next_a_a"); break;           // DS:7D85
    case Stage::ClosingPull: if (sound_) sound_->play(2, 4); break;            // Hebel01 (DS:7DA5)
    case Stage::ClosingResult: voice(std::string("pos_") + char(a + closing_)); break; // DS:7DB7, seg8:9616
    case Stage::ClosingWait: break;
    }
}

void BalanceView::update(Uint32 now) {
    if (!active_) return;
    switch (stage_) {
    case Stage::Intro:
        if (voiceDone(now, 1500)) {
            if (entries_.empty()) active_ = false;
            else enter(Stage::Pull, now); // the first player follows at once (seg8:8DFD)
        }
        break;
    case Stage::Wait: // seg8:8DF4: a click, or 300 passes (6 s) without a human
        if (clicked_ || (!humans_ && now - stageStart_ > 300 * kSceneStepMs)) {
            clicked_ = false;                                          // seg8:8E1A
            enter(index_ >= entries_.size() ? Stage::Closing : Stage::Pull, now); // seg8:8E20
        }
        break;
    case Stage::Pull:
        if (now - stageStart_ >= Uint32(kPull.frames.size() * kPull.delay) * kSceneStepMs) enter(Stage::Next, now);
        break;
    case Stage::Next:
        if (voiceDone(now, 1500)) enter(Stage::Result, now);
        break;
    case Stage::Result:
        if (voiceDone(now, 1500)) {
            ++index_;
            enter(Stage::Wait, now); // after the last player a click leads to the closing part
        }
        break;
    case Stage::Closing: // seg8:943B: Next_a_a played out, the lever follows
        if (voiceDone(now, 1500)) enter(Stage::ClosingPull, now);
        break;
    case Stage::ClosingPull:
        if (now - stageStart_ >= Uint32(kPull.frames.size() * kPull.delay) * kSceneStepMs) enter(Stage::ClosingResult, now);
        break;
    case Stage::ClosingResult:
        if (voiceDone(now, 1500)) enter(Stage::ClosingWait, now);
        [[fallthrough]];
    case Stage::ClosingWait: // seg8:9857: 500 passes without a human since the pos_ voice
        if (!humans_ && now - stageStart_ >= 500 * kSceneStepMs) active_ = false;
        break;
    }
}

void BalanceView::draw(Uint32 now) {
    if (!active_) return;
    const SDL_Rect window{(canvasWidth_ - kWindowW) / 2, (canvasHeight_ - kWindowH) / 2, kWindowW, kWindowH};
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, &window);
    const int ox = window.x + (kWindowW - kWidth) / 2, oy = window.y + (kWindowH - kHeight) / 2;
    const SDL_Rect area{ox, oy, kWidth, kHeight};
    SDL_RenderCopy(renderer_, background_.get(), nullptr, &area);
    SDL_RenderSetClipRect(renderer_, &area);
    const int t = int((now - stageStart_) / kSceneStepMs);
    const SceneRecord& master = (stage_ == Stage::Pull || stage_ == Stage::ClosingPull) ? kPull
                                : (stage_ == Stage::Wait || stage_ == Stage::ClosingWait) ? kHold
                                                                                         : kSpeaking;
    drawSceneFrame(renderer_, frames_, sceneFrame(master, t), ox + master.x, oy + master.y);
    drawSceneFrame(renderer_, frames_, sceneFrame(kFoot, t), ox + kFoot.x, oy + kFoot.y);
    if (stage_ != Stage::Pull && stage_ != Stage::ClosingPull) drawSceneFrame(renderer_, frames_, sceneFrame(kLever, t), ox + kLever.x, oy + kLever.y);
    SDL_RenderSetClipRect(renderer_, nullptr);
}

} // namespace dtvr
