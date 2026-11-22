#!/usr/bin/env python3
"""mk_cg.py <header dir> <out cg_closed.h> (lean_plan.md 1c, dev): the closed functions of each unit (regcache.py RC_RETLIVE's
RC_RET_SAVE files <header dir>/ret_<unit>.json) as sorted guest-address tables for cg_wrap.c, the x86 guard against a call through a
register (or from the kernel) reaching a closed function."""
import json, os, re, sys, glob
hd, out = sys.argv[1:3]
units = {}
for f in sorted(glob.glob(os.path.join(hd, 'ret_*.json'))):
    u = re.search(r'ret_(\w+)\.json$', f).group(1)
    units[u] = sorted(int(re.search(r'P([0-9a-f]+)$', k).group(1), 16) for k in json.load(open(f)))
o = ['/* mk_cg.py: the closed functions per unit (dev, x86 guard) */']
for u, a in units.items():
    o.append('static const uint32_t cg_%s[] = { %s };' % (u, ', '.join('0x%x' % x for x in a) or '0'))
o.append('static const struct { const char *name; const uint32_t *a; int n; } cg_stage[] = {')
o += ['  { "%s", cg_%s, %d },' % (u, u, len(a)) for u, a in units.items() if u != 'elf']
o.append('};')
open(out, 'w').write('\n'.join(o) + '\n')
print(', '.join('%s %d' % (u, len(a)) for u, a in units.items()))
