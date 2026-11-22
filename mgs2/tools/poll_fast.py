#!/usr/bin/env python3
"""poll_fast.py <gen header> [...]: rewrites the loop back-edge polls of generated code (in place) to the inline form of gen_pre.h IRQ_POLLF.
recomp/regcache.py writes a poll as "(R[2] = r2, ..., F[0] = f0, IRQ_POLL())": the dirty cached registers are stored, then rt_poll (boot.c
k_poll: time += K_POLL_CYCLES, interrupts / events / thread switch when due).  That store is only for the poll itself (an interrupt handler
or another thread may read the register file): regcache.py treats a poll inside a branch (every poll site is one: "if (c_) { ...; goto }")
as cleaning nothing, so the code never relies on it.  IRQ_POLLF((stores)) advances time inline and runs the stores and rt_poll_slow only
when the kernel's threshold k_poll_at is reached (kernel.h k_poll_arm: below it a poll only advances time).  Same results, fewer
instructions and no call per iteration.  Idempotent; prints the number of sites rewritten."""
import sys, re

def rewrite(s):
    out, i, n = [], 0, 0
    while True:
        j = s.find('IRQ_POLL()', i)
        if j < 0: break
        a, b, flush = j, j + len('IRQ_POLL()'), []        # the poll expression s[a:b], grown over the comma groups it ends
        while True:
            if a > 0 and s[a - 1] == '(' and s[b] == ')': a -= 1; b += 1; continue          # (x) -> x
            k = a - 1
            while k >= 0 and s[k] in ' \t': k -= 1
            if k < 0 or s[k] != ',': break
            p, depth = k - 1, 0                                                              # "(..., x)": the group's '('
            while p >= 0:
                c = s[p]
                if c == ')': depth += 1
                elif c == '(':
                    if depth == 0: break
                    depth -= 1
                p -= 1
            if p < 0 or s[b] != ')': raise SystemExit('unexpected poll site near: %r' % s[max(0, j - 80):j + 40])
            flush.insert(0, s[p + 1:k].strip()); a, b = p, b + 1
        out.append(s[i:a]); out.append(('IRQ_POLLF((%s))' % ', '.join(flush)) if flush else 'IRQ_POLLF((void)0)')
        i = b; n += 1
    out.append(s[i:])
    return ''.join(out), n

for fn in sys.argv[1:]:
    s = open(fn).read()
    t, n = rewrite(s)
    if t != s: open(fn, 'w').write(t)
    print('%s: %d poll sites rewritten' % (fn, n))
