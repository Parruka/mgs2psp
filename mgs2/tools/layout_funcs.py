#!/usr/bin/env python3
"""layout_funcs.py <MGS2T.elf> <ovl_<stage>.elf> [<build dir> <stage>]: the function list for tools/layout_sim.c ("region start size name";
region 0 = the main ELF's text, 1 = the overlay's .ovl section).  With the build directory, functions that share an input section (assembly,
an object built without -ffunction-sections, a library member) are merged into one unit named after its first function: the linker moves the
section as a whole (tools/layout_order.py).  A static function whose name another function of the region also has is named
name@<its source file> (the FILE symbol before it in the ELF's symbol table; layout_order.py resolves that name to its object), so
layout_sim's order says which one goes where; the names are printed to stderr."""
import sys, subprocess, os
from collections import Counter
env = dict(os.environ, PATH='/home/claude/pspsdk/pspdev/bin:' + os.environ['PATH'])
def run(*a): return subprocess.run(list(a), capture_output=True, text=True, env=env).stdout
def sec(f, name):
    for l in run('psp-objdump', '-h', f).split('\n'):
        p = l.split()
        if len(p) > 4 and p[1] == name: return int(p[3], 16), int(p[2], 16)
def srcs(f):
    """(address, name) -> source file of each static function of f: the linker writes an object's FILE symbol, then its local symbols"""
    out = {}; cur = None
    for l in run('psp-readelf', '-sW', f).split('\n'):
        p = l.split()
        if len(p) == 8 and p[3] == 'FILE': cur = os.path.basename(p[7])
        elif len(p) == 8 and p[3] == 'FUNC' and p[4] == 'LOCAL' and cur: out[(int(p[1], 16), p[7])] = cur
    return out
group = {0: {}, 1: {}}   # name (and name@source for a static function) -> shared section key
if len(sys.argv) > 4:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    bdir, stage = sys.argv[3], sys.argv[4]
    from layout_order import OBJS, LIBS, lib_statics
    for r, objs in OBJS(stage).items():
        for o in objs:
            if not os.path.exists(os.path.join(bdir, o)): continue
            syms = [l.split() for l in run('psp-objdump', '-t', os.path.join(bdir, o)).split('\n')]
            src = next((os.path.basename(p[-1]) for p in syms if len(p) >= 6 and p[1] == 'l' and p[2] == 'df'), None)
            for p in syms:
                if len(p) >= 6 and 'F' in p[1:3] and p[-3].startswith('.text') and not p[-3].endswith('.' + p[-1]):
                    group[r].setdefault(p[-1], (o, p[-3]))
                    if p[1] == 'l' and src: group[r].setdefault(p[-1] + '@' + src, (o, p[-3]))
    for lib in LIBS:
        path = run('psp-gcc', '-print-file-name=' + lib).strip()
        if not os.path.isabs(path): path = os.path.join(run('psp-config', '--pspsdk-path').strip(), 'lib', lib)
        if not os.path.exists(path): continue
        for l in run('psp-nm', '-A', '--defined-only', path).split('\n'):
            p = l.split()
            if len(p) == 3 and p[1] in 'Tt' and p[0].count(':') >= 2:
                member = p[0].rsplit(':', 2)[1]
                for r in ((0, 1) if lib == 'libgcc.a' else (0,)): group[r].setdefault(p[2], (lib, member))
        for member, name, src in lib_statics(path):
            for r in ((0, 1) if lib == 'libgcc.a' else (0,)): group[r].setdefault(name + '@' + src, (lib, member))
for region, f, s in ((0, sys.argv[1], '.text'), (1, sys.argv[2], '.ovl')):
    lo, sz = sec(f, s)
    fs = []
    for l in run('psp-nm', '-S', f).split('\n'):
        p = l.split()
        if len(p) == 4 and p[2] in 'Tt' and int(p[1], 16):
            a = int(p[0], 16)
            if lo <= a < lo + sz: fs.append([a, int(p[1], 16), p[3], None])
    n = Counter(x[2] for x in fs); src = srcs(f)
    for x in fs:
        if n[x[2]] > 1 and (x[0], x[2]) in src:
            x[2] += '@' + src[(x[0], x[2])]; print('region %d: %s' % (region, x[2]), file=sys.stderr)
        x[3] = group[region].get(x[2])
    fs.sort()
    out = []
    for x in fs:
        if out and x[3] is not None and out[-1][3] == x[3]:
            out[-1][1] = x[0] + x[1] - out[-1][0]   # same shared section: extend the unit
        else: out.append(x)
    for a, size, name, g in out: print(region, '%08x' % a, '%08x' % size, name)
