/* gl41 (attic/gl41s/, 2026-11-23): lever P1 (the VU0 micro-program call fast path), probes (a) (b) (c) and the 'gl41' counter line.
   Included at the end of runtime/vu0.h, in the runtime unit only (after gen_vu.h: it needs the programs' names).  The PSP's boot_rt.o
   and the x86 twin's both compile this file (runtime/ is on both include paths; rt/ has no vu0.h).
   Switches: MGS2_OPT2 bit 19 OPT2_P1 (runtime/host_time.h), alternated by turn with MGS2_OPT2_CYCLE like every OPT2 bit.
   Measurement only, off by default (read once at the first vblank, rt/ and runtime/ ee_io.h io_vblank -> l41_init):
     MGS2_L41_LOG=1        a 'gl41 levers' line every 100 vblanks (the levers' counters; printed in io_vblank, outside the programs)
     MGS2_PROBE41=<mask>   PSP only, run once at the first vblank: 1 = (a) a timed sequential walk over .text with and without
                           `cache 0x1e` one / two lines ahead; 2 = (b) the VFPU facts the box cull's P3 relies on (vmul of non-finite
                           lanes by 0, a denormal, finite, NaN, Inf in the cull's own instruction forms; vcmp ES; vmov bit copies)
     MGS2_PROBE41_CULL=<n> (c) at the n-th call of the box cull's VFPU form: the call's entry state recorded, then timed loops of the
                           gl39 form, the P3 form and the C form on it, then forced inputs (every lane of every input register set to
                           NaN / Inf / denormal / zero / max patterns) through both VFPU forms, compared result for result; the entry
                           state restored before the real call
   The probes read only (.text, their own locals and the VU0 registers they save and restore); no register write but user-mode code;
   no system call but the clock the runtime already imports (sceKernelGetSystemTimeWide). */
#ifndef VU_L41_H
#define VU_L41_H
#ifdef GUEST_SPLIT_RT
#if defined(GEN_VU_FNS) && defined(OPT2_P1) && defined(VP_FAST) && !defined(VU_CHECK) && !defined(YO_PROF) && !defined(VU_STATS) && \
    !defined(VU_FAST) && !defined(VU_TRACEH)
/* P1 (MGS2_OPT2 bit 19): vu_run(pc) of a clean image whose (hash, entry) has a compiled program is vuc_entry + vuc_dispatch: the FPU-mode
   test (vp_fast_env), then VPV(vupv_) VPF(vup_) or vup_() (every C form returns 1: tools/gl41_genvu.py checks it).  The cache holds, per
   (hash, entry), the two functions vuc_dispatch's case calls (gen_vu.h vuc_fns, made from vuc_dispatch's own cases by gl41_genvu.py);
   a hit calls them in the same order with the same test; a miss, a changed image or the other FPU mode takes vu_run, as gl39. */
typedef int (*vuc_fn_t)(void);
#ifndef VUC_FC_N
#define VUC_FC_N 64   /* entries (16 bytes each), a power of 2 */
#endif
#define VUC_FC_I(h, pc) (((h) ^ ((h) >> 16) ^ ((uint32_t)(pc) << 2)) & (VUC_FC_N - 1u))
static struct vuc_fc_t { uint32_t h; int32_t pc; vuc_fn_t v, f; } vuc_fc[VUC_FC_N] __attribute__((aligned(64)));
long g_l41_p1_miss, g_l41_p1_fill;
static __attribute__((noinline)) void vu_run_p1_miss(int pc)
{
  vu_run(pc); g_l41_p1_miss++;
  if (vu_prog_clean) { vuc_fn_t v, f; uint32_t h = vu_prog_hash;
    if (vuc_fns(h, pc, &v, &f)) { struct vuc_fc_t *e = &vuc_fc[VUC_FC_I(h, pc)]; e->h = h; e->pc = pc; e->v = v; e->f = f; g_l41_p1_fill++; } }
}
void vu_run_shared(int pc)
{
  if (g_opt2 & OPT2_P1) {
    if (__builtin_expect(vu_prog_clean, 1)) {
      uint32_t h = vu_prog_hash; const struct vuc_fc_t *e = &vuc_fc[VUC_FC_I(h, pc)];
      if (__builtin_expect(e->h == h && e->pc == pc && e->f != 0, 1) && vp_fast_env()) {
        if (e->v) { if (!e->v()) (void)VPF(e->f); } else (void)e->f();
        return;
      }
    }
    vu_run_p1_miss(pc); return;
  }
  vu_run(pc);   /* gl39's path */
}
#define L41_P1 1
#else
void vu_run_shared(int pc) { vu_run(pc); }   /* the generated units call the micro programs through this one copy */
#endif
#endif   /* GUEST_SPLIT_RT */

