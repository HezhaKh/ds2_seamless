#pragma once

namespace ds2sc::player_count {

// Single source of truth for the co-op player count N (including the local
// player; 1 = solo). M3 seeds this from `_debug_player_count`; M5 will drive it
// from the live lobby. Scaling multipliers are derived from this.
//
// Thread-safe (atomic). set() returns true if the value actually changed.
int  get();
bool set(int n);

}
