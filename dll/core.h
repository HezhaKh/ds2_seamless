#pragma once

#include <windows.h>

namespace ds2sc::core {

// Called from a worker thread spawned in DllMain. Loads settings, logs target version,
// and (in later milestones) installs game hooks. Returns when bootstrap is done; the
// thread then exits. Game hooks live on independently as installed trampolines.
DWORD WINAPI bootstrap(LPVOID instance);

// Called from DllMain on DLL_PROCESS_DETACH. Best-effort cleanup.
void teardown();

}
