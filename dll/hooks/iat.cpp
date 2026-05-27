#include "iat.h"
#include "../log.h"

#include <windows.h>
#include <cstdio>
#include <cstring>

namespace ds2sc::hooks {

namespace {

bool iequals(const char* a, const char* b) {
    return _stricmp(a, b) == 0;
}

}

bool patch_iat(HMODULE host,
               const char* dll_name,
               const char* func_name,
               void* new_func,
               void** out_old)
{
    auto base = reinterpret_cast<uint8_t*>(host);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        log::error("iat: host has no DOS header");
        return false;
    }
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        log::error("iat: host has no NT header");
        return false;
    }

    const IMAGE_DATA_DIRECTORY& imp_dir =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (imp_dir.VirtualAddress == 0 || imp_dir.Size == 0) {
        log::error("iat: host has no import directory");
        return false;
    }

    auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + imp_dir.VirtualAddress);
    for (; desc->Name != 0; ++desc) {
        const char* this_dll = reinterpret_cast<const char*>(base + desc->Name);
        if (!iequals(this_dll, dll_name)) continue;

        // OriginalFirstThunk = the lookup table (names);
        // FirstThunk         = the bound IAT (pointers we want to overwrite).
        auto* lookup = reinterpret_cast<IMAGE_THUNK_DATA*>(
            base + (desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk));
        auto* iat    = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);

        for (; lookup->u1.AddressOfData != 0; ++lookup, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(lookup->u1.Ordinal)) continue;
            auto* imp_by_name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                base + lookup->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(imp_by_name->Name), func_name) != 0) {
                continue;
            }

            void** slot = reinterpret_cast<void**>(&iat->u1.Function);
            DWORD old_prot = 0;
            if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old_prot)) {
                log::error("iat: VirtualProtect to RW failed");
                return false;
            }
            *out_old = *slot;
            *slot = new_func;
            VirtualProtect(slot, sizeof(void*), old_prot, &old_prot);

            char line[256];
            std::snprintf(line, sizeof(line),
                "iat: patched %s!%s  slot=%p  old=%p  new=%p",
                dll_name, func_name, (void*)slot, *out_old, new_func);
            log::info(line);
            return true;
        }
    }

    char line[256];
    std::snprintf(line, sizeof(line), "iat: import %s!%s not found", dll_name, func_name);
    log::warn(line);
    return false;
}

bool patch_iat_ordinal(HMODULE host,
                       const char* dll_name,
                       WORD ordinal,
                       void* new_func,
                       void** out_old)
{
    auto base = reinterpret_cast<uint8_t*>(host);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    const IMAGE_DATA_DIRECTORY& imp_dir =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (imp_dir.VirtualAddress == 0 || imp_dir.Size == 0) return false;

    auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + imp_dir.VirtualAddress);
    for (; desc->Name != 0; ++desc) {
        const char* this_dll = reinterpret_cast<const char*>(base + desc->Name);
        if (!iequals(this_dll, dll_name)) continue;

        auto* lookup = reinterpret_cast<IMAGE_THUNK_DATA*>(
            base + (desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk));
        auto* iat    = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);

        for (; lookup->u1.AddressOfData != 0; ++lookup, ++iat) {
            if (!IMAGE_SNAP_BY_ORDINAL(lookup->u1.Ordinal)) continue;
            if (IMAGE_ORDINAL(lookup->u1.Ordinal) != ordinal) continue;

            void** slot = reinterpret_cast<void**>(&iat->u1.Function);
            DWORD old_prot = 0;
            if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old_prot)) {
                log::error("iat: VirtualProtect to RW failed (ordinal)");
                return false;
            }
            *out_old = *slot;
            *slot = new_func;
            VirtualProtect(slot, sizeof(void*), old_prot, &old_prot);

            char line[256];
            std::snprintf(line, sizeof(line),
                "iat: patched %s!#%u  slot=%p  old=%p  new=%p",
                dll_name, ordinal, (void*)slot, *out_old, new_func);
            log::info(line);
            return true;
        }
    }

    char line[256];
    std::snprintf(line, sizeof(line), "iat: import %s!#%u not found", dll_name, ordinal);
    log::warn(line);
    return false;
}

}
