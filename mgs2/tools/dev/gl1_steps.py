#!/usr/bin/env python3
"""gl1_steps.py <steps.csv> [--from VB] [--win N] [--skip A:B] [--cap MS] [--list]: the live picture's timing windows (psp/gl1.sh,
MGS2T_gl1; renderer_notes 11.13 "3e"): mgs2.cfg's MGS2_OPT_CYCLE=<from>:<N>:<cfg list> gives each window of N steps one configuration in
turn (the cfg column).  A window's configuration shows in steps.csv from its second step on, so the windows are taken as the runs of
equal cfg in vblank order (a gap left by --skip also ends one).
Per configuration: steps, mean step and its parts in ms (logic = the step less IOP, GS walk, draw pipeline, translator, picture and GE
wait); the light steps (full draw-pipeline calls: a frame begins after each) and the frame steps (the others: the GS walk of those
calls' packets, the translator and the picture); GE wait per frame.  Then each window of a configuration other than 0 against the mean of
the nearest all-on windows before and after it (the route changes from one window to the next): mean and range of the differences.
Steps whose picture took more than --cap ms (default 200) are listed and left out of the means (single heavy steps follow the route, not
the configuration).  --skip A:B leaves those vblanks out (the digest window: its digests are inside the step's time).  --list prints each
window."""
import csv, sys

def args():
    a = sys.argv[1:]; o = dict(path=a[0], frm=31800, win=100, skip=None, cap=200.0, list=False)
    i = 1
    while i < len(a):
        if a[i] == '--from': o['frm'] = int(a[i + 1]); i += 2
        elif a[i] == '--win': o['win'] = int(a[i + 1]); i += 2
        elif a[i] == '--skip': x, y = a[i + 1].split(':'); o['skip'] = (int(x), int(y)); i += 2
        elif a[i] == '--cap': o['cap'] = float(a[i + 1]); i += 2
        elif a[i] == '--list': o['list'] = True; i += 1
        else: sys.exit('gl1_steps.py: unknown ' + a[i])
    return o

PARTS = ('step', 'logic', 'dg', 'iop', 'gs', 'xl', 'gl', 'gw')

def row_ms(r):
    s, i, g, d, x, l, w = (int(r[k]) for k in ('step_us', 'iop_us', 'gs_us', 'dg_us', 'xl_us', 'gl_us', 'gw_us'))
    return dict(step=s, logic=s - i - g - d - x - l - w, dg=d, iop=i, gs=g, xl=x, gl=l, gw=w)

def mean(rows, k): return sum(r[k] for r in rows) / len(rows) / 1000 if rows else float('nan')

def per_frame(rows, k):   # ms per frame: one frame per light step
    n = sum(1 for r in rows if r['full']); return sum(r[k] for r in rows) / n / 1000 if n else float('nan')

def main():
    o = args(); rows = []
    with open(o['path']) as f:
        for r in csv.DictReader(f):
            vb = int(r['vb'])
            if vb <= o['frm'] or (o['skip'] and o['skip'][0] <= vb <= o['skip'][1]): continue
            m = row_ms(r); m.update(vb=vb, cfg=int(r['cfg']), full=int(r['full']) > 0); rows.append(m)
    rows.sort(key=lambda r: r['vb'])
    cap = o['cap'] * 1000
    big = [r for r in rows if r['gl'] > cap]
    wins = []; prev = None
    for r in rows:
        if not wins or r['cfg'] != wins[-1][0]['cfg'] or r['vb'] != prev + 1: wins.append([])
        wins[-1].append(r); prev = r['vb']
    use = [r for r in rows if r['gl'] <= cap]
    cfgs = sorted(set(r['cfg'] for r in rows))
    print('cfg  steps   ' + '  '.join('%7s' % p for p in PARTS) + '   light steps (n, ms)   frame steps (n, ms)   GE wait a frame')
    for c in cfgs:
        rr = [r for r in use if r['cfg'] == c]; li = [r for r in rr if r['full']]; fr = [r for r in rr if not r['full']]
        print('%3d %6d   ' % (c, len(rr)) + '  '.join('%7.2f' % mean(rr, p) for p in PARTS) +
              '   %5d %7.2f          %5d %7.2f          %7.2f' % (len(li), mean(li, 'step'), len(fr), mean(fr, 'step'), per_frame(rr, 'gw')))
    if big:
        print('\nsteps over %.0f ms of picture (left out above and below):' % o['cap'])
        for r in big: print('  vb %d cfg %d: step %.2f ms, picture %.2f ms' % (r['vb'], r['cfg'], r['step'] / 1000, r['gl'] / 1000))
    wm = []
    for w in wins:
        u = [r for r in w if r['gl'] <= cap]
        wm.append(dict(cfg=w[0]['cfg'], a=w[0]['vb'], b=w[-1]['vb'], n=len(w), m={p: mean(u, p) for p in PARTS}, gwf=per_frame(u, 'gw')))
    if o['list']:
        print('\nwindows: first..last vb, cfg, steps, step / logic / GS walk / picture / GE wait (ms)')
        for w in wm:
            print('  %5d..%5d cfg %2d %4d   %7.2f %7.2f %7.2f %7.2f %7.2f' % (w['a'], w['b'], w['cfg'], w['n'], w['m']['step'], w['m']['logic'],
                                                                         w['m']['gs'], w['m']['gl'], w['m']['gw']))
    full = [i for i, w in enumerate(wm) if w['n'] >= o['win'] // 2]
    print('\neach window against the nearest all-on windows before and after it (ms a step):')
    for c in cfgs:
        if c == 0: continue
        d = []
        for i in full:
            if wm[i]['cfg'] != c: continue
            nb = [wm[j] for j in ([j for j in full if j < i and wm[j]['cfg'] == 0][-1:] + [j for j in full if j > i and wm[j]['cfg'] == 0][:1])]
            if not nb: continue
            d.append({p: wm[i]['m'][p] - sum(x['m'][p] for x in nb) / len(nb) for p in PARTS})
        if d:
            print('  cfg %2d: %2d windows, step %+.2f (from %+.2f to %+.2f); logic %+.2f, GS walk %+.2f, picture %+.2f, GE wait %+.2f' % (
                c, len(d), sum(x['step'] for x in d) / len(d), min(x['step'] for x in d), max(x['step'] for x in d),
                sum(x['logic'] for x in d) / len(d), sum(x['gs'] for x in d) / len(d), sum(x['gl'] for x in d) / len(d),
                sum(x['gw'] for x in d) / len(d)))

if __name__ == '__main__':
    main()
