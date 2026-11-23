"""mkpc39.py <emulator log.txt of the package> <out rd39pc.py>: fills rd39pc.tpl.py's fingerprints (the emulator run's hash lines and digest
lines, as rd39pc.py computes them) and the schedule (tools/sched39.py)."""
import sys, os, re, hashlib
sys.path.insert(0, '/home/claude/main/build_gl39/tools'); import sched39
def md(x): return hashlib.md5('\n'.join(x).encode()).hexdigest()[:12]
H, D = [], []
for s in open(sys.argv[1], encoding='latin-1').read().splitlines():
    m = re.match(r'hash: vb (\d+) ee ([0-9a-f]+) iop ([0-9a-f]+)', s)
    if m: H.append('%s %s %s' % m.groups()); continue
    m = re.match(r'picture: vb (\d+) buffer \d+ list ([0-9a-f]+) frame ([0-9a-f]+):.*?; dz (.*)$', s)
    if m: D.append(('%s %s %s' % m.groups()[:3], m.group(4).strip()))
t = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'rd39pc.tpl.py')).read()
sc = repr([(t_, 24000 + 100 * t_, sched39.S[t_][0], sched39.S[t_][1], sched39.S[t_][2], sched39.S[t_][3], 0 if t_ % 16 in sched39.M5OFF else 1, sched39.S[t_][4]) for t_ in range(sched39.N)])
for k, v in (('@@EH@@', md(H)), ('@@NH@@', str(len(H))), ('@@ED@@', md([a for a, _ in D])), ('@@EDZ@@', md([a + ' ' + z for a, z in D])), ('@@ND@@', str(len(D))), ('@@SCHED@@', sc)):
    assert k in t; t = t.replace(k, v)
open(sys.argv[2], 'w').write(t); print('hashes', len(H), md(H), 'digests', len(D), md([a for a, _ in D]))
