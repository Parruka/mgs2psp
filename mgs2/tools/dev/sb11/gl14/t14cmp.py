# t14cmp.py <turns log> <C1 off log> <C1 on log> [from n ncfg onslots]: gl14's x86 turns check (task #136).  Every hash and vuh line must
# equal the C1-off run's (the picture never feeds the game).  A frame (its "picture: vb N" lines) whose vblanks N_prev..N+1 all lie in
# turns with the same C1 setting must equal that run's lines (the other turns, T1 off, T2 off and GE overlap off, are exact); frames
# across a change are counted apart (their calls may take both forms; the emulator's every-frame digests check those).
import sys
def load(p):
    fr, ct, hs, vu = {}, {}, {}, []
    for l in open(p, errors='replace'):
        l = l.rstrip('\n')
        if l.startswith('picture: vb ') and ' content ' in l: ct[int(l.split()[2])] = l; continue
        if l.startswith('picture: vb ') and ' buffer ' in l: fr[int(l.split()[2])] = l; continue
        if l.startswith('hash: vb '): hs[l.split()[2]] = l; continue
        if l.startswith('vuh'): vu.append(l)
    return fr, ct, hs, vu
T, F, O = load(sys.argv[1]), load(sys.argv[2]), load(sys.argv[3])
frm, n, ncfg = 1000, 100, 6; on = {2, 5}
if len(sys.argv) > 4: frm, n, ncfg = int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6]); on = set(int(x) for x in sys.argv[7].split(','))
def c1(v): return v > frm and ((v - frm - 1) // n) % ncfg in on
print('hash lines: turns %d, off %d, on %d; turns = off: %s, off = on: %s' % (len(T[2]), len(F[2]), len(O[2]), T[2] == F[2], F[2] == O[2]))
print('vuh lines: turns %d, off %d; equal %s' % (len(T[3]), len(F[3]), T[3] == F[3]))
for name, i in (('frame lines', 0), ('content lines', 1)):
    ks = sorted(T[i]); prev = None; st = {'pure_on': [0, 0, []], 'pure_off': [0, 0, []]}; bnd = [0, 0, 0, 0]
    for k in ks:
        p = prev if prev is not None else k - 2; prev = k
        s = set(c1(v) for v in range(p, k + 2))
        if len(s) == 1:
            o = s.pop(); key = 'pure_on' if o else 'pure_off'; ref = (O if o else F)[i]
            st[key][0] += 1
            if ref.get(k) == T[i][k]: st[key][1] += 1
            elif len(st[key][2]) < 5: st[key][2].append(k)
        else:
            bnd[0] += 1; a, b = F[i].get(k) == T[i][k], O[i].get(k) == T[i][k]
            bnd[1] += a and b; bnd[2] += a and not b; bnd[3] += b and not a
    missing = len(set(F[i]) - set(T[i]))
    print('%s: turns %d (off run %d, %d of its frames missing here)' % (name, len(T[i]), len(F[i]), missing))
    for key in ('pure_off', 'pure_on'):
        print('  %s: %d frames, equal %d, differ %d %s' % (key, st[key][0], st[key][1], st[key][0] - st[key][1], st[key][2]))
    print('  across a change: %d frames: equal to both %d, to off only %d, to on only %d, to neither %d' % (bnd[0], bnd[1], bnd[2], bnd[3], bnd[0] - bnd[1] - bnd[2] - bnd[3]))
