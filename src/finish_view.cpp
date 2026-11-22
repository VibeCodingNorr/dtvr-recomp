// SPDX-License-Identifier: GPL-3.0-only
#include "finish_view.hpp"

#include <cctype>
#include <vector>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 480, kHeight = 360;    // ziel.bmp and the scene DIB
constexpr int kWindowW = 0x1F0, kWindowH = 0x178; // CreateWindow seg7:2658
constexpr Uint32 kStepMs = 20;                // seg6:0CDF(20) per loop (seg7:35DC)

// Animation records set up at seg7:2C55 ff. (DS:30 + 0x40 · i): position, start
// frame (+0xC), delay in steps and the frames of the definition.
struct Record {
    int x, y, first, delay;
    std::vector<const char*> frames;
};
const std::vector<Record>& records() {
    static const std::vector<Record> kRecords = {
        {209, 182, 0, 4, {"pauke00", "pauke01", "pauke02", "pauke03", "pauke00", "pauke01", "pauke02", "pauke03",
                          "pauke00", "pauke01", "pauke02", "pauke03"}}, // DS:4594
        {339, 221, 1, 4, {"duo00", "duo01", "duo02", "duo03", "duo04", "duo05", "duo06", "duo07", "duo08", "duo09",
                          "duo10", "duo11"}}, // DS:419C
        {130, 206, 0, 4, {"fl00", "fl01", "fl02", "fl02", "fl02", "fl05", "fl06", "fl07", "fl08", "fl09", "fl10",
                          "fl02"}}, // DS:427C
        {267, 209, 0, 4, {"tuba00", "tuba01", "tuba02", "tuba03", "tuba00", "tuba01", "tuba02", "tuba03", "tuba00",
                          "tuba01", "tuba02", "tuba02"}}, // DS:461C
        {25, 214, 2, 4, {"ge00", "ge01", "ge02", "ge03", "ge04", "ge05", "ge06", "ge07", "ge08", "ge08", "ge10",
                         "ge11", "ge12", "ge13", "ge14", "ge15", "ge11", "ge11", "ge11", "ge11"}}, // DS:4314
        {79, 182, 3, 4, {"ha00", "ha01", "ha02", "ha03", "ha04", "ha05", "ha06", "ha07", "ha08", "ha09", "ha10",
                         "ha11"}}, // DS:4420
        {405, 175, 4, 4, {"pa00", "pa01", "pa02", "pa03", "pa04", "pa05", "pa06", "pa07", "pa08", "pa09", "pa10",
                          "pa11"}}, // DS:4500
        {180, 189, 0, 4, {"jz00", "jz01", "jz02", "jz03", "jz04", "jz05", "jz06", "jz07", "jz08", "jz09", "jz00",
                          "jz11"}}, // DS:4490
        {460, 244, 3, 1, {"CAR1", "CAR2", "CAR3", "CAR2"}}, // DS:4760, the car (record 8)
        {269, 304, 3, 4, {"hut00", "hut01", "hut02", "hut03", "hut04", "hut05", "hut06", "hut07", "hut08", "hut09",
                          "hut10", "hut11"}}, // DS:43B0
        {175, 280, 0, 3, {"bal00", "bal01", "bal02", "bal03", "bal04", "bal05", "bal06", "bal07", "bal08", "bal09",
                          "bal10", "bal11"}}, // DS:4120
        {78, 296, 2, 4, {"fr00", "fr01", "fr02", "fr02", "fr04", "fr05", "fr06", "fr02", "fr08", "fr09", "fr10",
                         "fr11"}}, // DS:420C
        {29, 294, 4, 4, {"wi00", "wi01", "wi02", "wi03", "wi04", "wi05", "wi06", "wi07", "wi08", "wi09", "wi10",
                         "wi11"}}, // DS:468C
        {347, 300, 3, 4, {"zp00", "zp01", "zp02", "zp03", "zp04", "zp05", "zp06", "zp07", "zp08", "zp09", "zp10",
                          "zp11"}}, // DS:46FC
        {0, 82, 2, 4, {"pfahl"}},   // DS:4736
        {117, 129, 0, 4, {"lampe"}}, // DS:4744
    };
    return kRecords;
}
constexpr size_t kCar = 8;
// DS:4BE8: the files of butt/sbs/ziel loaded by seg7:2831.
const char* const kFiles[] = {"pauke.sbs", "duo.sbs", "floete.sbs", "tuba.sbs", "genie.sbs", "happy.sbs",
                              "paar.sbs", "jazz.sbs", "hut.sbs", "ballon.sbs", "frau.sbs", "winken.sbs",
                              "zoepfe.sbs", "pfahl.sbs", "lampe.sbs"};

