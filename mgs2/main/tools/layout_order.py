#!/usr/bin/env python3
"""(main/tools copy, 2026-11-18 gl20: OBJS has gl18me's ME-era objects too; otherwise the shared tools/layout_order.py 5d9fd966)
layout_order.py <order.txt> <build dir> <stage>: writes <build dir>/layout_elf.ld and layout_<stage>.ld (psp/Makefile LAYOUT) from
tools/layout_sim.c's order ("region name" per line; region 0 = main ELF, 1 = the stage overlay).

A function built with LAYOUT=1 (-ffunction-sections) sits in its own input section (.text.NAME, .text.hot.NAME, .text.unlikely.NAME,
.text.startup.NAME); it is named with its object file, so a static function of the same name in another object stays where it was.  A
function that shares its section (assembly, a library member: newlib has one function per member) brings the whole section, at the place
of its first listed function.  Functions found nowhere are left to the linker; all three kinds are reported, and `layout_sim -new` on the
relinked build gives the misses of the real addresses."""
import sys, os, subprocess
env = dict(os.environ, PATH='/home/claude/pspsdk/pspdev/bin:' + os.environ['PATH'])
def OBJS(stage): return {0: ['boot_rt.o', 'gen.o', 'gen_dispatch.o', 'main_psp.o', 'psp_ctx.o', 'iop_rt.o', 'iop_rt_libs.o', 'iop_rt_ucontext.o',
                             'jsifman.o', 'cdbios.o', 'iop_main.o', 'spu2_lite.o',
                             # the live picture's objects (psp/gl3.sh d, 2026-10-30 task #103; absent from older builds, so skipped there; ge_fxthin.o from gl10)
                             'xl_psp.o', 'ge_xlive.o', 'ge_xlate.o', 'ge_gsfront.o', 'ge_gsup2.o', 'ge_texconv.o', 'ge_texcache.o', 'ge_gelive.o',
                             'ge_gebatch.o', 'ge_xconv.o', 'ge_vu1conv.o', 'ge_gelist.o', 'ge_fxthin.o', 'gl1_glue.o', 'v26me_bat.o', 'exc3b.o', 'exc3b_s.o',
                             'ps2rc_kcall.o',
                             # main/ (gl20): gl18me's ME-era objects (scripts/psp.sh), so their functions are found and their shared sections grouped
                             'ge_xlme.o', 'ge_xme.o', 'ge_me_rt.o', 'ge_xme_me.o', 'hle_me.o'], 1: ['gen_s_%s.o' % stage]}
LIBS = ['libc.a', 'libm.a', 'libgcc.a', 'libcglue.a', 'libpthreadglue.a', 'libpspfpu.a', 'libpsppower.a', 'libpspdebug.a', 'libpspctrl.a', 'libpspdisplay.a', 'libpspuser.a',
        'libpspkernel.a', 'libpspsdk.a', 'libpspnet_inet.a', 'libpsputility.a', 'libpspge.a', 'libpspgu.a', 'libpspsystemctrl_user.a']

def run(*a): return subprocess.run(list(a), capture_output=True, text=True, env=env).stdout

def lib_statics(path):
    """(member, name, source file) for the static text functions of the archive at path (psp-readelf lists each member's FILE symbol;
    psp-objdump -t does not for archive members)"""
    out = []; m = src = None
    for l in run('psp-readelf', '-sW', path).split('\n'):
        if l.startswith('File: '): m, src = l[l.rfind('(') + 1:l.rfind(')')], None; continue
        p = l.split()
        if len(p) == 8 and p[3] == 'FILE': src = src or os.path.basename(p[7])
        elif len(p) == 8 and p[3] == 'FUNC' and p[4] == 'LOCAL' and src: out.append((m, p[7], src))
    return out

def obj_funcs(obj, bdir):
    """name -> (pattern, own section?) for the function symbols of obj.  A static function is also listed as name@<its source file>, the
    name tools/layout_funcs.py gives it when another function of the region has its name (gen.o and gen_dispatch.o both have a static
    gen_mm: v16's first deck order listed both as gen_mm, both went to gen.o's, and every later function sat 36 bytes off its simulated
    place, 605 k I-misses against the simulated 558 k)."""
    out = {}
    syms = [l.split() for l in run('psp-objdump', '-t', os.path.join(bdir, obj)).split('\n')]
    src = next((os.path.basename(p[-1]) for p in syms if len(p) >= 6 and p[1] == 'l' and p[2] == 'df'), None)
    for p in syms:
        if len(p) >= 6 and 'F' in p[1:3] and p[-3].startswith('.text'):
            sec, name = p[-3], p[-1]
            if sec.endswith('.' + name):   # its own section: any of the four names, so a rebuild whose profile moved it between hot / plain keeps its place
                v = ('%s(%s)' % (obj, ' '.join(k + name for k in ('.text.', '.text.hot.', '.text.unlikely.', '.text.startup.'))), True)
            else: v = ('%s(%s)' % (obj, sec), False)
            out.setdefault(name, v)
            if p[1] == 'l' and src: out.setdefault(name + '@' + src, v)
    return out

def main():
    order, bdir, stage = sys.argv[1:4]
    where = {0: {}, 1: {}}
    for r, objs in OBJS(stage).items():
        for o in objs:
            if os.path.exists(os.path.join(bdir, o)):
                for name, v in obj_funcs(o, bdir).items(): where[r].setdefault(name, v)
    for lib in LIBS:
        path = run('psp-gcc', '-print-file-name=' + lib).strip()
        if not os.path.isabs(path):
            path = os.path.join(run('psp-config', '--pspsdk-path').strip(), 'lib', lib)
        if not os.path.exists(path): continue
        for l in run('psp-nm', '-A', '--defined-only', path).split('\n'):
            p = l.split()
            if len(p) == 3 and p[1] in 'Tt':
                arch, member = p[0].rsplit(':', 2)[-3:-1] if p[0].count(':') >= 2 else (None, None)
                if member:
                    for r in ((0, 1) if lib == 'libgcc.a' else (0,)):   # the overlay links libgcc itself (psp/Makefile)
                        where[r].setdefault(p[2], ('*.a:%s(.text .text.*)' % member, False))
        for member, name, src in lib_statics(path):
            for r in ((0, 1) if lib == 'libgcc.a' else (0,)): where[r].setdefault(name + '@' + src, ('*.a:%s(.text .text.*)' % member, False))
    lines = {0: [], 1: []}; missing = {0: [], 1: []}; grouped = {0: 0, 1: 0}; done = set()
    for l in open(order):
        p = l.split()
        if len(p) != 2: continue
        r, name = int(p[0]), p[1]
        if name not in where[r]: missing[r].append(name); continue
        pat, own = where[r][name]
        if not own: grouped[r] += 1
        if pat in done: continue
        done.add(pat); lines[r].append('    ' + pat)
    with open(os.path.join(bdir, 'layout_elf.ld'), 'w') as f:
        f.write('.text : {\n' + '\n'.join(lines[0]) + '\n}\n')
    with open(os.path.join(bdir, 'layout_%s.ld' % stage), 'w') as f:
        f.write('\n'.join(lines[1]) + '\n')
    for r in (0, 1):
        print('region %d: %d input sections listed, %d functions in shared sections, %d left to the linker%s' % (
            r, len(lines[r]), grouped[r], len(missing[r]), (': ' + ' '.join(missing[r][:12])) if missing[r] else ''))

if __name__ == '__main__': main()
