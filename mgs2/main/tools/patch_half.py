#!/usr/bin/env python3
"""patch_half.py <in gen_elf.h> <out gen_elf.h>: gl19 (plan_deck_25fps.md Phase 1), MGS2_HALF: cosmetic actors every other game step.

The actor scheduler GV_ExecActorSystem (P13f140) calls each actor's act through a register at 0x13f1ac (the actor in R[4], return address
0x13f1b4).  This copy of the generated header changes two lines of P13f140 and nothing else (no line added or removed):
  - its entry, FN_ENTER(0x13f140), also counts the scheduler's passes in g_half_step (one pass = one game step: the main loop at 0x100190
    calls P13f140 once an iteration).  x86 and the PSP run the same code, so both count alike; the count never comes from real time;
  - the call at 0x13f1ac asks boot's rt_half_skip(act) first when g_half (MGS2_HALF / MGS2_HALF_CYCLE) is not 0, and makes the call
    (JALR, as before) unless it says skip.  With g_half 0 the line makes the same call as the original.
rt_half_skip is boot/boot.c's (the PSP) and boot/boot_x86.c's (the x86 twin): a small table of act addresses, each checked against the
actor's file name at +0x28.  Used for gen/WP -> gen/wh (x86) and gen/wg -> gen/wgh (the PSP) by scripts/hdr.sh, which asserts the md5s."""
import hashlib, sys

ENTRY = 'FN_ENTER(0x13f140);'
ENTRY_NEW = ('FN_ENTER(0x13f140); { extern uint32_t g_half_step; g_half_step++; }'
             '   /* gl19 tools/patch_half.py: the scheduler\'s passes (game steps) */')
CALL_HEAD = '/*13f1ac*/ { int64_t t_ = r2; r31 = 0x13f1b4;  ('
CALL_OLD = 'JALR((uint32_t)t_)'
CALL_NEW = '((g_half && rt_half_skip((uint32_t)t_)) ? (void)0 : JALR((uint32_t)t_))'
DECL = '/*13f1ac*/ { extern uint32_t g_half; int rt_half_skip(uint32_t a); int64_t t_ = r2; r31 = 0x13f1b4;  ('


def main():
    src, out = sys.argv[1], sys.argv[2]
    raw = open(src, 'rb').read()
    lines = raw.decode('latin-1').split('\n')
    d = [i for i, l in enumerate(lines) if l == 'static void P13f140(void)']
    if len(d) != 1 or lines[d[0] + 1] != '{':
        sys.exit('%s: P13f140\'s definition not found once' % src)
    i = d[0] + 2
    if lines[i] != ENTRY:
        sys.exit('%s: P13f140\'s first line is %r' % (src, lines[i][:60]))
    lines[i] = ENTRY_NEW
    e = i
    while lines[e] != '}':
        e += 1
    c = [j for j in range(i, e) if lines[j].startswith(CALL_HEAD)]
    if len(c) != 1 or lines[c[0]].count(CALL_OLD) != 1:
        sys.exit('%s: the call at 0x13f1ac not found once in P13f140' % src)
    j = c[0]
    if 'R[4] = r4' not in lines[j] or 'R[31] = r31' not in lines[j]:
        sys.exit('%s: the call at 0x13f1ac does not pass R[4] and R[31]' % src)
    lines[j] = DECL + lines[j][len(CALL_HEAD):].replace(CALL_OLD, CALL_NEW)
    data = '\n'.join(lines).encode('latin-1')
    if data.count(b'\n') != raw.count(b'\n'):
        sys.exit('line count changed')
    open(out, 'wb').write(data)
    print('%s (%s) -> %s (%s): lines %d and %d of P13f140 changed' % (src, hashlib.md5(raw).hexdigest()[:8], out,
                                                                     hashlib.md5(data).hexdigest()[:8], i + 1, j + 1))


main()
