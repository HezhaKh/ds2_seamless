# -*- coding: utf-8 -*-
# M3 chokepoints, third pass -- look for the row-lookup-by-ID service.
# Three "leaf" functions in pass 2 had identical 2053-caller counts:
#   FUN_140832f50 (152 bytes)
#   FUN_140832e70 ( 71 bytes)
#   FUN_140832f10 ( 50 bytes)
# That sibling-callercount pattern usually means a smart-pointer triple
# (lock / get / unlock) on a single resource type. If one of them is the
# "get row by id" call, it's our hook chokepoint.
#
# Also decompile FUN_1402ddb60 (the central resource-by-name loader) to
# confirm what it returns.
#
# Output: recon/chokepoints_m3_pass3.txt
#
# @runtime Jython

import os, traceback
args = getScriptArgs()
out_dir = args[0] if args else r"C:\Users\h\ds2sc\recon"
out = open(os.path.join(out_dir, "chokepoints_m3_pass3.txt"), "w")
def w(s=""): out.write(str(s) + "\n"); out.flush()
def pln(s):
    try: println("[m3_chk3] " + s)
    except: pass

addr_factory = currentProgram.getAddressFactory()
fn_mgr = currentProgram.getFunctionManager()
ref_mgr = currentProgram.getReferenceManager()
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
deco = DecompInterface(); deco.openProgram(currentProgram); monitor = ConsoleTaskMonitor()
def addr(h): return addr_factory.getAddress(h)
def fn(a):  return fn_mgr.getFunctionContaining(addr(a)) or fn_mgr.getFunctionAt(addr(a))
def decompile(f, n=300):
    r = deco.decompileFunction(f, 90, monitor)
    if not r.decompileCompleted(): return "(decompile failed)"
    t = r.getDecompiledFunction().getC()
    ls = t.split("\n")
    return "\n".join(ls[:n] + (["...(truncated %d)" % len(ls)] if len(ls) > n else []))

TARGETS = [
    ("resource_by_name_FUN_1402ddb60", "1402ddb60"),
    ("leaf_152b_FUN_140832f50",        "140832f50"),
    ("leaf_71b_FUN_140832e70",         "140832e70"),
    ("leaf_50b_FUN_140832f10",         "140832f10"),
    # one more candidate from the same family
    ("leaf_18b_FUN_140833dc0",         "140833dc0"),  # used by both loaders
]

w("ds2sc M3 chokepoints pass 3")
w("===========================\n")

for label, a in TARGETS:
    try:
        f = fn(a)
        w("=== %s ===" % label)
        if not f:
            w("(no function)"); w(""); continue
        w("Function: %s @ %s" % (f.getName(), str(f.getEntryPoint())))
        w("Body: %d bytes" % f.getBody().getNumAddresses())
        # sample 5 caller functions
        seen = set(); ks = 0
        for r in ref_mgr.getReferencesTo(f.getEntryPoint()):
            c = fn_mgr.getFunctionContaining(r.getFromAddress())
            if c is None: continue
            if c.getEntryPoint() in seen: continue
            seen.add(c.getEntryPoint())
            w("  caller sample: %s @ %s" % (c.getName(), str(c.getEntryPoint())))
            ks += 1
            if ks >= 5: break
        w("--- decompile ---")
        w(decompile(f))
        w("")
        pln("done " + label)
    except Exception:
        w("EXC:"); w(traceback.format_exc())

w("=== Done ==="); out.close(); pln("done")