#if defined(__psp__)
long long sceKernelGetSystemTimeWide(void);
static uint64_t l41_us(void) { return (uint64_t)sceKernelGetSystemTimeWide(); }
#else
#include <time.h>
static uint64_t l41_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t)t.tv_sec * 1000000u + (uint64_t)t.tv_nsec / 1000u; }
#endif
static int l41_log;

#if defined(GEN_VU_P3) && defined(VU_L41_P3_HOOK)
/* probe (c): the box cull's P3 form f3 of the real call (one of vupv_96936c0f_0_p3, vupv_8baafbc3_0_p3, vupv_660a267e_0_p3) and its gl39
   form f (gen_vu.h vuc_p3_pairs) on the recorded entry state of that call */
typedef struct { VQ vf[32]; uint32_t vi[16], acc[4], q, ir, clip, stat, mac, vmac, fb; int tpc; } L41Reg;
static void l41_get(L41Reg *s) { memcpy(s->vf, VF, sizeof VF); memcpy(s->vi, VIr, sizeof VIr); memcpy(s->acc, VACC, sizeof VACC); s->q = VQr; s->ir = VIreg;
  s->clip = VCLIP; s->stat = VSTAT; s->mac = VMAC; s->vmac = g_vmac; s->fb = g_vgrp_fb; s->tpc = vu_tpc; }
static void l41_put(const L41Reg *s) { memcpy(VF, s->vf, sizeof VF); memcpy(VIr, s->vi, sizeof VIr); memcpy(VACC, s->acc, sizeof VACC); VQr = s->q; VIreg = s->ir;
  VCLIP = s->clip; VSTAT = s->stat; VMAC = s->mac; g_vmac = s->vmac; g_vgrp_fb = s->fb; vu_tpc = s->tpc; }
