#pragma once

#include <windows.h>

namespace ds2sc::hooks {

// Walk the IAT of `host` and replace the entry for `dll_name!func_name` with
// `new_func`. On success, writes the previous pointer to *out_old and returns
// true. Hostname compare is case-insensitive; function name compare is exact.
//
// The patched slot survives ASLR because we resolve it from the live module's
// import directory each call — no hardcoded image addresses.
bool patch_iat(HMODULE host,
               const char* dll_name,
               const char* func_name,
               void* new_func,
               void** out_old);

}
