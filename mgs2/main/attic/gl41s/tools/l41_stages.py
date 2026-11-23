#!/usr/bin/env python3
"""l41_stages.py T39 FINAL OUT (gl41s): the stacked stages of the gl41 lever patches as trees OUT/s1 .. OUT/s3 (boot/, runtime/, rt/ of
T39 with the stage's files; s4 = FINAL itself): s1 = P1 + the 'gl41 levers' counter line, s2 = s1 + P3, s3 = s2 + E6, s4 = s3 + the
probes (a) (b) (c).  Each stage is cut from FINAL's files by exact text (an assertion when a piece is not found once)."""
import os, shutil, sys
T39, FIN, OUT = sys.argv[1:4]
R = lambda p: open(p).read()


def cut(s, a, b='', n=1):
    if s.count(a) != n:
        sys.exit(f'l41_stages: {s.count(a)} times (want {n}): {a[:90]!r}')
    return s.replace(a, b)


def between(s, a, z, keep_z=True):
    i = s.index(a); j = s.index(z, i)
    return s[:i] + (s[j:] if keep_z else s[j + len(z):])


F = {f: R(os.path.join(FIN, f)) for f in ('runtime/vu0.h', 'runtime/vu_l41.h', 'runtime/host_time.h', 'rt/ee_io.h', 'runtime/ee_io.h')}
O = {f: R(os.path.join(T39, f)) for f in ('runtime/vif.h', 'rt/ee_io.h', 'runtime/ee_io.h')}
st = {}
# ---- s3: no probes
v = F['runtime/vu0.h']
v = cut(v, '''   per-corner NaN / Inf checks left out; the same results, see the tool) when the switch is on; probe (c) (runtime/vu_l41.h) at the
   MGS2_PROBE41_CULL-th call.  Without the hook (another gen_vu.h, or this macro undefined) the forms are gl39's. */
static int l41_cull_n;   /* 0: never (the default) */
static int l41_cull_probe(int (*f3)(void));
#define VU_L41_P3_HOOK(f3) do { if (__builtin_expect(l41_cull_n != 0, 0) && --l41_cull_n == 0) return l41_cull_probe(f3); \\
                                if (g_opt2 & OPT2_P3) return f3(); } while (0)''', '''   per-corner NaN / Inf checks left out; the same results, see the tool) when the switch is on.  Without the hook (another gen_vu.h,
   or this macro undefined) the forms are gl39's. */
#define VU_L41_P3_HOOK(f3) do { if (g_opt2 & OPT2_P3) return f3(); } while (0)''')
v = cut(v, '#include "vu_l41.h"   /* gl41 (attic/gl41s): lever P1 (vu_run_shared), the probes, the gl41 counter line */',
        '#include "vu_l41.h"   /* gl41 (attic/gl41s): lever P1 (vu_run_shared), the gl41 counter line */')
l = F['runtime/vu_l41.h']
hdr_end = l.index('#ifndef VU_L41_H')
l = '''/* gl41 (attic/gl41s/, 2026-11-23): lever P1 (the VU0 micro-program call fast path) and the 'gl41' counter line.
   Included at the end of runtime/vu0.h, in the runtime unit only (after gen_vu.h: it needs the programs' names).  The PSP's boot_rt.o
   and the x86 twin's both compile this file (runtime/ is on both include paths; rt/ has no vu0.h).
   Switches: MGS2_OPT2 bit 19 OPT2_P1 (runtime/host_time.h), alternated by turn with MGS2_OPT2_CYCLE like every OPT2 bit.
   MGS2_L41_LOG=1: a 'gl41 levers' line every 100 vblanks (the levers' counters; printed in io_vblank, outside the programs; read once
   at the first vblank, rt/ and runtime/ ee_io.h io_vblank -> l41_init). */
''' + l[hdr_end:]
l = between(l, '#if defined(__psp__)\nlong long sceKernelGetSystemTimeWide(void);', 'static int l41_log;\n')
l = between(l, '#if defined(GEN_VU_P3) && defined(VU_L41_P3_HOOK)\n/* probe (c)', '/* l41_init: once')
l = cut(l, '''#if defined(GEN_VU_P3) && defined(VU_L41_P3_HOOK)
  if ((e = getenv("MGS2_PROBE41_CULL"))) l41_cull_n = atoi(e);
#endif
#if defined(__psp__)
  if ((e = getenv("MGS2_PROBE41"))) { int m = atoi(e); if (m & 2) l41_probe_b(); if (m & 1) l41_probe_a(); }
#endif
''')
st['s3'] = {'runtime/vu0.h': v, 'runtime/vu_l41.h': l, 'runtime/host_time.h': F['runtime/host_time.h'], 'rt/ee_io.h': F['rt/ee_io.h'],
            'runtime/ee_io.h': F['runtime/ee_io.h'], 'runtime/vif.h': R(os.path.join(FIN, 'runtime/vif.h'))}
