# tl6.py <log a> <log b> [...]: the lines cmp6.py leaves out as timings (its TIMING pattern), compared between the first log and each other
# one: how many such lines, how many differ, and for each differing pair the line with its numbers replaced by '#' (a line that still differs
# then is not a timing difference), plus the md5 of the timing-free lines that matched the pattern only through words such as "at a time".
import sys, re, gzip, hashlib, difflib
TIMING = re.compile(r'( in [0-9.]+ ms| [0-9.]+ ms|[0-9.]+ s\b|seconds|ms a |ms per|time|took|elapsed|wall)', re.I)
def lines(p):
    f = gzip.open(p, 'rt', errors='replace') if p.endswith('.gz') else open(p, errors='replace')
    return [l.rstrip('\n') for l in f if TIMING.search(l) and not l.startswith(('picture: vb ', 'hash: vb '))]
num = lambda l: re.sub(r'[0-9]+(\.[0-9]+)?', '#', l)
A = lines(sys.argv[1])
for p in sys.argv[2:]:
    B = lines(p)
    dd = [l for l in difflib.unified_diff(A, B, lineterm='', n=0) if not l.startswith(('---', '+++', '@@'))]
    shape = [l for l in difflib.unified_diff([num(x) for x in A], [num(x) for x in B], lineterm='', n=0) if not l.startswith(('---', '+++', '@@'))]
    print('%s vs %s: %d / %d timing-pattern lines, %d diff lines; with numbers masked %d' % (sys.argv[1].split('/')[-1], p.split('/')[-1], len(A), len(B), len(dd), len(shape)))
    for l in shape[:10]: print('   shape', l[:200])
    kinds = {}
    for l in dd: k = num(l[1:])[:60]; kinds[k] = kinds.get(k, 0) + 1
    for k, n in sorted(kinds.items(), key=lambda x: -x[1])[:12]: print('   %6d  %s' % (n, k))
