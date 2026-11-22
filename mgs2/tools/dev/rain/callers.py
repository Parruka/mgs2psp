# callers.py <hex addr>...: for each code address in w00a, the nearest function start at or below it that is a jal target or a
# character-table entry (a better start than "after jr ra"), and who calls that start (jal sites, with their own starts), two levels
import struct, sys, bisect
B = 0x1f1c00
d = open('/home/claude/mgs2/stage/w00a/nocache/w00a.bin', 'rb').read()
def w(a): return struct.unpack_from('<I', d, a - B)[0]
end_text = w(B + 4)
tab = {}; o = 0x10
while True:
    h, a = struct.unpack_from('<2I', d, o)
    if h == 0 and a == 0: break
    tab[a] = tab.get(a, []) + [h]; o += 8
calls = {}
for a in range(B + 0x10 + 8 * 149, end_text, 4):
    x = w(a)
    if x >> 26 == 3:
        t = ((a + 4) & 0xf0000000) | ((x & 0x3ffffff) << 2); calls.setdefault(t, []).append(a)
# data pointers into the text (function pointer tables): words in the image's data area equal to a text address
dptr = {}
for a in range(end_text, B + len(d) - 3, 4):
    v = w(a)
    if B <= v < end_text and v % 4 == 0: dptr.setdefault(v, []).append(a)
starts = sorted(set(calls) | set(tab) | set(dptr))
def fs(a):
    i = bisect.bisect_right(starts, a) - 1
    return starts[i]
def desc(f):
    s = '%06x' % f
    if f in tab: s += ' [table %s]' % ','.join('%x' % h for h in tab[f])
    if f in dptr: s += ' [data ptr at %s]' % ','.join('%06x' % p for p in dptr[f][:4])
    return s
for x in sys.argv[1:]:
    a = int(x, 16); f = fs(a)
    print('%06x in %s' % (a, desc(f)))
    for c in calls.get(f, [])[:12]:
        g = fs(c); print('   called at %06x in %s' % (c, desc(g)))
        for c2 in calls.get(g, [])[:6]:
            print('        called at %06x in %s' % (c2, desc(fs(c2))))
