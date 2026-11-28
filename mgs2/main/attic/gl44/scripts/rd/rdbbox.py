#!/usr/bin/env python3
# rdbbox.py [bbox.txt | folder] [log.txt]  (gl44 block A, H2; run it with python3 -I)
# Reads the black box the PSP harness writes once a second (psp/bbox.c, MGS2_BBOX=1): prints the last 20 records, then which case the
# end is:
#   - "the game counter STOPPED while the heartbeat went on N s": the game thread's counters (n, its phase changes, and v, the game's
#     vblank) stopped at a record and the black box thread wrote N more seconds of records: a stall the system survived for N s (a wait,
#     a hang in our code or in a system call); the phase tag names where the game thread was;
#   - "BOTH END IN THE SAME SECOND": the game counter moved up to the last record: the whole chip stopped (interrupts off, a bus lock,
#     power), or the run was switched off while it ran;
#   - the run's own end (tag EN: the end screen, waiting for X) or the program's orderly exit (tag EX: the last record);
# and the last phase tag.  With a folder: <folder>/bbox.txt (or bbox_prev1.txt with -p1, bbox_prev2.txt with -p2) and <folder>/log.txt
# (or log_prev1.txt / log_prev2.txt), whose "disc:" lines name the file of the last disc read.
import sys, os, re

PH = {'ST': 'the start (before the game: probes, the crash catcher\'s question)', 'LG': 'the game\'s logic',
      'IO': 'the IOP and sound', 'GS': 'the GS walk', 'DP': 'the draw pipeline', 'XL': 'the translator', 'PI': 'the picture\'s work',
      'GW': 'waiting for the GE', 'MW': 'waiting for the ME\'s loop', 'DR': 'a disc pack read', 'LC': 'log commit (log.txt write)',
      'PT': 'scePowerTick', 'OV': 'a stage overlay load', 'SH': 'a shot\'s write', 'HK': 'the vblank hook (timing, window line)',
      'HS': 'the vblank hook\'s hash part (state hash when due)', 'HE': 'the vblank hook\'s end (shots\' turn, picture hand-over)',
      'EN': 'the run\'s end (summary, waiting for X)', 'EX': 'the program exiting in order'}
MW = ['a copy as the transfer ended', 'a fill', 'the arena (a walk)', 'a walk from RAM', 'room in the queue', 'the thinning\'s slot',
      'the queue emptied', 'a heartbeat (hook probe)', 'a hand-over to the main CPU (SYNC)', 'the walk before at a hook']
GE = {'-': 'no list yet', '0': 'completed', '1': 'queued', '2': 'drawing', '3': 'stall reached', '4': 'paused/cancelled', 'E': 'error (id gone)'}
REC = re.compile(r's(\d{7}) t(\d{10}) c(\d{6}) v(\d{5}) n([0-9a-f]{5}) p(..)([0-9a-f]{2}) d([0-9a-f]{6})\+([0-9a-f]{3}) f(..) '
                 r'h([0-9a-f]{6}) a([0-9a-f]{4}) m([0-9a-f]{6}) q([0-9a-f]{2}) g([0-9a-f]{8}):(.) b(....),(...),(.)(.) l(\d{3}) w(\d{3}) *$')

def phase(tag, det):
    s = PH.get(tag, 'unknown tag ' + tag)
    if tag == 'MW':
        k = int(det, 16); s += ' (' + (MW[k] if k < len(MW) else 'kind %d' % k) + ')'
    return s

