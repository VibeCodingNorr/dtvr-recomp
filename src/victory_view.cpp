// SPDX-License-Identifier: GPL-3.0-only
#include "victory_view.hpp"

#include "scene.hpp"

#include <algorithm>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 0x1E0, kHeight = 0x168, kWindowW = 0x1F0, kWindowH = 0x178; // seg7:738D

} // namespace

VictoryView::VictoryView(SDL_Renderer* renderer, const fs::path& root, char language, const int& canvasWidth, const int& canvasHeight)
    : renderer_(renderer), root_(root), language_(language), canvasWidth_(canvasWidth), canvasHeight_(canvasHeight) {}

bool VictoryView::start(const std::vector<int>& ranking, Uint32 now) {
    active_ = false;
    if (ranking.empty()) return false;
    const fs::path butt("butt");
    const Surface scene = loadBmp(findCaseInsensitive(root_, butt / "Sieg.bmp")); // DS:5EE2 -> DS:A0AE
    if (!canvas_.setBackdrop(scene.get())) return false;
    // seg7:7EC1: the banner only for the language letters 'd' and 'D' (DS:A0F0).
    if (language_ == 'd' || language_ == 'D') {
        const fs::path file = findCaseInsensitive(root_, butt / "s_bann_d.bmp"); // DS:5EEE
        const Surface banner = file.empty() ? Surface(nullptr, SDL_FreeSurface) : loadBmp(file);
        if (banner && banner->format->BitsPerPixel == 8)
            blitImage(toPaintImage(banner.get()), canvas_.backdrop(), 3, 0x85, true); // seg7:7F99 (seg2:9497)
    }
    // The files of DS:5E6A in slot order (seg7:7FC7..8042).
    const std::vector<std::string> files = VictoryScene::files(ranking);
    for (size_t slot = 0; slot < files.size(); ++slot)
        canvas_.loadFile(slot, findCaseInsensitive(root_, butt / "sbs" / files[slot]));
    // Before the first pass WM_PAINT clears the DIB (DS:9CDE = 0, seg7:82C0..8302): the
    // window, shown at seg7:73E4, is black.
    canvas_.clear();
    for (const SceneFill& f : VictoryScene::fills()) canvas_.fillFile(f.slot, f.x, f.y); // seg7:748E..750E
    const int winner = ranking[0];
    const PaintImage* first = findPaintFrame(canvas_.frames(), VictoryScene::winner(winner).frames.front());
    scene_ = std::make_unique<VictoryScene>(winner, first ? first->height : 0); // seg7:7994..79AB
    // Second and third places stamped into the backdrop with colour key (seg7:79CB /
    // 7A6C -> seg7:1F52); a missing frame draws nothing.
    for (size_t place = 1; place < ranking.size() && place < 3; ++place) {
        const PaintImage* f = findPaintFrame(canvas_.frames(), VictoryScene::placeFrame(ranking[place]));
        if (!f) continue;
        int x = 0, y = 0;
        VictoryScene::placePosition(place, f->width, f->height, x, y);
        blitImage(*f, canvas_.backdrop(), x, y, true);
    }
    sync_ = loadVoiceSync(root_, language_);
    voice_ = findVoiceSync(sync_, "sieg.wav"); // seg1:3579 selects the sync.dat record
    started_ = lastPass_ = now;
    firstPass_ = false;
    active_ = true;
    return true;
}

bool VictoryView::closes(const SDL_Event& event, Uint32) const {
    if (!active_ || !firstPass_) return false;
    if (event.type == SDL_MOUSEBUTTONDOWN) return event.button.button == SDL_BUTTON_LEFT;
    if (event.type != SDL_KEYDOWN) return false;
    const SDL_Keycode k = event.key.keysym.sym;
    return k == SDLK_ESCAPE || k == SDLK_SPACE || k == SDLK_RETURN || k == SDLK_KP_ENTER;
}

void VictoryView::runPass() {
    // seg7:7BFF..7C46 read the voice position at this pass (TimerCount path of
    // seg1:3F46: milliseconds since the voice started).
    const uint32_t elapsed = lastPass_ - started_;
    const bool talking = voice_ && voiceSyncTalking(*voice_, elapsed);
    const bool cue = voice_ && voiceSyncCue(*voice_, elapsed);
    const bool playing = voice_ && voiceSyncPlaying(*voice_, elapsed);
    scene_->pass(talking, cue, playing);
    const std::vector<PaintRecord> records = scene_->records();
    // seg7:7C8F restore, seg7:7CBD draw changed records, seg7:7CC9 wait, then on the
    // first pass DS:9CDE = 1 with InvalidateRect + UpdateWindow (WM_PAINT seg7:811B
    // draws everything), otherwise seg2:72DE (seg7:7D2A).
    canvas_.pass(records, scene_->passes() != 1);
    if (scene_->passes() == 1) {
        firstPass_ = true;
        canvas_.paint(records);
    }
}

void VictoryView::update(Uint32 now) {
    if (!active_ || !scene_) return;
    if (now - lastPass_ > 10 * kSceneStepMs) lastPass_ = now - kSceneStepMs; // no long catch-up after a stall
    while (active_ && now - lastPass_ >= kSceneStepMs) {
        lastPass_ += kSceneStepMs;
        runPass();
        // seg7:7D4E: after pass 0x1388 the window gets VK_RETURN and is destroyed.
        if (scene_->done()) active_ = false;
    }
}

void VictoryView::draw(Uint32) {
    if (!active_) return;
    SDL_Texture* texture = canvas_.texture(renderer_);
    const SDL_Rect window{(canvasWidth_ - kWindowW) / 2, (canvasHeight_ - kWindowH) / 2 - lift_, kWindowW, kWindowH};
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, &window);
    drawWindowBorder(renderer_, borderSprites_, window, BorderSize::Thin); // seg6:1A68 (SIEG seg7:839C)
    const SDL_Rect area{window.x + (kWindowW - kWidth) / 2, window.y + (kWindowH - kHeight) / 2, kWidth, kHeight};
    if (texture) SDL_RenderCopy(renderer_, texture, nullptr, &area);
}

} // namespace dtvr
