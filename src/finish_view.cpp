// SPDX-License-Identifier: GPL-3.0-only
#include "finish_view.hpp"

#include "data_paths.hpp"
#include "scene.hpp"

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 0x1E0, kHeight = 0x168;    // ziel.bmp and the scene DIB
constexpr int kWindowW = 0x1F0, kWindowH = 0x178; // CreateWindow seg7:2658

} // namespace

FinishView::FinishView(SDL_Renderer* renderer, const fs::path& root, char language, const int& canvasWidth, const int& canvasHeight)
    : renderer_(renderer), root_(root), language_(language), canvasWidth_(canvasWidth), canvasHeight_(canvasHeight) {}

bool FinishView::start(char driverLetter, const std::string& oemTownName, Uint32 now) {
    active_ = false;
    const fs::path butt("butt");
    const fs::path zielFile = findCaseInsensitive(root_, butt / "ziel.bmp");
    const fs::path bannerFile = findCaseInsensitive(root_, butt / languageDir(language_) / "banner.bmp");
    const fs::path photoFile = findCaseInsensitive(root_, butt / languageDir(language_) / "ziel" / (finishPhotoName(oemTownName) + ".bmp"));
    if (zielFile.empty() || bannerFile.empty() || photoFile.empty()) return false; // seg7:26E8 / 2740 / 298C
    const Surface ziel = loadBmp(zielFile); // DS:ABF8
    if (!canvas_.setBackdrop(ziel.get())) return false;
    const Surface banner = loadBmp(bannerFile);
    const Surface photo = loadBmp(photoFile);
    if (!banner || banner->format->BitsPerPixel != 8 || !photo || photo->format->BitsPerPixel != 8) return false;
    blitImage(toPaintImage(banner.get()), canvas_.backdrop(), 6, 0x75, true); // seg7:27E5 (seg2:9497, key 0xFF)
    // The files of DS:4BE8 (seg7:2831 ff., the car in slot 8) are loaded before the photo.
    const std::vector<std::string> files = FinishScene::files(driverLetter);
    for (size_t slot = 0; slot < files.size(); ++slot)
        canvas_.loadFile(slot, findCaseInsensitive(root_, butt / "sbs" / "ziel" / files[slot]));
    // seg7:2A82: seg7:1CE0 with the DIB as the target: its 0xFF pixels take the photo's.
    fillTransparent(canvas_.backdrop(), toPaintImage(photo.get()), 0, 0);
    for (const SceneFill& f : FinishScene::fills()) canvas_.fillFile(f.slot, f.x, f.y); // seg7:2ADE..2C1C
    scene_ = FinishScene();
    lastPass_ = now;
    shown_ = false;
    active_ = true;
    return true;
}

void FinishView::runPass() {
    scene_.pass();                              // seg7:3564 car x - 2, seg7:35B6 seg2:6DCB
    canvas_.painter().forget(FinishScene::kCar); // seg7:356E: the car's +0x36 = ""
    const std::vector<PaintRecord> records = scene_.records();
    // seg7:35A6 restore, seg7:35D4 draw changed records, seg7:35E0 wait, then on the
    // first pass ShowWindow + InvalidateRect + UpdateWindow (seg7:35F0..3614: WM_PAINT
    // seg7:4E36 draws everything), otherwise seg2:72DE (seg7:363C).
    canvas_.pass(records, scene_.passes() != 1);
    if (scene_.passes() == 1) {
        canvas_.paint(records);
        shown_ = true;
    }
}

bool FinishView::update(Uint32 now) {
    if (!active_) return false;
    if (now - lastPass_ > 10 * kSceneStepMs) lastPass_ = now - kSceneStepMs; // no long catch-up after a stall
    while (now - lastPass_ >= kSceneStepMs) {
        lastPass_ += kSceneStepMs;
        if (scene_.carGone()) { // seg7:3546 -> DestroyWindow (seg7:3555)
            active_ = false;
            return true;
        }
        runPass();
    }
    return false;
}

void FinishView::draw(Uint32) {
    if (!active_ || !shown_) return; // created without WS_VISIBLE (style 0x44000000, seg7:264F)
    SDL_Texture* texture = canvas_.texture(renderer_);
    if (!texture) return;
    // Window centred on the canvas (seg6:0000, round 111); the scene inside the
    // 8-pixel frame.
    const SDL_Rect window{(canvasWidth_ - kWindowW) / 2, (canvasHeight_ - kWindowH) / 2, kWindowW, kWindowH};
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, &window);
    drawWindowBorder(renderer_, borderSprites_, window, BorderSize::Thin); // seg6:1A68 (ZIEL seg7:5084)
    const SDL_Rect area{window.x + (kWindowW - kWidth) / 2, window.y + (kWindowH - kHeight) / 2, kWidth, kHeight};
    SDL_RenderCopy(renderer_, texture, nullptr, &area);
}

} // namespace dtvr
