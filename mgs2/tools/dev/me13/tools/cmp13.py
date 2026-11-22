# cmp13.py <ref log> <me13 log> [from vb]: task #147: the me13 run's state hashes ("hash: vb N ...") and frames ("picture: vb N buffer B list L frame F",
# "picture: vb N content C") against the reference run's, up to the me13 run's last hash vblank (frames from <from vb> on: the take-back's cold start); the me13 totals and G0 lines
import sys, re
rh = re.compile(r'^hash: vb (\d+) (.*)$')
rl = re.compile(r'^picture: vb (\d+) buffer \d+ list ([0-9a-f]+) frame ([0-9a-f]+)')
rc = re.compile(r'^picture: vb (\d+) content ([0-9a-f]+)')
def load(fn):
    h, p, tail = {}, {}, []
    for line in open(fn, errors='replace'):
        m = rh.match(line)
        if m: h[int(m.group(1))] = m.group(2); continue
        m = rl.match(line)
        if m: p.setdefault(int(m.group(1)), {})['lf'] = (m.group(2), m.group(3)); continue
        m = rc.match(line)
        if m: p.setdefault(int(m.group(1)), {})['c'] = m.group(2); continue
        if line.startswith('me13 dl totals') or line.startswith('me13 g0') or line.startswith('me9 totals') or line.startswith('me13 log') or line.startswith('me13 lw'): tail.append(line.rstrip())
    return h, p, tail
rh_, rp, _ = load(sys.argv[1]); mh, mp, tail = load(sys.argv[2])
last = max(mh) if mh else 0; fr = int(sys.argv[3]) if len(sys.argv) > 3 else 0
hv = [v for v in sorted(mh) if v in rh_]
hd = [v for v in hv if mh[v] != rh_[v]]
pv = sorted(v for v in set(rp) | set(mp) if fr <= v <= last)
miss = [v for v in pv if v not in rp or v not in mp]
dl = [v for v in pv if v in rp and v in mp and rp[v].get('lf', ('', ''))[0] != mp[v].get('lf', ('', ''))[0]]
dfr = [v for v in pv if v in rp and v in mp and rp[v].get('lf') != mp[v].get('lf')]
dc = [v for v in pv if v in rp and v in mp and rp[v].get('c') != mp[v].get('c')]
anyd = sorted(set(dfr) | set(dc))
print('%s: %sto vb %d: hash lines %d compared, %d differ%s; frames %d (%d missing on one side), differing: list %d, list or frame %d, content %d, any %d%s'
      % (sys.argv[2].split('/')[-1], ('frames from vb %d ' % fr) if fr else '', last, len(hv), len(hd), (' (first vb %d)' % hd[0]) if hd else '', len(pv), len(miss), len(dl), len(dfr), len(dc),
         len(anyd), (' (first vb %s)' % ', '.join(map(str, anyd[:8]))) if anyd else ''))
for t in tail: print('  ' + t[:700])
