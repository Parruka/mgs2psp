#!/usr/bin/env python3
"""gl45 study: instruction slots on one path through a PSP function (psp-objdump -d --no-show-raw-insn of one section).
usage: psp_path.py <object> <function> [taken-branch offsets in hex, comma separated]
Walks from the entry to the first `jr ra`: an unconditional b / j is followed; a conditional branch is taken only when its offset is listed
(default: not taken); a listed <offset>*<n> is taken n times, then not, <offset>@<n> not taken n times, then taken (a loop's exit); the delay slot counts once; a call (jal / jalr) counts itself and its slot and is not followed.  Prints the count,
split into VFPU instructions (v*, lv.q, sv.q, bvt*) and the rest, and the conditional branches met (to review the default)."""
import subprocess, sys, re
obj, fn = sys.argv[1], sys.argv[2]
taken = {}; after = {}
for x in (sys.argv[3].split(',') if len(sys.argv) > 3 and sys.argv[3] else []):
    if '@' in x: a, k = x.split('@'); after[int(a, 16)] = int(k); continue           # <offset>@<n>: not taken n times, then taken
    a, _, k = x.partition('*'); taken[int(a, 16)] = int(k) if k else 1 << 30   # <offset>[*<times>]: taken that many times, then not
out = subprocess.run(['psp-objdump', '-d', '--no-show-raw-insn', '-j', '.text.' + fn, obj], capture_output=True, text=True).stdout
ins = {}
for l in out.split('\n'):
    m = re.match(r'\s*([0-9a-f]+):\t(\S+)\t?(.*)', l)
    if m: ins[int(m.group(1), 16)] = (m.group(2), m.group(3))
pc = 0; n = nv = 0; conds = []; calls = 0; seen = 0
def isv(op): return op.startswith('v') or op in ('lv.q', 'sv.q', 'lv.s', 'sv.s', 'bvtl', 'bvt', 'bvf', 'bvfl', 'mtv', 'mfv')
while True:
    op, args = ins[pc]; n += 1; nv += isv(op); seen += 1
    if seen > 100000: sys.exit('loop')
    if op == 'jr':
        n += 1; nv += isv(ins[pc + 4][0]); break
    if op in ('jal', 'jalr'):
        calls += 1; n += 1; nv += isv(ins[pc + 4][0]); pc += 8; continue
    if op == 'j':
        print('tail jump at %x: %s' % (pc, args)); n += 1; break
    if op[0] == 'b' and op not in ('break',):
        m = re.search(r'([0-9a-f]+) <', args); tgt = int(m.group(1), 16)
        uncond = op == 'b' or (op in ('beq', 'beqz') and False)
        n += 1; nv += isv(ins[pc + 4][0])
        if op == 'b': pc = tgt; continue
        t = taken.get(pc, 0) > 0
        if pc in after:
            t = after[pc] <= 0; after[pc] -= 1
        if t and pc in taken: taken[pc] -= 1
        conds.append('%x:%s%s' % (pc, op, '(T)' if t else ''))
        pc = tgt if t else pc + 8; continue
    pc += 4
print('%s %s: %d instruction slots on the path (%d VFPU, %d other), %d calls; conditionals: %s' % (obj.split('/')[-1], fn, n, nv, n - nv, calls, ' '.join(conds)))