// Copies `src` into `dst` at (x, y), leaving out palette index 0xFF (seg2:9497 / seg7:1CE0).
void blitKeyed(SDL_Surface* src, SDL_Surface* dst, int x, int y) {
    if (!src || !dst || src->format->BitsPerPixel != 8) return;
    SDL_SetColorKey(src, SDL_TRUE, 0xFF);
    SDL_Rect to{x, y, src->w, src->h};
    SDL_BlitSurface(src, nullptr, dst, &to);
}

// seg7:1CE0: copies `src` into `dst` at (x, y) only where `dst` holds index 0xFF
// (the white sky of ziel.bmp; the trees in front stay).
void fillKeyed(SDL_Surface* src, SDL_Surface* dst, int x, int y) {
    if (!src || !dst || src->format->BitsPerPixel != 8 || dst->format->BitsPerPixel != 8) return;
    SDL_LockSurface(src);
    SDL_LockSurface(dst);
    for (int row = 0; row < src->h && y + row < dst->h; ++row)
        for (int col = 0; col < src->w && x + col < dst->w; ++col) {
            uint8_t& to = static_cast<uint8_t*>(dst->pixels)[(y + row) * dst->pitch + x + col];
            if (to == 0xFF) to = static_cast<const uint8_t*>(src->pixels)[row * src->pitch + col];
        }
    SDL_UnlockSurface(dst);
    SDL_UnlockSurface(src);
}

} // namespace

FinishView::FinishView(SDL_Renderer* renderer, const fs::path& root, int canvasWidth, int canvasHeight)
    : renderer_(renderer), root_(root), canvasWidth_(canvasWidth), canvasHeight_(canvasHeight) {}

bool FinishView::start(char driverLetter, const std::string& oemTownName, Uint32 now) {
    active_ = false;
    const fs::path butt("butt");
    const fs::path zielFile = findCaseInsensitive(root_, butt / "ziel.bmp");
    const fs::path bannerFile = findCaseInsensitive(root_, butt / "d" / "banner.bmp");
    const fs::path photoFile = findCaseInsensitive(root_, butt / "d" / "ziel" / (finishPhotoName(oemTownName) + ".bmp"));
    if (zielFile.empty() || bannerFile.empty() || photoFile.empty()) return false; // seg7:26E8 / 298C
    Surface scene = loadBmp(zielFile);
    if (!scene || !scene->format->palette) return false;
    Surface banner = loadBmp(bannerFile);
    Surface photo = loadBmp(photoFile);
    blitKeyed(banner.get(), scene.get(), 6, 117); // seg7:27E5
    fillKeyed(photo.get(), scene.get(), 0, 0);    // seg7:2A82
    background_.reset(SDL_CreateTextureFromSurface(renderer_, scene.get()));
    frames_.clear();
    const SDL_Palette* palette = scene->format->palette;
    for (const char* file : kFiles)
        loadSbsTextures(renderer_, findCaseInsensitive(root_, butt / "sbs" / "ziel" / file), palette, frames_, 0xFF);
    const std::string car = std::string(1, char(std::tolower(static_cast<unsigned char>(driverLetter)))) + "zielcar.sbs";
    loadSbsTextures(renderer_, findCaseInsensitive(root_, butt / "sbs" / "ziel" / car), palette, frames_, 0xFF);
    started_ = now;
    active_ = background_ != nullptr;
    return active_;
}

int FinishView::steps(Uint32 now) const { return int((now - started_) / kStepMs); }

void FinishView::update(Uint32 now) {
    if (active_ && records()[kCar].x - 2 * steps(now) < -160) active_ = false; // 0xFF60
}

void FinishView::draw(Uint32 now) {
    if (!active_) return;
    // Window centred on the canvas (assumption: x = y = 0 is centred as for other
    // child windows); the scene is drawn inside an 8-pixel frame.
    const SDL_Rect window{(canvasWidth_ - kWindowW) / 2, (canvasHeight_ - kWindowH) / 2, kWindowW, kWindowH};
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, &window);
    const int ox = window.x + (kWindowW - kWidth) / 2, oy = window.y + (kWindowH - kHeight) / 2;
    const SDL_Rect area{ox, oy, kWidth, kHeight};
    SDL_RenderCopy(renderer_, background_.get(), nullptr, &area);
    SDL_RenderSetClipRect(renderer_, &area);
    const int t = steps(now);
    for (size_t i = 0; i < records().size(); ++i) { // seg2:74E4 draws the records in order
        const Record& r = records()[i];
        const size_t n = r.frames.size();
        const char* name = r.frames[(size_t(r.first) + size_t(t / r.delay)) % n]; // seg2:6DCB
        auto it = frames_.find(upperAscii(name));
        if (it == frames_.end() || !it->second) continue;
        int w = 0, h = 0;
        SDL_QueryTexture(it->second.get(), nullptr, nullptr, &w, &h);
        const int x = i == kCar ? r.x - 2 * t : r.x; // seg7:3564
        const SDL_Rect dst{ox + x, oy + r.y, w, h};
        SDL_RenderCopy(renderer_, it->second.get(), nullptr, &dst);
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
}

} // namespace dtvr
