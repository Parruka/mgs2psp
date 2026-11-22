#!/usr/bin/env python3
"""rewrite.py <in.h> <out.h> <unit>: RDF( -> RDFS(site, ; RDFB( -> M1RDFB(site, pc, (v30, kind lwc1b: sites.py counts it as lwc1); I2F((uint32_t) (mtc1 only, not F2INT) -> I2FS(site, (uint32_t).  The site is
the guest pc of the statement (the /*hex*/ comment after it).  Writes <out.h>.sites: site_index unit pc kind context."""
import re, sys
src, dst, unit = sys.argv[1], sys.argv[2], sys.argv[3]
s = open(src).read()
pcre = re.compile(r'/\*([0-9a-f]{5,8})\*/')
pat = re.compile(r'RDFS\(|RDFB\(|RDF\(|I2F\(\(uint32_t\)(?!F2IN)')   # RDFB (v30): tools/mm_base.py's base + offset form
out = []; pos = 0; sites = []
base = int(sys.argv[4]) if len(sys.argv) > 4 else 0
for m in pat.finditer(s):
    pm = None
    for pm in pcre.finditer(s, max(0, m.start() - 300), m.start()): pass   # the statement's own pc: the last /*hex*/ before it
    pc = pm.group(1) if pm else '0'
    g = m.group(0); kind = 'lwc1s' if g.startswith('RDFS') else 'lwc1b' if g.startswith('RDFB') else 'lwc1' if g.startswith('RDF') else 'mtc1'
    # the statement and the next two after it, for context
    ctx = s[m.start():min(len(s), m.end() + 200)].replace('\n', ' ')
    idx = base + len(sites)
    sites.append('%d %s %s %s %s' % (idx, unit, pc, kind, ctx))
    out.append(s[pos:m.start()])
    out.append({'lwc1': 'M1RDF(%du, 0x%su, ', 'lwc1b': 'M1RDFB(%du, 0x%su, ', 'lwc1s': 'M1RDFS(%du, 0x%su, ', 'mtc1': 'M1I2F(%du, 0x%su, (uint32_t)'}[kind] % (idx, pc))
    pos = m.end()
out.append(s[pos:])
open(dst, 'w').write(''.join(out))
open(dst + '.sites', 'w').write('\n'.join(sites) + '\n')
print(unit, len(sites), 'sites:', ' '.join('%s %d' % (k, sum(1 for x in sites if (' %s ' % k) in x)) for k in ('lwc1', 'lwc1s', 'mtc1')))
