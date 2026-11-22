# bkread.py <tag> [<tag2>] (task #160): the bucket model's split of one window (sb17km/model/<tag>.buckets + log_<tag>.txt), ms a step:
# parts (logic = actors + key 0; draw pipeline, GS walk, IOP, picture = its own work + the converters' calls), logic by actor, picture by
# converter entry and owner.  With <tag2>: per row, tag2 - tag.  ms = (1.647 instr + 68.2 I-miss + 85.9 D-miss) / 333,000 / steps.
# STEPS=80 by default; NAMES=1 prints the act / owner tables too.
import os, re, sys, collections
D = '/home/claude/sb17km/model/'; P = '/mnt/project-files/mgs2/prof/'
ST = float(os.environ.get('STEPS', '80')); W = (1.647, 68.2, 85.9)
def ms(v): return (W[0] * v[0] + W[1] * v[1] + W[2] * v[2]) / 333e3 / ST
ENT = {0x1991d8: 'rooms df0', 0x19a9f0: 'fans 25f0', 0x19ac78: 'strips 2870', 0x19fe08: 'people P06', 0x1a0160: 'people P06S',
       0x1a0540: 'people P06T', 0x1a08c0: 'people P06ST', 0x1a31e0: 'codec P07', 0x19bb50: 'sprites 3740', 0x19be50: 'quads 3a38',
       0x19af68: 'streaks 2b60'}
REG = {0: 'IOP+sound', 1: 'GS walk', 2: 'draw pipeline', 3: 'translator', 4: 'picture: frame work', 5: 'GE wait'}
OWN = {0: '-', 0xfff1: 'no source', 0xfff2: 'no block', 0xfff3: 'no owner', 0xfffe: 'passes', 0xffef: 'table full'}
FALLBACK = {}
for fn in ('actors_free_v26.txt', 'actors_busy_v22.txt'):
    try:
        for ln in open(P + fn):
            m = re.match(r'([0-9a-f]{6}) (.+?)\s{2,}', ln)
            if m: FALLBACK.setdefault(int(m.group(1), 16), re.sub(r' x\d+$', '', m.group(2).strip()))
    except OSError: pass
def load(tag):
    bk = {}
    for ln in open(D + tag + '.buckets'):
        p = ln.split()
        if len(p) == 4: bk[int(p[0], 16)] = [int(p[1]), int(p[2]), int(p[3])]
    ents, owners, acts = {}, {}, {}
    for ln in open(D + 'log_' + tag + '.txt', errors='replace'):
        if not ln.startswith('fxbk: '): continue
        if ln.startswith('fxbk: entries'):
            for m in re.finditer(r' (\d+)=([0-9a-f]+)\((\d+)\)', ln): ents[int(m.group(1))] = int(m.group(2), 16)
        m = re.match(r'fxbk: owner (\d+) ([0-9a-f]+) (\d+) (.*)', ln)
        if m: owners[int(m.group(1))] = m.group(4).strip() or '?'
        m = re.match(r'fxbk: act ([0-9a-f]+) level (\d+) (.*)', ln)
        if m: acts[int(m.group(1), 16)] = m.group(3).strip() or '?'
    return bk, ents, owners, acts
def rows(tag):
    bk, ents, owners, acts = load(tag); R = collections.OrderedDict(); part = collections.defaultdict(float)
    for k, v in bk.items():
        t = k >> 24
        if t == 0xfd: part['(measurement: owner look-up)'] += ms(v); continue
        if t == 0xff:
            r = (k >> 16) & 0xff
            if r == 4: part['picture'] += ms(v); R[('picture', 'frame work, the calls\' wrapper')] = ms(v)
            else: part[REG.get(r, 'region?')] += ms(v)
            continue
        if t == 0xfe:
            e = ents.get((k >> 16) & 0xff, 0); o = k & 0xffff
            en = ENT.get(e, '%06x' % e); on = OWN.get(o, owners.get(o, 'owner %d' % o))
            part['picture'] += ms(v); key = ('picture', en + ' / ' + on); R[key] = R.get(key, 0) + ms(v); continue
        part['logic'] += ms(v)
        nm = 'main loop, scheduler (key 0)' if k == 0 else acts.get(k) or FALLBACK.get(k) or '?'
        key = ('logic', '%06x %s' % (k, nm)); R[key] = R.get(key, 0) + ms(v)
    return part, R
def show(tags):
    A = [rows(t) for t in tags]
    names = ['logic', 'draw pipeline', 'IOP+sound', 'GS walk', 'translator', 'picture', 'GE wait', '(measurement: owner look-up)']
    print('%-34s' % 'part' + ''.join('%12s' % t for t in tags) + ('%10s' % 'diff' if len(tags) == 2 else ''))
    tot = [sum(v for k, v in a[0].items() if not k.startswith('(')) for a in A]
    for n in names:
        x = [a[0].get(n, 0) for a in A]
        print('%-34s' % n + ''.join('%12.2f' % y for y in x) + ('%+10.2f' % (x[1] - x[0]) if len(x) == 2 else ''))
    print('%-34s' % 'total (no look-up)' + ''.join('%12.2f' % y for y in tot) + ('%+10.2f' % (tot[1] - tot[0]) if len(tot) == 2 else ''))
    for grp in ('logic', 'picture'):
        keys = set()
        for a in A: keys |= {k for k in a[1] if k[0] == grp}
        lst = sorted(keys, key=lambda k: -max(a[1].get(k, 0) for a in A))
        print('\n%s, by %s' % (grp, 'actor (act function)' if grp == 'logic' else 'converter entry / owner'))
        for k in lst:
            x = [a[1].get(k, 0) for a in A]
            if max(abs(y) for y in x) < 0.05: continue
            print('  %-52s' % k[1][:52] + ''.join('%10.2f' % y for y in x) + ('%+9.2f' % (x[1] - x[0]) if len(x) == 2 else ''))
show(sys.argv[1:])
