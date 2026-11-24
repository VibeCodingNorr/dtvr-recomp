// SPDX-License-Identifier: GPL-3.0-only
#include "balance_view.hpp"

#include "scene.hpp"

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 480, kHeight = 360, kWindowW = 0x1F0, kWindowH = 0x178;
constexpr int kVoiceChannel = 0;
// Closing part (round 141): the lever DS:4A30 starts in the pass that finds Next_a_a
// played out (seg8:943B) and advances in that pass, so pass t shows frame
// 1 + t / 4 (delay 3, a frame stays delay + 1 passes). The state logic of pass q sees
// the record after pass q - 1: frame 9 with countdown 1 in pass 35 plays Hebel01
// through the voice path (seg8:94E7-9547), frame > 13 holds from pass 53 (seg8:959A).
constexpr int kClosingHebelPass = 35, kClosingDrawPass = 53, kClosingFollowPass = 88;

// The record definitions (DS:4800, 4798, 4AE8, 4A30, 4938) are in balance_players.cpp.
const SceneRecord& kSpeaking = kBilanzSpeaking;
const SceneRecord& kFoot = kBilanzFoot;
const SceneRecord& kLever = kBilanzLever;
const SceneRecord& kPull = kBilanzPull;
const SceneRecord& kHold = kBilanzHold;
// A voice that never ends (a stalled audio device) must not hold the show.
constexpr int kVoicePassLimit = 20000 / int(kSceneStepMs);

} // namespace

BalanceView::BalanceView(SDL_Renderer* renderer, const fs::path& root, char language, const int& canvasWidth, const int& canvasHeight)
    : renderer_(renderer), root_(root), language_(language), canvasWidth_(canvasWidth), canvasHeight_(canvasHeight) {}

bool BalanceView::start(int intro, const std::vector<Game::BalanceEntry>& entries, int closing, bool humans,
                        SoundPlayer* sound, Uint32 now, Commands commands) {
    active_ = false;
    const fs::path butt("butt");
    Surface scene = loadBmp(findCaseInsensitive(root_, butt / "bil_leer.bmp")); // DS:7CDA
    if (!scene || !scene->format->palette) return false;                    // seg8:89C0: no window
    background_.reset(SDL_CreateTextureFromSurface(renderer_, scene.get()));
    frames_.clear();
    for (const char* file : {"ziel/hebel.sbs", "intro/red_foot.sbs", "intro/redet.sbs"}) // DS:7C4A ff.
        loadSbsTextures(renderer_, findCaseInsensitive(root_, butt / "sbs" / file), scene->format->palette, frames_, 0xFF);
    commands_ = std::move(commands);
    sync_ = loadVoiceSync(root_, language_); // dat/<c>/sync.dat: the talk intervals (seg1:4071)
    nextSync_ = findVoiceSync(sync_, "next_a_a.wav");
    intro_ = intro;
    closing_ = closing;
    humans_ = humans;
    sound_ = sound;
    started_ = now;
    active_ = background_ != nullptr;
    if (!active_) return false;
    stage_ = Stage::Players;
    stageStart_ = now;
    pass_ = 0;
    nextPass_ = now;
    voice(std::string("bilanz_") + char('a' + intro_)); // seg8:8C74, before the loop
    players_.start(entries);
    return true;
}

void BalanceView::voice(const std::string& name) {
    voiceSync_ = findVoiceSync(sync_, name + ".wav"); // seg1:3579 looks up the sync record
    voicePass_ = pass_;
    if (sound_) sound_->playVoice(kVoiceChannel, findCaseInsensitive(root_, fs::path("butt") / languageDir(language_) / "voice" / "bilanz" / (name + ".wav")));
    if (subtitles_) // seg6:64A5 below this window (0x1F0 x 0x178, centred by seg6:0000)
        subtitles_->voice(name + ".wav", SDL_Rect{(canvasWidth_ - kWindowW) / 2, (canvasHeight_ - kWindowH) / 2 - lift(), kWindowW, kWindowH},
                          SDL_GetTicks());
}

bool BalanceView::voiceDone(Uint32 now, Uint32 minimum) const {
    if (now - stageStart_ < minimum) return false;
    if (now - stageStart_ > 20000) return true; // a stalled audio device must not hold the scene
    return !sound_ || !sound_->busy(kVoiceChannel);
}

