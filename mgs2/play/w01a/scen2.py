"""scen.py: play scenarios as fork-server lines.  Each scenario is a list of (start vblank offset, length, mask) presses relative to the
fork vblank; overlapping presses combine.  Masks: UP 1000 DOWN 4000 LEFT 8000 RIGHT 2000 CROSS 0040 CIRCLE 0020 SQUARE 0080
TRIANGLE 0010 R1 0008 L1 0004 R2 0002 L2 0001 SELECT 0100 START 0800."""
import sys
U, D, L, R = 0x1000, 0x4000, 0x8000, 0x2000
X, O, SQ, TR, R1, L1, R2, L2, SEL, ST = 0x40, 0x20, 0x80, 0x10, 0x8, 0x4, 0x2, 0x1, 0x100, 0x800
def pad(fork, presses, end):
    m = [0] * (end - fork + 2)
    for s, n, k in presses:
        for v in range(max(0, s), min(len(m), s + n)): m[v] |= k
    out, cur = [], 0
    for v, k in enumerate(m):
        if k != cur: out.append('%d:%04x' % (fork + v, k)); cur = k
    return ','.join(out) or '-'
def taps(k, start, every, count, n=6): return [(start + i * every, n, k) for i in range(count)]
def line(tag, fork, length, presses, cap_every=500, cap_n=3):
    end = fork + length; cnt = length // cap_every
    return '%s %d %d:%d:%d:%d %s' % (tag, end, fork + 1, cap_n, cap_every, cnt, pad(fork, presses, end))
