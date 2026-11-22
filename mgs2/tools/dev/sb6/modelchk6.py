# modelchk6.py <model dir> <sb5 model dir>: sb6's model runs (run6m.sh) read from their logs: per window the hash and vuh lines (off against
# on, and against sb5's run with lanes 1, 2 and 4 on: s5<w>on), the split finder's lines, and the memory lines (the break's highest point,
# the headroom at its lowest, the heap at the end).  Run with python3 -I.
import sys, re
M, M5 = sys.argv[1], sys.argv[2]
def rd(p):
    h, v, mem, split = {}, [], [], []
    for l in open(p, errors='replace'):
        l = l.rstrip('\n')
        if l.startswith('hash: vb '): q = l.split(); h[int(q[2])] = ' '.join(q[3:7])
        elif l.startswith('vuh:'): v.append(l)
        elif l.startswith('split') and 'reference hashes from' not in l: split.append(l)   # (a found split, not the line naming hashref.txt)
        elif l.startswith(('memory:', 'memory at the end', 'free memory now')): mem.append(l)
    return h, v, mem, split
for w in 'cdi':
    a, b, r = rd('%s/log_s6%sof.txt' % (M, w)), rd('%s/log_s6%son.txt' % (M, w)), rd('%s/log_s5%son.txt' % (M5, w))
    dh = [k for k in sorted(set(a[0]) | set(b[0])) if a[0].get(k) != b[0].get(k)]
    dr = [k for k in sorted(set(b[0]) | set(r[0])) if b[0].get(k) != r[0].get(k)]
    print('window %s: hash lines off %d, on %d, differ %d %s; on vs sb5 s5%son: %d, differ %d %s' % (w, len(a[0]), len(b[0]), len(dh), dh[:3], w, len(r[0]), len(dr), dr[:3]))
    print('  vuh lines off %d, on %d, the same %s; sb5 %d, the same as on %s; split lines %d / %d' % (len(a[1]), len(b[1]), a[1] == b[1], len(r[1]), r[1] == b[1], len(a[3]), len(b[3])))
    for nm, x in (('off', a), ('on', b), ('sb5', r)):
        for l in x[2]:
            if l.startswith(('memory:', 'memory at the end')): print('  %-3s %s' % (nm, l[:200]))
