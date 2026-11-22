#!/usr/bin/env python3
"""native_apply.py <gen_s_w00a.h> [out]: hook the native forms of boot/native_w00a.c into the w00a stage translation.

line_sph (S_w00a_P333380, the deck's rain streaks) gets five hooks, each at an exact line of the translation (the tool stops if a line is not
there exactly once, e.g. after a change of the generator):
  L333490 (a prim starts)      NAT_LS_GUARD: write the pending scratchpad image first if this prologue could meet it
  0x3334e0 (first copy call)   nat_ls_prim: the stretch to 0x333774 natively when it can be exact; else the translation runs
  after 0x33376c (last copy)   the label NL333774 the native stretch continues at (NAT_CHECK: the comparison with the translation)
  0x33377c (back-edge poll)    NAT_LS_PREPOLL: write the image before a poll that will run handlers
  L333788 (every return)       NAT_LS_FLUSH
boot/native_w00a.h is included before the function and boot/native_w00a.c at the end of the header, so every generated function keeps its
place (PGO profiles match by definition order; only S_w00a_P333380's own profile is lost)."""
import sys

FN = 'static void S_w00a_P333380(void)\n'
PRIM_HEAD = '/*3334e0*/ { r31 = 0x3334e8; f21 = RDF((uint32_t)(r16 + 8)); '
PRIM_HOOK = '''if (NAT_LS_PRIM((int32_t)(r2 - r23), (int32_t)r5, (int32_t)r16, (int32_t)r17, (int32_t)r19, (int32_t)r21, (int32_t)r22, (int32_t)r23, (int32_t)r29, (int32_t)r30, f20, f21)) {
#if defined(NAT_ON) && defined(NAT_CHECK)
  nat_chk_swap();
#else
  r1 = R[1], r3 = R[3], r4 = R[4], r5 = R[5], r6 = R[6], r16 = R[16], r18 = R[18], r20 = R[20], r21 = R[21], r31 = R[31];
  f0 = F[0], f1 = F[1], f2 = F[2], f3 = F[3], f4 = F[4], f5 = F[5], f6 = F[6];
  goto NL333774;
#endif
  }
'''
LAST_COPY = '/*33376c*/ '
AFTER_COPY = '''NL333774:;
#if defined(NAT_ON) && defined(NAT_CHECK)
{ int64_t l_[10] = { r1, r3, r4, r5, r6, r16, r18, r20, r21, r31 }; float lf_[7] = { f0, f1, f2, f3, f4, f5, f6 }; nat_chk_cmp(l_, lf_); }
#endif
'''
POLL_HEAD = '/*33377c*/ { if (r2 != r0) { r5 = (int32_t)RD32((uint32_t)(r16 + 64)); { IRQ_POLLF('
POLL_TAIL = '); goto L333490; } } }\n'   # the flush list between them follows regcache.py's poll sets (v26: (R[2] = r2, R[5] = r5))


def main():
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else src
    lines = open(src).read().split('\n')
    lines = [l + '\n' for l in lines[:-1]] + ([lines[-1]] if lines[-1] else [])
    if any('native_w00a' in l for l in lines):
        sys.exit('native_apply: %s already has the hooks' % src)
    starts = [i for i, l in enumerate(lines) if l == FN]
    if len(starts) != 1:
        sys.exit('native_apply: %d definitions of S_w00a_P333380' % len(starts))
    a = starts[0]
    b = next(i for i in range(a + 1, len(lines)) if lines[i] == '}\n')
    body = range(a, b + 1)

    def one(pred, what):
        hits = [i for i in body if pred(lines[i])]
        if len(hits) != 1:
            sys.exit('native_apply: %s found %d times in S_w00a_P333380' % (what, len(hits)))
        return hits[0]
    i_l490 = one(lambda l: l == 'L333490:;\n', 'L333490')
    i_prim = one(lambda l: l.startswith(PRIM_HEAD) and 'S_w00a_P33bd38()' in l, 'the call at 0x3334e0')
    i_last = one(lambda l: l.startswith(LAST_COPY) and 'S_w00a_P33bd58()' in l and l.rstrip().endswith('}'), 'the call at 0x33376c')
    i_poll = one(lambda l: l.startswith(POLL_HEAD) and l.endswith(POLL_TAIL) and l.count('{') == 3, 'the poll at 0x33377c')
    i_l788 = one(lambda l: l == 'L333788:;\n', 'L333788')
    if not (i_l490 < i_prim < i_last < i_poll < i_l788):
        sys.exit('native_apply: the hooked lines are not in the expected order')
    new = {}
    new[i_l490] = lines[i_l490] + 'NAT_LS_GUARD((int32_t)r5, (int32_t)r16, (int32_t)r23);\n'
    new[i_prim] = PRIM_HEAD + '\n' + PRIM_HOOK + lines[i_prim][len(PRIM_HEAD):]
    new[i_last] = lines[i_last] + AFTER_COPY
    new[i_poll] = lines[i_poll].replace('{ IRQ_POLLF(', '{ NAT_LS_PREPOLL(); IRQ_POLLF(', 1)
    new[i_l788] = lines[i_l788] + 'NAT_LS_FLUSH();\n'
    res = []
    for i, l in enumerate(lines):
        if i == a:
            res.append('#include "native_w00a.h"   /* tools/native_apply.py */\n')
        res.append(new.get(i, l))
    if res and not res[-1].endswith('\n'):
        res[-1] += '\n'
    res.append('#include "native_w00a.c"   /* tools/native_apply.py: after every generated function */\n')
    open(out, 'w').write(''.join(res))
    print('native_apply: S_w00a_P333380 hooked (lines %d-%d of %s) -> %s' % (a + 1, b + 1, src, out))


if __name__ == '__main__':
    main()
