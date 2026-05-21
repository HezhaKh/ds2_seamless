#pragma once

namespace ds2sc::version_gate {

// Hashes the host exe (DarkSoulsII.exe — resolved via GetModuleFileNameW(NULL))
// and compares against the expected SOTFS v1.0.3.0 SHA-256. Logs the result.
// Returns true iff the hash matches and hooks are safe to install.
bool check();

}