void BalanceView::click() {
    if (!active_) return;
    if (closingPart()) { active_ = false; return; } // seg8:986C
    players_.click(); // DS:8D5C: used by the next pass that looks (seg8:8D17, 8DF4)
}

void BalanceView::escape(Uint32 now) {
    if (!active_) return;
    if (closingPart()) { active_ = false; return; } // seg8:984E
    const bool pending = players_.clickPending();   // DS:8D5C survives the jump (seg8:9253)
    enter(Stage::Closing, now);                     // Next_a_a starts (seg8:928A)
    if (pending) active_ = false;                   // seg8:986C ends the first pass
}

void BalanceView::enter(Stage stage, Uint32 now) {
    stage_ = stage;
    stageStart_ = now;
    const char a = 'a';
    switch (stage) {
    case Stage::Players: break;
    case Stage::Closing: // seg8:925C stops the voice; DS:7D85; record 0 becomes DS:4938 (seg8:92A7)
        if (sound_) sound_->stop(kVoiceChannel);
        holdPass_ = -1;
        passesSeen_ = 0;
        voice("next_a_a");
        break;
    case Stage::ClosingPull: hebelPlayed_ = false; break;                      // Hebel01 follows (DS:7DA5)
    case Stage::ClosingResult: // seg8:9605 draws the closing letter at the lever
        if (commands_.close) {
            const int c = commands_.close();
            if (c >= 0) closing_ = c;
        }
        voice(std::string("pos_") + char(a + closing_)); // DS:7DB7, seg8:9616
        break;
    case Stage::ClosingWait: break;
    }
}

void BalanceView::update(Uint32 now) {
    if (!active_) return;
    switch (stage_) {
    case Stage::Players: runPlayers(now); break;
    case Stage::Closing: { // seg8:943B: Next_a_a played out, the lever follows
        // seg8:9408: hold DS:4938 on frame 0 while the voice talks or has ended.
        const int pass = int((now - stageStart_) / kSceneStepMs);
        for (; passesSeen_ <= pass; ++passesSeen_) {
            const uint32_t ms = uint32_t(passesSeen_) * kSceneStepMs;
            const bool talking = nextSync_ && voiceSyncTalking(*nextSync_, ms);
            const bool ended = nextSync_ ? !voiceSyncPlaying(*nextSync_, ms) : false;
            if (talking || ended) holdPass_ = passesSeen_;
        }
        if (voiceDone(now, 1500)) enter(Stage::ClosingPull, now);
        break;
    }
    case Stage::ClosingPull: {
        // seg8:94E7: Hebel01 (DS:7DA5) through seg1:3579, the voice path, so
        // seg1:3E64 waits for it; seg8:956A: pos_ follows (and Game::yearBalance's
        // closing rand() % 2 belongs here, seg8:9605) in the first pass from 53 on
        // in which it has played out.
        const int t = int((now - stageStart_) / kSceneStepMs);
        if (!hebelPlayed_ && t >= kClosingHebelPass) {
            hebelPlayed_ = true;
            if (sound_) sound_->playVoice(kVoiceChannel, findCaseInsensitive(root_, fs::path("snd") / "fx" / "hebel01.wav"));
        }
        const bool played = !sound_ || !sound_->busy(kVoiceChannel) || now - stageStart_ > 20000;
        if (hebelPlayed_ && t >= kClosingDrawPass && played) enter(Stage::ClosingResult, now);
        break;
    }
    case Stage::ClosingResult:
        if (voiceDone(now, 1500)) { // the pass counter keeps running from the pos_ voice
            const Uint32 start = stageStart_;
            enter(Stage::ClosingWait, now);
            stageStart_ = start;
        }
        [[fallthrough]];
    case Stage::ClosingWait: // seg8:9857: 500 passes without a human since the pos_ voice (seg8:9656)
        if (!humans_ && now - stageStart_ >= 500 * kSceneStepMs) active_ = false;
        break;
    }
}