def main(a):
    gen = ''
    if '-p1' in a: gen = '_prev1'; a = [x for x in a if x != '-p1']
    if '-p2' in a: gen = '_prev2'; a = [x for x in a if x != '-p2']
    src = a[0] if a else '.'
    if os.path.isdir(src):
        bb = os.path.join(src, 'bbox%s.txt' % gen); lg = a[1] if len(a) > 1 else os.path.join(src, 'log%s.txt' % gen)
    else:
        bb = src; lg = a[1] if len(a) > 1 else os.path.join(os.path.dirname(src) or '.', 'log%s.txt' % gen)
    raw = open(bb, 'rb').read()
    print('rdbbox: %s, %d bytes (%d slots of 128)%s' % (bb, len(raw), len(raw) // 128, '' if len(raw) == 460800 else ' (NOT 460,800: not made at its full size?)'))
    recs, bad = [], 0
    for i in range(0, len(raw) - 127, 128):
        l = raw[i:i + 128]
        if l[:1] != b's':
            if l.strip(): bad += 1
            continue
        m = REC.match(l[:127].decode('ascii', 'replace'))
        if not m or l[127:128] != b'\n': bad += 1; continue
        g = m.groups()
        recs.append(dict(seq=int(g[0]), t=int(g[1]), c=int(g[2]), v=int(g[3]), n=int(g[4], 16), tag=g[5], det=g[6], lsn=int(g[7], 16),
                         k=int(g[8], 16), f=g[9], h=int(g[10], 16), at=int(g[11], 16), me=int(g[12], 16), q=int(g[13], 16),
                         gid=g[14], gst=g[15], mv=g[16], pc=g[17], chg=g[18], ext=g[19], late=int(g[20]), w=int(g[21]),
                         line=l[:127].decode('ascii').rstrip()))
    if not recs:
        print('rdbbox: no records (%d unreadable slots): the black box was off, or the run stopped before record 0' % bad); return 1
    recs.sort(key=lambda r: r['seq'])
    # the run's records: the last chain of consecutive sequence numbers (slots older than a wrap of 3,600 are overwritten)
    j = len(recs) - 1
    while j > 0 and recs[j - 1]['seq'] == recs[j]['seq'] - 1: j -= 1
    run = recs[j:]
    for k in range(1, len(run)):   # t is written modulo 10,000 s (10 digits of us): unwrap it along the chain
        while run[k]['t'] < run[k - 1]['t']: run[k]['t'] += 10 ** 10
    print('rdbbox: %d records read (%d unreadable slots); the run\'s chain: seq %d to %d (%d records, %.0f s of records)%s' % (
        len(recs), bad, run[0]['seq'], run[-1]['seq'], len(run), (run[-1]['t'] - run[0]['t']) / 1e6,
        '' if j == 0 else '; %d older records not in it' % j))
    files = []
    if os.path.isfile(lg):
        for l in open(lg, 'rb').read().decode('latin-1').splitlines():
            m = re.match(r'disc: (\S+)\s+lsn\s+([0-9a-f]+) size (\d+) \(pack\)', l)
            if m: files.append((int(m.group(2), 16), int(m.group(3)), m.group(1)))
    def disc(r):
        s = 'sector %06x, %d sectors' % (r['lsn'], r['k'])
        for lsn, size, p in files:
            if lsn <= r['lsn'] < lsn + (size + 2047) // 2048:
                return '%s: %s at byte %d, %d KB' % (s, p, (r['lsn'] - lsn) * 2048, r['k'] * 2)
        return s + ('' if not files else ' (no file of the log\'s disc table holds it)')
    print('\n== the last 20 records (s seq, t us, c PSP display vblanks, v game vblank, n game thread\'s phase changes, p phase, d last disc read,'
          ' f picture turn/where, h loop heart, a loop place, m ME heart, q entries waiting, g GE list:state, b mV,%,charging,external,'
          ' l wake lateness ms, w write before ms)')
    for r in run[-20:]: print('  ' + r['line'])
    last = run[-1]
    # the game counter: n (and v) as in the record before
    i = len(run) - 1
    while i > 0 and (run[i]['n'], run[i]['v']) == (run[i - 1]['n'], run[i - 1]['v']): i -= 1
    print('\n== the end')
    print('last record: seq %d, t %.1f s%s, game vblank %d, phase %s%s = %s' % (last['seq'], last['t'] / 1e6,
          ' (a record keeps t modulo 10,000 s: about %d s after the black box began, by its sequence number)' % last['seq']
          if last['t'] >= 10 ** 10 or last['seq'] >= 10000 else '', last['v'], last['tag'], last['det'], phase(last['tag'], last['det'])))
    if last['tag'] == 'EX':
        print('CASE: the program EXITED IN ORDER (the last record is the exit\'s); the game\'s last phase before it: %s' % (
            phase(run[-2]['tag'], run[-2]['det']) if len(run) > 1 else 'none'))
    elif last['tag'] == 'EN':
        e0 = next(r for r in run if r['tag'] == 'EN')
        print('CASE: the run REACHED ITS END (end screen, waiting for X) and the records stop at seq %d, %s%.1f s after the run\'s end began%s'
              % (last['seq'], 'at least ' if e0 is run[0] else '', (last['t'] - e0['t']) / 1e6,
                 ' (the end\'s first records are overwritten: the black box wraps after 3,600)' if e0 is run[0] else ''))
    elif i == len(run) - 1:
        print('CASE: BOTH END IN THE SAME SECOND: the game thread\'s counter moved up to the last record (n %05x -> %05x since the record '
              'before), then no more records: the whole chip stopped (interrupts off, a bus lock or the power), or the PSP was switched off '
              'while the game ran' % (run[-2]['n'] if len(run) > 1 else 0, last['n']))
    else:
        st = run[i]; n_s = (last['t'] - st['t']) / 1e6
        print('CASE: the game counter STOPPED while the heartbeat went on %.0f s (%d more records): the game thread\'s counters last moved by '
              'seq %d (t %.1f s, vblank %d); a stall the system survived for %.0f s' % (n_s, len(run) - 1 - i, st['seq'], st['t'] / 1e6, st['v'], n_s))
        print('  the game thread\'s phase since then: %s%s = %s' % (last['tag'], last['det'], phase(last['tag'], last['det'])))
        hm = last['h'] != st['h']; mm = last['me'] != st['me']; cm = last['c'] != st['c']
        print('  after the stop: the ME loop\'s heartbeat %s, the ME\'s own heartbeat %s, the PSP\'s display vblanks %s; picture %s; GE list %s %s; '
              'black box lateness up to %d ms' % ('MOVED' if hm else 'stopped', 'MOVED' if mm else 'stopped', 'moved' if cm else 'STOPPED',
              last['f'], last['gid'], GE.get(last['gst'], last['gst']), max(r['late'] for r in run[i:])))
    print('last phase tag: %s%s = %s' % (last['tag'], last['det'], phase(last['tag'], last['det'])))
    print('last disc read: %s' % disc(last))
    print('battery at the end: %s mV, %s %%, charging %s, external power %s' % (last['mv'], last['pc'], 'yes' if last['chg'] == 'c' else 'no',
                                                                               'yes' if last['ext'] == 'x' else 'no'))
    ws = [r['w'] for r in run[1:]]; ls = [r['late'] for r in run[1:]]
    if ws:
        print('the black box\'s own cost (ms, from its records): write time mean %.2f, max %d; wake lateness mean %.2f, max %d'
              % (sum(ws) / len(ws), max(ws), sum(ls) / len(ls), max(ls)))
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
