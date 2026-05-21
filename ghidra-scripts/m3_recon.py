# -*- coding: utf-8 -*-
# M3 recon -- scan DarkSoulsII.exe for the enemy/boss HP/damage chokepoint
# that the scaling hook will live on. Companion to m2_recon.
#
# Strategy: DS2 (like other FRPG2 titles) names many internal classes/params
# in embedded debug strings. We grep defined-data for scaling-relevant tokens
# (Hp, NpcParam, ChrIns, etc.) and dump (address, string, xref count, first
# xref function) for human triage.
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

status_path = os.path.join(out_dir, "status_m3.log")
status = open(status_path, "w")

def log(msg):
    status.write(str(msg) + "\n")
    status.flush()
    println("[m3_recon] " + str(msg))

log("script started")
log("out_dir = " + out_dir)

# Tokens we search for in defined strings. Keep this narrow -- "hp" alone
# would match thousands of unrelated strings.
KEYWORDS = [
    # param names
    "NpcParam", "EnemyParam", "ChrInitParam", "PlayerStatusParam",
    "EnemyDamageParam", "AtkParam", "WepAbsorpParam",
    # class / instance names
    "ChrIns", "EnemyIns", "PlayerIns", "ChrCommon",
    "EnemyManager", "ChrManager",
    # stat fields the game logs
    "MaxHp", "MaxHP", "max_hp", "maxHp",
    "HitPoint", "hitpoint",
    "SetHp", "GetHp", "AddHp", "SubHp", "ApplyDamage", "TakeDamage",
    "_damageRate", "DamageRate", "damage_rate",
    "atkPower", "AtkPower", "AttackPower",
    "Defense", "Absorp", "absorp",
    # multiplier / scaling hints
    "scale", "Scale", "multiplier", "Multiplier",
    "PhantomParam", "MultiPlayCorrection",
    # boss tagging
    "isBoss", "IsBoss", "boss_", "_boss",
    "BossBattle", "BossRoom",
    # phantoms = co-op presence (multiplier source)
    "phantom", "Phantom", "summon", "Summon",
    "WhitePhantom", "BlackPhantom",
]

LOWER_KEYS = [k.lower() for k in KEYWORDS]

try:
    listing = currentProgram.getListing()
    ref_mgr = currentProgram.getReferenceManager()
    fn_mgr  = currentProgram.getFunctionManager()
    sym_table = currentProgram.getSymbolTable()
    log("got managers")

    # -------- strings_scaling.csv -----------------------------------------
    log("strings: scanning...")
    strings_path = os.path.join(out_dir, "strings_scaling.csv")
    f = open(strings_path, "w")
    f.write("address,type,value,xref_count,first_xref,first_xref_fn\n")
    n_hit = 0
    n_seen = 0
    di = listing.getDefinedData(True)
    while di.hasNext():
        d = di.next()
        n_seen += 1
        if d is None: continue
        dt = d.getDataType().getName().lower()
        if "string" not in dt and "char" not in dt and "unicode" not in dt:
            continue
        v = d.getValue()
        if v is None: continue
        s = str(v)
        s_lower = s.lower()
        if len(s) > 400: continue  # skip giant blobs / file paths
        hit = False
        for k in LOWER_KEYS:
            if k in s_lower:
                hit = True
                break
        if not hit: continue

        refs = ref_mgr.getReferencesTo(d.getAddress())
        xc = 0
        first_x = ""
        first_x_fn = ""
        for r in refs:
            if xc == 0:
                first_x = str(r.getFromAddress())
                fn = fn_mgr.getFunctionContaining(r.getFromAddress())
                first_x_fn = fn.getName() if fn else ""
            xc += 1
        s_clean = s.replace('"', "'").replace("\n", " ").replace("\r", " ")
        f.write('"%s","%s","%s",%d,"%s","%s"\n' % (
            str(d.getAddress()), d.getDataType().getName(),
            s_clean, xc, first_x, first_x_fn))
        n_hit += 1
    f.close()
    log("strings: %d hits / %d scanned -> %s" % (n_hit, n_seen, strings_path))

    # -------- symbols_scaling.csv -----------------------------------------
    # Class methods and global symbols that match our tokens by name.
    log("symbols: scanning...")
    sym_path = os.path.join(out_dir, "symbols_scaling.csv")
    f = open(sym_path, "w")
    f.write("name,address,namespace,xref_count\n")
    n_sym = 0
    it = sym_table.getAllSymbols(True)
    while it.hasNext():
        sym = it.next()
        if sym is None: continue
        n = sym.getName()
        if not n: continue
        n_lower = n.lower()
        hit = False
        for k in LOWER_KEYS:
            if k in n_lower:
                hit = True
                break
        if not hit: continue
        ns = sym.getParentNamespace().getName() if sym.getParentNamespace() else ""
        xc = 0
        for r in sym.getReferences():
            xc += 1
        n_clean = n.replace('"', "'")
        ns_clean = ns.replace('"', "'")
        f.write('"%s","%s","%s",%d\n' % (n_clean, str(sym.getAddress()), ns_clean, xc))
        n_sym += 1
    f.close()
    log("symbols: %d hits -> %s" % (n_sym, sym_path))

    # -------- summary_m3.txt ---------------------------------------------
    log("summary: writing...")
    summary_path = os.path.join(out_dir, "summary_m3.txt")
    f = open(summary_path, "w")
    f.write("ds2sc M3 recon summary\n")
    f.write("======================\n\n")
    f.write("Program: %s\n" % currentProgram.getName())
    f.write("Image base: %s\n\n" % str(currentProgram.getImageBase()))
    f.write("Keyword set:\n")
    for k in KEYWORDS:
        f.write("  " + k + "\n")
    f.write("\nResults:\n")
    f.write("  scaling strings: %d  (-> strings_scaling.csv)\n" % n_hit)
    f.write("  scaling symbols: %d  (-> symbols_scaling.csv)\n" % n_sym)
    f.close()
    log("=== Done ===")

except Exception:
    log("EXCEPTION:")
    log(traceback.format_exc())
finally:
    status.close()
