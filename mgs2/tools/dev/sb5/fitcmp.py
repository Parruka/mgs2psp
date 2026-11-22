# fitcmp.py <dump off> <dump on> <steps> <build dir> [functions...]: two PspProf dumps of one build (lane 4 off and on, renderer_notes 11.32)
# with the refitted weights of 11.25 (ms = (1.647 instr + 68.2 I-miss + 85.9 D-miss) / 333k a step): the total, each part of profcat_fit.py
# and each named function (default: lane 4's), off -> on.  Run with python3 -I.
import bisect, collections, glob, os, re, subprocess, sys
WI, WIM, WDM = 1.647, 68.2, 85.9
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
d0, d1, steps, bdir = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4]
names = sys.argv[5:] or ['vm_all', 'xl_vif1_span', 'vif_cmd', 'ud_new', 'call', 'xl_lane_p07', 'vc_p07', 'vc_p07_rec', 'vc_p07_uv2_rec',
                          'vc_p06_uv2', 'xc_lit', 'io_dma_start', 'gif_qw', 'xl_flush', 'vm_dec', 'own_fill']
elfs = [os.path.join(bdir, 'MGS2T.elf')] + sorted(glob.glob(os.path.join(bdir, 'ovl_*.elf')))
objsyms = collections.defaultdict(list)
for o in glob.glob(os.path.join(bdir, '*.o')):
    for ln in subprocess.run([NM, '-S', '--defined-only', o], capture_output=True, text=True).stdout.split('\n'):
        p = ln.split()
        if len(p) == 4 and p[2] in 'tTwW': objsyms[p[3]].append((int(p[1], 16), os.path.basename(o)))
syms = {}
for e in elfs:
    for ln in subprocess.run([NM, '-n', '-S', e], capture_output=True, text=True).stdout.split('\n'):
        p = ln.split()
        if len(p) == 4 and p[2] in 'tTwW' and p[3] != '_ftext':
            a, sz, n = int(p[0], 16), int(p[1], 16), p[3]
            c = objsyms.get(n, [])
            if len(c) == 1: obj = c[0][1]
            elif c: m = [x for x in c if x[0] == sz]; obj = m[0][1] if len(m) == 1 else '?' + '/'.join(sorted(set(x[1] for x in c)))
            else: obj = 'libc' if not e.endswith('.o') else '?'
            syms.setdefault(a, (n, obj))
        elif len(p) == 3 and p[1] in 'tTwW' and p[2] != '_ftext':
            syms.setdefault(int(p[0], 16), (p[2], 'libc'))
S = sorted(syms.items()); A = [a for a, _ in S]
def who(pc):
    i = bisect.bisect_right(A, pc) - 1
    return S[i][1] if i >= 0 else ('?', '?')
WALK = re.compile(r'^(io_|gif_|dma_|vif1|gs_|xlv_|spr_|dmac_)')
def part(n, obj):   # profcat_fit.py's parts
    if obj.startswith('gen'): return 'game code'
    if obj == 'boot_rt.o':
        if n.startswith('vupv_') or n.startswith('vu0') or n.startswith('vu_'): return 'compiled VU0 + VU0 model'
        if WALK.match(n): return 'walk (DMA, VIF1, GIF)'
        if n.startswith(('iop', 'sif', 'spu', 'sd_', 'cd')): return 'IOP / SIF / sound'
        return 'runtime (helpers, kernel, other)'
    if obj in ('ge_xlate.o', 'ge_xlive.o'): return 'Stage A: translator'
    if obj == 'ge_gsfront.o': return 'Stage A: GS front end'
    if obj == 'ge_gsup2.o': return 'textures: map (gsup2)'
    if obj in ('ge_texcache.o', 'ge_texconv.o'): return 'textures: cache + conversion'
    if obj in ('ge_xconv.o', 'ge_vu1conv.o'): return 'converters'
    if obj in ('ge_gelive.o', 'ge_gebatch.o'): return 'frame + batches'
    if obj == 'ge_gelist.o': return 'GE list'
    if obj in ('xl_psp.o', 'main_psp.o', 'gl1_glue.o', 'psp_ctx.o'): return 'harness glue'
    if obj.startswith(('iop', 'jsif', 'spu2', 'cdbios')): return 'IOP / SIF / sound'
    if obj == 'libc': return 'libc / libgcc'
    return 'other: ' + obj
def load(dump):
    agg = collections.defaultdict(lambda: [0, 0, 0]); fn = collections.defaultdict(lambda: [0, 0, 0]); tot = [0, 0, 0]
    for ln in open(dump):
        if ln[0] == '#': continue
        pc, c, im, dm = ln.split(); n, obj = who(int(pc, 16)); v = (int(c), int(im), int(dm)); k = part(n, obj)
        for i in range(3): agg[k][i] += v[i]; fn[n][i] += v[i]; tot[i] += v[i]
    return agg, fn, tot
ms = lambda a: (WI * a[0] + WIM * a[1] + WDM * a[2]) / steps / 333e3
def parts3(a): return '(instr %5.2f, I %5.2f, D %5.2f)' % (WI * a[0] / steps / 333e3, WIM * a[1] / steps / 333e3, WDM * a[2] / steps / 333e3)
L0, L1 = load(d0), load(d1)
print('total: off %.2f ms a step %s, on %.2f %s, change %+.2f (%+.1f%%)' % (ms(L0[2]), parts3(L0[2]), ms(L1[2]), parts3(L1[2]), ms(L1[2]) - ms(L0[2]),
      100 * (ms(L1[2]) - ms(L0[2])) / ms(L0[2])))
for k in sorted(set(L0[0]) | set(L1[0]), key=lambda k: -ms(L0[0].get(k, [0, 0, 0]))):
    a, b = L0[0].get(k, [0, 0, 0]), L1[0].get(k, [0, 0, 0])
    if max(ms(a), ms(b)) >= 0.01: print('  %-34s %6.2f -> %6.2f  %+6.2f ms  on %s' % (k, ms(a), ms(b), ms(b) - ms(a), parts3(b)))
print('functions:')
for n in names:
    a, b = L0[1].get(n, [0, 0, 0]), L1[1].get(n, [0, 0, 0])
    print('  %-20s %6.2f -> %6.2f  %+6.2f ms   off %s  on %s' % (n, ms(a), ms(b), ms(b) - ms(a), parts3(a), parts3(b)))
