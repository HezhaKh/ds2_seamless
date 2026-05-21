# -*- coding: utf-8 -*-
# M2 chokepoint analysis -- given the anchor addresses from m2_recon (login URL
# xref, .sl2 xref, CCallback object for LoginTaskForNP), identify the enclosing
# functions, decompile them, and list their callers.
#
# Output: recon/chokepoints.txt
#
# @author ds2sc
# @category ds2sc
# @runtime Jython

import os
import traceback

args = getScriptArgs()
out_dir = args[0] if args else r"C:\Users\h\ds2sc\recon"
if not os.path.isdir(out_dir):
    os.makedirs(out_dir)

out_path = os.path.join(out_dir, "chokepoints.txt")
out = open(out_path, "w")

def w(line=""):
    out.write(str(line) + "\n")
    out.flush()

def println_safe(s):
    try: println("[m2_chokepoints] " + s)
    except: pass

println_safe("started, out=" + out_path)

addr_factory = currentProgram.getAddressFactory()
fn_mgr       = currentProgram.getFunctionManager()
listing      = currentProgram.getListing()
ref_mgr      = currentProgram.getReferenceManager()
sym_table    = currentProgram.getSymbolTable()

# Lazy-import the decompiler — keeps the script working even if the interface
# moved between Ghidra versions.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task    import ConsoleTaskMonitor

try:
    deco = DecompInterface()
    deco.openProgram(currentProgram)
    monitor = ConsoleTaskMonitor()
except Exception:
    println_safe("decompiler init failed; will skip decompiled output")
    deco = None
    monitor = None

def addr(hex_no_prefix):
    return addr_factory.getAddress(hex_no_prefix)

def fn_at_or_containing(a):
    f = fn_mgr.getFunctionContaining(a)
    if f: return f
    return fn_mgr.getFunctionAt(a)

def decompile(fn, max_lines=400):
    if not deco: return "(decompiler unavailable)"
    res = deco.decompileFunction(fn, 60, monitor)
    if not res.decompileCompleted():
        return "(decompile failed: " + str(res.getErrorMessage()) + ")"
    txt = res.getDecompiledFunction().getC()
    lines = txt.split("\n")
    if len(lines) > max_lines:
        lines = lines[:max_lines] + ["... (truncated, total %d lines)" % len(lines)]
    return "\n".join(lines)

def dump_callers(fn, label):
    w("--- callers of %s ---" % label)
    entry = fn.getEntryPoint()
    refs = ref_mgr.getReferencesTo(entry)
    cnt = 0
    for r in refs:
        caller = fn_mgr.getFunctionContaining(r.getFromAddress())
        caller_name = caller.getName() if caller else "<no fn>"
        caller_entry = str(caller.getEntryPoint()) if caller else ""
        w("  from %s  in %s @ %s" % (str(r.getFromAddress()), caller_name, caller_entry))
        cnt += 1
    if cnt == 0:
        w("  (no callers found via ref_mgr; may be virtual/dispatched)")
    w("")

# ---------------------------------------------------------------------------
# Anchor list. Each entry: (label, instruction_address_with_xref_to_data)
# Some entries are data symbols (CCallback objects), some are code that
# references a key string. We resolve each to a function and decompile.
# ---------------------------------------------------------------------------
ANCHORS = [
    ("login_url_xref",  "14028a020", "MOV/LEA referencing 'frpg2-steam64-ope-login.fromsoftware-game.net'"),
    ("sl2_extension",   "140a8739e", "MOV/LEA referencing '.sl2'"),
    ("lobby_chat_log",  "140a73394", "MOV/LEA referencing the '_OnLobbyChatUpdate' debug string"),
    ("statue_bloodstain","140c8237d", "MOV/LEA referencing 'Frpg2RequestMessage.RequestCreateBloodstain'"),
    ("disconnect_user", "140c824fd", "MOV/LEA referencing 'Frpg2RequestMessage.RequestDisconnectUser'"),
]

# Also resolve the CCallback class object address -- its xrefs point to where
# the class is constructed (LoginTaskForNP instance), which is the actual login
# task creator function we want to find.
CCALLBACK_DATA = [
    ("LoginTaskForNP_CCallback", "141591ce0"),
    ("SteamSessionLight_ValidateAuthTicket", "1415c3110"),
    ("SteamSurveillance_ServersConnected",   "1415c2240"),
    ("SteamSurveillance_ServersDisconnected","1415c22a0"),
    ("SteamSessionLight_P2PConnectFail",     "1415c30b0"),
]

w("ds2sc M2 chokepoint analysis")
w("============================")
w("Program: " + currentProgram.getName())
w("Image base: " + str(currentProgram.getImageBase()))
w("")

# Anchors that are code addresses (instruction with xref) -- find containing fn
for label, addr_hex, desc in ANCHORS:
    try:
        a = addr(addr_hex)
        fn = fn_at_or_containing(a)
        w("=== %s ===" % label)
        w("Anchor:    %s   (%s)" % (addr_hex, desc))
        if not fn:
            w("Function:  (no containing function)")
            w("")
            continue
        w("Function:  %s @ %s" % (fn.getName(), str(fn.getEntryPoint())))
        w("Signature: " + str(fn.getSignature(False)))
        w("Body size: %d bytes" % fn.getBody().getNumAddresses())
        w("")
        dump_callers(fn, "%s (%s)" % (fn.getName(), str(fn.getEntryPoint())))
        w("--- decompile ---")
        w(decompile(fn))
        w("")
        println_safe("processed " + label)
    except Exception:
        w("EXCEPTION processing %s:" % label)
        w(traceback.format_exc())
        w("")

# CCallback data objects -- xrefs to them are where the class is constructed
for label, addr_hex in CCALLBACK_DATA:
    try:
        a = addr(addr_hex)
        w("=== %s (data object) ===" % label)
        w("Address: %s" % addr_hex)
        refs_to = ref_mgr.getReferencesTo(a)
        n = 0
        for r in refs_to:
            from_a = r.getFromAddress()
            caller = fn_mgr.getFunctionContaining(from_a)
            caller_str = "%s @ %s" % (caller.getName(), str(caller.getEntryPoint())) if caller else "<no fn>"
            w("  xref from %s   in  %s" % (str(from_a), caller_str))
            n += 1
        if n == 0:
            w("  (no xrefs)")
        w("")
        println_safe("processed " + label)
    except Exception:
        w("EXCEPTION processing %s:" % label)
        w(traceback.format_exc())
        w("")

# Also dump the steam_api64.dll!SteamAPI_Init caller chain -- the function that
# calls SteamAPI_Init is the Steam-init entry, useful context.
w("=== SteamAPI_Init caller(s) ===")
try:
    for sym in sym_table.getExternalSymbols():
        if sym.getName() != "SteamAPI_Init": continue
        if sym.getParentNamespace().getName().lower() != "steam_api64.dll": continue
        for ref in sym.getReferences():
            from_a = ref.getFromAddress()
            caller = fn_mgr.getFunctionContaining(from_a)
            caller_str = "%s @ %s" % (caller.getName(), str(caller.getEntryPoint())) if caller else "<no fn>"
            w("  from %s   in  %s" % (str(from_a), caller_str))
            if caller:
                w("--- decompile of SteamAPI_Init caller ---")
                w(decompile(caller, 200))
                w("")
except Exception:
    w("EXCEPTION:")
    w(traceback.format_exc())

w("=== Done ===")
out.close()
println_safe("done")
