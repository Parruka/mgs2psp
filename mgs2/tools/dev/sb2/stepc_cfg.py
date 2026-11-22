#!/usr/bin/env python3
# stepc_cfg.py <build dir> > <cfg>: the input of stepc_sim.c (renderer_notes 11.24 "Next" 1) for a PSP build: the entries it follows,
# every function of MGS2T.elf as a range with its part (as profcat.py parts the model: each function placed by the object that defines
# it), the overlay arena as game code, and the return address of every linking call (jal, jalr, bal, bgezal, bltzal) in the text,
# so that a function entered by a tail call (j) is told apart.
import bisect, collections, glob, os, re, subprocess, sys
B = sys.argv[1]; T = '/home/claude/pspsdk/pspdev/bin/'
elf = os.path.join(B, 'MGS2T.elf')
objsyms = collections.defaultdict(list)
for o in glob.glob(os.path.join(B, '*.o')):
    for ln in subprocess.run([T + 'psp-nm', '-S', '--defined-only', o], capture_output=True, text=True).stdout.split('\n'):
        p = ln.split()
        if len(p) == 4 and p[2] in 'tTwW': objsyms[p[3]].append((int(p[1], 16), os.path.basename(o)))
WALK = re.compile(r'^(io_|gif_|dma_|vif1|gs_|xlv_|spr_|dmac_)')
PARTS = ['game code', 'compiled VU0 + VU0 model', 'walk (DMA, VIF1, GIF)', 'IOP / SIF / sound', 'runtime (helpers, kernel, other)',
         'Stage A: translator', 'Stage A: GS front end', 'textures: map (gsup2)', 'textures: cache + conversion', 'converters',
         'frame + batches', 'GE list', 'harness glue', 'libc / libgcc', 'other']
def part(n, obj):
    if obj.startswith('gen'): return 0
    if obj == 'boot_rt.o':
        if n.startswith(('vupv_', 'vu0', 'vu_')): return 1
        if WALK.match(n): return 2
        if n.startswith(('iop', 'sif', 'spu', 'sd_', 'cd')): return 3
        return 4
    m = {'ge_xlate.o': 5, 'ge_xlive.o': 5, 'ge_gsfront.o': 6, 'ge_gsup2.o': 7, 'ge_texcache.o': 8, 'ge_texconv.o': 8, 'ge_xconv.o': 9,
         'ge_vu1conv.o': 9, 'ge_gelive.o': 10, 'ge_gebatch.o': 10, 'ge_gelist.o': 11, 'xl_psp.o': 12, 'main_psp.o': 12, 'gl1_glue.o': 12,
         'psp_ctx.o': 12}
    if obj in m: return m[obj]
    if obj.startswith(('iop', 'jsif', 'spu2', 'cdbios')): return 3
    if obj == 'libc': return 13
    return 14
fns = {}; ent = {}
for ln in subprocess.run([T + 'psp-nm', '-n', '-S', elf], capture_output=True, text=True).stdout.split('\n'):
    p = ln.split()
    if len(p) == 4 and p[2] in 'tTwW' and p[3] != '_ftext':
        a, sz, n = int(p[0], 16), int(p[1], 16), p[3]
        c = objsyms.get(n, [])
        if len(c) == 1: obj = c[0][1]
        elif c: mm = [x for x in c if x[0] == sz]; obj = mm[0][1] if len(mm) == 1 else '?'
        else: obj = 'libc'
        fns.setdefault(a, (a + sz, part(n, obj))); ent.setdefault(n, a)
    elif len(p) == 4 and p[3] == 'ovl_arena': ent['ovl_arena'] = int(p[0], 16)
OS = None
for ln in open(os.path.join(B, 'make.log')):
    m = re.search(r'OVL_SIZE=(0x[0-9a-f]+)', ln)
    if m: OS = int(m.group(1), 16); break
need = ['gl_call', 'gl_pass', 'xtex_cb', 'xc_gs', 'vc_df0_rec', 'vc_df0', 'vc_p06_rec', 'vc_p06', 'xl_flush']
for n in need: print('entry %s %08x' % (n, ent[n]))
for i, n in enumerate(PARTS): print('part %d %s' % (i, n))
print('range %08x %08x 0' % (ent['ovl_arena'], ent['ovl_arena'] + OS))
for a in sorted(fns): print('range %08x %08x %d' % (a, fns[a][0], fns[a][1]))
rets = set(); LINK = re.compile(r'^\s*([0-9a-f]+):\s+[0-9a-f]{8}\s+(jal|jalr|bal|bgezal|bltzal|bgezall|bltzall)\s')
pr = subprocess.Popen([T + 'psp-objdump', '-d', '--no-show-raw-insn' if False else '-d', elf], stdout=subprocess.PIPE, text=True)
for ln in pr.stdout:
    m = LINK.match(ln)
    if m: rets.add(int(m.group(1), 16) + 8)
pr.wait()
for r in sorted(rets): print('ret %08x' % r)
