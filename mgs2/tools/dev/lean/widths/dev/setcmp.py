#!/usr/bin/env python3
"""setcmp.py <dir A> <dir B>: the interprocedural (ipo_*.json) and return (ret_*.json) sets of two header runs, unit by unit"""
import sys, json, os
A, B = sys.argv[1:3]
for u in ['elf', 'init', 'r_title', 'r_tnk0', 'n_title', 'd00t', 'd01t', 'w00a', 'w01a']:
    for k in ('ipo', 'ret'):
        pa = os.path.join(A, '%s_%s.json' % (k, u)); pb = os.path.join(B, '%s_%s.json' % (k, u))
        if not (os.path.exists(pa) and os.path.exists(pb)): print(u, k, 'missing'); continue
        a = json.load(open(pa)); b = json.load(open(pb))
        d = [x for x in set(a) | set(b) if a.get(x) != b.get(x)]
        print('%-8s %s: %d / %d entries, %d differ %s' % (u, k, len(a), len(b), len(d), sorted(d)[:4]))
