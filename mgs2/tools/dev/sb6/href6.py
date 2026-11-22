# href6.py (href5.py, reading .gz logs too) <hashref> <log>...: each log's state hash lines ("hash: vb N ee X iop Y vfb Z") against the route's reference
# (prof/hashref_x86_*_h2.txt: "N ee vfb"), and its vuh lines counted
import sys, gzip
H = {}
for l in open(sys.argv[1]):
    q = l.split()
    if len(q) >= 3 and q[0].isdigit(): H[int(q[0])] = q[1:]
for p in sys.argv[2:]:
    n = same = 0; diff = []; vuh = 0
    for l in (gzip.open(p, 'rt', errors='replace') if p.endswith('.gz') else open(p, errors='replace')):
        if l.startswith('vuh: vb '): vuh += 1
        if not l.startswith('hash: vb '): continue
        q = l.split(); vb = int(q[2])
        if vb not in H: continue
        n += 1
        if q[4] == H[vb][0] and q[8] == H[vb][1]: same += 1
        else: diff.append(vb)
    print('%s: %d hash lines with a reference entry, %d equal, differ %s; vuh lines %d' % (p, n, same, diff[:5], vuh))
