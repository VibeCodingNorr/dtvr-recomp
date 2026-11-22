// SPDX-License-Identifier: GPL-3.0-only
#include "stage_view.hpp"

#include "scene.hpp"

#include <cctype>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 480, kHeight = 360, kWindowW = 0x1F0, kWindowH = 0x178;
constexpr int kVoiceChannel = 0;
constexpr int kLightX = 0xB0, kLightY = 0x24; // seg7:3DEF / 3E0C: field cell + offset

const SceneRecord kSpeaking{79, 155, 0, 3, {"rd014", "rd012", "rd011", "rd012", "rd014", "rd012", "rd011", "rd012",
                                           "rd011", "rd012", "rd013", "rd012", "rd013", "rd012", "rd014", "rd012"}}; // DS:4800
const SceneRecord kFoot{74, 276, 0, 999, {"red_foot"}}; // DS:4798
const SceneRecord kLever{150, 184, 0, 999, {"HEBEL"}};  // DS:4AE8
const SceneRecord kPull{80, 159, 0, 3, {"HEBEL05", "HEBEL06", "HEBEL07", "HEBEL08", "HEBEL08", "HEBEL08", "HEBEL08",
                                        "HEBEL08", "HEBEL09", "HEBEL10", "HEBEL11", "HEBEL12", "HEBEL13", "HEBEL13",
                                        "HEBEL13", "HEBEL13", "HEBEL13", "HEBEL13", "HEBEL13", "HEBEL13", "HEBEL13",
                                        "HEBEL13", "HEBEL13"}};         // DS:4A30
const SceneRecord kHold{80, 159, 0, 3, {"HEBEL02", "HEBEL03", "HEBEL02", "HEBEL03", "HEBEL04"}}; // DS:4938
const SceneRecord kLight{0, 0, 0, 1, {"licht1", "licht2"}};  // DS:4782 (delay 0: every pass)

} // namespace

StageView::StageView(SDL_Renderer* renderer, const fs::path& root, int canvasWidth, int canvasHeight,
                     const Board& board, const std::vector<Town>& towns)
    : renderer_(renderer), root_(root), canvasWidth_(canvasWidth), canvasHeight_(canvasHeight), board_(board),
      towns_(towns) {}

bool StageView::start(int mode, int variant, int destination, SoundPlayer* sound, Uint32 now) {
    active_ = false;
    ending_ = skipped_ = false;
    const fs::path butt("butt");
    Surface scene = loadBmp(findCaseInsensitive(root_, butt / "club_map.bmp")); // DS:4D19
    if (!scene || !scene->format->palette) return false;
    background_.reset(SDL_CreateTextureFromSurface(renderer_, scene.get()));
    frames_.clear();
    for (const char* file : {"ziel/hebel.sbs", "intro/red_foot.sbs", "intro/redet.sbs", "ziel/licht.sbs"})
        loadSbsTextures(renderer_, findCaseInsensitive(root_, butt / "sbs" / file), scene->format->palette, frames_, 0xFF);
    candidates_.clear();
    for (size_t i = 0; i < board_.fields.size(); ++i)
        if (board_.fields[i].kind == 6) candidates_.push_back(int(i));
    mode_ = mode;
    variant_ = variant;
    destination_ = destination;
    sound_ = sound;
    light_ = -1;
    jumps_ = wait_ = 0;
    lastPass_ = 0;
    seed_ = now | 1u;
    active_ = background_ != nullptr && destination >= 0;
    if (active_) enter(Stage::Intro, now);
    return active_;
}

void StageView::voice(const std::string& name) {
    if (sound_) sound_->playFile(kVoiceChannel, findCaseInsensitive(root_, fs::path("butt") / "d" / "voice" / "zielsuch" / (name + ".wav")));
    if (subtitles_) // seg6:64A5 below this window (0x1F0 x 0x178, centred)
        subtitles_->voice(name + ".wav", SDL_Rect{(canvasWidth_ - kWindowW) / 2, (canvasHeight_ - kWindowH) / 2, kWindowW, kWindowH},
                          SDL_GetTicks());
}

bool StageView::voiceDone(Uint32 now, Uint32 minimum) const {
    if (skipped_) return true; // seg1:3E64 reports the stopped voice as finished
    if (now - stageStart_ < minimum) return false;
    if (now - stageStart_ > 20000) return true; // a stalled audio device must not hold the scene
    return !sound_ || !sound_->busy(kVoiceChannel);
}

int StageView::passes(Uint32 now) const { return int((now - stageStart_) / kSceneStepMs); }

void StageView::jump() { // the light shows a random destination town (display only)
    if (candidates_.empty()) return;
    seed_ = seed_ * 1103515245u + 12345u;
    light_ = candidates_[(seed_ >> 16) % candidates_.size()];
}

