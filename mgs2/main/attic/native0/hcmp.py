#!/usr/bin/env python3
"""hcmp.py <log a> <log b>: the hash lines (ee, iop) and time lines (k_time) of two twin runs, per checkpoint: how many differ, by field,
and the first differing checkpoint of each."""
import sys, re
def load(fn):
    h, t = {}, {}
    for l in open(fn, errors='replace'):
        m = re.search(r'hash: vb (\d+) ee (\w+) iop (\w+)', l)
        if m: h[int(m.group(1))] = (m.group(2), m.group(3)); continue
        m = re.match(r'time: vb (\d+) k_time (\d+)', l)
        if m: t[int(m.group(1))] = m.group(2)
    return h, t
a = load(sys.argv[1]); b = load(sys.argv[2])
ks = sorted(set(a[0]) & set(b[0])); kt = sorted(set(a[1]) & set(b[1]))
de = [v for v in ks if a[0][v][0] != b[0][v][0]]; di = [v for v in ks if a[0][v][1] != b[0][v][1]]; dt = [v for v in kt if a[1][v] != b[1][v]]
print('checkpoints %d (time %d): ee differs %d (first vb %s), iop differs %d (first %s), k_time differs %d (first %s)' %
      (len(ks), len(kt), len(de), de[0] if de else '-', len(di), di[0] if di else '-', len(dt), dt[0] if dt else '-'))