static int l41_cform(void) { return VPF(vup_96936c0f_0); }
static __attribute__((noinline, cold)) int l41_cull_probe(int (*f3)(void))   /* (noinline, cold: out of the box cull's text) */
{
  int (*f)(void) = 0, k;
  for (k = 0; k < VUC_P3_N; k++) if (vuc_p3_pairs[k][1] == f3) f = vuc_p3_pairs[k][0];
  if (!f) { fprintf(stderr, "L41 probe c: no gl39 form for %p: not run\n", (void *)f3); return 0; }   /* (0: the C form runs, as on a fallback) */
  static L41Reg in, oa, ob; static const uint32_t val[10] = { 0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffffffffu, 0x7f800001u, 0x00000001u,
    0x80000001u, 0x00000000u, 0x80000000u, 0x7f7fffffu };
  static const int reg[17] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 31, -1 };   /* the registers the VFPU form loads; -1: ACC */
  const int K = 400; uint64_t t[5]; int i, r, c, v, ra, rb; long nc = 0, nf = 0, np = 0, nd = 0, ndr = 0; uint32_t sink = 0, o2 = g_opt2;
  l41_get(&in); g_opt2 = o2 & ~OPT2_P3;   /* f is gl39's form while the bit is off (restored below) */
  t[0] = l41_us(); for (i = 0; i < K; i++) l41_put(&in);
  t[1] = l41_us(); for (i = 0; i < K; i++) { l41_put(&in); sink += (uint32_t)f(); }
  t[2] = l41_us(); for (i = 0; i < K; i++) { l41_put(&in); sink += (uint32_t)f3(); }
  t[3] = l41_us(); for (i = 0; i < K; i++) { l41_put(&in); sink += (uint32_t)l41_cform(); }
  t[4] = l41_us();
  for (r = 0; r < 18; r++) for (c = 0; c < 4; c++) for (v = 0; v < 10; v++) {   /* r 17: the I register (lane 0; the forms clamp it) */
    uint32_t *p; if (r == 17 && c) continue;
    l41_put(&in); p = r == 17 ? &VIreg : reg[r] < 0 ? &VACC[c] : &VF[reg[r]].u[c]; *p = val[v]; ra = f(); l41_get(&oa);
    l41_put(&in); p = r == 17 ? &VIreg : reg[r] < 0 ? &VACC[c] : &VF[reg[r]].u[c]; *p = val[v]; rb = f3(); l41_get(&ob);
    nc++; if (ra != rb) ndr++; else if (memcmp(&oa, &ob, sizeof oa)) nd++; else if (ra) np++; else nf++;
    if ((ra != rb || memcmp(&oa, &ob, sizeof oa)) && nd + ndr <= 8)
      fprintf(stderr, "L41 probe c: DIFFERENT: register %d lane %d = %08x: gl39 form %d, P3 form %d\n", r == 17 ? 99 : reg[r], c, (unsigned)val[v], ra, rb);
  }
  l41_put(&in); g_opt2 = o2;
  fprintf(stderr, "L41 probe c: box cull (P3 form at %p), one recorded call, %d loops: ns a call: restore %.0f, VFPU form (gl39) %.0f, without the "
          "corner checks (P3) %.0f, C form (VPF, image 96936c0f's) %.0f (each with the restore; returned %u)\n", (void *)f3, K, (double)(t[1] - t[0]) * 1000.0 / K,
          (double)(t[2] - t[1]) * 1000.0 / K, (double)(t[3] - t[2]) * 1000.0 / K, (double)(t[4] - t[3]) * 1000.0 / K, (unsigned)sink);
  fprintf(stderr, "L41 probe c: forced inputs %ld: both forms fell back %ld, both completed %ld, DIFFERENT results %ld, different return %ld\n",
          nc, nf, np, nd, ndr);
  return (g_opt2 & OPT2_P3) ? f3() : f();   /* the real call (l41_cull_n is 0 now: no second probe) */
}
#endif   /* GEN_VU_P3 */

#if defined(__psp__)
/* probe (a): .text read as data (a word per 64-byte line, or all 16 words), regions of 4096 lines (256 KB: 16 times the D-cache, so every
   line is a miss when the walk reaches it), plain or with `cache 0x1e` (fill) 1 or 2 lines ahead; ns a line.  Read only. */
extern char _ftext[];
static __attribute__((noinline)) uint32_t l41_wk(const uint32_t *p, uint32_t n, int ahead, int all)
{
  uint32_t s = 0;
  if (ahead == 0 && !all) while (n--) { s += *(const volatile uint32_t *)p; p += 16; }
  else if (ahead == 1 && !all) while (n--) { __asm__ volatile ("cache 0x1e, 64(%0)" :: "r"(p) : "memory"); s += *(const volatile uint32_t *)p; p += 16; }
  else if (ahead == 2 && !all) while (n--) { __asm__ volatile ("cache 0x1e, 128(%0)" :: "r"(p) : "memory"); s += *(const volatile uint32_t *)p; p += 16; }
  else if (ahead == 3) while (n--) { __asm__ volatile ("cache 0x1e, 0(%0)" :: "r"(p) : "memory"); p += 16; }   /* the fill alone */
  else while (n--) { const volatile uint32_t *q = p;
    if (ahead == 1) __asm__ volatile ("cache 0x1e, 64(%0)" :: "r"(p) : "memory"); else if (ahead == 2) __asm__ volatile ("cache 0x1e, 128(%0)" :: "r"(p) : "memory");
    s += q[0] + q[1] + q[2] + q[3] + q[4] + q[5] + q[6] + q[7] + q[8] + q[9] + q[10] + q[11] + q[12] + q[13] + q[14] + q[15]; p += 16; }
  return s;
}
static __attribute__((noinline, cold)) void l41_probe_a(void)
{
  const uint32_t L = 4096; static const char *nm[7] = { "1 load", "1 load + fill 1 ahead", "1 load + fill 2 ahead", "16 loads", "16 loads + fill 1 ahead",
    "16 loads + fill 2 ahead", "fill alone" };
  static const int ah[7] = { 0, 1, 2, 0, 1, 2, 3 }, al[7] = { 0, 0, 0, 1, 1, 1, 0 };
  double ns[2][7]; uint32_t sink = 0; int round, k;
  const uint32_t *base = (const uint32_t *)(((uintptr_t)_ftext + 63) & ~(uintptr_t)63);
  for (round = 0; round < 2; round++) for (k = 0; k < 7; k++) {   /* region (k + 3 round) % 8: each kind on another region in round 2 */
    const uint32_t *p = base + (uint32_t)((k + 3 * round) % 8) * L * 16u; uint64_t t0 = l41_us(); sink += l41_wk(p, L, ah[k], al[k]);
    ns[round][k] = (double)(l41_us() - t0) * 1000.0 / L; }
  for (k = 0; k < 7; k++) fprintf(stderr, "L41 probe a: %s: ns a 64-byte line not in the cache %.1f, %.1f\n", nm[k], ns[0][k], ns[1][k]);
  if (sink == 0x12345678u) fprintf(stderr, " \n");
}
/* probe (b): the VFPU behaviour P3 relies on, in the box cull's own instruction forms (tools/gl41_genvu.py: between a corner's check and
   the next check of that register the register is only written by vmul.q d, d, t with the destination's z, w masked) */
