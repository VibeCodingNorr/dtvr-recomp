// SPDX-License-Identifier: GPL-3.0-only
#include "balance_view.hpp"

#include "scene.hpp"

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 480, kHeight = 360, kWindowW = 0x1F0, kWindowH = 0x178;
constexpr int kVoiceChannel = 0;
const SceneRecord& kFoot = kBilanzFoot;   // definitions in balance_players.cpp
const SceneRecord& kLever = kBilanzLever;
// Not in rally.exe: a voice counts as ended after 60 s (3000 passes) even when the audio
// device has not finished it, so a stalled device cannot hold a computers-only show.
// The longest voice of the show (Bilanz_D, German data) lasts 19.6 s.
constexpr int kVoicePassLimit = 60000 / int(kSceneStepMs);

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
    legend_.clear(); // the graph's frames D, F, E, M, U, J, I, V, copied opaque by seg2:913A
    loadSbsTextures(renderer_, findCaseInsensitive(root_, butt / "sbs" / "bilanz" / "bilanz.sbs"), scene->format->palette,
                    legend_, -1);
    palette_.assign(scene->format->palette->colors, scene->format->palette->colors + scene->format->palette->ncolors);
    board_ = -1; // seg8:88F8
    commands_ = std::move(commands);
    sync_ = loadVoiceSync(root_, language_); // dat/<c>/sync.dat: the talk intervals (seg1:4071)
    intro_ = intro;
    closing_ = closing;
    humans_ = humans;
    sound_ = sound;
    started_ = now;
    active_ = background_ != nullptr;
    if (!active_) return false;
    stage_ = Stage::Players;
    endAfterPass_ = false;
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

void BalanceView::click() {
    if (!active_) return;
    if (closingPart()) { finish(); return; } // seg8:986C after the pump
    players_.click(); // DS:8D5C: used by the next pass that looks (seg8:8D17, 8DF4)
}

void BalanceView::escape(Uint32) {
    if (!active_) return;
    if (closingPart()) { finish(); return; }      // seg8:984E
    endAfterPass_ = players_.clickPending();      // DS:8D5C survives the jump (seg8:9253)
    enterClosing();
}

void BalanceView::finish() {
    if (sound_) sound_->stop(kVoiceChannel); // seg8:98B3: seg1:3C3B, waveOutReset
    active_ = false;
}

void BalanceView::update(Uint32 now) {
    if (active_) runPasses(now);
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
    int close() override { // seg8:9605 draws through Game::balanceClose
        if (v_.commands_.close) {
            const int c = v_.commands_.close();
            if (c >= 0) v_.closing_ = c;
        }
        return v_.closing_;
    }

private:
    BalanceView& v_;
};

void BalanceView::enterClosing() { // seg8:925C..93CA (DS:A6F2 = 1 at seg8:92A1)
    PlayersIo io(*this);
    stage_ = Stage::Closing;
    closingPart_.start(io);
}

void BalanceView::runPasses(Uint32 now) {
    // A pass takes at least 20 ms (seg8:91F0 / 97EB); after a stall the original goes on
    // with the next pass instead of catching up.
    PlayersIo io(*this);
    for (int n = 0; active_ && n < 3 && int(now - nextPass_) >= 0; ++n) {
        nextPass_ += kSceneStepMs;
        if (stage_ == Stage::Players) {
            const bool closing = players_.pass(io);
            ++pass_;
            board_ = players_.board(); // DS:8D5E, repainted at once (seg8:8FCA UpdateWindow)
            if (closing) enterClosing(); // seg8:8E28 -> 925C
        } else {
            const bool timedOut = closingPart_.pass(io);
            ++pass_;
            if (closingPart_.graph()) board_ = kBoardGraph; // seg8:954C
            if (timedOut || endAfterPass_) finish();       // seg8:9857 / 986C
        }
    }
    if (int(now - nextPass_) > int(5 * kSceneStepMs)) nextPass_ = now;
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
    drawBoard(ox, oy); // WM_PAINT draws the board before the records (seg8:9B93, A651)
    const bool closing = closingPart();
    const ShowRecord& m = closing ? closingPart_.master() : players_.master(); // records 0 and 2 as
    drawSceneFrame(renderer_, frames_, m.name(), ox + m.x, oy + m.y);          // the last pass left them
    const bool lever = closing ? closingPart_.leverShown() : players_.leverShown();
    const int t = int((now - started_) / kSceneStepMs); // the foot and lever have one frame each
    drawSceneFrame(renderer_, frames_, sceneFrame(kFoot, t), ox + kFoot.x, oy + kFoot.y);
    if (lever) drawSceneFrame(renderer_, frames_, sceneFrame(kLever, t), ox + kLever.x, oy + kLever.y);
    SDL_RenderSetClipRect(renderer_, nullptr);
}

} // namespace dtvr