void StageView::skip(Uint32 now) {
    if (!active_ || ending_) return;
    if (sound_) sound_->stop(kVoiceChannel); // seg5:607B(-1)
    if (mode_ == 3) {                        // loop seg7:4730..4B85
        if (stage_ == Stage::Intro) { skipped_ = true; return; } // seg7:4B38: state 0 goes on without Master1
        ending_ = true;                                          // the loop ends, 60 passes follow
        endAt_ = now + 60 * kSceneStepMs;
        return;
    }
    // Loop seg7:3BB5..46E8 (seg7:44B1): before the final part, or while Part03 plays,
    // the final part starts (state 4, seg7:46A7); later the announcement ends.
    if (stage_ != Stage::Final || queue_.size() == 2) {
        enter(Stage::Final, now);
        return;
    }
    active_ = false;
}

void StageView::enter(Stage stage, Uint32 now) {
    skipped_ = false;
    stage_ = stage;
    stageStart_ = now;
    lastPass_ = 0;
    const std::string v(1, char('a' + variant_));
    switch (stage) {
    case Stage::Intro: voice(mode_ == 3 ? "master1" : "part01" + v); break; // DS:4D3A / 4DF0
    case Stage::Pull: break;
    case Stage::Search: if (mode_ != 3) voice("part02" + v); break;       // DS:4D6E
    case Stage::Final: {
        light_ = destination_; // seg7:3F43: DS:A10D = the destination chosen first
        int town = -1;
        const Field& f = board_.fields[size_t(destination_)];
        for (size_t t = 0; t < towns_.size(); ++t)
            if (towns_[t].x == f.x && towns_[t].y == f.y) town = int(t);
        std::string name = town >= 0 ? finishPhotoName(towns_[size_t(town)].name) : "";
        for (char& c : name) c = char(std::tolower(static_cast<unsigned char>(c)));
        if (mode_ == 3) queue_ = {"master2"};                             // DS:4E2A
        else queue_ = {"part03" + v, name, "part04" + v};                 // DS:4D90, 4DB2, 4DCE
        voice(queue_.front());
        queue_.erase(queue_.begin());
        break;
    }
    }
}

void StageView::update(Uint32 now) {
    if (!active_) return;
    if (ending_) {
        if (int(now - endAt_) >= 0) active_ = false;
        return;
    }
    const int pass = passes(now);
    if (stage_ == Stage::Pull || stage_ == Stage::Search) {
        // seg7:3D35: until the 20th jump every 4 passes, then slower (wait > jumps - 16).
        for (; lastPass_ < pass; ++lastPass_) {
            if (jumps_ == 0) continue;
            ++wait_;
            if ((wait_ > 3 && jumps_ < 20) || (jumps_ >= 20 && jumps_ - 16 < wait_)) {
                wait_ = 0;
                ++jumps_;
                jump();
            }
        }
    }
    switch (stage_) {
    case Stage::Intro:
        if (voiceDone(now, 1500)) enter(Stage::Pull, now);
        break;
    case Stage::Pull:
        if (jumps_ == 0 && pass >= 9 * kPull.delay) { // seg7:3CE4: frame 9 starts the search
            jumps_ = 1;
            jump();
            if (sound_) sound_->play(2, 4); // Hebel01 (DS:4D5C)
        }
        if (pass >= int(kPull.frames.size()) * kPull.delay) {
            const int keepJumps = jumps_, keepWait = wait_;
            enter(Stage::Search, now);
            jumps_ = keepJumps;
            wait_ = keepWait;
        }
        break;
    case Stage::Search: // seg7:3F28: when Part02 has played out
        if (voiceDone(now, 3000)) enter(Stage::Final, now);
        break;
    case Stage::Final:
        if (voiceDone(now, 800)) {
            if (queue_.empty()) active_ = false;
            else {
                stageStart_ = now;
                voice(queue_.front());
                queue_.erase(queue_.begin());
            }
        }
        break;
    }
}

void StageView::draw(Uint32 now) {
    if (!active_) return;
    const SDL_Rect window{(canvasWidth_ - kWindowW) / 2, (canvasHeight_ - kWindowH) / 2, kWindowW, kWindowH};
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, &window);
    const int ox = window.x + (kWindowW - kWidth) / 2, oy = window.y + (kWindowH - kHeight) / 2;
    const SDL_Rect area{ox, oy, kWidth, kHeight};
    SDL_RenderCopy(renderer_, background_.get(), nullptr, &area);
    SDL_RenderSetClipRect(renderer_, &area);
    const int t = passes(now);
    const SceneRecord& master = stage_ == Stage::Pull ? kPull : stage_ == Stage::Search ? kHold : kSpeaking;
    drawSceneFrame(renderer_, frames_, sceneFrame(master, t), ox + master.x, oy + master.y);
    drawSceneFrame(renderer_, frames_, sceneFrame(kFoot, t), ox + kFoot.x, oy + kFoot.y);
    if (stage_ != Stage::Pull) drawSceneFrame(renderer_, frames_, sceneFrame(kLever, t), ox + kLever.x, oy + kLever.y);
    if (light_ >= 0) {
        const Field& f = board_.fields[size_t(light_)];
        drawSceneFrame(renderer_, frames_, sceneFrame(kLight, t), ox + f.x + kLightX, oy + f.y + kLightY);
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
}

} // namespace dtvr
