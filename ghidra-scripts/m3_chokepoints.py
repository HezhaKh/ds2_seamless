# -*- coding: utf-8 -*-
# M3 chokepoints -- given the anchor strings from m3_recon, decompile the
# candidate functions and dump their callers. The goal is to identify where
# enemy/boss HP and damage are read so the scaling hook can wrap that read.
#
# Output: recon/chokepoints_m3.txt
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

out_path = os.path.join(out_dir, "chokepoints_m3.txt")
out = open(out_path, "w")

def w(s=""):
    out.write(str(s) + "\n")
    out.flush()

def pln(s):
    try: println("[m3_chokepoints] " + s)
    except: pass

pln("started, out=" + out_path)

addr_factory = currentProgram.getAddressFactory()
fn_mgr       = currentProgram.getFunctionManager()
ref_mgr      = currentProgram.getReferenceManager()

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task    import ConsoleTaskMonitor

try:
    deco = DecompInterface()
    deco.openProgram(currentProgram)
    monitor = ConsoleTaskMonitor()
except Exception:
    pln("decompiler init failed")
    deco = None
    monitor = None

def addr(hex_no_prefix):
    return addr_factory.getAddress(hex_no_prefix)

def fn_containing(a):
    f = fn_mgr.getFunctionContaining(a)
    if f: return f
    return fn_mgr.getFunctionAt(a)

def decompile(fn, max_lines=300):
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
        nm = caller.getName() if caller else "<no fn>"
        ent = str(caller.getEntryPoint()) if caller else ""
        w("  from %s  in %s @ %s" % (str(r.getFromAddress()), nm, ent))
        cnt += 1
    if cnt == 0:
        w("  (no callers found)")
    w("")

# Anchor data addresses -- xrefs to these are where the string gets used,
# i.e. the param-registration or table-lookup code.
DATA_ANCHORS = [
    ("EnemyParam_strref",      "1410f1570"),
    ("EnemyDamageParam_strref","1410f22a0"),
    ("BossBattleParam_strref", "1410f3460"),
    ("ChrNetworkPhantomParam_strref",       "1410f2080"),
    ("ChrNetworkPhantomSoulRateParam_strref","1410f20b0"),
    ("ChrPhantomParam_strref", "1410f3c80"),
    ("PlayerStatusParam_strref","1410f1f40"),
    ("NpcPlayerStatusParam_strref","1410f1f98"),
    ("ChrCommonParam_strref",  "1410f1508"),
]

# Functions to fully decompile -- discovered as "first_xref_fn" of the anchor strings
# in strings_scaling.csv.
FN_ANCHORS = [
    ("ParamRegistry_FUN_14048b620", "14048b620"),
    ("EnemyDamage_FUN_14048ba80",   "14048ba80"),
    ("BossBattle_FUN_14048bf00",    "14048bf00"),
    ("ChrPhantom_FUN_14048c980",    "14048c980"),
]

w("ds2sc M3 chokepoints")
w("====================")
w("Program: " + currentProgram.getName())
w("Image base: " + str(currentProgram.getImageBase()))
w("")

# Data anchors -- list every caller of the string. This tells us which
# functions touch each scaling-relevant param table.
for label, addr_hex in DATA_ANCHORS:
    try:
        a = addr(addr_hex)
        w("=== %s ===" % label)
        w("Data: %s" % addr_hex)
        refs_to = ref_mgr.getReferencesTo(a)
        n = 0
        for r in refs_to:
            from_a = r.getFromAddress()
            caller = fn_mgr.getFunctionContaining(from_a)
            cs = "%s @ %s" % (caller.getName(), str(caller.getEntryPoint())) if caller else "<no fn>"
            w("  xref from %s  in  %s" % (str(from_a), cs))
            n += 1
        if n == 0: w("  (no xrefs)")
        w("")
        pln("processed " + label)
    except Exception:
        w("EXCEPTION %s:" % label)
        w(traceback.format_exc())

# Full decompile of candidate functions.
for label, addr_hex in FN_ANCHORS:
    try:
        a = addr(addr_hex)
        fn = fn_containing(a)
        w("=== %s ===" % label)
        if not fn:
            w("(no function at %s)" % addr_hex)
            w("")
            continue
        w("Function:  %s @ %s" % (fn.getName(), str(fn.getEntryPoint())))
        w("Signature: " + str(fn.getSignature(False)))
        w("Body size: %d bytes" % fn.getBody().getNumAddresses())
        w("")
        dump_callers(fn, "%s @ %s" % (fn.getName(), str(fn.getEntryPoint())))
        w("--- decompile ---")
        w(decompile(fn))
        w("")
        pln("processed " + label)
    except Exception:
        w("EXCEPTION %s:" % label)
        w(traceback.format_exc())

w("=== Done ===")
out.close()
pln("done")