# ---- s2: no E6
v2 = between(v, '#if !defined(VU_STATS) && !defined(VU_DUMP)\n/* gl41 E6', 'static uint32_t vu_hash(void) {')
v2 = cut(v2, '  if (VU_HASH_E6()) { vu_hash_e6(); return vu_prog_hash; }   /* gl41 E6 */\n')
l2 = cut(l, '''#if !defined(VU_STATS) && !defined(VU_DUMP)
  fprintf(stderr, "gl41 levers vb %ld: opt2 %08x, p1 misses %ld fills %ld, e6 hits %ld misses %ld words hashed %ld, e6 check differing %ld\\n",
          vb, (unsigned)g_opt2, p1m, p1f, g_l41_e6_hit, g_l41_e6_miss, g_l41_e6_cmpw, g_l41_e6_bad);
#endif''', '''  fprintf(stderr, "gl41 levers vb %ld: opt2 %08x, p1 misses %ld fills %ld\\n", vb, (unsigned)g_opt2, p1m, p1f);''')
h2 = between(F['runtime/host_time.h'], '#define OPT2_E6 2097152u', '#define OPT2_DEFAULT')
def no_e6(s):
    s = cut(s, 'vu_prog_clean = 0; E6_LOST(); return (uint8_t *)VUPROG', 'vu_prog_clean = 0; return (uint8_t *)VUPROG')
    s = cut(s, '{ memcpy(d + k, s + k, 4 * (m - k)); vu_prog_clean = 0; E6_DIRTY(b + k, b + m); }   /* gl41 E6: the words changed */',
            '{ memcpy(d + k, s + k, 4 * (m - k)); vu_prog_clean = 0; }')
    return cut(s, 'vu_prog_clean = 0; E6_LOST(); i += cnt * 2; break; }', 'vu_prog_clean = 0; i += cnt * 2; break; }')
st['s2'] = {'runtime/vu0.h': v2, 'runtime/vu_l41.h': l2, 'runtime/host_time.h': h2, 'rt/ee_io.h': no_e6(F['rt/ee_io.h']),
            'runtime/ee_io.h': no_e6(F['runtime/ee_io.h']), 'runtime/vif.h': O['runtime/vif.h']}
# ---- s1: no P3
v1 = between(v2, '#if defined(MGS2_GEN_VU) && !defined(GUEST_SPLIT_GEN) && defined(OPT2_P3)\n', '#ifdef MGS2_GEN_VU\n#include MGS2_GEN_VU')
h1 = between(h2, '#define OPT2_P3 1048576u', '#define OPT2_DEFAULT')
st['s1'] = dict(st['s2'], **{'runtime/vu0.h': v1, 'runtime/host_time.h': h1})
for s, files in st.items():
    d = os.path.join(OUT, s)
    if os.path.exists(d):
        sys.exit(f'l41_stages: {d} exists')
    for sub in ('boot', 'runtime', 'rt'):
        shutil.copytree(os.path.join(T39, sub), os.path.join(d, sub), symlinks=True)
    for f, txt in files.items():
        open(os.path.join(d, f), 'w').write(txt)
    print(f'{d}: ' + ', '.join(sorted(files)))
