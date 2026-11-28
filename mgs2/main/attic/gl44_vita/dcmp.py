import sys, struct
from collections import Counter
for vb in (24000, 27000, 30000, 33000, 36000):
    a = open('/home/claude/v44/dump/off.%d' % vb, 'rb').read(); b = open('/home/claude/v44/dump/on.%d' % vb, 'rb').read()
    if a == b: print(vb, 'equal'); continue
    n = len(a) // 4; wa = struct.unpack('<%dI' % n, a); wb = struct.unpack('<%dI' % n, b)
    d = [i * 4 for i in range(n) if wa[i] != wb[i]]
    # runs
    runs = []; s = p = d[0]
    for x in d[1:]:
        if x - p > 64: runs.append((s, p)); s = x
        p = x
    runs.append((s, p))
    c = Counter()
    for x in d:
        c['scratchpad (host 0..0x3fff)' if x < 0x4000 else 'kernel/elf 0x4000-0x1f1bff' if x < 0x1f1c00 else 'stage image 0x1f1c00-0x418f0f' if x < 0x418f10 else 'heap/stack 0x418f10+'] += 1
    print(vb, 'words differing', len(d), dict(c), 'runs', len(runs))
    for s, e in runs[:40]:
        cnt = sum(1 for x in d if s <= x <= e)
        print('   %07x-%07x %6d words' % (s, e + 3, cnt))
    if len(runs) > 40: print('   ... %d more runs' % (len(runs) - 40))
