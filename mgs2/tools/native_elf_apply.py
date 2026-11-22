#!/usr/bin/env python3
"""native_elf_apply.py <gen_elf.h> <out>: hook the native forms of boot/native_elf.c into the ELF translation.

P12dbc0 (the sort key of a sort-pass mesh) gets two hooks, each at an exact line of the translation (the tool stops if a line is not
there exactly once, e.g. after a change of the generator):
  before L12dc00 (the loop head, entered from 0x12dbfc)   NAT_DBC0: the whole vertex loop natively when it can be exact; else the
                                                         translation runs it
  before 0x12dc24 (after the loop)                       the label NL12dc24 the native loop continues at (NAT_DBC0_CHECK: the comparison)
boot/native_elf.h is included before the function and boot/native_elf.c at the end of the header, so every generated function keeps its
place (PGO profiles match by definition order; only P12dbc0's own profile is lost).  The x86 and PSP headers differ only in the asm text of
the groups, which the hooks do not touch."""
import sys

FN = 'static void P12dbc0(void)\n'
ACC0 = '/*12dbfc*/ vu_vgrp('
HEAD = 'L12dc00:;\n'
HOOK = ('if (NAT_DBC0((uint32_t)r12, (uint32_t)r13)) { r12 = (int32_t)((uint32_t)r12 + 16u * (r13 ? (uint32_t)r13 : 1u)); r13 = r13 ? 0 : -1;'
        ' goto NL12dc24; }   /* tools/native_elf_apply.py */\n')
AFTER = '/*12dc24*/ \n'
LABEL = 'NL12dc24:;\nNAT_DBC0_CMP(r12, r13);\n'


def main():
    src, out = sys.argv[1], sys.argv[2]
    lines = open(src).read().split('\n')
    lines = [l + '\n' for l in lines[:-1]] + ([lines[-1]] if lines[-1] else [])
    if any('native_elf' in l for l in lines):
        sys.exit('native_elf_apply: %s already has the hooks' % src)
    starts = [i for i, l in enumerate(lines) if l == FN]
    if len(starts) != 1:
        sys.exit('native_elf_apply: %d definitions of P12dbc0' % len(starts))
    a = starts[0]
    b = next(i for i in range(a + 1, len(lines)) if lines[i] == '}\n')
    body = range(a, b + 1)

    def one(pred, what):
        hits = [i for i in body if pred(lines[i])]
        if len(hits) != 1:
            sys.exit('native_elf_apply: %s found %d times in P12dbc0' % (what, len(hits)))
        return hits[0]
    i_acc = one(lambda l: l.startswith(ACC0), 'the group at 0x12dbfc')
    i_head = one(lambda l: l == HEAD, 'L12dc00')
    i_after = one(lambda l: l == AFTER, 'the line 0x12dc24')
    if not (i_acc + 1 == i_head < i_after):
        sys.exit('native_elf_apply: the hooked lines are not in the expected order')
    # the loop: r12 = r5 and r13 = the count before it, r12 / r13 / VF1 / ACC and the poll the only state it changes
    if not any(l.startswith('/*12dbf0*/ r12 = r5 + r0;') for l in lines[a:i_acc]) or not any(l.startswith('/*12dbf4*/ r13 = r3 + r0;') for l in lines[a:i_acc]):
        sys.exit('native_elf_apply: r12 / r13 are not set as expected before the loop')
    loop = ''.join(lines[i_head:i_after])
    for need in ('/*12dc10*/ VLQC2(1, ((uint32_t)(r12 + 16)) & ~15u);', '/*12dc14*/ r13 = (int64_t)(int32_t)(r13 + -1);',
                 '/*12dc18*/ r12 = (int64_t)(int32_t)(r12 + 16);', '/*12dc1c*/ { int c_ = (r13 > 0);  if (c_) { IRQ_POLLF(', 'goto L12dc00; } }'):
        if loop.count(need) != 1:
            sys.exit('native_elf_apply: the loop body differs from the expected one (%s)' % need)
    new = {i_head: HOOK + HEAD, i_after: LABEL + AFTER}
    res = []
    for i, l in enumerate(lines):
        if i == a:
            res.append('#include "native_elf.h"   /* tools/native_elf_apply.py */\n')
        res.append(new.get(i, l))
    if res and not res[-1].endswith('\n'):
        res[-1] += '\n'
    res.append('#include "native_elf.c"   /* tools/native_elf_apply.py: after every generated function */\n')
    open(out, 'w').write(''.join(res))
    print('native_elf_apply: P12dbc0 hooked (lines %d-%d of %s) -> %s' % (a + 1, b + 1, src, out))


if __name__ == '__main__':
    main()
