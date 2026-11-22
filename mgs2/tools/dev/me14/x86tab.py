# x86tab.py <runs_f.log> <runs_r4.log> [<turns_cmp.txt>]: the notes' x86 table from cmp13.py's lines (task #149 step 2); turns_cmp.txt:
# '<d|i|c>: ' and cmp13.py's line for 2b in turns against sb14's own twin in the same turns (turns_ref.sh)
import sys, re
rows = {}
pat = re.compile(r'^(\S+) (\S+): out_\S+: (?:to vb (\d+)|frames from vb (\d+) to vb (\d+)): hash lines (\d+) compared, (\d+) differ; '
                 r'frames (\d+) \((\d+) missing on one side\), differing: list (\d+), list or frame (\d+), content (\d+), any (\d+)')
def take(p, prefix=''):
    for l in open(p, encoding='utf-8', errors='replace'):
        m = pat.match(l.strip())
        if m:
            tree, tag = m.group(1), m.group(2)
            rows[prefix + tag] = dict(hl=int(m.group(6)), hd=int(m.group(7)), fr=int(m.group(8)), miss=int(m.group(9)), anyd=int(m.group(13)),
                                       a=m.group(4), b=m.group(5))
take(sys.argv[1])
if len(sys.argv) > 2:
    for l in open(sys.argv[2], encoding='utf-8', errors='replace'):
        m = re.match(r'^([dic]): out_\S+: to vb (\d+): hash lines (\d+) compared, (\d+) differ; frames (\d+) \((\d+) missing on one side\), '
                     r'differing: list (\d+), list or frame (\d+), content (\d+), any (\d+)', l.strip())
        if m: rows['r4' + m.group(1)] = dict(hl=int(m.group(3)), hd=int(m.group(4)), fr=int(m.group(5)), miss=int(m.group(6)), anyd=int(m.group(10)), a=None, b=None)
if len(sys.argv) > 3:
    for l in open(sys.argv[3], encoding='utf-8', errors='replace'):
        m = re.match(r'^([dic]): out_\S+: to vb (\d+): hash lines (\d+) compared, (\d+) differ; frames (\d+) \((\d+) missing on one side\), '
                     r'differing: list (\d+), list or frame (\d+), content (\d+), any (\d+)', l.strip())
        if m: rows['tr' + m.group(1)] = dict(hl=int(m.group(3)), hd=int(m.group(4)), fr=int(m.group(5)), miss=int(m.group(6)), anyd=int(m.group(10)), a=None, b=None)
def cell(k):
    r = rows.get(k)
    if not r: return 'not run'
    return '%s / %s' % ('0' if not r['hd'] else '%d of %d' % (r['hd'], r['hl']), '0' if not (r['anyd'] or r['miss']) else '%d of %d (%d missing)' % (r['anyd'], r['fr'], r['miss']))
print('| run | deck (252 hash lines / 11,766 frames) | indoor (378 / 17,664) | codec (400 / 18,931) |')
print('|---|---|---|---|')
for name, keys in [('A: today\'s path, the picture code ME-safe', ('df0', 'if0', 'cf0')),
                   ('A with the PSP\'s heap: a 768 KB first region and the hooks', ('r4d', 'r4i', 'r4c')),
                   ('2a: step 1\'s split (the copy at its deadline, the guards\' log, the picture two steps late)', ('gdf0', 'gif0', 'gcf0')),
                   ('2b: 2a with the ME\'s loop as one file (`ge/xlme.c`)', ('g2df0', 'g2if0', 'g2cf0')),
                   ('2b, every switch following its record, four configurations in turns every 100 vblanks from vb 1,000, against sb14\'s own twin in the same turns', ('trd', 'tri', 'trc'))]:
    print('| %s | %s | %s | %s |' % (name, cell(keys[0]), cell(keys[1]), cell(keys[2])))
tb = [('gd15f15', 'deck'), ('gd60f15', 'deck'), ('gd158f15', 'deck'), ('gd240f15', 'deck'), ('gi330f15', 'indoor'), ('gc360f15', 'codec')]
print()
print('| take-back in 2a at a frame\'s end, the picture\'s state kept (TBK 15) | route | hash lines | frames after it, different |')
print('|---|---|---|---|')
for k, r in tb:
    x = rows.get(k)
    if not x: print('| %s | %s | not run | |' % (k, r)); continue
    print('| from vb %s%s | %s | %d, %s | %s, %s |' % (format(int(x['a']), ','), ' to %s' % format(int(x['b']), ',') if k == 'gd15f15' else '', r, x['hl'],
          '0 different' if not x['hd'] else '%d different' % x['hd'], format(x['fr'], ','), '0' if not (x['anyd'] or x['miss']) else '%d (%d missing)' % (x['anyd'], x['miss'])))
