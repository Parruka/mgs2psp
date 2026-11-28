#!/usr/bin/env python3
"""gl45 batch 3: a generated header with one guest instruction (and one taken block) a line in the hot functions, for the PSP debug
compile whose line table ties each PSP instruction to its counter.  White space only: the code is the same (and the header's
#line marks, which keep the PGO build's file identity, are left out so that the line table names this file).
usage: g45_split.py <in.h> <out.h> <keys.json> <map.json>
map.json: {"lines": {line number (1-based): counter index}, "calls": {line number: counter index of the function's first instruction
(wrapper lines)}, "fn": {function or copy name: [first line, last line]}}; a NAME_s copy (the fallback) gets no counters."""
import re, sys, json, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import glib
src, dst, keysf, mapf = sys.argv[1:5]
L, fns, wr = glib.load2(src); K = json.load(open(keysf))
out = []; lines = {}; calls = {}; fnl = {}; i = 0; bad = []; skipped = []
def ext(ls): out.extend(l for l in ls if not l.startswith("#line "))     # the PGO build's #line marks would hide this file's own line numbers
for name, (a, b) in sorted(fns.items(), key=lambda x: x[1][0]):
    base = re.sub(r"_(s|f)$", "", name)
    ext(L[i:a]); i = b + 1
    if base not in K: ext(L[a:b + 1]); continue
    k0, ks = K[base]; n0 = len(out) + 1; out.append(L[a]); out.append(L[a + 1]); kp = 0; last = k0
    if not name.endswith("_s"): lines[n0] = lines[n0 + 1] = k0      # the opening lines: the entry's count
    for j in range(a + 2, b):
        if L[j].startswith("#line "): continue      # a mark inside a body: left out as well (it would rename every line after it)
        toks = list(glib.TOK.finditer(L[j])); kidx = []
        for m in toks:           # each token's counter: the keys in order; a key the text does not have (a twin-only goto) is passed over
            want = ("at", m.group(1), "") if m.group(1) else ("goto", "", m.group(2))
            while kp < len(ks) and tuple(ks[kp]) != want and ks[kp][0] == "goto": kp += 1; skipped.append(name)
            if kp >= len(ks) or tuple(ks[kp]) != want: bad.append(name); kidx.append(None)
            else: kidx.append(k0 + kp); kp += 1
        for text, k in glib.pieces(L[j]):
            if not text.strip(): continue
            out.append(text)
            if name.endswith("_s"): continue
            idx = kidx[k[1]] if k is not None else None
            if idx is None: lines[len(out)] = last
            else:
                lines[len(out)] = idx
                if k[0] == "at": last = idx
    if kp != len(ks) and not name.endswith("_s"): bad.append(name)
    out.append(L[b]); fnl[name] = [n0, len(out)]
ext(L[i:])
# wrappers: found again in the output
for n, l in enumerate(out):
    m = re.match(r"static void ((?:S_\w+?_)?P[0-9a-f]{6})\(void\) \{ if \(", l)
    if m and m.group(1) in K: calls[n + 1] = K[m.group(1)][0]
open(dst, "w").write("\n".join(out)); json.dump({"lines": lines, "calls": calls, "fn": fnl, "bad": sorted(set(bad))}, open(mapf, "w"))
print("g45_split: %s: %d functions split, %d lines with a counter, %d wrappers, token mismatch in %s, twin-only keys passed over %d" % (dst, len(fnl), len(lines), len(calls), sorted(set(bad))[:8], len(skipped)))
