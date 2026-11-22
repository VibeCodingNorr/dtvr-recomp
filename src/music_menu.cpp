// SPDX-License-Identifier: GPL-3.0-only
#include "music_menu.hpp"

#include <algorithm>

namespace dtvr {

MusicMenu::MusicMenu(SDL_Renderer* renderer, int width, int height, MusicPlayer& player,
                     std::filesystem::path preferences, std::vector<std::string> issues)
    : renderer_(renderer), width_(width), height_(height), player_(player),
      preferences_(std::move(preferences)), issues_(std::move(issues)) {}

void MusicMenu::changePack(int delta) {
    const auto& packs = player_.packs();
    if (packs.empty()) return;
    const auto it = std::find_if(packs.begin(), packs.end(), [&](const auto& p) { return p.id == player_.preferences().pack; });
    const int index = it == packs.end() ? 0 : int(it - packs.begin());
    player_.choosePack(packs[size_t((index + delta + int(packs.size())) % int(packs.size()))].id);
}
void MusicMenu::save() {
    std::string error;
    if (!saveMusicPreferences(preferences_, player_.preferences(), error)) issues_.push_back("Speichern: " + error);
}
bool MusicMenu::handle(const SDL_Event& event) {
    if (!open_) return false;
    if (event.type != SDL_KEYDOWN || event.key.repeat) return false;
    switch (event.key.keysym.sym) {
    case SDLK_ESCAPE: case SDLK_RETURN: open_ = false; return false;
    case SDLK_SPACE: player_.setEnabled(!player_.enabled()); break;
    case SDLK_UP: changePack(-1); break;
    case SDLK_DOWN: changePack(1); break;
    case SDLK_LEFT: player_.setVolume(player_.preferences().volume - 5); break;
    case SDLK_RIGHT: player_.setVolume(player_.preferences().volume + 5); break;
    default: return false;
    }
    save();
    return true;
}
void MusicMenu::draw(TextRenderer& text, bool hint) {
    if (!open_) {
        if (hint) {
            SDL_SetRenderDrawColor(renderer_, 16, 28, 46, 255);
            const SDL_Rect strip{0, height_ - 22, width_, 22};
            SDL_RenderFillRect(renderer_, &strip);
            TextStyle hintStyle;
            hintStyle.pixelHeight = 13;
            text.draw(renderer_, "F8: Musik / Klangset", {12, height_ - 19, width_ - 24, 20}, hintStyle);
        }
        return;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 210);
    SDL_RenderFillRect(renderer_, nullptr);
    const int x = 28, w = width_ - 56;
    TextStyle style;
    style.pixelHeight = 16;
    style.color = {245, 237, 218, 255};
    auto line = [&](int y, const std::string& label) { text.draw(renderer_, label, {x, y, w, 24}, style); };
    line(25, "Musik / Klangset");
    line(62, std::string("Musik: ") + (player_.enabled() ? "Ein" : "Aus") + "    Lautstaerke: " + std::to_string(player_.preferences().volume) + "%");
    std::string selected = "Kein vollstaendiges Ogg-Klangset installiert";
    for (const auto& pack : player_.packs()) if (pack.id == player_.preferences().pack) selected = pack.name;
    text.drawWrapped(renderer_, "Klangset: " + selected, {x, 104, w, 66}, style);
    const bool pending = player_.preferences().pack != player_.activePack();
    line(178, pending ? "Wechsel beim naechsten Musikstueck vorgemerkt." : "Aktuelles Stueck: " + player_.cue());
    style.pixelHeight = 13;
    text.drawWrapped(renderer_, player_.message(), {x, 218, w, 54}, style);
    if (player_.packs().empty()) text.drawWrapped(renderer_, "Im Installer ein vollstaendiges Ogg-Set aus einem eigenen Soundfont vorbereiten.", {x, 270, w, 65}, style);
    if (!issues_.empty()) text.drawWrapped(renderer_, "Nicht verfuegbare Klangsets: " + std::to_string(issues_.size()) + ". " + issues_.back(), {x, 335, w, 75}, style);
    line(height_ - 92, "Hoch/Runter: Klangset    Links/Rechts: Lautstaerke");
    line(height_ - 62, "Leertaste: Ein/Aus    Esc oder F8: Zurueck");
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
}

} // namespace dtvr
