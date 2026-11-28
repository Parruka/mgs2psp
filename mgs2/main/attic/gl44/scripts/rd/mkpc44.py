"""mkpc44.py <emulator log.txt of the package> <out rd44pc.py> (gl44: mkpc43.py with sched44 and rd44pc.tpl.py; gl43: build_gl42/rd/mkpc42.py with sched43 and rd43pc.tpl.py): fills rd43pc.tpl.py's fingerprints (the emulator run's hash lines, digest
lines and content lines, as rd42pc.py computes them) and the schedule (build_gl43/tools/sched43.py)."""
import sys, os, re, hashlib
sys.path.insert(0, '/home/claude/main/build_gl44/tools'); import sched44 as sched42
def md(x): return hashlib.md5('\n'.join(x).encode()).hexdigest()[:12]
H, D, C = [], [], []
for s in open(sys.argv[1], encoding='latin-1').read().splitlines():
    m = re.match(r'hash: vb (\d+) ee ([0-9a-f]+) iop ([0-9a-f]+)', s)
    if m: H.append('%s %s %s' % m.groups()); continue
    m = re.match(r'picture: vb (\d+) buffer \d+ list ([0-9a-f]+) frame ([0-9a-f]+):.*?; dz (.*)$', s)
    if m: D.append(('%s %s %s' % m.groups()[:3], m.group(4).strip())); continue
    m = re.match(r'picture: vb (\d+) content ([0-9a-f]+)$', s)
    if m: C.append('%s %s' % m.groups())
t = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'rd44pc.tpl.py')).read()
K = sched42.K
sc = repr([(t_, 24000 + 100 * t_, K[t_], sched42.NGE[K[t_]], sched42.BOUND[K[t_]], sched42.dig(t_), sched42.xp(t_), sched42.scene(t_), sched42.stretch(t_)) for t_ in range(sched42.N)])
for k, v in (('@@EH@@', md(H)), ('@@NH@@', str(len(H))), ('@@ED@@', md([a for a, _ in D])), ('@@EDZ@@', md([a + ' ' + z for a, z in D])), ('@@ND@@', str(len(D))),
             ('@@EC@@', md(C)), ('@@NC@@', str(len(C))), ('@@SCHED@@', sc)):
    assert k in t, k; t = t.replace(k, v)
import subprocess, re as _re, tempfile
nop = '[]'
tmp = sys.argv[2] + '.pass1.py'; open(tmp, 'w').write(t.replace('@@EXPNOP@@', '[]'))
r = subprocess.run([sys.executable, '-I', tmp, os.path.dirname(os.path.abspath(sys.argv[1]))], capture_output=True, text=True); os.remove(tmp)
for l in r.stdout.splitlines():
    if l.startswith('== SCHEDULE CHECK'):
        nop = repr(sorted(int(v) for v in _re.findall(r"\((\d+), 'K-N2 / K-N2R without people records'\)", l)))
t = t.replace('@@EXPNOP@@', nop); print('no-people turns (the emulator run):', nop)
open(sys.argv[2], 'w').write(t); print('hashes', len(H), md(H), 'digests', len(D), md([a for a, _ in D]), 'content', len(C), md(C))
