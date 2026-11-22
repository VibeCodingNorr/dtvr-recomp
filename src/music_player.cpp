// SPDX-License-Identifier: GPL-3.0-only
#include "music_player.hpp"

#include <algorithm>
#include <iostream>

namespace dtvr {

MusicPlayer::MusicPlayer(std::vector<MusicPack> packs, MusicPreferences preferences)
    : packs_(std::move(packs)), preferences_(std::move(preferences)) {
    if (packs_.empty()) message_ = "Kein vollstaendiges Ogg-Klangset installiert.";
    else {
        const auto selected = std::find_if(packs_.begin(), packs_.end(), [&](const auto& p) { return p.id == preferences_.pack; });
        if (selected == packs_.end()) {
            if (!preferences_.pack.empty()) message_ = "Gespeichertes Klangset fehlt; erstes vollstaendiges Set gewaehlt.";
            preferences_.pack = packs_.front().id;
        }
        applyPack();
    }
    setVolume(preferences_.volume);
}

void MusicPlayer::applyPack() {
    const auto it = std::find_if(packs_.begin(), packs_.end(), [&](const auto& p) { return p.id == preferences_.pack; });
    if (it == packs_.end()) return;
    const int selected = int(it - packs_.begin());
    if (selected != active_) {
        active_ = selected;
        // No raw seek position is carried across different render timelines.
        background_.clear();
        backgroundPosition_ = 0;
    }
}
void MusicPlayer::begin(const std::string& cue, double position) {
    if (!enabled_ || active_ < 0) return;
    const auto it = packs_[size_t(active_)].tracks.find(cue);
    if (it == packs_[size_t(active_)].tracks.end()) { message_ = "Musik-Cue fehlt: " + cue; return; }
    cue_ = cue;
    stream_.load(it->second, position);
    stream_.pause(!focused_);
    std::cout << "Musik: " << packs_[size_t(active_)].id << "/" << cue << "\n";
}
void MusicPlayer::interrupt(const std::string& cue) {
    if (!enabled_ || active_ < 0 || !packs_[size_t(active_)].tracks.count(cue)) return;
    if (cue_.size() == 2 && cue_[0] == 'g') {
        background_ = cue_;
        backgroundPosition_ = stream_.status().position;
    }
    applyPack();
    begin(cue);
}
void MusicPlayer::nextBackground() {
    if (!enabled_) return;
    applyPack();
    const std::string next = "g" + std::to_string(next_);
    next_ = (next_ + 1) % 6;
    background_ = next;
    backgroundPosition_ = 0;
    backgroundTurn_ = turn_;
    begin(next);
}
void MusicPlayer::returnToBackground() {
    if (!enabled_) return;
    if (active_ < 0 || packs_[size_t(active_)].id != preferences_.pack) {
        applyPack();
        nextBackground();
    } else if (background_.empty()) nextBackground();
    else begin(background_, backgroundPosition_);
}
void MusicPlayer::tick() {
    if (!enabled_ || !focused_ || cue_.empty()) return;
    const auto status = stream_.status();
    if (!status.error.empty()) {
        message_ = status.error + " (" + cue_ + ")";
        std::cerr << "Musik: " << message_ << "\n";
        stream_.stop();
        cue_.clear();
        if (++failures_ >= 6) {
            enabled_ = false;
            message_ += " Wiedergabe gestoppt; Klangset bitte pruefen.";
        } else nextBackground();
    } else if (status.finished) {
        failures_ = 0;
        nextBackground(); // successful completion, never stop/cancel, advances the playlist
    }
}
void MusicPlayer::turn(int counter) {
    if (counter == turn_) return;
    if (counter < turn_) backgroundTurn_ = counter; // new/loaded game
    turn_ = counter;
    if (counter > 0 && counter % 6 == 0 && counter - backgroundTurn_ >= 6) nextBackground();
}
void MusicPlayer::setEnabled(bool enabled) {
    if (enabled == enabled_) return;
    enabled_ = enabled;
    if (enabled) { failures_ = 0; nextBackground(); }
    else { stream_.stop(); cue_.clear(); }
}
void MusicPlayer::setFocused(bool focused) { focused_ = focused; stream_.pause(!focused); }
void MusicPlayer::choosePack(const std::string& id) {
    if (std::none_of(packs_.begin(), packs_.end(), [&](const auto& p) { return p.id == id; })) return;
    preferences_.pack = id;
    if (cue_.empty()) applyPack();
}
void MusicPlayer::setVolume(int percent) {
    preferences_.volume = std::clamp(percent, 0, 100);
    stream_.volume(preferences_.volume);
}
std::string MusicPlayer::activePack() const { return active_ < 0 ? "" : packs_[size_t(active_)].id; }

} // namespace dtvr