static void l41_vmul_tx(const uint32_t a[4], uint32_t x, uint32_t o[4])   /* vpfxt x,x,x,x; vpfxd ,,m,m; vmul.q C130, C130, C620 */
{
  __asm__ volatile ("lv.q C130, 0(%1)\n\tmtv %2, S620\n\tvpfxt x,x,x,x\n\tvpfxd ,,m,m\n\tvmul.q C130, C130, C620\n\tsv.q C130, 0(%0)"
                    :: "r"(o), "r"(a), "r"(x) : "memory");
}
static void l41_vmul_q(const uint32_t a[4], const uint32_t b[4], uint32_t o[4])   /* vpfxd ,,m,m; vmul.q C030, C030, C330 */
{
  __asm__ volatile ("lv.q C030, 0(%1)\n\tlv.q C330, 0(%2)\n\tvpfxd ,,m,m\n\tvmul.q C030, C030, C330\n\tsv.q C030, 0(%0)" :: "r"(o), "r"(a), "r"(b) : "memory");
}
static uint32_t l41_es(const uint32_t a[4])   /* the generated check: 1 when a lane is NaN / Inf */
{
  uint32_t r; __asm__ volatile (".set push\n\t.set noreorder\n\tlv.q C000, 0(%1)\n\tmove %0, $0\n\tvcmp.q ES, C000, C000\n\tbvtl 4, 1f\n\tori %0, %0, 1\n\t1:\n\t.set pop"
                                : "=&r"(r) : "r"(a) : "memory"); return r;
}
static __attribute__((noinline, cold)) void l41_probe_b(void)
{
  static const uint32_t x[8] = { 0x00000000u, 0x80000000u, 0x00000001u, 0x80400000u, 0x3a03126fu, 0x7f7fffffu, 0x7f800000u, 0x7fc00000u };
  static const uint32_t a[3][4] __attribute__((aligned(16))) = { { 0x7f800000u, 0xff800000u, 0x3f800000u, 0x3f800000u },
    { 0xffffffffu, 0x7f800001u, 0x3f800000u, 0x7f800000u }, { 0x3f800000u, 0x3f800000u, 0x7fc00000u, 0xff800000u } };
  uint32_t o[4] __attribute__((aligned(16))), b[4] __attribute__((aligned(16))); int i, k, bad = 0;
  for (k = 0; k < 3; k++) for (i = 0; i < 8; i++) {
    l41_vmul_tx(a[k], x[i], o);
    fprintf(stderr, "L41 probe b: vmul.q t.x (%08x %08x %08x %08x) * %08x, z w masked = %08x %08x %08x %08x, ES %u (in %u)\n", (unsigned)a[k][0],
            (unsigned)a[k][1], (unsigned)a[k][2], (unsigned)a[k][3], (unsigned)x[i], (unsigned)o[0], (unsigned)o[1], (unsigned)o[2], (unsigned)o[3],
            (unsigned)l41_es(o), (unsigned)l41_es(a[k]));
    if (l41_es(a[k]) && !l41_es(o)) bad++;
  }
  for (k = 0; k < 3; k++) for (i = 0; i + 3 < 8; i += 4) {
    b[0] = x[i]; b[1] = x[i + 1]; b[2] = x[i + 2]; b[3] = x[i + 3]; l41_vmul_q(a[k], b, o);
    fprintf(stderr, "L41 probe b: vmul.q (%08x %08x %08x %08x) * (%08x %08x %08x %08x), z w masked = %08x %08x %08x %08x, ES %u\n", (unsigned)a[k][0],
            (unsigned)a[k][1], (unsigned)a[k][2], (unsigned)a[k][3], (unsigned)b[0], (unsigned)b[1], (unsigned)b[2], (unsigned)b[3],
            (unsigned)o[0], (unsigned)o[1], (unsigned)o[2], (unsigned)o[3], (unsigned)l41_es(o));
    if (l41_es(a[k]) && !l41_es(o)) bad++;
  }
  { static const uint32_t m[4] __attribute__((aligned(16))) = { 0x7f800001u, 0xfffc56e0u, 0x00000001u, 0x80000000u };
    __asm__ volatile ("lv.q C700, 0(%1)\n\tvmov.q C710, C700\n\tsv.q C710, 0(%0)" :: "r"(o), "r"(m) : "memory");
    fprintf(stderr, "L41 probe b: vmov.q (%08x %08x %08x %08x) = %08x %08x %08x %08x\n", (unsigned)m[0], (unsigned)m[1], (unsigned)m[2], (unsigned)m[3],
            (unsigned)o[0], (unsigned)o[1], (unsigned)o[2], (unsigned)o[3]);
    if (memcmp(o, m, 16)) bad++; }
  fprintf(stderr, "L41 probe b: P3's rule (a non-finite lane stays non-finite through these vmul forms; vmov copies bits): %s (%d cases broke it)\n",
          bad ? "BROKEN" : "holds", bad);
}
#endif   /* __psp__ */

