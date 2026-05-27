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

    // Debug only (M3): pretend this many players are in the session so scaling
    // can be tested before M5 lobbies exist. 1 = vanilla (no scaling). M5 will
    // replace this with the live co-op player count.
    int debug_player_count     = 1;

    // [PASSWORD]
    std::string cooppassword;

    // [SAVE]
    std::string save_file_extension = "co2";

    // [NETWORK] — M5a coordinator (ds3os). Empty host = no redirect (the game
    // stays offline behind the M2 DNS block). When set, the login hostname +
    // RSA public key are rewritten to point at this coordinator.
    std::string coordinator_host;                              // e.g. 127.0.0.1 or a friend's IP/host
    std::string coordinator_pubkey_file = "coordinator_pubkey.pem";  // PEM next to the DLL
    int         coordinator_port = 50050;                      // ds3os LoginServerPort (game's 50031 is rewritten to this)

    // [LANGUAGE]
    std::string mod_language_override;
};

// Load settings from <ini_path>. Missing keys retain defaults. Returns true if the
// file existed and parsed without I/O errors. Malformed lines are logged and skipped.
bool load_settings(const std::wstring& ini_path, Settings& out);

}
