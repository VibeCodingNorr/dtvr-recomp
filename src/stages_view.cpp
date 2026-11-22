// SPDX-License-Identifier: GPL-3.0-only
#include "stages_view.hpp"

#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace dtvr {
namespace {

constexpr int kWidth = 0x186;       // CreateWindow seg8:0828
constexpr int kExtra = 40;          // recomb: the slider strip below the original window
constexpr int kFieldW = 0xBE, kFieldH = 0x5A; // SBINPUT seg8:0976
constexpr int kInputLimit = 0x13;   // seg1:1D2A: window word +0xC
constexpr SDL_Color kText{67, 27, 0, 255}; // palette colour 0x45
constexpr SDL_Color kBlack{0, 0, 0, 255};  // SBINPUT sets no text colour

int resolutionIndex(int width) { return width == 1024 ? 0 : width == 800 ? 1 : 2; } // seg6:12FA

} // namespace

StagesView::StagesView(SDL_Renderer* renderer, const fs::path& root, const ScreenLayout& layout,
                       const std::map<int, std::string>& texts, TextRenderer& text)
    : renderer_(renderer), root_(root), layout_(layout), texts_(texts), text_(text) {}

int StagesView::parse(const std::string& text) {
    const std::string head = text.substr(0, 4);
    size_t i = 0;
    while (i < head.size() && (head[i] == ' ' || head[i] == '\t')) ++i; // MS C atoi
    bool negative = false;
    if (i < head.size() && (head[i] == '+' || head[i] == '-')) negative = head[i++] == '-';
    int value = 0;
    while (i < head.size() && std::isdigit(static_cast<unsigned char>(head[i]))) value = value * 10 + (head[i++] - '0');
    return negative ? -value : value;
}

void StagesView::open(int profile) {
    stages_ = profile < 0 || profile > kMaxStages ? 99 : profile; // seg8:09A6
    input_ = std::to_string(stages_);                              // itoa(…, 10)
    dragging_ = false;
    sound_ = -1;
    open_ = true;
    SDL_StartTextInput(); // SetFocus on the field (seg8:09DB)
}

SDL_Texture* StagesView::bitmap(const fs::path& file) {
    auto it = cache_.find(file.string());
    if (it == cache_.end()) {
        Texture t(nullptr, SDL_DestroyTexture);
        if (const fs::path path = findCaseInsensitive(root_, file); !path.empty()) {
            auto surface = loadBmp(path);
            t.reset(SDL_CreateTextureFromSurface(renderer_, surface.get()));
        }
        it = cache_.emplace(file.string(), std::move(t)).first;
    }
    return it->second.get();
}

SDL_Rect StagesView::window() const {
    int h = 64;
    if (SDL_Texture* sm = const_cast<StagesView*>(this)->bitmap(fs::path("bmp") / std::to_string(layout_.artDir) / "sm.bmp"))
        SDL_QueryTexture(sm, nullptr, nullptr, nullptr, &h);
    // seg8:082D: sm height + 0x24 (plus the slider strip), created at (0, 0) and
    // centred on the canvas by WM_CREATE (seg8:09E7 -> seg6:0000).
    const int height = h + 0x24 + kExtra;
    return SDL_Rect{(layout_.width - kWidth) / 2, (layout_.height - height) / 2, kWidth, height};
}

SDL_Rect StagesView::field() const {
    const auto& e = layout_.elements[8]; // seg8:095B: seg6:1140(8)
    return SDL_Rect{e[0], e[1], kFieldW, kFieldH};
}

SDL_Rect StagesView::track() const {
    const SDL_Rect w = window();
    return SDL_Rect{w.x + 20, w.y + w.h - kExtra + 8, w.w - 40, 20};
}

void StagesView::setFromSlider(int x) {
    const SDL_Rect t = track();
    const int pos = std::clamp(x - t.x, 0, t.w);
    stages_ = 1 + (pos * (kMaxStages - 1) + t.w / 2) / t.w;
    input_ = std::to_string(stages_);
}

bool StagesView::accept() {
    const int value = parse(input_);
    if (value <= 0 || value > kMaxStages) { // seg8:0A2C / 0A45: Zaehler2, the window stays
        sound_ = 7;
        return false;
    }
    stages_ = value;
    open_ = false; // DestroyWindow (seg8:0A81)
    SDL_StopTextInput();
    return true;
}

