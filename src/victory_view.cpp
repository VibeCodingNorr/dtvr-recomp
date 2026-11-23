// SPDX-License-Identifier: GPL-3.0-only
#include "victory_view.hpp"

#include "scene.hpp"

#include <cctype>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 480, kHeight = 360, kWindowW = 0x1F0, kWindowH = 0x178; // seg7:738D
constexpr char kLetters[8] = {'D', 'F', 'E', 'M', 'U', 'J', 'I', 'V'};        // seg2:2D43

// Records of seg7:7457 ff. in drawing order (the winner record follows the car).
const std::vector<SceneRecord>& records() {
    static const std::vector<SceneRecord> kRecords = {
        {0, 147, 0, 4, {"ml00", "ml01", "ml02", "ml03", "ml04", "ml05", "ml06", "ml07", "ml08", "ml09", "ml10", "ml11",
                        "ml12", "ml13", "ml14", "ml15", "ml16"}}, // DS:557A
        {311, 144, 0, 3, {"mr00", "mr01", "mr02", "mr03", "mr04", "mr05", "mr06", "mr07", "mr08", "mr09", "mr10",
                          "mr11", "mr12", "mr13", "mr14", "mr15", "mr16"}}, // DS:5618
        {365, 200, 0, 4, {"kon00", "kon01", "kon02", "kon03", "kon04", "kon05", "kon06", "kon07", "kon08", "kon09",
                          "kon10", "kon11"}}, // DS:56A8
        {305, 195, 0, 4, {"hutman-1", "hutman-2"}}, // DS:5518
        {405, 260, 0, 1, {"CAR1"}},                 // DS:5828, the winner's car
        {8, 198, 0, 4, {"kap00", "kap01", "kap02", "kap03", "kap04", "kap05", "kap06", "kap07", "kap08", "kap09",
                        "kap10", "kap11"}}, // DS:5724
        {298, 289, 0, 4, {"zp00", "zp01", "zp02", "zp03", "zp04", "zp05", "zp06", "zp07", "zp07", "zp07", "zp07",
                          "zp07", "zp07", "zp07", "zp07", "zp07", "zp07", "zp07", "zp08", "zp09", "zp10",
                          "zp11"}}, // DS:57C6
        {39, 239, 1, 3, {"rd014", "rd012", "rd011", "rd012", "rd014", "rd012", "rd011", "rd012", "rd011", "rd012",
                         "rd013", "rd012", "rd013", "rd012", "rd014", "rd012"}}, // DS:4800 (intro\redet.sbs)
    };
    return kRecords;
}
// DS:5D6C / DS:5D8E per driver slot: the winner's definition and x offset (x = 0xE4 - offset).
struct Winner { int offset, delay; std::vector<const char*> frames; };
const Winner& winner(int slot) {
    static const Winner kWinners[8] = {
        {27, 4, {"DW01", "DW02", "DW03", "DW04", "DW04", "DW04", "DW04", "DW04", "DW04", "DW03", "DW02", "DW01",
                 "DW01", "DW01", "DW01", "DW01"}},
        {34, 4, {"FW01", "FW02", "FW03", "FW02", "FW01", "FW04", "FW05", "FW06", "FW05", "FW04"}},
        {42, 4, {"EW01", "EW02", "EW03", "EW04", "EW05", "EW05", "EW05", "EW05", "EW04", "EW03", "EW02", "EW01",
                 "EW01", "EW01", "EW01"}},
        {39, 4, {"MW01", "MW02", "MW03", "MW04", "MW05", "MW04", "MW03", "MW02", "MW01", "MW02", "MW03", "MW04",
                 "MW05", "MW10", "MW10", "MW11", "MW11", "MW11", "MW10", "MW04", "MW03", "MW02"}},
        {30, 4, {"UW01", "UW02", "UW03", "UW04", "UW05", "UW06", "UW07", "UW07", "UW07", "UW07", "UW07", "UW06",
                 "UW05", "UW04", "UW03", "UW02", "UW01", "UW01", "UW01", "UW01"}},
        {20, 4, {"JW01", "JW02", "JW03", "JW04", "JW05", "JW06", "JW07", "JW07", "JW07", "JW07", "JW07", "JW06",
                 "JW05", "JW04", "JW03", "JW02", "JW01", "JW01", "JW01", "JW01"}},
        {27, 4, {"IW01", "IW02", "IW03", "IW04", "IW05", "IW05", "IW05", "IW04", "IW03", "IW02", "IW01", "IW01",
                 "IW01", "IW01", "IW01"}},
        {28, 4, {"VW01", "VW02", "VW03", "VW04", "VW05", "VW05", "VW05", "VW05", "VW04", "VW03", "VW02", "VW01",
                 "VW01", "VW01", "VW01"}},
    };
    return kWinners[slot & 7];
}

int frameHeight(const std::map<std::string, Texture>& frames, const std::string& name, int* width = nullptr) {
    auto it = frames.find(upperAscii(name));
    int w = 0, h = 0;
    if (it != frames.end() && it->second) SDL_QueryTexture(it->second.get(), nullptr, nullptr, &w, &h);
    if (width) *width = w;
    return h;
}

} // namespace

