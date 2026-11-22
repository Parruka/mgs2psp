# c4cmp.py <a.log> <b.log> [ref.log]: task #144, sb14's checks: every line of two x86 runs equal but for the lines that may differ by
# the settings (xlive mode:, the df0 C3 counters, timing: xlive time:, state hash 2's ms); with ref.log, the hash: lines against it
import sys, re
skip = re.compile(r'^(xlive mode:|xlive time:|state hash 2:)|df0 C3 ')
def lines(p):
    with open(p, errors='replace') as f:
        return [l.rstrip('\n') for l in f if not skip.search(l)]
a, b = lines(sys.argv[1]), lines(sys.argv[2])
n = 0
for i in range(max(len(a), len(b))):
    x = a[i] if i < len(a) else '<none>'; y = b[i] if i < len(b) else '<none>'
    if x != y:
        n += 1
        if n <= 6: print('  diff at line %d:\n    %s\n    %s' % (i + 1, x[:220], y[:220]))
print('%s vs %s: %d lines, %d lines, %d differ' % (sys.argv[1].split('/')[-1], sys.argv[2].split('/')[-1], len(a), len(b), n))
if len(sys.argv) > 3:
    h = lambda L: [l for l in L if l.startswith('hash:')]
    r = h(lines(sys.argv[3])); hb = h(b)
    print('  hash lines: %d, %d in the reference, %s' % (len(hb), len(r), 'equal' if hb == r else 'DIFFERENT'))
for l in b:
    if 'df0 C3' in l: print('  ' + l[:400])