class BalanceView::PlayersIo : public BalancePlayers::Io {
public:
    explicit PlayersIo(BalanceView& view) : v_(view) {}
    bool voicePlaying() override { // seg1:3E64; SoundPlayer::busy follows its TimerCount path too
        return v_.sound_ && v_.sound_->busy(kVoiceChannel) && v_.pass_ - v_.voicePass_ < kVoicePassLimit;
    }
    bool talking() override { // seg1:4071 with the position of the current voice
        return v_.voiceSync_ && voiceSyncTalking(*v_.voiceSync_, uint32_t(v_.pass_ - v_.voicePass_) * kSceneStepMs);
    }
    bool noHuman() override { return !v_.humans_; }
    void stopVoice() override {
        if (v_.sound_) v_.sound_->stop(kVoiceChannel);
        v_.voiceSync_ = nullptr;
    }
    void playVoice(const std::string& name) override { v_.voice(name); }
    void playHebel() override { // %s\FX\HEBEL01.wav (DS:7D37, base DS:72) through the voice path
        v_.voiceSync_ = findVoiceSync(v_.sync_, "hebel01.wav");
        v_.voicePass_ = v_.pass_;
        if (v_.sound_) v_.sound_->playVoice(kVoiceChannel, findCaseInsensitive(v_.root_, fs::path("snd") / "fx" / "hebel01.wav"));
    }
    int announce(size_t index) override { return v_.commands_.announce ? v_.commands_.announce(index) : -1; }

private:
    BalanceView& v_;
};

void BalanceView::runPlayers(Uint32 now) {
    // A pass of seg8:8CC5..9258 takes at least 20 ms (seg8:91F0); after a stall the
    // original goes on with the next pass instead of catching up.
    PlayersIo io(*this);
    for (int n = 0; n < 3 && int(now - nextPass_) >= 0; ++n) {
        nextPass_ += kSceneStepMs;
        const bool closing = players_.pass(io);
        ++pass_;
        if (closing) { // seg8:8E28 -> 925C
            enter(Stage::Closing, now);
            return;
        }
    }
    if (int(now - nextPass_) > int(5 * kSceneStepMs)) nextPass_ = now;
}

const char* BalanceView::closingHoldFrame(int pass) const {
    // seg8:9428 / 9436 set countdown 2 and frame 0 before seg2:6DCB steps: frame 0 for
    // that pass and the next, then frame 1 and every delay + 1 passes the next frame.
    if (holdPass_ < 0) return sceneFrame(kHold, pass); // not held yet: the record runs
    const int k = pass - holdPass_;
    if (k < 2) return kHold.frames[0];
    return kHold.frames[size_t((1 + (k - 2) / (kHold.delay + 1)) % int(kHold.frames.size()))];
}

void BalanceView::draw(Uint32 now) {
    if (!active_) return;
    const SDL_Rect window{(canvasWidth_ - kWindowW) / 2, (canvasHeight_ - kWindowH) / 2 - lift(), kWindowW, kWindowH};
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, &window);
    drawWindowBorder(renderer_, borderSprites_, window, BorderSize::Thin); // seg6:1A68 (ZIEL seg7:5084, SIEG 839C, BILANZ seg8:A7D7)
    const int ox = window.x + (kWindowW - kWidth) / 2, oy = window.y + (kWindowH - kHeight) / 2;
    const SDL_Rect area{ox, oy, kWidth, kHeight};
    SDL_RenderCopy(renderer_, background_.get(), nullptr, &area);
    SDL_RenderSetClipRect(renderer_, &area);
    const int t = int((now - stageStart_) / kSceneStepMs);
    bool lever = stage_ != Stage::ClosingPull;
    if (stage_ == Stage::Players) { // records 0 and 2 as the last pass left them
        const ShowRecord& m = players_.master();
        drawSceneFrame(renderer_, frames_, m.name(), ox + m.x, oy + m.y);
        lever = players_.leverShown();
    } else {
        const SceneRecord& master = stage_ == Stage::ClosingPull ? kPull
                                    : (stage_ == Stage::ClosingWait || stage_ == Stage::Closing) ? kHold
                                                                                                 : kSpeaking;
        const char* shown = stage_ == Stage::Closing ? closingHoldFrame(t) : sceneFrame(master, t);
        if (stage_ == Stage::ClosingPull) // frame 1 + t / 4 of DS:4A30, then its follow-up DS:4968 (HEBEL03)
            shown = t < kClosingFollowPass ? kPull.frames[size_t(1 + t / 4)] : "HEBEL03";
        drawSceneFrame(renderer_, frames_, shown, ox + master.x, oy + master.y);
    }
    drawSceneFrame(renderer_, frames_, sceneFrame(kFoot, t), ox + kFoot.x, oy + kFoot.y);
    if (lever) drawSceneFrame(renderer_, frames_, sceneFrame(kLever, t), ox + kLever.x, oy + kLever.y);
    SDL_RenderSetClipRect(renderer_, nullptr);
}

} // namespace dtvr
