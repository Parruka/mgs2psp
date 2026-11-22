import bisect, subprocess, sys
# wwmap.py <boot> <ww file> <vb lo> <vb hi>: the watched writes in those vblanks with their writers' names (PIE base 0x555555554000, setarch -R)
syms = []
for l in subprocess.run(['nm', '-n', sys.argv[1]], capture_output=True, text=True).stdout.splitlines():
    p = l.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
addrs = [s[0] for s in syms]
lo, hi = int(sys.argv[3]), int(sys.argv[4])
for l in open(sys.argv[2]):
    p = l.split()
    if len(p) < 6 or p[0] != 'wwatch:': continue
    vb = int(p[2])
    if vb < lo or vb > hi: continue
    x = int(p[5], 16) - 0x555555554000
    i = bisect.bisect_right(addrs, x) - 1
    print(vb, p[3], '%s+%x' % (syms[i][1], x - syms[i][0]), ' '.join(p[6:]))
