# whoagg.py [-a] <log>...: the x86 census's "fxwho:" lines (ge/fxthin.c, MGS2_FX_WHO=1..3) summed by the actor's file name, per kind.
# -a: every owner (else the 14 largest).  The totals line is the census's own; "rows not read" are calls in rows whose line the log's
# other output broke (stdout and stderr share the file), so their name is not known here.
import sys, re, collections
K = ["sprites +3740", "quads +3a38", "streaks +2b60", "fans +25f0", "strips +2870"]
args = sys.argv[1:]; allrows = '-a' in args; args = [a for a in args if a != '-a']
for path in args:
    agg = {k: collections.Counter() for k in K}; tot = {}; sh = {}
    for l in open(path, errors='replace'):
        if l.startswith('fxwhosh: calls from a block'):
            for k in K:
                m = re.search(re.escape(k) + r' (\d+);', l)
                if m: sh[k] = int(m.group(1))
            continue
        if not l.startswith('fxwho: '): continue
        if l.startswith('fxwho: calls by kind'):
            for k in K:
                m = re.search(re.escape(k) + r' (\d+) \(no src (\d+), no block (\d+), no owner (\d+)\)', l)
                tot[k] = tuple(int(x) for x in m.groups())
            continue
        body = l[7:].rstrip('\n')
        name, rest = body[:28].strip(), body[28:]
        for k in K:
            m = re.search(re.escape(k) + r' (\d+)', rest)
            if m: agg[k][name] += int(m.group(1))
    print('==', path)
    for k in K:
        c = agg[k]; s = sum(c.values()); t = tot[k]; lost = t[0] - t[1] - t[2] - t[3] - s
        print('%s: calls %d, owned %d%s, no block %d, no owner %d%s' % (k, t[0], s, ', rows not read %d' % lost if lost else '', t[2], t[3],
              ', from blocks more than one actor points at %d' % sh[k] if k in sh else ''))
        print('   ' + '; '.join('%s %d' % (n, v) for n, v in c.most_common(None if allrows else 14)))
