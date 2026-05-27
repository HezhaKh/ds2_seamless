#pragma once

namespace ds2sc::scaling {

// Store the configured per-player percentages (from settings). Call once at
// bootstrap before any apply().
void init(int enemy_health_pct, int boss_health_pct);

// Apply HP scaling to the EnemyParam table for the current player_count.
// Snapshots each row's original HP on first touch so re-apply never compounds
// and restore() is exact.
//
// Returns:
//   >= 0  number of rows patched (param table was ready / save loaded)
//   -1    param table not resolvable yet (still at title screen / loading)
int apply();

// Restore every patched row to its snapshotted original and clear the snapshot.
// Safe to call when nothing was patched.
void restore();

}
