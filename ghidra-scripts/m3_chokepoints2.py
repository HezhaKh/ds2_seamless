# -*- coding: utf-8 -*-
# M3 chokepoints, second pass -- the first pass identified that the
# param-name lookup tables are just filename indexers (return L"param:/X.param").
# This pass walks one level outward to find the actual param-row reader
# and the EnemyParam -> ChrIns init path.
#
# Strategy:
#   * decompile the immediate callers of the filename indexers
#   * decompile any function that touches BossBattleParam / ChrPhantomParam
#     literals directly (they have 1 caller each, so single chokepoints)
#   * dump a list of any function whose name or body string contains "param:/"
#     references -- candidates for a "load param by name" service
#
# Output: recon/chokepoints_m3_pass2.txt
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

out_path = os.path.join(out_dir, "chokepoints_m3_pass2.txt")
out = open(out_path, "w")

def w(s=""):
    out.write(str(s) + "\n")
    out.flush()

def pln(s):
    try: println("[m3_chk2] " + s)
    except: pass

pln("started")

addr_factory = currentProgram.getAddressFactory()
fn_mgr       = currentProgram.getFunctionManager()
ref_mgr      = currentProgram.getReferenceManager()
listing      = currentProgram.getListing()

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task    import ConsoleTaskMonitor

deco = DecompInterface()
deco.openProgram(currentProgram)
monitor = ConsoleTaskMonitor()

def addr(hex_no_prefix):
    return addr_factory.getAddress(hex_no_prefix)

def fn_containing(a):
    f = fn_mgr.getFunctionContaining(a)
    if f: return f
    return fn_mgr.getFunctionAt(a)

def decompile(fn, max_lines=500):
    res = deco.decompileFunction(fn, 90, monitor)
    if not res.decompileCompleted():
        return "(decompile failed: " + str(res.getErrorMessage()) + ")"
    txt = res.getDecompiledFunction().getC()
    lines = txt.split("\n")
    if len(lines) > max_lines:
        lines = lines[:max_lines] + ["... (truncated, total %d lines)" % len(lines)]
    return "\n".join(lines)

def dump_callers(fn, label, decomp_each=False, max_callers=8):
    w("--- callers of %s ---" % label)
    refs = ref_mgr.getReferencesTo(fn.getEntryPoint())
    seen = set()
    cnt = 0
    cs = []
    for r in refs:
        caller = fn_mgr.getFunctionContaining(r.getFromAddress())
        if caller is None:
            w("  from %s  in <no fn>" % str(r.getFromAddress()))
            continue
        if caller.getEntryPoint() in seen: continue
        seen.add(caller.getEntryPoint())
        cs.append(caller)
        w("  from %s  in %s @ %s" % (str(r.getFromAddress()),
                                     caller.getName(), str(caller.getEntryPoint())))
        cnt += 1
    w("")
    if decomp_each:
        for c in cs[:max_callers]:
            w("--- caller decompile: %s @ %s ---" % (c.getName(), str(c.getEntryPoint())))
            w(decompile(c, 200))
            w("")

# 4 known interesting functions from pass 1 -- decompile + show callers
FN_TARGETS = [
    ("char_param_loader_FUN_140359bb0",    "140359bb0", True),   # calls 0x14048b620 indexer
    ("damage_param_loader_FUN_1403b5930",  "1403b5930", True),   # calls 0x14048ba80 indexer
    ("BossBattle_consumer_FUN_1404500b0",  "1404500b0", True),   # calls 0x14048bf00 (BossBattleParam getter)
    ("ChrPhantom_consumer_FUN_14024a110",  "14024a110", True),   # calls 0x14048c980 (ChrPhantomParam getter)
]

w("ds2sc M3 chokepoints (pass 2)")
w("=============================")
w("Program: " + currentProgram.getName())
w("")

for label, addr_hex, with_callers in FN_TARGETS:
    try:
        a = addr(addr_hex)
        fn = fn_containing(a)
        w("=== %s ===" % label)
        if not fn:
            w("(no function at %s)" % addr_hex)
            w("")
            continue
        w("Function:  %s @ %s" % (fn.getName(), str(fn.getEntryPoint())))
        w("Body size: %d bytes" % fn.getBody().getNumAddresses())
        w("")
        if with_callers:
            dump_callers(fn, "%s @ %s" % (fn.getName(), str(fn.getEntryPoint())), decomp_each=False)
        w("--- decompile ---")
        w(decompile(fn))
        w("")
        pln("processed " + label)
    except Exception:
        w("EXCEPTION %s:" % label)
        w(traceback.format_exc())

# Bonus: find all functions whose body contains the literal "EnemyParam"
# (i.e. functions other than the filename indexer that touch the string).
# We do this by enumerating strings whose value == "EnemyParam" and looking
# at every xref's enclosing function.
w("=== Every function that references the L\"EnemyParam\" string ===")
try:
    ENEMY_PARAM_ADDR = addr("1410f1570")
    refs = ref_mgr.getReferencesTo(ENEMY_PARAM_ADDR)
    by_fn = {}
    for r in refs:
        fn = fn_mgr.getFunctionContaining(r.getFromAddress())
        key = (fn.getName(), str(fn.getEntryPoint())) if fn else ("<no fn>", "")
        by_fn.setdefault(key, []).append(str(r.getFromAddress()))
    for k, xrefs in by_fn.items():
        w("  %s @ %s  (%d xref)" % (k[0], k[1], len(xrefs)))
    w("")
except Exception:
    w("EXCEPTION on EnemyParam scan:")
    w(traceback.format_exc())

# Hunt for "load param row by ID" style functions -- they tend to be the
# common backend behind dozens of param accesses. Heuristic: a small function
# with no string xrefs but many callers, that fits the (table*, id) -> row
# pattern. We surface "very-popular" small functions.
w("=== Top-20 small leaf functions by caller count ===")
try:
    candidates = []
    fi = fn_mgr.getFunctions(True)
    for f in fi:
        body = f.getBody().getNumAddresses()
        if body < 16 or body > 200: continue
        # ignore thunks
        if f.isThunk(): continue
        cc = 0
        for _ in ref_mgr.getReferencesTo(f.getEntryPoint()):
            cc += 1
            if cc > 9999: break
        if cc >= 30:
            candidates.append((cc, body, f))
    candidates.sort(reverse=True)
    for cc, body, f in candidates[:20]:
        w("  %s @ %s  body=%d bytes  callers=%d" % (f.getName(), str(f.getEntryPoint()), body, cc))
    w("")
except Exception:
    w("EXCEPTION on leaf scan:")
    w(traceback.format_exc())

w("=== Done ===")
out.close()
pln("done")
