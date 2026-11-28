#!/usr/bin/env python3
"""gl45 batch 3: g45_rows.py <size.json> [n]: the functions of one tools/g45_size.py result by slots a guest line (outliers first and last)"""
import json, sys, statistics
r = json.load(open(sys.argv[1])); K = json.load(open("/home/claude/g45/out/keys.json")); d = json.load(open("/home/claude/g45/out/bc_rain.json"))
n = int(sys.argv[2]) if len(sys.argv) > 2 else 8; rows = []
for fn, v in r.items():
    k0, ks = K[fn]; g = sum(d[k0 + i] for i, k in enumerate(ks) if k[0] == "at") / 1200
    rows.append((v["slots"] / g if g else 0, fn, v["slots"], g, v["calls"], "misfit %.0f of %.0f" % (v["misfit"], v["lsum"])))
rows.sort(reverse=True)
for x in rows[:n]: print("%8.2f %-16s slots %9.0f guest lines %8.0f calls %7.2f %s" % x)
print("...")
for x in rows[-n // 2:]: print("%8.2f %-16s slots %9.0f guest lines %8.0f calls %7.2f %s" % x)
print("median slots a guest line %.2f; total slots %.0f, guest lines %.0f" % (statistics.median(x[0] for x in rows if x[3] > 0), sum(x[2] for x in rows), sum(x[3] for x in rows)))
