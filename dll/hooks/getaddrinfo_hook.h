#pragma once

namespace ds2sc::hooks::dns {

// Patches the host module's IAT entries for ws2_32!getaddrinfo and
// ws2_32!gethostbyname so that lookups against FromSoft matchmaking hostnames
// are denied while everything else (Steam, etc.) passes through unmodified.
//
// Idempotent: subsequent calls are no-ops.
void install();

}
