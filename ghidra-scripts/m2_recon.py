# -*- coding: utf-8 -*-
# M2 recon -- dump everything we need from DarkSoulsII.exe to find the
# online-disconnect chokepoint and confirm imports we'll hook.
#
# Writes a status.log incrementally so we can see exactly where it dies
# if the headless run is otherwise silent.
#
# @author ds2sc
# @category ds2sc
# @runtime Jython

import os
import csv
import traceback

args = getScriptArgs()
out_dir = args[0] if args else r"C:\Users\h\ds2sc\recon"

if not os.path.isdir(out_dir):
    os.makedirs(out_dir)

status_path = os.path.join(out_dir, "status.log")
status = open(status_path, "w")

def log(msg):
    status.write(str(msg) + "\n")
    status.flush()
    println("[m2_recon] " + str(msg))

log("script started")
log("out_dir = " + out_dir)

try:
    log("currentProgram = " + str(currentProgram.getName()))
    log("image base = " + str(currentProgram.getImageBase()))

    listing  = currentProgram.getListing()
    sym_table = currentProgram.getSymbolTable()
    ext_mgr  = currentProgram.getExternalManager()
    fn_mgr   = currentProgram.getFunctionManager()
    ref_mgr  = currentProgram.getReferenceManager()
    log("got managers")

    # -------- imports.csv --------------------------------------------------
    log("imports: collecting...")
    n_imports = 0
    imports_path = os.path.join(out_dir, "imports.csv")
    f = open(imports_path, "w")
    f.write("library,function,address\n")
    for sym in sym_table.getExternalSymbols():
        lib_name = sym.getParentNamespace().getName()
        refs = sym.getReferences()
        if refs and len(refs) > 0:
            addr = str(refs[0].getFromAddress())
        else:
            addr = ""
        f.write('"%s","%s","%s"\n' % (lib_name, sym.getName(), addr))
        n_imports += 1
    f.close()
    log("imports: %d rows -> %s" % (n_imports, imports_path))

    # -------- exports.csv --------------------------------------------------
    log("exports: collecting...")
    exports_path = os.path.join(out_dir, "exports.csv")
    n_exports = 0
    f = open(exports_path, "w")
    f.write("name,address\n")
    # entry points are the program's exported / entry symbols
    for addr in sym_table.getExternalEntryPointIterator():
        sym = sym_table.getPrimarySymbol(addr)
        name = sym.getName() if sym else ""
        f.write('"%s","%s"\n' % (name, str(addr)))
        n_exports += 1
    f.close()
    log("exports: %d rows -> %s" % (n_exports, exports_path))

    # -------- strings_netcode.csv -----------------------------------------
    KEYWORDS = [
        "connect", "reconnect", "matchmaking", "matchmake",
        "session", "lobby", "online", "offline",
        "netman", "frpgnet", "frpg_netman", "frpgnetmsg",
        "ghoststone", "bloodstain", "bloodmessage",
        "soulmemory", "soul memory",
        "searching for messages",
        "steam",
        "anticheat",
        ".sl2", "sl2",
        "phantom", "summon",
        "regulation",
    ]

    log("strings: scanning...")
    n_strings = 0
    n_seen = 0
    strings_path = os.path.join(out_dir, "strings_netcode.csv")
    f = open(strings_path, "w")
    f.write("address,type,value,xref_count,first_xref\n")
    di = listing.getDefinedData(True)
    while di.hasNext():
        d = di.next()
        n_seen += 1
        if d is None:
            continue
        dt_name = d.getDataType().getName().lower()
        if "string" not in dt_name and "char" not in dt_name and "unicode" not in dt_name:
            continue
        v = d.getValue()
        if v is None:
            continue
        s = str(v)
        s_lower = s.lower()
        matched = False
        for k in KEYWORDS:
            if k in s_lower:
                matched = True
                break
        if not matched:
            continue
        refs = ref_mgr.getReferencesTo(d.getAddress())
        xc = 0
        first_x = ""
        for r in refs:
            if xc == 0:
                first_x = str(r.getFromAddress())
            xc += 1
        # escape internal quotes
        s_clean = s.replace('"', "'")
        f.write('"%s","%s","%s",%d,"%s"\n' % (str(d.getAddress()), d.getDataType().getName(), s_clean, xc, first_x))
        n_strings += 1
    f.close()
    log("strings: %d matched out of %d defined-data items -> %s" % (n_strings, n_seen, strings_path))

    # -------- steam_api64.dll xrefs ---------------------------------------
    log("steam xrefs: collecting...")
    n_steam = 0
    steam_path = os.path.join(out_dir, "steam_api_xrefs.csv")
    f = open(steam_path, "w")
    f.write("steam_function,caller_address,caller_function\n")
    for sym in sym_table.getExternalSymbols():
        if sym.getParentNamespace().getName().lower() != "steam_api64.dll":
            continue
        for ref in sym.getReferences():
            from_addr = ref.getFromAddress()
            fn = fn_mgr.getFunctionContaining(from_addr)
            fn_name = fn.getName() if fn else ""
            f.write('"%s","%s","%s"\n' % (sym.getName(), str(from_addr), fn_name))
            n_steam += 1
    f.close()
    log("steam xrefs: %d rows -> %s" % (n_steam, steam_path))

    # -------- summary.txt --------------------------------------------------
    log("summary: writing...")
    summary_path = os.path.join(out_dir, "summary.txt")
    f = open(summary_path, "w")
    f.write("ds2sc M2 recon summary\n")
    f.write("======================\n\n")
    f.write("Program:       %s\n" % currentProgram.getName())
    f.write("Image base:    %s\n" % str(currentProgram.getImageBase()))
    f.write("Language:      %s\n" % str(currentProgram.getLanguageID()))
    f.write("Compiler spec: %s\n\n" % str(currentProgram.getCompilerSpec()))
    libs = sorted(list(ext_mgr.getExternalLibraryNames()))
    f.write("External libraries (%d):\n" % len(libs))
    for lib in libs:
        f.write("  " + lib + "\n")
    f.write("\nCounts:\n")
    f.write("  imports:      %d\n" % n_imports)
    f.write("  exports:      %d\n" % n_exports)
    f.write("  strings hit:  %d (of %d defined-data scanned)\n" % (n_strings, n_seen))
    f.write("  steam xrefs:  %d\n" % n_steam)
    f.close()
    log("summary: done")

    log("=== Done ===")

except Exception:
    log("EXCEPTION:")
    log(traceback.format_exc())
finally:
    status.close()
