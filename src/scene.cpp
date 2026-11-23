// SPDX-License-Identifier: GPL-3.0-only
#include "scene.hpp"

namespace dtvr {

namespace {
int span(const SceneRecord& record) { return (record.delay > 0 ? record.delay : 0) + 1; }
} // namespace

int sceneFrameIndex(const SceneRecord& record, int pass) {
    if (record.frames.empty()) return 0;
    const int n = int(record.frames.size());
    return (record.first + 1 + (pass > 0 ? pass : 0) / span(record)) % n;
}

const char* sceneFrame(const SceneRecord& record, int pass) {
    if (record.frames.empty()) return "";
    return record.frames[size_t(sceneFrameIndex(record, pass))];
}

int scenePassAt(const SceneRecord& record, int frame, int countdown) {
    // Frame f is entered in the step of pass (f - first - 1) * span; the next pass
    // starts with countdown = delay, each later one with one less.
    const int entered = (frame - record.first - 1) * span(record);
    return entered + 1 + (span(record) - 1 - countdown);
}

int sceneLength(const SceneRecord& record) {
    const int n = int(record.frames.size());
    return n > 0 ? (n - 1 - record.first) * span(record) : 0;
}

bool drawSceneFrame(SDL_Renderer* renderer, const std::map<std::string, Texture>& frames, const char* name, int x,
                    int y) {
    auto it = frames.find(upperAscii(name));
    if (it == frames.end() || !it->second) return false;
    int w = 0, h = 0;
    SDL_QueryTexture(it->second.get(), nullptr, nullptr, &w, &h);
    const SDL_Rect dst{x, y, w, h};
    SDL_RenderCopy(renderer, it->second.get(), nullptr, &dst);
    return true;
}

void blitKeyed(SDL_Surface* src, SDL_Surface* dst, int x, int y) {
    if (!src || !dst || src->format->BitsPerPixel != 8) return;
    SDL_SetColorKey(src, SDL_TRUE, 0xFF);
    SDL_Rect to{x, y, src->w, src->h};
    SDL_BlitSurface(src, nullptr, dst, &to);
}

} // namespace dtvr
