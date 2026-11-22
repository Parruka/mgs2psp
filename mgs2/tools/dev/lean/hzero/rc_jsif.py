"""rc_jsif.py <header>: the SIF command dispatcher's two table calls renamed JSIF (lean_plan.md 1b, dev), then rc4's regcache.py on the
file (RC_* options from the environment).  For mk_headers.sh as RCPY."""
import os, sys
p = sys.argv[1]; s = open(p).read(); n = 0
for pc in ('175d98', '175dd4'):
    old = '/*%s*/ { int64_t t_ = R[6]; R[31] = 0x%x; R[4] = R[29] + R[0]; JALR((uint32_t)t_); }' % (pc, int(pc, 16) + 8)
    if s.count(old) == 1: s = s.replace(old, old.replace('JALR(', 'JSIF(')); n += 1
open(p, 'w').write(s); print('rc_jsif: %d SIF table calls renamed JSIF' % n, flush=True)
os.execvp(sys.executable, [sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'regcache.py'), p])
