#!/usr/bin/env python3
# score38.py [MODEL-DIR [PREFIX]] (PREFIX n: the no-PGO build's runs; gl38's scoring [C model], after model38.sh; python3 -I).  The port's cost model:
#   ms = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000, a window of 80 vblanks = 40 pictures.
# The model's own unit is ms a PICTURE (2 PSP steps); every figure here is printed in both: "pic" (as gl36's and gl37's notes) and
# "PSP step" (= pic / 2, the unit of the PSP log's "ms a step").  'all' windows count both processors' instructions and the waits
# that spin, so only differences mean anything, and a difference is WORK LEFT OUT ON BOTH PROCESSORS TOGETHER, not a step time.
import os, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__)); R = os.path.dirname(here)
d = sys.argv[1] if len(sys.argv) > 1 else os.path.join(R, "model"); X = sys.argv[2] if len(sys.argv) > 2 else ""; g36 = "/home/claude/main/build_gl36/model"
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
def tot(p):
    if not os.path.isfile(p): return None
    f = open(p).readline().split(); k = dict(zip(f[1::2], f[2::2])); return [int(k['total']) / 40, int(k['imiss']) / 40, int(k['dmiss']) / 40]
suf = lambda i: "" if i == 0 else ".%d" % i
SCH = {r[0]: r for r in eval(subprocess.run([sys.executable, "-I", os.path.join(here, "sched38.py"), "py"], capture_output=True, text=True, check=True).stdout)}
KN = ["K0 all drawn", "K1 ship off", "K2 people off", "K3 sorted off (rain kept)", "K4 rain off but the near sheet", "K5 stage program off", "K6 ship+people+sorted+rain off", "K7 nothing drawn, ME rests"]
print("== THE RETRAIN'S GUARD: gl38's final build against gl36's (the same cfg file, no bound turns), ms [C model]; limit +0.50 a picture (+0.25 a PSP step)")
G = [("g38c", "g36c", ["rain wait, ME turn 29,810-29,890", "rain wait, main turn 29,910-29,990", "rain walk, ME turn 31,010-31,090", "rain walk, main turn 31,110-31,190"], ["", ".1", ".2", ".3"], "cull 1, re-use 0"),
     ("g38rc", "g36rc", ["rain wait, ME turn 29,810-29,890", "rain wait, main turn 29,910-29,990", "rain walk, ME turn 31,010-31,090", "rain walk, main turn 31,110-31,190"], ["", ".1", ".2", ".3"], "cull 1, re-use 1 (gl36's every-run rule)"),
     ("g38rcd", "g36rcc1", ["opening, ME turn 16,010-16,090", "opening, main turn 16,110-16,190"], ["", ".1"], "cull 1, re-use 1"),
     ("g38rcd:2", "g36rcc2", ["steady deck, ME turn 27,410-27,490", "steady deck, main turn 27,510-27,590"], ["", ".1"], "cull 1, re-use 1")]
worst = None
for a, b, names, sf, what in G:
    a, _, o = a.partition(":"); o = int(o or 0); pm = []
    for i, n in enumerate(names):
        x, y = tot(os.path.join(d, X + a + suf(i + o))), tot(os.path.join(g36, b + sf[i]))
        if x is None or y is None: print("  %-36s %s: NOT RUN (%s / %s)" % (n, what, a + suf(i + o), b + sf[i])); continue
        dm = ms(x) - ms(y); pm.append(dm)
        if worst is None or dm > worst[0]: worst = (dm, n, what)
        print("  %-36s %-40s gl38 %7.2f gl36 %7.2f: %+6.2f pic = %+6.2f PSP step  (instr %+8.0f, I-miss %+6.0f, D-miss %+6.0f a picture)%s" % (
            n, what, ms(x), ms(y), dm, dm / 2, x[0] - y[0], x[1] - y[1], x[2] - y[2], "   OVER THE LIMIT" if dm > 0.5 else ""))
    if len(pm) >= 2:
        for j in range(0, len(pm) - 1, 2): print("     pair mean (ME turn + main turn): %+6.2f pic = %+6.2f PSP step%s" % ((pm[j] + pm[j + 1]) / 2, (pm[j] + pm[j + 1]) / 4, "   OVER THE LIMIT" if (pm[j] + pm[j + 1]) / 2 > 0.5 else ""))
if worst: print("  worst window: %+.2f ms a picture (%s, %s): %s" % (worst[0], worst[1], worst[2], "GUARD HOLDS in every window" if worst[0] <= 0.5 else "GUARD BROKEN (see above)"))
print("== EACH KIND IN ITS OWN TURN of the gl38 cfg's schedule (the same build, the same run), vblanks 10-90 of the turn, ms [C model]")
K = [("k38d", 1, 8), ("k38c", 27, 3), ("k38a", 51, 8), ("k38b", 59, 8), ("k38c:3", 68, 5)]
rows = {}
for a, t0, n in K:
    a, _, o = a.partition(":"); o = int(o or 0)
    for i in range(n):
        x = tot(os.path.join(d, X + a + suf(i + o))); t = t0 + i
        if x is None: continue
        _, vb, k, r, f1, scene = SCH[t]; rows.setdefault(scene, []).append((t, vb, k, r, f1, x))
NOTE = {"first deck scene": "a cutscene whose content moves from turn to turn: indicative only",
        "rain wait, first turns": "29,100-29,500: no frame digests (they begin at 29,490), the scene still settles; K0 at both ends",
        "rain wait": "29,500-30,800: frame digests in every turn; K0 turns in the middle; 30,600 is a second copy in a heavier part of the scene",
        "rain walk": "the content moves from turn to turn (ship groups a turn 22,900 / 14,800 / 14,400): indicative only"}
for scene, rs in rows.items():
    print(" -- %s%s" % (scene, " (%s)" % NOTE[scene] if scene in NOTE else ""))
    k0 = [q for q in rs if q[2] == 0 and q[4] == 0]
    for t, vb, k, r, f1, x in rs:
        ship = k in (0, 2, 3, 4, 5) and scene == "rain wait"   # the kinds that draw the ship: the re-use arm matters
        base = [q for q in k0 if q[3] == r] if ship else k0
        if k == 0 and f1 == 0: dl = "(a K0 turn)"
        elif not base: dl = "no K0 turn of this arm in the windows"
        else:
            bm = sum(ms(q[5]) for q in base) / len(base); bi = [sum(q[5][j] for q in base) / len(base) for j in range(3)]
            dm = ms(x) - bm
            dl = "MINUS K0%s (turn%s %s): %+7.2f pic = %+7.2f PSP step (instr %+9.0f, I-miss %+6.0f, D-miss %+6.0f a picture)" % (
                " R%d" % r if ship else "", "s" if len(base) > 1 else "", ",".join(str(q[1]) for q in base), dm, dm / 2, x[0] - bi[0], x[1] - bi[1], x[2] - bi[2])
        print("  vb %5d t%-2d %-31s R%d F1 %d: %8.2f pic %7.2f PSP step  %s" % (vb, t, KN[k], r, f1, ms(x), ms(x) / 2, dl))
    if len(k0) > 1:
        v = [ms(q[5]) for q in k0]; print("  the K0 turns among themselves (%s): spread %.2f pic = %.2f PSP step" % (", ".join("%d R%d" % (q[1], q[3]) for q in k0), max(v) - min(v), (max(v) - min(v)) / 2))
