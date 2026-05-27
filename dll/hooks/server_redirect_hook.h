#pragma once

#include <string>

namespace ds2sc::hooks::server_redirect {

// Set the coordinator host (e.g. "127.0.0.1"), its RSA public key (PEM text),
// and its login port (the game's hardcoded 50031 is rewritten to this at
// connect time). Call before install(). If host is empty, install() is a no-op.
void configure(const std::string& host, const std::string& pubkey_pem, int login_port);

// Rewrite the login hostname and RSA public-key strings in the host module to
// point at the configured coordinator (ds3os). Plaintext on DS2 — AOB-find +
// in-place overwrite (bounded by the original buffer length). Reusable across
// the process lifetime; idempotent.
void install();

}
