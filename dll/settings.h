#pragma once

#include <string>

namespace ds2sc {

struct Settings {
    // [GAMEPLAY]
    bool allow_invaders        = false;  // off by default for personal-coop scope
    bool death_debuffs         = true;
    int  overhead_player_display = 0;
    bool skip_intros           = true;

    // [SCALING] — defaults mirror DS3SC numbers; will be applied at M3
    int enemy_health_scaling   = 35;
    int enemy_damage_scaling   = 0;
    int enemy_posture_scaling  = 15;
    int boss_health_scaling    = 100;
    int boss_damage_scaling    = 0;
    int boss_posture_scaling   = 20;

    // [PASSWORD]
    std::string cooppassword;

    // [SAVE]
    std::string save_file_extension = "co2";

    // [LANGUAGE]
    std::string mod_language_override;
};

// Load settings from <ini_path>. Missing keys retain defaults. Returns true if the
// file existed and parsed without I/O errors. Malformed lines are logged and skipped.
bool load_settings(const std::wstring& ini_path, Settings& out);

}
