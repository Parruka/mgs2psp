#!/usr/bin/env python3
"""split36pc.py GL36_LOG GL35_LOG  (stdlib only; python -I; reads two log.txt files, writes only to stdout)
gl36's PSP turns split by BOTH of its levers, each turn compared with gl35's run on the same vblanks (same game state):
  turn t = (vb - 24001) // 100 for a vb inside the turn; ME turn when (start vb / 100) is even;
  gl36: reuse on when t % 4 < 2 (MGS2_NAT_REUSE_CYCLE=1,1,0,0), cull on when t % 8 < 4 (MGS2_NAT_CULL_CYCLE=1,1,1,1,0,0,0,0);
  gl35: keeps 15 when t % 4 < 2 (MGS2_NAT_KEEP_CYCLE=15,15,0,0), keeps 0 otherwise.
Per turn: the 'window vb B' mean ms a step (step time) and, in ME turns, the 'me turn vb A-B' walks / idle / main waits and the
'me chains' ms of chains 2 (people), 3 (ship), 6 (sorted prims + meshes), all as ms a step (turn ms / 100).
Effects (gl36 - gl35 per turn, so the scene cancels):
  cull  = mean d(C1) - mean d(C0), with the reuse state matched (gl35's arm is then the same on both sides);
  reuse = mean d(R1) - mean d(R0), cull matched; this still holds gl35's keeps-15-vs-0 difference (about -0.3 ms a step on gl35's PSP
          in rain ME turns [I]), so the reuse's own effect is this value PLUS gl35's keep effect;
  base  = mean d over R0 C0 turns (gl36's retrain + keep 1 + fixes against gl35's keep-0 turns)."""
import sys, re, collections

SCENES = [('d01t deck', 24000, 25100), ('steady deck', 26300, 28700), ('rain wait', 29200, 30400), ('rain walk', 30800, 31400)]

def parse(path):
    W, M, C = {}, {}, {}
    for l in open(path, encoding='utf-8', errors='replace'):
        m = re.match(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), '
                     r'GS walk ([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\)', l)
        if m:
            b = int(m.group(1)); W[b - 100] = dict(step=float(m.group(3)), logic=float(m.group(4)), gewait=float(m.group(10)))
            continue
        m = re.match(r'me turn vb (\d+)-(\d+) \(([\d.]+) ms\): the ME\'s own time \(ms\): records ([\d.]+), thinning ([\d.]+), '
                     r'fills ([\d.]+), walks ([\d.]+), hooks ([\d.]+) .*?idle ([\d.]+), all ([\d.]+);.*?the main CPU waited \(ms\): '
                     r'copies as they ended ([\d.]+), fills ([\d.]+), the arena ([\d.]+), the rest ([\d.]+)', l)
        if m:
            a = int(m.group(1)); n = (int(m.group(2)) - a) / 1.0
            if n <= 0: continue
            M[a] = dict(walks=float(m.group(7)) / n, hooks=float(m.group(8)) / n, idle=float(m.group(9)) / n,
                        mwait=(float(m.group(11)) + float(m.group(12)) + float(m.group(13))) / n)
            continue
        m = re.match(r'me chains vb (\d+)-(\d+) .*?chain: transfers, ms, converter calls, native ms \(groups\): (.*)', l)
        if m:
            a = int(m.group(1)); n = (int(m.group(2)) - a) / 1.0; d = {}
            if n <= 0: continue
            for k, ms in re.findall(r'(\d+): \d+, ([\d.]+), \d+, [\d.]+ \(\d+\)', m.group(3)):
                d['ch' + k] = float(ms) / n
            C[a] = d
    return W, M, C

def arms36(a):
    t = (a + 50 - 24001) // 100
    return (1 if t % 4 < 2 else 0), (1 if t % 8 < 4 else 0)

def main():
    W6, M6, C6 = parse(sys.argv[1]); W5, M5, C5 = parse(sys.argv[2])
    keys = ['step', 'logic', 'gewait', 'walks', 'idle', 'mwait', 'ch2', 'ch3', 'ch6']
    rows = []
    for a in sorted(W6):
        if a < 24000 or a not in W5: continue
        me = (a // 100) % 2 == 0
        r6 = dict(W6[a]); r5 = dict(W5[a])
        if me:
            r6.update(M6.get(a, {})); r6.update(C6.get(a, {})); r5.update(M5.get(a, {})); r5.update(C5.get(a, {}))
        R, Cc = arms36(a)
        rows.append((a, me, R, Cc, r6, r5))
    print('== PER TURN (ms a step): vb, turn, reuse, cull | gl36 step (gl35) d | ME: walks, idle, main waits, ch2, ch3, ch6 (gl36 - gl35)')
    for a, me, R, Cc, r6, r5 in rows:
        s = '%5d %-4s R%d C%d | %6.2f (%6.2f) %+6.2f' % (a, 'me' if me else 'main', R, Cc, r6['step'], r5['step'], r6['step'] - r5['step'])
        if me and 'walks' in r6 and 'walks' in r5:
            s += ' | ' + ' '.join('%s %5.1f%+5.1f' % (k, r6.get(k, 0), r6.get(k, 0) - r5.get(k, 0)) for k in ['walks', 'idle', 'mwait', 'ch2', 'ch3', 'ch6'])
        print(s)
    def mean(xs): return sum(xs) / len(xs) if xs else float('nan')
    for name, lo, hi in SCENES:
        print('\n== %s (turns starting vb %d-%d)' % (name, lo, hi - 100))
        for me in (True, False):
            sel = [x for x in rows if lo <= x[0] < hi and x[1] == me]
            if not sel: continue
            print('  %s turns: %d' % ('ME' if me else 'main', len(sel)))
            for k in (keys if me else ['step', 'logic', 'gewait']):
                d = collections.defaultdict(list); v = collections.defaultdict(list)
                for a, _, R, Cc, r6, r5 in sel:
                    if k in r6 and k in r5:
                        d[(R, Cc)].append(r6[k] - r5[k]); v[(R, Cc)].append(r6[k])
                if not d: continue
                cell = lambda R, Cc: ('%6.2f %+6.2f n%d' % (mean(v[(R, Cc)]), mean(d[(R, Cc)]), len(d[(R, Cc)]))) if d[(R, Cc)] else '   -  '
                cull = [mean(d[(R, 1)]) - mean(d[(R, 0)]) for R in (0, 1) if d[(R, 1)] and d[(R, 0)]]
                reuse = [mean(d[(1, Cc)]) - mean(d[(0, Cc)]) for Cc in (0, 1) if d[(1, Cc)] and d[(0, Cc)]]
                print('    %-6s R1C1 %s | R0C1 %s | R1C0 %s | R0C0 %s || cull %s | reuse(+gl35 keep) %s | base %s' % (
                    k, cell(1, 1), cell(0, 1), cell(1, 0), cell(0, 0),
                    ('%+.2f' % mean(cull)) if cull else '-', ('%+.2f' % mean(reuse)) if reuse else '-',
                    ('%+.2f' % mean(d[(0, 0)])) if d[(0, 0)] else '-'))
    print('\nCells: gl36 value, gl36 - gl35 on the same vblanks, number of turns. Small n: read with care.')

main()