StagesView::Result StagesView::handle(const SDL_Event& event) {
    if (!open_) return Result::None;
    const SDL_Rect w = window(), t = track(), f = field();
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const SDL_Point at{event.button.x, event.button.y};
        SDL_Rect grab{t.x - 8, t.y - 6, t.w + 16, t.h + 12};
        if (SDL_PointInRect(&at, &grab)) { // recomb slider
            dragging_ = true;
            setFromSlider(at.x);
            return Result::None;
        }
        const SDL_Rect inField{w.x + f.x, w.y + f.y, f.w, std::min(f.h, w.h - f.y)}; // the field is a child of TEMP
        if (SDL_PointInRect(&at, &inField)) return Result::None; // SBINPUT takes the focus (seg1:2228)
        if (SDL_PointInRect(&at, &w)) // seg8:0D95: WM_LBUTTONDOWN accepts like Enter
            return accept() ? Result::Accept : Result::None;
        return Result::None;
    }
    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) dragging_ = false;
    if (event.type == SDL_MOUSEMOTION && dragging_) setFromSlider(event.motion.x);
    if (event.type == SDL_TEXTINPUT) { // seg1:1F31: alphanumeric characters and spaces
        for (const char* c = event.text.text; *c; ++c) {
            const unsigned char ch = static_cast<unsigned char>(*c);
            if (int(input_.size()) >= kInputLimit) break;
            if (std::isalnum(ch) || ch == ' ') input_ += char(ch);
        }
        if (const int v = parse(input_); v >= 1 && v <= kMaxStages) stages_ = v;
    }
    if (event.type == SDL_KEYDOWN) {
        const SDL_Keycode k = event.key.keysym.sym;
        if (k == SDLK_BACKSPACE && !input_.empty()) { // seg1:1FB3
            input_.pop_back();
            if (const int v = parse(input_); v >= 1 && v <= kMaxStages) stages_ = v;
        }
        // seg1:2144: SBINPUT posts only Enter to TEMP (seg8:0D8E), which accepts.
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER) return accept() ? Result::Accept : Result::None;
    }
    return Result::None;
}

void StagesView::draw() {
    if (!open_) return;
    const SDL_Rect w = window();
    const std::string dir = std::to_string(layout_.artDir);
    if (SDL_Texture* paper = bitmap(fs::path("butt") / dir / "p1.bmp")) { // seg8:0B8B at (0, 0)
        int pw = 0, ph = 0;
        SDL_QueryTexture(paper, nullptr, nullptr, &pw, &ph);
        const SDL_Rect src{0, 0, std::min(pw, w.w), std::min(ph, w.h)};
        const SDL_Rect dst{w.x, w.y, src.w, src.h};
        SDL_RenderCopy(renderer_, paper, &src, &dst);
    }
    int sw = 0, sh = 0;
    if (SDL_Texture* sm = bitmap(fs::path("bmp") / dir / "sm.bmp")) { // seg8:0C02 at the inflated rect (10, 10)
        SDL_QueryTexture(sm, nullptr, nullptr, &sw, &sh);
        const SDL_Rect dst{w.x + 10, w.y + 10, sw, sh};
        SDL_RenderCopy(renderer_, sm, nullptr, &dst);
    }
    const int res = resolutionIndex(layout_.width);
    // seg8:0C0A: seg6:04F9(18 / 16 / 14, weight 600); left + sm width + 10; text 810,
    // DT_WORDBREAK | DT_NOCLIP (0x110), colour 0x45.
    static constexpr int kSizes[3] = {0x12, 0x10, 0xE};
    const auto it = texts_.find(810);
    const int left = 10 + sw + 10;
    text_.drawWrapped(renderer_, it == texts_.end() ? std::string() : it->second,
                      ScreenRect{w.x + left, w.y + 10, w.w - 10 - left, w.h - kExtra - 20},
                      TextStyle{kSizes[res], true, false, false, false, 0, kText});
    // SBINPUT (seg1:1DC8): TextOut at (0, 0) of the field, Times New Roman italic weight
    // 600 at 20 / 17 / 15 (seg1:1C85), transparent, the caret after the text.
    static constexpr int kInput[3] = {0x14, 0x11, 0xF};
    const SDL_Rect f = field();
    TextStyle input{kInput[res], true, true, false, false, 0, kBlack};
    input.serif = true;
    text_.draw(renderer_, input_ + "|", ScreenRect{w.x + f.x, w.y + f.y, f.w, f.h}, input);
    // Recomb slider: a track and a knob in the dialog colour, 1 and 300 at its ends.
    const SDL_Rect t = track();
    SDL_SetRenderDrawColor(renderer_, kText.r, kText.g, kText.b, 255);
    const SDL_Rect line{t.x, t.y + t.h / 2 - 1, t.w, 3};
    SDL_RenderFillRect(renderer_, &line);
    for (int mark = 0; mark <= 6; ++mark) { // ticks every 50 stages
        const SDL_Rect tick{t.x + mark * t.w / 6, t.y + t.h / 2 - 5, 1, 10};
        SDL_RenderFillRect(renderer_, &tick);
    }
    const int knobX = t.x + (stages_ - 1) * t.w / (kMaxStages - 1);
    const SDL_Rect knob{knobX - 5, t.y, 11, t.h};
    SDL_RenderFillRect(renderer_, &knob);
    SDL_SetRenderDrawColor(renderer_, 255, 247, 0, 255);
    const SDL_Rect inner{knob.x + 2, knob.y + 2, knob.w - 4, knob.h - 4};
    SDL_RenderFillRect(renderer_, &inner);
}

} // namespace dtvr
