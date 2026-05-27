#pragma once

#include <string>

namespace ds2sc::hooks::savefile {

// Set the save extension that ".sl2" is rewritten to (e.g. "co2"). Call before
// install(). Defaults to "co2" if never configured or given an empty string.
void configure(const std::string& extension);

// IAT-hook CreateFileW + CreateFileA so the game's save files (paths containing
// ".sl2") are redirected to the configured extension. The vanilla ".sl2" file
// is never opened by the game once hooked, so it stays untouched as a backup.
// Idempotent.
void install();

}
