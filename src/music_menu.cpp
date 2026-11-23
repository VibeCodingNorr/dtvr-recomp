// SPDX-License-Identifier: GPL-3.0-only
#include "music_menu.hpp"

namespace dtvr {

RecompMenu::Music musicMenuState(const MusicPlayer& player, const std::vector<std::string>& issues) {
    RecompMenu::Music m;
    for (const auto& pack : player.packs()) {
        if (pack.id == player.preferences().pack) m.pack = int(m.ids.size());
        m.ids.push_back(pack.id);
        m.names.push_back(pack.name);
    }
    m.playing = player.cue();
    if (m.ids.empty())
        m.note = "Kein vollständiges Ogg-Klangset installiert. Im Installer ein Set aus einem eigenen Soundfont "
                 "oder der CD-Musik vorbereiten.";
    else if (player.preferences().pack != player.activePack())
        m.note = "Wechsel beim nächsten Musikstück vorgemerkt.";
    else if (!player.message().empty())
        m.note = player.message();
    if (!issues.empty())
        m.note += (m.note.empty() ? "" : " ") + std::string("Nicht verfügbare Klangsets: ") + std::to_string(issues.size())
                  + " (" + issues.back() + ").";
    return m;
}

bool applyMusicChoice(MusicPlayer& player, const RecompMenu::Music& music, const std::filesystem::path& preferences,
                      std::string& error) {
    if (music.pack >= 0 && music.pack < int(music.ids.size()) && music.ids[size_t(music.pack)] != player.preferences().pack)
        player.choosePack(music.ids[size_t(music.pack)]);
    return saveMusicPreferences(preferences, player.preferences(), error);
}

} // namespace dtvr