VictoryView::VictoryView(SDL_Renderer* renderer, const fs::path& root, int canvasWidth, int canvasHeight)
    : renderer_(renderer), root_(root), canvasWidth_(canvasWidth), canvasHeight_(canvasHeight) {}

bool VictoryView::start(const std::vector<int>& ranking, Uint32 now) {
    active_ = false;
    if (ranking.empty()) return false;
    const fs::path butt("butt");
    Surface scene = loadBmp(findCaseInsensitive(root_, butt / "Sieg.bmp")); // DS:5EE2
    if (!scene || !scene->format->palette) return false;
    if (const fs::path banner = findCaseInsensitive(root_, butt / "s_bann_d.bmp"); !banner.empty()) {
        Surface b = loadBmp(banner);
        blitKeyed(b.get(), scene.get(), 3, 0x85); // seg7:7F99
    }
    frames_.clear();
    const SDL_Palette* palette = scene->format->palette;
    for (const char* file : {"sieg/many-l.sbs", "sieg/many-r.sbs", "sieg/konfetti.sbs", "sieg/hutman.sbs",
                             "sieg/kapelle.sbs", "sieg/zoepfe.sbs", "intro/redet.sbs"}) // DS:5E6A
        loadSbsTextures(renderer_, findCaseInsensitive(root_, butt / "sbs" / file), palette, frames_, 0xFF);
    auto lower = [](char c) { return char(std::tolower(static_cast<unsigned char>(c))); };
    winner_ = ranking[0];
    loadSbsTextures(renderer_, findCaseInsensitive(root_, butt / "sbs" / "ziel" / (std::string(1, lower(kLetters[winner_])) + "zielcar.sbs")),
                    palette, frames_, 0xFF);
    for (size_t place = 0; place < ranking.size() && place < 3; ++place) // x_winner.sbs with the letter
        loadSbsTextures(renderer_, findCaseInsensitive(root_, butt / "sbs" / "sieg" / (std::string(1, lower(kLetters[ranking[place]])) + "_winner.sbs")),
                        palette, frames_, 0xFF);
    background_.reset(SDL_CreateTextureFromSurface(renderer_, scene.get()));
    // Second and third places are drawn into the scene (seg7:79CB / 7A6C, seg7:1F52):
    // centred at x 0xB9 / 0x10F with their feet at y 0xDC / 0xE3.
    places_.clear();
    static const int kCenter[2] = {0xB9, 0x10F}, kFoot[2] = {0xDC, 0xE3};
    for (size_t place = 1; place < ranking.size() && place < 3; ++place) {
        const std::string name = std::string(1, kLetters[ranking[place]]) + "W00"; // DS:5D9C: <L>W00
        int w = 0;
        const int h = frameHeight(frames_, name, &w);
        places_.push_back({name, kCenter[place - 1] - w / 2, kFoot[place - 1] - h});
    }
    winnerY_ = 0xCA - frameHeight(frames_, winner(winner_).frames.front()); // seg7:79A0
    started_ = now;
    active_ = background_ != nullptr;
    return active_;
}

bool VictoryView::closes(const SDL_Event& event, Uint32 now) const {
    if (!active_ || now - started_ < kSceneStepMs) return false;
    if (event.type == SDL_MOUSEBUTTONDOWN) return event.button.button == SDL_BUTTON_LEFT;
    if (event.type != SDL_KEYDOWN) return false;
    const SDL_Keycode k = event.key.keysym.sym;
    return k == SDLK_ESCAPE || k == SDLK_SPACE || k == SDLK_RETURN || k == SDLK_KP_ENTER;
}

void VictoryView::update(Uint32 now) {
    if (active_ && (now - started_) / kSceneStepMs >= 5000) active_ = false; // 0x1388 passes
}

void VictoryView::draw(Uint32 now) {
    if (!active_) return;
    const SDL_Rect window{(canvasWidth_ - kWindowW) / 2, (canvasHeight_ - kWindowH) / 2 - lift_, kWindowW, kWindowH};
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer_, &window);
    drawWindowBorder(renderer_, borderSprites_, window, BorderSize::Thin); // seg6:1A68 (ZIEL seg7:5084, SIEG 839C, BILANZ seg8:A7D7)
    const int ox = window.x + (kWindowW - kWidth) / 2, oy = window.y + (kWindowH - kHeight) / 2;
    const SDL_Rect area{ox, oy, kWidth, kHeight};
    SDL_RenderCopy(renderer_, background_.get(), nullptr, &area);
    SDL_RenderSetClipRect(renderer_, &area);
    for (const Place& p : places_) drawSceneFrame(renderer_, frames_, p.name.c_str(), ox + p.x, oy + p.y);
    const int t = int((now - started_) / kSceneStepMs);
    for (size_t i = 0; i < records().size(); ++i) {
        drawSceneFrame(renderer_, frames_, sceneFrame(records()[i], t), ox + records()[i].x, oy + records()[i].y);
        if (i == 4 && winner_ >= 0) { // record 9: the winner, after the car
            const Winner& w = winner(winner_);
            const SceneRecord r{0xE4 - w.offset, winnerY_, 0, w.delay, w.frames};
            drawSceneFrame(renderer_, frames_, sceneFrame(r, t), ox + r.x, oy + r.y);
        }
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
}

} // namespace dtvr