/* l41_init: once, at the first vblank (rt/ and runtime/ ee_io.h io_vblank) */
static __attribute__((noinline, cold)) void l41_init(void)
{
  const char *e;
  l41_log = (e = getenv("MGS2_L41_LOG")) && atoi(e) != 0;
#if defined(GEN_VU_P3) && defined(VU_L41_P3_HOOK)
  if ((e = getenv("MGS2_PROBE41_CULL"))) l41_cull_n = atoi(e);
#endif
#if defined(__psp__)
  if ((e = getenv("MGS2_PROBE41"))) { int m = atoi(e); if (m & 2) l41_probe_b(); if (m & 1) l41_probe_a(); }
#endif
}
/* the levers' counters, every 100 vblanks with MGS2_L41_LOG=1 (rt/ and runtime/ ee_io.h io_vblank) */
static __attribute__((noinline, cold)) void l41_report(long vb)
{
#if defined(L41_P1)
  long p1m = g_l41_p1_miss, p1f = g_l41_p1_fill;
#else
  long p1m = -1, p1f = -1;
#endif
  fprintf(stderr, "gl41 levers vb %ld: opt2 %08x, p1 misses %ld fills %ld\n", vb, (unsigned)g_opt2, p1m, p1f);
}
/* (a call in io_vblank: its own CFG unchanged.  The vblank starts are counted here: io_vblank's io_vb counts only where gscap_vblank is
   compiled in (the x86 twin); on the PSP build gscap_vblank(++io_vb) is ((void)0) and io_vb stays 0.) */
static void l41_vblank(int start) { static long n; if (start && ++n % 100 == 0 && l41_log) l41_report(n); }
#define L41_VBLANK(start, vb) l41_vblank(start)
#endif   /* VU_L41_H */
