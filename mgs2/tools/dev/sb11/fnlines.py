#!/usr/bin/env python3
# fnlines.py <dump> <steps> <elf> <func> <-g object> <section>: a PspProf dump's per-PC counts inside one function, by source line
# (innermost inline frame) and by the function's own lines (outermost frame), with the fitted weights (renderer_notes 11.25).
import sys, subprocess, collections
dump, steps, elf, fn, obj, sec = sys.argv[1], float(sys.argv[2]), sys.argv[3], sys.argv[4], sys.argv[5], sys.argv[6]
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'; A2L = '/home/claude/pspsdk/pspdev/bin/psp-addr2line'
lo = size = None
for l in subprocess.run([NM, '-S', elf], capture_output=True, text=True).stdout.splitlines():
    t = l.split()
    if len(t) == 4 and t[3] == fn: lo, size = int(t[0], 16), int(t[1], 16)
assert lo is not None, fn
cnt = {}
for l in open(dump):
    if l.startswith('#'): continue
    t = l.split()
    pc = int(t[0], 16)
    if lo <= pc < lo + size: cnt[pc - lo] = (int(t[1]), int(t[2]), int(t[3]))
offs = sorted(cnt)
out = subprocess.run([A2L, '-j', sec, '-e', obj, '-f', '-i', '-a'] + ['0x%x' % o for o in offs], capture_output=True, text=True).stdout.splitlines()
# parse: '0x...' then pairs (function, file:line) for each inline frame
frames = {}; cur = None; i = 0
while i < len(out):
    l = out[i]
    if l.startswith('0x'): cur = int(l, 16); frames[cur] = []; i += 1; continue
    frames[cur].append((l, out[i + 1].split('/')[-1].split(' ')[0])); i += 2
W = (1.647, 68.2, 85.9)
def ms(v): return sum(a * b for a, b in zip(v, W)) / 333000 / steps
inner = collections.defaultdict(lambda: [0, 0, 0]); outer = collections.defaultdict(lambda: [0, 0, 0])
for o in offs:
    fr = frames.get(o) or [('?', '?')]
    k1 = '%s %s' % (fr[0][0], fr[0][1]); k2 = '%s %s' % (fr[-1][0], fr[-1][1])
    for j in range(3): inner[k1][j] += cnt[o][j]; outer[k2][j] += cnt[o][j]
tot = [sum(cnt[o][j] for o in offs) for j in range(3)]
print('%s: %d bytes, %.1f k instructions a step, %.2f ms (instr %.2f, I %.2f, D %.2f)' % (fn, size, tot[0] / steps / 1e3, ms(tot),
      tot[0] * W[0] / 333000 / steps, tot[1] * W[1] / 333000 / steps, tot[2] * W[2] / 333000 / steps))
for name, d in (('innermost frame (function file:line)', inner), ('the function\'s own lines (outermost frame)', outer)):
    print('by %s:' % name)
    for k, v in sorted(d.items(), key=lambda kv: -ms(kv[1]))[:int(__import__('os').environ.get('TOP', '25'))]:
        print('  %-56s %7.1f k instr  %5.2f ms (I %5.2f, D %5.2f)' % (k[:56], v[0] / steps / 1e3, ms(v), v[1] * W[1] / 333000 / steps, v[2] * W[2] / 333000 / steps))
