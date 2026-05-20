#include "settings.h"
#include "log.h"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <string>

namespace ds2sc {

namespace {
    std::string trim(std::string s) {
        auto not_space = [](unsigned char c) { return !std::isspace(c); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
        s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
        return s;
    }

    std::string to_lower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    bool parse_bool(const std::string& v, bool& out) {
        if (v == "1" || v == "true" || v == "TRUE")  { out = true;  return true; }
        if (v == "0" || v == "false" || v == "FALSE") { out = false; return true; }
        return false;
    }

    bool parse_int(const std::string& v, int& out) {
        const auto* first = v.data();
        const auto* last  = v.data() + v.size();
        int tmp = 0;
        auto [ptr, ec] = std::from_chars(first, last, tmp);
        if (ec != std::errc{} || ptr != last) return false;
        out = tmp;
        return true;
    }
}

bool load_settings(const std::wstring& ini_path, Settings& out) {
    std::ifstream f(ini_path);
    if (!f) {
        log::warn("settings: ini not found, using defaults");
        return false;
    }

    std::string section;
    std::string line;
    int lineno = 0;

    while (std::getline(f, line)) {
        ++lineno;
        // strip CR if present (file might be CRLF)
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // strip comments — ini-style ';' anywhere starts a comment
        if (auto p = line.find(';'); p != std::string::npos) line.erase(p);
        line = trim(line);
        if (line.empty()) continue;

        if (line.front() == '[' && line.back() == ']') {
            section = to_lower(line.substr(1, line.size() - 2));
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) {
            log::warn(("settings: malformed line " + std::to_string(lineno)).c_str());
            continue;
        }

        std::string key = to_lower(trim(line.substr(0, eq)));
        std::string val = trim(line.substr(eq + 1));

        if (section == "gameplay") {
            if      (key == "allow_invaders")          parse_bool(val, out.allow_invaders);
            else if (key == "death_debuffs")           parse_bool(val, out.death_debuffs);
            else if (key == "overhead_player_display") parse_int(val,  out.overhead_player_display);
            else if (key == "skip_intros")             parse_bool(val, out.skip_intros);
        } else if (section == "scaling") {
            if      (key == "enemy_health_scaling")  parse_int(val, out.enemy_health_scaling);
            else if (key == "enemy_damage_scaling")  parse_int(val, out.enemy_damage_scaling);
            else if (key == "enemy_posture_scaling") parse_int(val, out.enemy_posture_scaling);
            else if (key == "boss_health_scaling")   parse_int(val, out.boss_health_scaling);
            else if (key == "boss_damage_scaling")   parse_int(val, out.boss_damage_scaling);
            else if (key == "boss_posture_scaling")  parse_int(val, out.boss_posture_scaling);
        } else if (section == "password") {
            if (key == "cooppassword") out.cooppassword = val;
        } else if (section == "save") {
            if (key == "save_file_extension") out.save_file_extension = val;
        } else if (section == "language") {
            if (key == "mod_language_override") out.mod_language_override = val;
        }
    }

    return true;
}

}
