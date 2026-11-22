// SPDX-License-Identifier: GPL-3.0-only
#include "scene.hpp"

namespace dtvr {

const char* sceneFrame(const SceneRecord& record, int step) {
    if (record.frames.empty()) return "";
    const int delay = record.delay > 0 ? record.delay : 1;
    return record.frames[(size_t(record.first) + size_t(step / delay)) % record.frames.size()];
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
