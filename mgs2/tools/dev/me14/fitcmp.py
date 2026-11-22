# fitcmp.py <fit A> <fit B>: two profcat_fit.py outputs side by side by category (ms a step: total, instr, I, D), B - A
import sys, re
def rd(p):
    d = {}; tot = None
    for l in open(p, encoding='utf-8', errors='replace'):
        m = re.match(r'total ([\d.]+) ms a step .*instructions ([\d.]+) ms, I-misses ([\d.]+), D-misses ([\d.]+)', l)
        if m: tot = tuple(float(x) for x in m.groups()); continue
        m = re.match(r'(.+?)\s+([\d.]+) ms\s+[\d.]+%\s+\(instr\s+([\d.]+), I\s+([\d.]+), D\s+([\d.]+) ms\)', l)
        if m: d[m.group(1).strip()] = tuple(float(x) for x in m.groups()[1:])
    return tot, d
ta, a = rd(sys.argv[1]); tb, b = rd(sys.argv[2])
print('%-36s %8s %8s %7s   %6s %6s %6s' % ('category', 'A', 'B', 'B-A', 'instr', 'I', 'D'))
print('%-36s %8.2f %8.2f %+7.2f   %+6.2f %+6.2f %+6.2f' % ('TOTAL', ta[0], tb[0], tb[0]-ta[0], tb[1]-ta[1], tb[2]-ta[2], tb[3]-ta[3]))
ks = sorted(set(a) | set(b), key=lambda k: -max(a.get(k, (0,))[0], b.get(k, (0,))[0]))
for k in ks:
    x = a.get(k, (0, 0, 0, 0)); y = b.get(k, (0, 0, 0, 0))
    if max(x[0], y[0]) < 0.3 and abs(y[0]-x[0]) < 0.1: continue
    print('%-36s %8.2f %8.2f %+7.2f   %+6.2f %+6.2f %+6.2f' % (k, x[0], y[0], y[0]-x[0], y[1]-x[1], y[2]-x[2], y[3]-x[3]))
