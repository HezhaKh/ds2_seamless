# M2 recon — dump everything we need from DarkSoulsII.exe to find the
# online-disconnect chokepoint and confirm imports we'll hook.
#
# Run via scripts\ghidra-analyze.cmd. Output written to args[0] (the recon/ dir).
#
# Outputs:
#   imports.csv            DLL!Function rows for every external import
#   exports.csv            Functions the binary itself exports (DS2 has few)
#   strings_netcode.csv    String-literal matches for online/network/steam keywords
#                          plus the address(es) that reference each string
#   steam_api_xrefs.csv    For every steam_api64.dll import, list functions that call it
#   summary.txt            Human-readable rollup
#
# Jython 2.7 (Ghidra's embedded interpreter). Watch the syntax — no f-strings.
#@author ds2sc
#@category ds2sc
#@runtime Jython

import os
import csv

from ghidra.program.model.symbol import SourceType
from ghidra.program.model.symbol import RefType
from ghidra.app.util import XReferenceUtil

# Arguments: [0] = output directory
args = getScriptArgs()
if not args:
    print("ERROR: pass output dir as first arg")
    raise SystemExit
out_dir = args[0]
if not os.path.isdir(out_dir):
    os.makedirs(out_dir)

program = currentProgram
listing = program.getListing()
sym_table = program.getSymbolTable()
ext_mgr = program.getExternalManager()

print("=== ds2sc M2 recon on " + program.getName() + " ===")
print("Image base:    " + str(program.getImageBase()))
print("Language:      " + str(program.getLanguageID()))
print("Compiler spec: " + str(program.getCompilerSpec()))

# ---------------------------------------------------------------------------
# Imports
# ---------------------------------------------------------------------------
imports_path = os.path.join(out_dir, "imports.csv")
with open(imports_path, "w") as f:
    w = csv.writer(f, lineterminator="\n")
    w.writerow(["library", "function", "address"])
    for lib_name in ext_mgr.getExternalLibraryNames():
        for sym in sym_table.getExternalSymbols():
            if sym.getParentNamespace().getName() == lib_name:
                addr = ""
                refs = list(sym.getReferences())
                if refs:
                    addr = str(refs[0].getFromAddress())
                w.writerow([lib_name, sym.getName(), addr])
print("Wrote " + imports_path)

# ---------------------------------------------------------------------------
# Exports (game binaries typically have very few — mostly DllMain-style entries)
# ---------------------------------------------------------------------------
exports_path = os.path.join(out_dir, "exports.csv")
with open(exports_path, "w") as f:
    w = csv.writer(f, lineterminator="\n")
    w.writerow(["name", "address"])
    for sym in sym_table.getAllSymbols(True):
        if sym.isExternalEntryPoint():
            w.writerow([sym.getName(), str(sym.getAddress())])
print("Wrote " + exports_path)

# ---------------------------------------------------------------------------
# Strings of interest
# ---------------------------------------------------------------------------
KEYWORDS = [
    "connect", "reconnect", "matchmaking", "matchmake",
    "session", "lobby", "online", "offline",
    "NetMan", "FrpgNet", "FRPG_NetMan", "FrpgNetMsg",
    "GhostStone", "BloodStain", "BloodMessage",
    "SoulMemory", "Soul memory", "soulmemory",
    "Searching for messages", "Connecting",
    "Steam", "SteamAPI", "matchmake",
    "AntiCheat", "anticheat",
    ".sl2", "SL2", "save",
    "Phantom", "phantom", "summon",
    "regulation", "Regulation",
]

# Lowercase-compare keyword set
kw_lower = [k.lower() for k in KEYWORDS]

strings_path = os.path.join(out_dir, "strings_netcode.csv")
with open(strings_path, "w") as f:
    w = csv.writer(f, lineterminator="\n")
    w.writerow(["address", "type", "value", "xref_count", "first_xref"])

    data_iter = listing.getDefinedData(True)
    while data_iter.hasNext():
        d = data_iter.next()
        if d is None:
            continue
        dt_name = d.getDataType().getName().lower()
        # Strings show up as "string", "unicode", "TerminatedCString", etc.
        if "string" not in dt_name and "char" not in dt_name and "unicode" not in dt_name:
            continue
        v = d.getValue()
        if v is None:
            continue
        s = str(v)
        s_lower = s.lower()
        if not any(k in s_lower for k in kw_lower):
            continue

        xrefs = XReferenceUtil.getXReferences(d, 100)
        first = str(xrefs[0].getFromAddress()) if xrefs else ""
        w.writerow([str(d.getAddress()), d.getDataType().getName(),
                    s.encode("utf-8", "replace"), len(xrefs), first])
print("Wrote " + strings_path)

# ---------------------------------------------------------------------------
# steam_api64.dll cross-references
# ---------------------------------------------------------------------------
steam_path = os.path.join(out_dir, "steam_api_xrefs.csv")
with open(steam_path, "w") as f:
    w = csv.writer(f, lineterminator="\n")
    w.writerow(["steam_function", "caller_address", "caller_function"])

    for sym in sym_table.getExternalSymbols():
        if sym.getParentNamespace().getName().lower() != "steam_api64.dll":
            continue
        for ref in sym.getReferences():
            from_addr = ref.getFromAddress()
            fn = listing.getFunctionContaining(from_addr)
            fn_name = fn.getName() if fn else ""
            w.writerow([sym.getName(), str(from_addr), fn_name])
print("Wrote " + steam_path)

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
summary_path = os.path.join(out_dir, "summary.txt")
with open(summary_path, "w") as f:
    f.write("ds2sc M2 recon summary\n")
    f.write("======================\n\n")
    f.write("Program:       " + program.getName() + "\n")
    f.write("Image base:    " + str(program.getImageBase()) + "\n")
    f.write("Language:      " + str(program.getLanguageID()) + "\n")
    f.write("Compiler spec: " + str(program.getCompilerSpec()) + "\n\n")

    libs = list(ext_mgr.getExternalLibraryNames())
    f.write("External libraries (" + str(len(libs)) + "):\n")
    for l in sorted(libs):
        f.write("  " + l + "\n")
print("Wrote " + summary_path)

print("=== Done ===")
