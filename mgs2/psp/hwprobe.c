/* hwprobe.c: hardware facts the port needs and the emulator cannot give, measured once at the start of a harness run and printed to
   log.txt (main_psp.c hw_probe).  No hardware counters, no game state, under a second.
   1. VFPU arithmetic against the FPU: vadd.s / vsub.s / vmul.s / vdot.q results compared bit for bit with the FPU's round-toward-zero
      mode (what the VU0 model computes in: FCR31 0x01000001) and round-to-nearest (0x01000000).  If the VFPU matches round-toward-zero,
      VU0 code can move to the VFPU exactly (recomp/vuvfpu.py); if not, it cannot.
   2. VFPU denormal handling (inputs and results), raw results printed.
   3. Load latency and throughput: the PSP scratchpad (0x00010000), cached RAM, uncached RAM (0x40000000 mirror), and a cache miss
      (pointer chase over 4 MB): whether the small hot host arrays (guest register file, VU0 state) are worth moving to the scratchpad.
   4. (v9) The cost of an instruction-cache miss: generated code in a 1 MB buffer, a chain of jumps that lands on a new 64-byte line each
      time in random order (every jump misses), the same chain over 2 KB (every jump hits), and 1 MB of straight-line code (sequential
      misses).  The PspProf cost model charges 125 cycles for every miss, I and D; v8 measured a D-miss at about 65 extra cycles.
   5. (v11) What the VFPU does with the values the game feeds VU0 macro ops that the VU reads as huge numbers and IEEE as NaN / Inf
      (0xffffffff, 0xfffc56e0, 0x7f800000: seen on x86, MGS2_VFPU_CHK), the sign of flushed denormals, vmax / vmin on signed zeros,
      vcmp ES (the Inf / NaN check a VFPU form needs to fall back to the exact form), and vadd.q / vmul.q lanes against vadd.s / vmul.s.
   6. (v11) Time per VU0 macro op of a 4x4 transform (MULAx, MADDAy, MADDAz, MADDw, the hot loop 0x165d78): the VFPU with VF / ACC in
      memory around each op (the measured prototype), the VFPU with the registers held across the four ops plus one ES check, and a C
      form on the FPU with the VU input normalization (an approximation of the exact generated code).  The cost model counts
      instructions and misses only; VFPU latencies (dependent lv.q -> vmul -> vadd -> sv.q) are what this measures.
   7. (v14) Reading the VFPU condition code after vcmp.  v12 on hardware fell back to the exact form less often than the emulator and
      the x86 model (vb 6000: 30,835 against 48,209) and left the reference at vb 16000, while every result of the probes above matched;
      the checked groups read the code with `vcmp; nop; mfvc $131` right after the op that computes the checked value.  Each trial sets
      the code to the wrong answer first, computes x * x * 1 (x = 1.5, or 1e30 which overflows to infinity, as a group's overflow does),
      vcmp ES on it, and reads bit 4 (any lane) by one method; a read that returns the earlier code counts as wrong.  Also vcmp.s / .p / .t,
      and the same question for data: mfv of a result right after the vmul, and sv.q of it followed by lw.
   8. (v15) VFPU overflow.  v14 (check by branch, probe 7 right) still left the reference at vb 16000 with 1,080 fewer fallbacks than the
      x86 model; on x86 the first fallbacks after vb 15500 are groups of the opening stage whose garbage lanes overflow (a product of
      6.4e26 and -9.7e15; an arctangent series fed 3.4e38 from a division by zero).  Products and sums past the largest float, their
      vcmp ES, and the two groups exactly as generated (v14 form) on the inputs they had on x86, with every register they write.
      (v15 also reads vcmp ES after a vsync in vq_es, so probe 5's ES line is valid from v15 on.)
   9. (v15) VFPU bit behaviour a VFPU form of the compiled VU0 micro programs would rely on: does vmov.q (plain, with a swizzle prefix, with
      a write mask) copy every bit pattern (NaN patterns, denormals, -0); vmin / vmax against +-FLT_MAX (the clamp of a result to the VU's
      range) on zeros, denormals, infinities and NaN patterns; vcmp GT / LT on denormals and zeros (CLIP); |x| by prefix.
  10. (v15) VFPU vmul / vadd / vsub of special inputs (NaN patterns 0xffffffff / 0xfffc56e0 / 0x7fc00000, +-Inf, denormals, +-max) with
      0, 1, +Inf and +max: whether every result x86 gives as NaN / infinity (a fallback of the group check) is NaN / infinity on the PSP.
      The v14 diagnostic run matched the x86 reference at every vblank 15500-15754 and in RAM at vb 15739 and 15755, so the split comes
      after vb 15755, where the x86 fallbacks start to climb (about 70 per vblank).
  11. (v16) The FPU's rules for zero and denormal operands, in the game's mode (FCR31 0x01000001) and round to nearest (0x01000000, where
      a failed VFPU check's fallback runs from v16): add / sub with one zero operand (TT lessons A9: the other operand comes back unchanged,
      a denormal too; x86 models that from v16, guest.h FADD / FSUB), both operands zero (the sign), denormal sums and differences, mul /
      div by +-1 and 0, sqrt, compares against a denormal, trunc.w.s.  v15 left the x86 reference at vb 15794: in 0x11cf88 a garbage word
      0x0031685c met +0, 0 + d stayed d on the PSP and flushed to 0 on x86, so x86 divided by zero and its arctangent run fell back where
      the PSP's did not.  The chain line repeats those steps.
  12. (v18) The data cache on stores and the cache hint instructions, each over 4096 lines (256 KB) that are not in the cache, in ns per
      64-byte line: one load per line (clean lines replaced), one store per line, 16 stores per line, the same after `cache 0x18` (create
      dirty exclusive: the line without reading memory), 4 sv.q per line plain and with the wb bit, `cache 0x1e` (fill) alone and the
      loads after it, loads with a fill 4 lines ahead, loads that replace dirty lines (the write-back cost), 16 uncached stores per line, and
      64 KB copies (newlib memcpy; boot.c rt_qcopy_nov plain and with claimed lines).  Whether a store miss reads the line first (the
      emulator's cost model charges a store miss like a load miss), and what the copies of the deck's scratchpad DMA and qword idiom
      (about 15 k store misses per deck step) can save. */
#include <psputils.h>
#include <pspkernel.h>
#include <pspfpu.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef void (*OutFn)(const char *fmt, ...);
static uint32_t hp_s = 12345u;
static uint32_t rnd(void) { hp_s = hp_s * 1664525u + 1013904223u; return hp_s; }
static uint32_t rnd_float(void)            /* sign and mantissa at random, exponent 2^-27 .. 2^26: no overflow, no denormal products */
{
  uint32_t m = rnd(), e = 100 + (rnd() >> 8) % 54;
  return (m & 0x807fffffu) | (e << 23);
}
static uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

static __attribute__((noinline)) uint32_t fpu_op(int op, uint32_t a, uint32_t b)
{
  float x = u2f(a), y = u2f(b), r;
  if (op == 0) __asm__ volatile ("add.s %0, %1, %2" : "=f"(r) : "f"(x), "f"(y));
  else if (op == 1) __asm__ volatile ("sub.s %0, %1, %2" : "=f"(r) : "f"(x), "f"(y));
  else __asm__ volatile ("mul.s %0, %1, %2" : "=f"(r) : "f"(x), "f"(y));
  return f2u(r);
}
static __attribute__((noinline)) uint32_t vfpu_op(int op, uint32_t a, uint32_t b)
{
  uint32_t in[4] __attribute__((aligned(16))) = { a, b, 0, 0 }, o[4] __attribute__((aligned(16))) = { 0, 0, 0, 0 };
  if (op == 0) __asm__ volatile ("lv.s S000, 0(%1)\n\tlv.s S001, 4(%1)\n\tvadd.s S002, S000, S001\n\tsv.s S002, 0(%0)" : : "r"(o), "r"(in) : "memory");
  else if (op == 1) __asm__ volatile ("lv.s S000, 0(%1)\n\tlv.s S001, 4(%1)\n\tvsub.s S002, S000, S001\n\tsv.s S002, 0(%0)" : : "r"(o), "r"(in) : "memory");
  else __asm__ volatile ("lv.s S000, 0(%1)\n\tlv.s S001, 4(%1)\n\tvmul.s S002, S000, S001\n\tsv.s S002, 0(%0)" : : "r"(o), "r"(in) : "memory");
  return o[0];
}
static __attribute__((noinline)) uint32_t vfpu_dot(const uint32_t *a, const uint32_t *b)
{
  uint32_t va[4] __attribute__((aligned(16))), vb[4] __attribute__((aligned(16))), o[4] __attribute__((aligned(16))) = { 0, 0, 0, 0 };
  memcpy(va, a, 16); memcpy(vb, b, 16);
  __asm__ volatile ("lv.q C000, 0(%1)\n\tlv.q C010, 0(%2)\n\tvdot.q S020, C000, C010\n\tsv.s S020, 0(%0)" : : "r"(o), "r"(va), "r"(vb) : "memory");
  return o[0];
}

static void probe_round(OutFn out)
{
  static const char *const nm[3] = { "vadd.s", "vsub.s", "vmul.s" };
  int op, i, n = 4096;
  for (op = 0; op < 3; op++) {
    int rz = 0, rn = 0, shown = 0; hp_s = 12345u + op;
    for (i = 0; i < n; i++) {
      uint32_t a = rnd_float(), b = rnd_float(), v = vfpu_op(op, a, b), z, e;
      pspFpuSetFCR31(0x01000001u); z = fpu_op(op, a, b);
      pspFpuSetFCR31(0x01000000u); e = fpu_op(op, a, b);
      pspFpuSetFCR31(0x01000001u);
      rz += v == z; rn += v == e;
      if (v != z && v != e && shown < 3) { shown++; out("  %s %08x %08x: VFPU %08x, FPU RZ %08x, RN %08x\n", nm[op], (unsigned)a, (unsigned)b, (unsigned)v, (unsigned)z, (unsigned)e); }
    }
    out("probe %s: %d pairs, VFPU equals FPU round-toward-zero %d, round-to-nearest %d\n", nm[op], n, rz, rn);
  }
  {
    int seq_z = 0, pair_z = 0, seq_n = 0, shown = 0; hp_s = 777u;
    for (i = 0; i < n; i++) {
      uint32_t a[4], b[4], v, p[4], s, t; int k;
      for (k = 0; k < 4; k++) { a[k] = rnd_float(); b[k] = rnd_float(); }
      v = vfpu_dot(a, b);
      pspFpuSetFCR31(0x01000001u);
      for (k = 0; k < 4; k++) p[k] = fpu_op(2, a[k], b[k]);
      s = fpu_op(0, fpu_op(0, fpu_op(0, p[0], p[1]), p[2]), p[3]); seq_z += v == s;
      t = fpu_op(0, fpu_op(0, p[0], p[1]), fpu_op(0, p[2], p[3])); pair_z += v == t;
      if (v != s && v != t && shown < 3) { shown++; out("  vdot.q: VFPU %08x, FPU RZ in order %08x, in pairs %08x\n", (unsigned)v, (unsigned)s, (unsigned)t); }
      pspFpuSetFCR31(0x01000000u);
      for (k = 0; k < 4; k++) p[k] = fpu_op(2, a[k], b[k]);
      s = fpu_op(0, fpu_op(0, fpu_op(0, p[0], p[1]), p[2]), p[3]); seq_n += v == s;
      pspFpuSetFCR31(0x01000001u);
    }
    out("probe vdot.q: %d vectors, VFPU equals FPU RZ ((x+y)+z)+w %d, RZ (x+y)+(z+w) %d, RN ((x+y)+z)+w %d\n", n, seq_z, pair_z, seq_n);
  }
}

static void probe_denormal(OutFn out)
{
  static const uint32_t c[][3] = {           /* op, a, b */
    { 0, 0x00000001u, 0x00000000u },         /* a denormal input plus zero */
    { 0, 0x00400000u, 0x00400000u },         /* two denormals whose sum is normal (2^-126) */
    { 1, 0x00800001u, 0x00800000u },         /* a difference of normals that is denormal (2^-149) */
    { 2, 0x1f800000u, 0x1f800000u },         /* 2^-64 * 2^-64: a denormal product */
    { 2, 0x00400000u, 0x4b000000u },         /* a denormal input times 2^23 */
  };
  unsigned i;
  for (i = 0; i < sizeof c / sizeof *c; i++) {
    uint32_t v = vfpu_op(c[i][0], c[i][1], c[i][2]), z;
    pspFpuSetFCR31(0x01000001u); z = fpu_op(c[i][0], c[i][1], c[i][2]);
    out("probe denormal %s %08x %08x: VFPU %08x, FPU (flush, RZ) %08x\n", c[i][0] == 0 ? "add" : c[i][0] == 1 ? "sub" : "mul",
        (unsigned)c[i][1], (unsigned)c[i][2], (unsigned)v, (unsigned)z);
  }
}

static uint64_t hp_now(void) { return (uint64_t)sceKernelGetSystemTimeWide(); }
static __attribute__((noinline)) uint32_t loads(const volatile uint32_t *p, uint32_t nwords, int reps)
{
  uint32_t s = 0, i; int r;
  for (r = 0; r < reps; r++) for (i = 0; i < nwords; i += 4) s += p[i] + p[i + 1] + p[i + 2] + p[i + 3];
  return s;
}
static __attribute__((noinline)) uint32_t chase(const volatile uint32_t *p, uint32_t start, uint32_t steps)
{
  uint32_t k = start;
  while (steps--) k = p[k];
  return k;
}
/* a random cycle over n slots spaced `stride` words apart: p[slot * stride] holds the next slot's word index */
static void make_chain(uint32_t *p, uint32_t n, uint32_t stride)
{
  uint32_t *ord = malloc(n * sizeof *ord), i;
  if (!ord) return;
  for (i = 0; i < n; i++) ord[i] = i;
  for (i = n - 1; i > 0; i--) { uint32_t j = rnd() % (i + 1), t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
  for (i = 0; i < n; i++) p[ord[i] * stride] = ord[(i + 1) % n] * stride;
  free(ord);
}
static void probe_memory(OutFn out)
{
  volatile uint32_t *spr = (volatile uint32_t *)0x00010000;   /* the PSP scratchpad, 16 KB */
  static uint32_t ram[2048] __attribute__((aligned(64)));      /* 8 KB: stays in the 16 KB D-cache */
  uint32_t *big = malloc((4u << 20) + 64), *bp, sink = 0, i; uint64_t t0, t1;
  const uint32_t nl = 1u << 18, nc = 1u << 16;
  for (i = 0; i < 4096; i++) spr[i] = i;
  for (i = 0; i < 2048; i++) ram[i] = i;
  sceKernelDcacheWritebackInvalidateAll();
  t0 = hp_now(); sink += loads(spr, 4096, nl / 4096); t1 = hp_now();
  out("probe loads, independent: scratchpad %.2f ns", (double)(t1 - t0) * 1000.0 / nl);
  sink += loads(ram, 2048, 1);
  t0 = hp_now(); sink += loads(ram, 2048, nl / 2048); t1 = hp_now();
  out(", cached RAM %.2f ns", (double)(t1 - t0) * 1000.0 / nl);
  t0 = hp_now(); sink += loads((volatile uint32_t *)((uintptr_t)ram | 0x40000000u), 2048, 8); t1 = hp_now();
  out(", uncached RAM %.2f ns per load\n", (double)(t1 - t0) * 1000.0 / (8 * 2048));
  hp_s = 4242u; make_chain((uint32_t *)spr, 1024, 4);   /* 1024 slots, one per 16 bytes */
  t0 = hp_now(); sink += chase(spr, 0, nc); t1 = hp_now();
  out("probe dependent loads: scratchpad %.2f ns", (double)(t1 - t0) * 1000.0 / nc);
  make_chain(ram, 512, 4); sink += chase(ram, 0, 512);
  t0 = hp_now(); sink += chase(ram, 0, nc); t1 = hp_now();
  out(", cached RAM %.2f ns", (double)(t1 - t0) * 1000.0 / nc);
  if (big) {
    bp = (uint32_t *)(((uintptr_t)big + 63) & ~(uintptr_t)63);
    make_chain(bp, (4u << 20) / 64, 16);                 /* one slot per 64-byte line over 4 MB: every load misses */
    sceKernelDcacheWritebackInvalidateAll();
    t0 = hp_now(); sink += chase(bp, 0, nc); t1 = hp_now();
    out(", cache miss (4 MB chase) %.2f ns", (double)(t1 - t0) * 1000.0 / nc);
    free(big);
  }
  out("\n");
  if (sink == 0x12345678u) out(" \n");                   /* (keeps the loads) */
}

/* line i of a code buffer: a jump to line `to` (or, for to < 0, a return), then a nop for the delay slot; the rest of the line stays nop */
static void line_jump(uint32_t *code, uint32_t i, int32_t to)
{
  uint32_t *w = code + i * 16;
  w[0] = to < 0 ? 0x03e00008u : 0x08000000u | (((uint32_t)(uintptr_t)(code + (uint32_t)to * 16) >> 2) & 0x03ffffffu);   /* jr ra / j */
  w[1] = 0;
}
static void probe_icache(OutFn out)
{
  const uint32_t nl = (1u << 20) / 64, small = 32;           /* 16384 lines over 1 MB; the hit chain: 32 lines (2 KB) */
  uint32_t *raw = malloc((1u << 20) + 64), *code, *ord, i; uint64_t t0, t1; void (*fn)(void);
  if (!raw) { out("probe icache: no memory\n"); return; }
  ord = malloc(nl * sizeof *ord);
  if (!ord) { free(raw); out("probe icache: no memory\n"); return; }
  code = (uint32_t *)(((uintptr_t)raw + 63) & ~(uintptr_t)63);
  /* random chain over every line */
  memset(code, 0, 1u << 20);
  for (i = 0; i < nl; i++) ord[i] = i;
  hp_s = 999u;
  for (i = nl - 1; i > 0; i--) { uint32_t j = rnd() % (i + 1), t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
  for (i = 0; i + 1 < nl; i++) line_jump(code, ord[i], (int32_t)ord[i + 1]);
  line_jump(code, ord[nl - 1], -1);
  sceKernelDcacheWritebackAll(); sceKernelIcacheInvalidateAll();
  fn = (void (*)(void))(void *)(code + ord[0] * 16);
  t0 = hp_now(); fn(); t1 = hp_now();
  out("probe icache: jump to a new line (1 MB, random order) %.1f ns", (double)(t1 - t0) * 1000.0 / nl);
  /* the same kind of chain over 32 lines, run 512 times: hits after the first pass */
  memset(code, 0, small * 64);
  for (i = 0; i + 1 < small; i++) line_jump(code, i, (int32_t)(i + 1));
  line_jump(code, small - 1, -1);
  sceKernelDcacheWritebackAll(); sceKernelIcacheInvalidateAll();
  fn = (void (*)(void))(void *)code; fn();
  t0 = hp_now(); for (i = 0; i < nl / small; i++) fn(); t1 = hp_now();
  out(", in cache %.1f ns", (double)(t1 - t0) * 1000.0 / nl);
  /* 1 MB of straight-line nops, then a return: one miss per 16 instructions, in address order */
  memset(code, 0, 1u << 20);
  code[(1u << 18) - 2] = 0x03e00008u; code[(1u << 18) - 1] = 0;
  sceKernelDcacheWritebackAll(); sceKernelIcacheInvalidateAll();
  fn = (void (*)(void))(void *)code;
  t0 = hp_now(); fn(); t1 = hp_now();
  out(", straight-line code %.1f ns per 64-byte line (%.2f ns per instruction)\n", (double)(t1 - t0) * 1000.0 / nl, (double)(t1 - t0) * 1000.0 / (1u << 18));
  sceKernelIcacheInvalidateAll();
  free(ord); free(raw);
}

/* 5. VFPU semantics on the VU's special values */
static __attribute__((noinline)) void vq_op(const char *op, const uint32_t *a, const uint32_t *b, uint32_t *o)
{
  uint32_t va[4] __attribute__((aligned(16))), vb[4] __attribute__((aligned(16))), vo[4] __attribute__((aligned(16)));
  memcpy(va, a, 16); memcpy(vb, b, 16);
  if (op[1] == 'a') __asm__ volatile ("lv.q C000, 0(%1)\n\tlv.q C010, 0(%2)\n\tvadd.q C020, C000, C010\n\tsv.q C020, 0(%0)" : : "r"(vo), "r"(va), "r"(vb) : "memory");
  else if (op[1] == 'm') __asm__ volatile ("lv.q C000, 0(%1)\n\tlv.q C010, 0(%2)\n\tvmul.q C020, C000, C010\n\tsv.q C020, 0(%0)" : : "r"(vo), "r"(va), "r"(vb) : "memory");
  else if (op[2] == 'a') __asm__ volatile ("lv.q C000, 0(%1)\n\tlv.q C010, 0(%2)\n\tvmax.q C020, C000, C010\n\tsv.q C020, 0(%0)" : : "r"(vo), "r"(va), "r"(vb) : "memory");
  else __asm__ volatile ("lv.q C000, 0(%1)\n\tlv.q C010, 0(%2)\n\tvmin.q C020, C000, C010\n\tsv.q C020, 0(%0)" : : "r"(vo), "r"(va), "r"(vb) : "memory");
  memcpy(o, vo, 16);
}
static __attribute__((noinline)) uint32_t vq_es(const uint32_t *a)   /* vcmp.q ES: the VFPU condition bits (lanes 0-3, any 4, all 5) */
{
  uint32_t va[4] __attribute__((aligned(16))), cc;
  memcpy(va, a, 16);
  __asm__ volatile ("lv.q C000, 0(%1)\n\tvcmp.q ES, C000, C000\n\tvsync\n\tmfvc %0, $131" : "=r"(cc) : "r"(va) : "memory");   /* (v15: vsync; the nop form read the old bits, probe 7) */
  return cc & 0x3f;
}
static void probe_vfpu_special(OutFn out)
{
  static const uint32_t A[6][4] = {
    { 0xfffc56e0u, 0xffffffffu, 0x7f800000u, 0x7f7fffffu },   /* + 0: the VU gives -max, -max, +max, +max */
    { 0xffffffffu, 0xffffffffu, 0x7f800000u, 0x7f7fffffu },   /* * (-1, -max, 0, 2): VU +max, +max, 0, +max */
    { 0x80000001u, 0x80400000u, 0x00400000u, 0x80800000u },   /* + 0 / * 1: denormal inputs; last: -min normal - x (denormal result) */
    { 0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u },   /* vmax / vmin with (-0, +0, +0, -0) */
    { 0x3f800000u, 0x7fc00000u, 0xff800000u, 0x00000001u },   /* vcmp ES: 1, NaN, -Inf, denormal */
    { 0x7f7fffffu, 0x7f800000u, 0xfffc56e0u, 0x80000000u } }; /* vcmp ES: max, Inf, the VU's huge negative, -0 */
  static const uint32_t B[4][4] = {
    { 0, 0, 0, 0 },
    { 0xbf800000u, 0xff7fffffu, 0x00000000u, 0x40000000u },
    { 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u },
    { 0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u } };
  uint32_t o[4];
  vq_op("vadd", A[0], B[0], o); out("probe vfpu special: vadd.q (fffc56e0 ffffffff 7f800000 7f7fffff) + 0 = %08x %08x %08x %08x\n", o[0], o[1], o[2], o[3]);
  vq_op("vmul", A[1], B[1], o); out("probe vfpu special: vmul.q (ffffffff ffffffff 7f800000 7f7fffff) * (bf800000 ff7fffff 0 40000000) = %08x %08x %08x %08x\n", o[0], o[1], o[2], o[3]);
  vq_op("vadd", A[2], B[2], o); out("probe vfpu special: vadd.q (80000001 80400000 00400000 80800000) + (0 0 0 1) = %08x %08x %08x %08x\n", o[0], o[1], o[2], o[3]);
  { static const uint32_t one[4] = { 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u };
    vq_op("vmul", A[2], one, o); out("probe vfpu special: vmul.q (80000001 80400000 00400000 80800000) * 1 = %08x %08x %08x %08x\n", o[0], o[1], o[2], o[3]); }
  { static const uint32_t sm[4] = { 0x80800000u, 0x00800000u, 0x80800001u, 0x3f000000u }, sm2[4] = { 0x00800001u, 0x80800001u, 0x00800000u, 0x80800000u };
    vq_op("vadd", sm, sm2, o); out("probe vfpu special: vadd.q (80800000 00800000 80800001 3f000000) + (00800001 80800001 00800000 80800000) = %08x %08x %08x %08x\n", o[0], o[1], o[2], o[3]); }
  vq_op("vmax", A[3], B[3], o); out("probe vfpu special: vmax.q (0 80000000 0 80000000) (80000000 0 80000000 0) = %08x %08x %08x %08x\n", o[0], o[1], o[2], o[3]);
  vq_op("vmin", A[3], B[3], o); out("probe vfpu special: vmin.q same = %08x %08x %08x %08x\n", o[0], o[1], o[2], o[3]);
  out("probe vfpu special: vcmp.q ES (3f800000 7fc00000 ff800000 00000001) cc %02x, (7f7fffff 7f800000 fffc56e0 80000000) cc %02x\n", vq_es(A[4]), vq_es(A[5]));
  { int i, c, nq = 1024, eqa = 0, eqm = 0; hp_s = 777u;
    for (i = 0; i < nq; i++) {
      uint32_t a[4], b[4], oa[4], om[4];
      for (c = 0; c < 4; c++) { a[c] = rnd_float(); b[c] = rnd_float(); }
      vq_op("vadd", a, b, oa); vq_op("vmul", a, b, om);
      for (c = 0; c < 4; c++) { eqa += oa[c] == vfpu_op(0, a[c], b[c]); eqm += om[c] == vfpu_op(2, a[c], b[c]); }
    }
    out("probe vfpu lanes: vadd.q lanes equal vadd.s %d / %d, vmul.q lanes equal vmul.s %d / %d\n", eqa, 4 * nq, eqm, 4 * nq);
  }
}

/* 6. time per VU0 macro op of the hot transform, three forms */
static uint32_t hp_vf[32 * 4] __attribute__((aligned(16))), hp_acc[4] __attribute__((aligned(16)));
static __attribute__((noinline)) void vt_mem(uint32_t *vf, uint32_t *acc, int n)
{
  while (n--)
    __asm__ volatile (
      "lv.q C700, 64(%0)\n\tlv.q C710, 128(%0)\n\tvpfxt x,x,x,x\n\tvmul.q C730, C700, C710\n\tsv.q C730, 0(%1)\n\t"
      "lv.q C700, 80(%0)\n\tlv.q C710, 128(%0)\n\tlv.q C720, 0(%1)\n\tvpfxt y,y,y,y\n\tvmul.q C600, C700, C710\n\tvadd.q C730, C720, C600\n\tsv.q C730, 0(%1)\n\t"
      "lv.q C700, 96(%0)\n\tlv.q C710, 128(%0)\n\tlv.q C720, 0(%1)\n\tvpfxt z,z,z,z\n\tvmul.q C600, C700, C710\n\tvadd.q C730, C720, C600\n\tsv.q C730, 0(%1)\n\t"
      "lv.q C700, 112(%0)\n\tlv.q C710, 128(%0)\n\tlv.q C720, 0(%1)\n\tvpfxt w,w,w,w\n\tvmul.q C600, C700, C710\n\tvadd.q C730, C720, C600\n\tsv.q C730, 144(%0)"
      : : "r"(vf), "r"(acc) : "memory");
}
static __attribute__((noinline)) int vt_reg(uint32_t *vf, uint32_t *acc, int n)
{
  int bad = 0;
  while (n--) {
    __asm__ volatile (
      "lv.q C400, 64(%0)\n\tlv.q C410, 80(%0)\n\tlv.q C420, 96(%0)\n\tlv.q C430, 112(%0)\n\tlv.q C500, 128(%0)\n\t"
      "vpfxt x,x,x,x\n\tvmul.q C510, C400, C500\n\t"
      "vpfxt y,y,y,y\n\tvmul.q C520, C410, C500\n\tvadd.q C510, C510, C520\n\t"
      "vpfxt z,z,z,z\n\tvmul.q C520, C420, C500\n\tvadd.q C510, C510, C520\n\t"
      "vpfxt w,w,w,w\n\tvmul.q C520, C430, C500\n\tvadd.q C530, C510, C520"
      : : "r"(vf) : "memory");
    __asm__ goto (".set push\n\t.set noreorder\n\tvcmp.q ES, C530, C530\n\tnop\n\tbvt 4, %l[fb]\n\tnop\n\t.set pop" : : : : fb);
    __asm__ volatile ("sv.q C510, 0(%1)\n\tsv.q C530, 144(%0)" : : "r"(vf), "r"(acc) : "memory");
    continue;
  fb: bad++;
  }
  return bad;
}
static inline __attribute__((always_inline)) float hp_vin(uint32_t u)
{ uint32_t e = u & 0x7f800000u; if (!e) u &= 0x80000000u; else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu; return u2f(u); }
static __attribute__((noinline)) void vt_fpu(uint32_t *vf, uint32_t *acc, int n)
{
  while (n--) {
    int c; float x = hp_vin(vf[32]), y = hp_vin(vf[33]), z = hp_vin(vf[34]), w = hp_vin(vf[35]);
    for (c = 0; c < 4; c++) acc[c] = f2u(hp_vin(vf[16 + c]) * x);
    for (c = 0; c < 4; c++) acc[c] = f2u(u2f(acc[c]) + hp_vin(vf[20 + c]) * y);
    for (c = 0; c < 4; c++) acc[c] = f2u(u2f(acc[c]) + hp_vin(vf[24 + c]) * z);
    for (c = 0; c < 4; c++) vf[36 + c] = f2u(u2f(acc[c]) + hp_vin(vf[28 + c]) * w);
    { uint32_t f; __asm__ volatile ("cfc1 %0, $31" : "=r"(f)); if (f & 0x10) acc[0] = 0; }   /* the overflow check of VU_MCL */
    __asm__ volatile ("" : : : "memory");
  }
}
static void probe_vfpu_cost(OutFn out)
{
  int i, n = 200000, bad; uint64_t t0, t1, t2, t3;
  for (i = 0; i < 32 * 4; i++) hp_vf[i] = rnd_float();
  vt_mem(hp_vf, hp_acc, 100); vt_reg(hp_vf, hp_acc, 100); vt_fpu(hp_vf, hp_acc, 100);
  t0 = hp_now(); vt_mem(hp_vf, hp_acc, n); t1 = hp_now(); bad = vt_reg(hp_vf, hp_acc, n); t2 = hp_now(); vt_fpu(hp_vf, hp_acc, n); t3 = hp_now();
  out("probe vfpu cost per VU0 macro op (4x4 transform, %d x 4 ops): VFPU with VF / ACC in memory %.1f ns, VFPU in registers + one ES check %.1f ns (%d checks failed), FPU C form %.1f ns\n",
      n, (double)(t1 - t0) * 1000.0 / (4.0 * n), (double)(t2 - t1) * 1000.0 / (4.0 * n), bad, (double)(t3 - t2) * 1000.0 / (4.0 * n));
}

/* 7: the condition code read.  C030 = 1.0 (loaded in each block); C020 = the vector that sets the code to the wrong answer first. */
#define CC_PRE "lv.q C030, 0(%3)\n\tlv.q C020, 0(%1)\n\tvcmp.q ES, C020, C020\n\tlv.q C000, 0(%2)\n\tvmul.q C010, C000, C000\n\tvmul.q C010, C010, C030\n\t"
#define CC_FN(name, cmp, rd) static __attribute__((noinline)) uint32_t name(const uint32_t *pre, const uint32_t *in, const uint32_t *one) \
  { uint32_t cc; __asm__ volatile (".set push\n\t.set noreorder\n\t" CC_PRE cmp "\n\t" rd "\n\t.set pop" : "=&r"(cc) : "r"(pre), "r"(in), "r"(one) : "memory"); return cc & 0x10u; }
CC_FN(cc_nop1, "vcmp.q ES, C010, C010", "nop\n\tmfvc %0, $131")                          /* the v12 / v13 group check */
CC_FN(cc_bvt, "vcmp.q ES, C010, C010", "li %0, 0\n\tbvtl 4, 1f\n\tli %0, 0x10\n1:")            /* the v14 group check */
CC_FN(cc_nop4, "vcmp.q ES, C010, C010", "nop\n\tnop\n\tnop\n\tnop\n\tmfvc %0, $131")
CC_FN(cc_nop16, "vcmp.q ES, C010, C010", "nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tmfvc %0, $131")
CC_FN(cc_vsync, "vcmp.q ES, C010, C010", "vsync\n\tmfvc %0, $131")
CC_FN(cc_mf255, "vcmp.q ES, C010, C010", ".word 0x486000ff\n\tmfvc %0, $131")                /* mfvc $zero, $255: an interlock some games use (PPSSPP note) */
CC_FN(cc_s_nop1, "vcmp.s ES, S010, S010", "nop\n\tmfvc %0, $131")
CC_FN(cc_s_bvt, "vcmp.s ES, S010, S010", "li %0, 0\n\tbvtl 4, 1f\n\tli %0, 0x10\n1:")
CC_FN(cc_p_bvt, "vcmp.p ES, C010, C010", "li %0, 0\n\tbvtl 4, 1f\n\tli %0, 0x10\n1:")
CC_FN(cc_t_bvt, "vcmp.t ES, C010, C010", "li %0, 0\n\tbvtl 4, 1f\n\tli %0, 0x10\n1:")
/* the same question for data: a VFPU result read by mfv right after the op, and stored by sv.q then loaded by lw (buf holds the
   previous trial's value, C010 the opposite input's square first) */
static __attribute__((noinline)) uint32_t rd_mfv(const uint32_t *pre, const uint32_t *in)
{ uint32_t r; __asm__ volatile ("lv.q C010, 0(%1)\n\tlv.q C000, 0(%2)\n\tvmul.q C010, C000, C000\n\tmfv %0, S010" : "=r"(r) : "r"(pre), "r"(in) : "memory"); return r; }
static __attribute__((noinline)) uint32_t rd_svlw(uint32_t *buf, const uint32_t *in)
{ uint32_t r; __asm__ volatile ("lv.q C000, 0(%2)\n\tvmul.q C010, C000, C000\n\tsv.q C010, 0(%1)\n\tlw %0, 0(%1)" : "=r"(r) : "r"(buf), "r"(in) : "memory"); return r; }
static void probe_vfpu_cc(OutFn out)
{
  typedef uint32_t (*CcFn)(const uint32_t *, const uint32_t *, const uint32_t *);
  static const CcFn fn[] = { cc_nop1, cc_bvt, cc_nop4, cc_nop16, cc_vsync, cc_mf255, cc_s_nop1, cc_s_bvt, cc_p_bvt, cc_t_bvt };
  static const char *name[] = { "vcmp.q nop mfvc (v12 check)", "vcmp.q bvtl (v14 check)", "4 nops mfvc", "16 nops mfvc", "vsync mfvc",
                                "mfvc $255 mfvc", "vcmp.s nop mfvc", "vcmp.s bvtl", "vcmp.p bvtl", "vcmp.t bvtl" };
  static uint32_t big[4] __attribute__((aligned(16))) = { 0x7149f2cau, 0x7149f2cau, 0x7149f2cau, 0x7149f2cau };   /* 1e30: squared, infinity */
  static uint32_t small[4] __attribute__((aligned(16))) = { 0x3fc00000u, 0x3fc00000u, 0x3fc00000u, 0x3fc00000u }; /* 1.5 */
  static uint32_t inf[4] __attribute__((aligned(16))) = { 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u };
  static uint32_t one[4] __attribute__((aligned(16))) = { 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u };
  for (unsigned m = 0; m < sizeof fn / sizeof fn[0]; m++) {
    int n = 4096, wrong = 0, fails = 0;
    for (int i = 0; i < n; i++) {
      int sp = rnd() & 1;                       /* expected: special */
      uint32_t cc = fn[m](sp ? one : inf, sp ? big : small, one);
      fails += sp; wrong += (cc != 0) != sp;
    }
    out("probe vfpu cc: %-28s wrong %d / %d (%d special)\n", name[m], wrong, n, fails);
  }
  { static uint32_t a[4] __attribute__((aligned(16))) = { 0x3fc00000u, 0, 0, 0 }, b[4] __attribute__((aligned(16))) = { 0x40400000u, 0, 0, 0 };
    static uint32_t sa[4] __attribute__((aligned(16))) = { 0x40100000u, 0, 0, 0 }, sb[4] __attribute__((aligned(16))) = { 0x41100000u, 0, 0, 0 };
    static uint32_t buf[4] __attribute__((aligned(16)));   /* 1.5^2 = 2.25 (0x40100000), 3^2 = 9 (0x41100000) */
    int n = 4096, w1 = 0, w2 = 0;
    for (int i = 0; i < n; i++) {
      int x = rnd() & 1;
      w1 += rd_mfv(x ? sa : sb, x ? b : a) != (x ? 0x41100000u : 0x40100000u);
      buf[0] = x ? 0x40100000u : 0x41100000u;
      w2 += rd_svlw(buf, x ? b : a) != (x ? 0x41100000u : 0x40100000u);
    }
    out("probe vfpu data: vmul then mfv wrong %d / %d, vmul sv.q then lw wrong %d / %d\n", w1, n, w2, n); }
}

/* 8. VFPU overflow */
static uint32_t hp8_vf[32 * 4] __attribute__((aligned(16))), hp8_acc[4] __attribute__((aligned(16)));
static __attribute__((noinline)) uint32_t grp_p11cf88(void)   /* gen_elf.h 0x11d02c (v14 headers) */
{ uint32_t vcc_, *VF = hp8_vf, *VACC = hp8_acc; __asm__ volatile (".set push\n\t.set noreorder\n\tmove %[cc], $0\n\tlv.q C000, 64(%[vf])\n\tlv.q C020, 16(%[vf])\n\tlv.q C320, 32(%[vf])\n\tlv.q C330, 48(%[vf])\n\tvmul.q C010, C000, C000\n\tvpfxt x,x,x,x\n\tvmul.q C030, C000, C020\n\tvmul.q C100, C010, C000\n\tvmul.q C110, C010, C010\n\tvmul.q C120, C100, C010\n\tvmul.q C130, C100, C100\n\tvmul.q C200, C110, C110\n\tvmul.q C210, C110, C100\n\tvmul.q C220, C110, C120\n\tvmul.q C230, C120, C130\n\tvpfxt y,y,y,y\n\tvmul.q C730, C100, C020\n\tvadd.q C030, C030, C730\n\tvmul.q C300, C210, C130\n\tvmul.q C310, C210, C200\n\tvpfxt z,z,z,z\n\tvmul.q C730, C120, C020\n\tvadd.q C030, C030, C730\n\tvpfxt w,w,w,w\n\tvmul.q C730, C210, C020\n\tvadd.q C030, C030, C730\n\tvpfxt x,x,x,x\n\tvmul.q C730, C220, C320\n\tvadd.q C030, C030, C730\n\tvpfxt y,y,y,y\n\tvmul.q C730, C230, C320\n\tvadd.q C030, C030, C730\n\tvpfxt z,z,z,z\n\tvmul.q C730, C300, C320\n\tvadd.q C030, C030, C730\n\tvpfxt w,w,w,w\n\tvmul.q C730, C310, C320\n\tvadd.q C030, C030, C730\n\tvpfxt 1,1,1,1\n\tvmul.q C730, C330, C700\n\tvadd.q C000, C030, C730\n\tvcmp.q ES, C000, C000\n\tbvtl 4, 1f\n\tori %[cc], %[cc], 0x10\n\t1:\n\tvcmp.q ES, C300, C300\n\tbvtl 4, 1f\n\tori %[cc], %[cc], 0x10\n\t1:\n\tvcmp.q ES, C230, C230\n\tbvtl 4, 1f\n\tori %[cc], %[cc], 0x10\n\t1:\n\tvcmp.q ES, C220, C220\n\tbvtl 4, 1f\n\tori %[cc], %[cc], 0x10\n\t1:\n\t.set pop" : [cc] "=&r"(vcc_) : [vf] "r"(VF) : "memory"); __asm__ volatile ("sv.q C000, 64(%[vf])\n\tsv.q C010, 208(%[vf])\n\tsv.q C030, 0(%[acc])\n\tsv.q C100, 96(%[vf])\n\tsv.q C110, 224(%[vf])\n\tsv.q C120, 112(%[vf])\n\tsv.q C130, 240(%[vf])\n\tsv.q C200, 256(%[vf])\n\tsv.q C210, 128(%[vf])\n\tsv.q C220, 144(%[vf])\n\tsv.q C230, 160(%[vf])\n\tsv.q C300, 176(%[vf])\n\tsv.q C310, 192(%[vf])" : : [vf] "r"(VF), [acc] "r"(VACC) : "memory"); return vcc_; }
static __attribute__((noinline)) uint32_t grp_d00t(void)      /* gen_s_d00t.h 0x32fcc0 (v14 headers) */
{ uint32_t vcc_, *VF = hp8_vf, *VACC = hp8_acc; __asm__ volatile (".set push\n\t.set noreorder\n\tmove %[cc], $0\n\tlv.q C000, 16(%[vf])\n\tlv.q C010, 80(%[vf])\n\tlv.q C020, 64(%[vf])\n\tvpfxs 0,0,0,1\n\tvpfxt 1,1,1,1\n\tvpfxd ,,,m\n\tvadd.q C000, C700, C700\n\tvpfxt x,x,x,x\n\tvmul.q C030, C010, C020\n\tvpfxt y,y,y,y\n\tvmul.q C100, C010, C020\n\tvpfxt z,z,z,z\n\tvmul.q C110, C010, C020\n\tvpfxt w,w,w,w\n\tvmul.q C120, C010, C020\n\tvpfxt w,w,w,w\n\tvmul.q C730, C000, C030\n\tvpfxd ,m,m,m\n\tvadd.q C120, C120, C730\n\tvpfxt z,z,z,z\n\tvmul.q C730, C000, C100\n\tvpfxd ,m,m,m\n\tvadd.q C120, C120, C730\n\tvpfxt y,y,y,y\n\tvmul.q C730, C000, C110\n\tvpfxd ,m,m,m\n\tvsub.q C020, C120, C730\n\tvpfxt w,w,w,w\n\tvmul.q C730, C000, C100\n\tvpfxd m,,m,m\n\tvadd.q C120, C120, C730\n\tvpfxt x,x,x,x\n\tvmul.q C730, C000, C110\n\tvpfxd m,,m,m\n\tvadd.q C120, C120, C730\n\tvpfxt z,z,z,z\n\tvmul.q C730, C000, C030\n\tvpfxd m,,m,m\n\tvsub.q C020, C120, C730\n\tvpfxt w,w,w,w\n\tvmul.q C730, C000, C110\n\tvpfxd m,m,,m\n\tvadd.q C120, C120, C730\n\tvpfxt y,y,y,y\n\tvmul.q C730, C000, C030\n\tvpfxd m,m,,m\n\tvadd.q C120, C120, C730\n\tvpfxt x,x,x,x\n\tvmul.q C730, C000, C100\n\tvpfxd m,m,,m\n\tvsub.q C020, C120, C730\n\tvpfxs 0,0,0,1\n\tvpfxt x,x,x,x\n\tvmul.q C730, C700, C030\n\tvpfxd m,m,m,\n\tvsub.q C120, C120, C730\n\tvpfxs 0,0,0,1\n\tvpfxt y,y,y,y\n\tvmul.q C730, C700, C100\n\tvpfxd m,m,m,\n\tvsub.q C120, C120, C730\n\tvpfxs 0,0,0,1\n\tvpfxt z,z,z,z\n\tvmul.q C730, C700, C110\n\tvpfxd m,m,m,\n\tvsub.q C020, C120, C730\n\tvcmp.q ES, C020, C020\n\tbvtl 4, 1f\n\tori %[cc], %[cc], 0x10\n\t1:\n\tvcmp.q ES, C120, C120\n\tbvtl 4, 1f\n\tori %[cc], %[cc], 0x10\n\t1:\n\tvcmp.q ES, C110, C110\n\tbvtl 4, 1f\n\tori %[cc], %[cc], 0x10\n\t1:\n\tvcmp.q ES, C100, C100\n\tbvtl 4, 1f\n\tori %[cc], %[cc], 0x10\n\t1:\n\tvcmp.q ES, C030, C030\n\tbvtl 4, 1f\n\tori %[cc], %[cc], 0x10\n\t1:\n\tvcmp.t ES, C000, C000\n\tbvtl 4, 1f\n\tori %[cc], %[cc], 0x10\n\t1:\n\t.set pop" : [cc] "=&r"(vcc_) : [vf] "r"(VF) : "memory"); __asm__ volatile ("sv.q C000, 16(%[vf])\n\tsv.q C020, 64(%[vf])\n\tsv.q C030, 96(%[vf])\n\tsv.q C100, 112(%[vf])\n\tsv.q C110, 128(%[vf])\n\tsv.q C120, 0(%[acc])" : : [vf] "r"(VF), [acc] "r"(VACC) : "memory"); return vcc_; }
static void pr_q(OutFn out, const char *what, const uint32_t *o)
{ out("probe vfpu ovf: %s = %08x %08x %08x %08x (vcmp ES %02x)\n", what, o[0], o[1], o[2], o[3], vq_es(o)); }
static void probe_vfpu_ovf(OutFn out)
{
  static const uint32_t m1a[4] = { 0x6c048010u, 0x5953fcb2u, 0x7f7fffffu, 0x7149f2cau }, m1b[4] = { 0xda0a3747u, 0x6c048010u, 0x7f7fffffu, 0x7149f2cau };
  static const uint32_t m2a[4] = { 0x7f000000u, 0x7f000000u, 0x7e800000u, 0x7f7fffffu }, m2b[4] = { 0x40000000u, 0x7f000000u, 0x40800000u, 0x3f800001u };
  static const uint32_t a1a[4] = { 0x7f7fffffu, 0xff7fffffu, 0x7f7fffffu, 0x7f7fffffu }, a1b[4] = { 0x7f7fffffu, 0xff7fffffu, 0x73000000u, 0x73000001u };
  static const uint32_t pw[4] = { 0x6c048010u, 0x7f7fffffu, 0x5953fcb2u, 0xbf68c0d5u };
  uint32_t o[4], o2[4]; int r, c;
  vq_op("vmul", m1a, m1b, o); pr_q(out, "vmul.q (6c048010 5953fcb2 7f7fffff 7149f2ca) * (da0a3747 6c048010 7f7fffff 7149f2ca)", o);
  vq_op("vmul", m2a, m2b, o); pr_q(out, "vmul.q (7f000000 7f000000 7e800000 7f7fffff) * (40000000 7f000000 40800000 3f800001)", o);
  vq_op("vadd", a1a, a1b, o); pr_q(out, "vadd.q (7f7fffff ff7fffff 7f7fffff 7f7fffff) + (7f7fffff ff7fffff 73000000 73000001)", o);
  vq_op("vmul", pw, pw, o); pr_q(out, "x^2, x = (6c048010 7f7fffff 5953fcb2 bf68c0d5)", o);
  vq_op("vmul", o, o, o2); pr_q(out, "x^4", o2);
  vq_op("vmul", o2, pw, o); pr_q(out, "x^5", o);
  { static const uint32_t in1[4] = { 0x3f7ffff5u, 0xbeaaa61cu, 0x3e4c40a6u, 0xbe0e6c63u }, in2[4] = { 0x3dc577dfu, 0xbd6501c4u, 0x3cb31652u, 0xbb84d7e7u },
      in3[4] = { 0x3f490fdbu, 0x3f490fdbu, 0x3f490fdbu, 0x3f490fdbu }, in4[4] = { 0xbf68c0d5u, 0xbf800000u, 0x7f7fffffu, 0xbf800000u };
    static const int wr[] = { 4, 13, 6, 14, 7, 15, 8, 16, 9, 10, 11, 12 };
    uint32_t cc; memset(hp8_vf, 0, sizeof hp8_vf); memset(hp8_acc, 0, sizeof hp8_acc);
    memcpy(hp8_vf + 4, in1, 16); memcpy(hp8_vf + 8, in2, 16); memcpy(hp8_vf + 12, in3, 16); memcpy(hp8_vf + 16, in4, 16);
    cc = grp_p11cf88();
    out("probe vfpu ovf: group 0x11d02c (arctangent series, x = bf68c0d5 bf800000 7f7fffff bf800000): check %02x (x86: falls back)\n", cc);
    for (r = 0; r < (int)(sizeof wr / sizeof wr[0]); r++) { c = wr[r]; out("probe vfpu ovf:   VF%d = %08x %08x %08x %08x\n", c, hp8_vf[4 * c], hp8_vf[4 * c + 1], hp8_vf[4 * c + 2], hp8_vf[4 * c + 3]); }
    out("probe vfpu ovf:   ACC = %08x %08x %08x %08x\n", hp8_acc[0], hp8_acc[1], hp8_acc[2], hp8_acc[3]); }
  { static const uint32_t in1[4] = { 0x3f800000u, 0x3f800000u, 0x3f800000u, 0 }, in5[4] = { 0x10000004u, 0, 0x01000101u, 0x6c048010u },
      in4[4] = { 0xda0a3747u, 0x5cc7cdb9u, 0x5b8bc68du, 0x5953fcb2u };
    static const int wr[] = { 1, 4, 6, 7, 8 };
    uint32_t cc; memset(hp8_vf, 0, sizeof hp8_vf); memset(hp8_acc, 0, sizeof hp8_acc);
    memcpy(hp8_vf + 4, in1, 16); memcpy(hp8_vf + 20, in5, 16); memcpy(hp8_vf + 16, in4, 16);
    cc = grp_d00t();
    out("probe vfpu ovf: group 0x32fcc0 (stage d00t, VF4 = da0a3747 5cc7cdb9 5b8bc68d 5953fcb2, VF5 = 10000004 0 01000101 6c048010): check %02x (x86: falls back)\n", cc);
    for (r = 0; r < (int)(sizeof wr / sizeof wr[0]); r++) { c = wr[r]; out("probe vfpu ovf:   VF%d = %08x %08x %08x %08x\n", c, hp8_vf[4 * c], hp8_vf[4 * c + 1], hp8_vf[4 * c + 2], hp8_vf[4 * c + 3]); }
    out("probe vfpu ovf:   ACC = %08x %08x %08x %08x\n", hp8_acc[0], hp8_acc[1], hp8_acc[2], hp8_acc[3]); }
}

/* 9. VFPU bit behaviour */
#define HP9_OP(name, body) static __attribute__((noinline)) void name(const uint32_t *a, const uint32_t *b, uint32_t *o) \
{ uint32_t va[4] __attribute__((aligned(16))), vb[4] __attribute__((aligned(16))), vo[4] __attribute__((aligned(16))) = { 0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u }; \
  memcpy(va, a, 16); memcpy(vb, b, 16); \
  __asm__ volatile ("lv.q C000, 0(%1)\n\tlv.q C010, 0(%2)\n\tlv.q C020, 0(%0)\n\t" body "\n\tsv.q C020, 0(%0)" : : "r"(vo), "r"(va), "r"(vb) : "memory"); memcpy(o, vo, 16); }
HP9_OP(hp9_mov, "vmov.q C020, C000")
HP9_OP(hp9_swz, "vpfxs y, z, w, x\n\tvmov.q C020, C000")
HP9_OP(hp9_msk, "vpfxd , m, , m\n\tvmov.q C020, C000")
HP9_OP(hp9_abs, "vpfxs |x|, |y|, |z|, |w|\n\tvmov.q C020, C000")
HP9_OP(hp9_min, "vmin.q C020, C000, C010")
HP9_OP(hp9_max, "vmax.q C020, C000, C010")
HP9_OP(hp9_lvsv, "vmov.q C020, C000\n\tsv.q C000, 0(%0)\n\tlv.q C020, 0(%0)")
static __attribute__((noinline)) uint32_t hp9_cmp(int lt, const uint32_t *a, const uint32_t *b)   /* vcmp.q GT / LT a, b: lane bits (after vsync) */
{
  uint32_t va[4] __attribute__((aligned(16))), vb[4] __attribute__((aligned(16))), cc;
  memcpy(va, a, 16); memcpy(vb, b, 16);
  if (lt) __asm__ volatile ("lv.q C000, 0(%1)\n\tlv.q C010, 0(%2)\n\tvcmp.q LT, C000, C010\n\tvsync\n\tmfvc %0, $131" : "=r"(cc) : "r"(va), "r"(vb) : "memory");
  else __asm__ volatile ("lv.q C000, 0(%1)\n\tlv.q C010, 0(%2)\n\tvcmp.q GT, C000, C010\n\tvsync\n\tmfvc %0, $131" : "=r"(cc) : "r"(va), "r"(vb) : "memory");
  return cc & 0x3f;
}
static void probe_vfpu_bits(OutFn out)
{
  static const uint32_t P[4][4] = { { 0xffffffffu, 0xfffc56e0u, 0x7f800000u, 0x00000001u }, { 0x80000001u, 0x807fffffu, 0x80000000u, 0x7fc00000u },
                                    { 0x00000000u, 0x80000000u, 0x00400000u, 0x80400000u }, { 0x7f800001u, 0xff800000u, 0x3f800000u, 0xbf800000u } };
  static const uint32_t MX[4] = { 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu }, MN[4] = { 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu };
  static const uint32_t Z[4] = { 0, 0x80000000u, 0, 0x80000000u }, Z2[4] = { 0x80000000u, 0, 0x80000000u, 0 };
  static const struct { void (*f)(const uint32_t *, const uint32_t *, uint32_t *); const char *n; const uint32_t *b; } T[] = {
    { hp9_mov, "vmov.q", MX }, { hp9_swz, "vmov.q swizzle yzwx", MX }, { hp9_msk, "vmov.q mask y, w kept (11111111 .. 44444444 before)", MX },
    { hp9_abs, "vmov.q |x|", MX }, { hp9_lvsv, "sv.q + lv.q", MX }, { hp9_min, "vmin.q with +max", MX }, { hp9_max, "vmax.q with -max", MN },
    { hp9_min, "vmin.q with -max", MN }, { hp9_max, "vmax.q with +max", MX } };
  uint32_t o[4]; unsigned t, i;
  for (t = 0; t < sizeof T / sizeof T[0]; t++)
    for (i = 0; i < 4; i++) {
      T[t].f(P[i], T[t].b, o);
      out("probe vfpu bits: %s (%08x %08x %08x %08x) = %08x %08x %08x %08x\n", T[t].n, P[i][0], P[i][1], P[i][2], P[i][3], o[0], o[1], o[2], o[3]);
    }
  for (i = 0; i < 4; i++)
    out("probe vfpu bits: vcmp.q (%08x %08x %08x %08x) vs (0 -0 0 -0): GT %02x LT %02x; vs (-0 0 -0 0): GT %02x LT %02x\n", P[i][0], P[i][1], P[i][2], P[i][3],
        hp9_cmp(0, P[i], Z), hp9_cmp(1, P[i], Z), hp9_cmp(0, P[i], Z2), hp9_cmp(1, P[i], Z2));
}

/* 10. VFPU arithmetic on special inputs (NaN patterns, infinities, denormals, max) against 0, 1, +Inf, +max: whether a result the x86
   reference gives as NaN / infinity (the group check's fallback) is one on the PSP too */
HP9_OP(hp10_mul, "vmul.q C020, C000, C010")
static __attribute__((noinline)) uint32_t fpu_op2(int op, uint32_t a, uint32_t b)   /* 0 add 1 sub 2 mul 3 div 4 sqrt 5 trunc.w.s 6 c.eq 7 c.lt 8 c.le */
{
  float x = u2f(a), y = u2f(b), r = 0.0f; uint32_t c;
  switch (op) {
  case 0: __asm__ volatile ("add.s %0, %1, %2" : "=f"(r) : "f"(x), "f"(y)); break;
  case 1: __asm__ volatile ("sub.s %0, %1, %2" : "=f"(r) : "f"(x), "f"(y)); break;
  case 2: __asm__ volatile ("mul.s %0, %1, %2" : "=f"(r) : "f"(x), "f"(y)); break;
  case 3: __asm__ volatile ("div.s %0, %1, %2" : "=f"(r) : "f"(x), "f"(y)); break;
  case 4: __asm__ volatile ("sqrt.s %0, %1" : "=f"(r) : "f"(x)); break;
  case 5: __asm__ volatile ("trunc.w.s %0, %1" : "=f"(r) : "f"(x)); break;
  case 6: __asm__ volatile (".set push\n\t.set noreorder\n\tc.eq.s %1, %2\n\tnop\n\tbc1t 1f\n\tli %0, 1\n\tli %0, 0\n1:\n\t.set pop" : "=&r"(c) : "f"(x), "f"(y)); return c;
  case 7: __asm__ volatile (".set push\n\t.set noreorder\n\tc.lt.s %1, %2\n\tnop\n\tbc1t 1f\n\tli %0, 1\n\tli %0, 0\n1:\n\t.set pop" : "=&r"(c) : "f"(x), "f"(y)); return c;
  default: __asm__ volatile (".set push\n\t.set noreorder\n\tc.le.s %1, %2\n\tnop\n\tbc1t 1f\n\tli %0, 1\n\tli %0, 0\n1:\n\t.set pop" : "=&r"(c) : "f"(x), "f"(y)); return c;
  }
  return f2u(r);
}
static void probe_fpu_zero(OutFn out)
{
#define D 0x0031685cu
#define ND 0x8031685cu
#define D1 0x00000001u
#define Z 0u
#define NZ 0x80000000u
#define ONE 0x3f800000u
#define M1 0xbf800000u
  static const uint32_t c[][3] = {           /* op, a, b */
    { 0, D, Z }, { 0, Z, D }, { 0, D, NZ }, { 0, NZ, D }, { 0, ND, Z }, { 0, Z, ND }, { 0, ONE, Z }, { 0, Z, NZ }, { 0, NZ, Z }, { 0, NZ, NZ },
    { 0, D1, D1 }, { 0, D, ND }, { 0, 0x00800000u, 0x80000001u }, { 0, 0x00800001u, 0x80800000u },
    { 1, D, Z }, { 1, Z, D }, { 1, D, NZ }, { 1, NZ, D }, { 1, Z, Z }, { 1, NZ, Z }, { 1, Z, NZ }, { 1, NZ, NZ }, { 1, D, D }, { 1, ONE, ONE },
    { 2, D, ONE }, { 2, ONE, D }, { 2, D, M1 }, { 2, D, Z }, { 2, D, 0x40000000u }, { 2, D, 0x4b000000u },
    { 3, D, ONE }, { 3, D, D }, { 3, Z, D }, { 3, ONE, D }, { 3, ND, D },
    { 4, D, Z }, { 4, NZ, Z }, { 5, D, Z }, { 5, ND, Z },
    { 6, D, Z }, { 6, ND, Z }, { 7, Z, D }, { 7, ND, Z }, { 8, D, Z },
  };
  static const char *const nm[] = { "add", "sub", "mul", "div", "sqrt", "trunc", "c.eq", "c.lt", "c.le" };
  unsigned i;
  for (i = 0; i < sizeof c / sizeof *c; i++) {
    uint32_t z, n;
    pspFpuSetFCR31(0x01000001u); z = fpu_op2(c[i][0], c[i][1], c[i][2]);
    pspFpuSetFCR31(0x01000000u); n = fpu_op2(c[i][0], c[i][1], c[i][2]);
    out("probe fpu zero: %s %08x %08x: RZ %08x, RN %08x\n", nm[c[i][0]], (unsigned)c[i][1], (unsigned)c[i][2], (unsigned)z, (unsigned)n);
  }
  pspFpuSetFCR31(0x01000001u);
  {   /* 0x11cf88, lane z at vb 15793: f2 = +0 (x), f0 = 0x0031685c (y), f3 = 0 */
    uint32_t f2 = Z, f0 = D, lt = fpu_op2(7, f2, f0), f1 = fpu_op2(0, f2, f0), d = fpu_op2(1, f2, f0), eq = fpu_op2(6, f1, Z);
    out("probe fpu chain 0x11cf88: |x| < |y| %u, f1 = 0 + d %08x, f0 = 0 - d %08x, f1 == 0 %u, f0 / f1 %08x\n", (unsigned)lt, (unsigned)f1, (unsigned)d, (unsigned)eq,
        (unsigned)fpu_op2(3, d, f1));
  }
#undef D
#undef ND
#undef D1
#undef Z
#undef NZ
#undef ONE
#undef M1
}

HP9_OP(hp10_add, "vadd.q C020, C000, C010")
HP9_OP(hp10_sub, "vsub.q C020, C000, C010")
static void probe_vfpu_arith(OutFn out)
{
  static const uint32_t P[4][4] = { { 0xffffffffu, 0xfffc56e0u, 0x7f800000u, 0x00000001u }, { 0x80000001u, 0x807fffffu, 0x7f7fffffu, 0x7fc00000u },
                                    { 0xff800000u, 0x7f800001u, 0x00400000u, 0xff7fffffu }, { 0x7f000000u, 0xfffffffeu, 0x3f800000u, 0x7fffffffu } };
  static const uint32_t B[4][4] = { { 0, 0, 0, 0 }, { 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u },
                                    { 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u }, { 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu } };
  static const char *bn[4] = { "0", "1", "+inf", "+max" };
  static const struct { void (*f)(const uint32_t *, const uint32_t *, uint32_t *); const char *n; } T[] = { { hp10_mul, "vmul" }, { hp10_add, "vadd" }, { hp10_sub, "vsub" } };
  uint32_t o[4]; unsigned t, i, j;
  for (t = 0; t < sizeof T / sizeof T[0]; t++)
    for (j = 0; j < 4; j++)
      for (i = 0; i < 4; i++) {
        T[t].f(P[i], B[j], o);
        out("probe vfpu arith: %s.q (%08x %08x %08x %08x), %s = %08x %08x %08x %08x\n", T[t].n, P[i][0], P[i][1], P[i][2], P[i][3], bn[j], o[0], o[1], o[2], o[3]);
      }
}

/* 12. the data cache on stores, and the cache hints (v18) */
#define P12_L 4096u                                           /* lines per run: 256 KB, 16 times the D-cache */
static __attribute__((noinline)) uint32_t p12_ld(const uint32_t *p, uint32_t n) { uint32_t s = 0; while (n--) { s += *(const volatile uint32_t *)p; p += 16; } return s; }
static __attribute__((noinline)) void p12_st1(uint32_t *p, uint32_t n) { while (n--) { *(volatile uint32_t *)p = n; p += 16; } }
static __attribute__((noinline)) void p12_st16(uint32_t *p, uint32_t n)
{ while (n--) { volatile uint32_t *q = p; q[0] = n; q[1] = n; q[2] = n; q[3] = n; q[4] = n; q[5] = n; q[6] = n; q[7] = n;
    q[8] = n; q[9] = n; q[10] = n; q[11] = n; q[12] = n; q[13] = n; q[14] = n; q[15] = n; p += 16; } }
static __attribute__((noinline)) void p12_cde16(uint32_t *p, uint32_t n)
{ while (n--) { volatile uint32_t *q = p; __asm__ volatile ("cache 0x18, 0(%0)" :: "r"(p) : "memory");
    q[0] = n; q[1] = n; q[2] = n; q[3] = n; q[4] = n; q[5] = n; q[6] = n; q[7] = n;
    q[8] = n; q[9] = n; q[10] = n; q[11] = n; q[12] = n; q[13] = n; q[14] = n; q[15] = n; p += 16; } }
static __attribute__((noinline)) void p12_svq(uint32_t *p, uint32_t n, int wb)
{ __asm__ volatile ("vzero.q C000" ::: "memory");
  if (wb) while (n--) { __asm__ volatile ("sv.q C000, 0(%0), wb\n\tsv.q C000, 16(%0), wb\n\tsv.q C000, 32(%0), wb\n\tsv.q C000, 48(%0), wb" :: "r"(p) : "memory"); p += 16; }
  else while (n--) { __asm__ volatile ("sv.q C000, 0(%0)\n\tsv.q C000, 16(%0)\n\tsv.q C000, 32(%0)\n\tsv.q C000, 48(%0)" :: "r"(p) : "memory"); p += 16; } }
static __attribute__((noinline)) void p12_fill(const uint32_t *p, uint32_t n) { while (n--) { __asm__ volatile ("cache 0x1e, 0(%0)" :: "r"(p) : "memory"); p += 16; } }
static __attribute__((noinline)) uint32_t p12_ldpf(const uint32_t *p, uint32_t n)   /* the fill runs to at most 4 lines past the end */
{ uint32_t s = 0; while (n--) { __asm__ volatile ("cache 0x1e, 256(%0)" :: "r"(p) : "memory"); s += *(const volatile uint32_t *)p; p += 16; } return s; }
static double p12_ns(uint64_t t0, uint64_t t1, uint32_t n) { return (double)(t1 - t0) * 1000.0 / n; }
static void probe_dcache(OutFn out)
{
  void rt_qcopy_nov(uint8_t *d, const uint8_t *s, uint32_t n); extern uint32_t g_cde_ok, g_cde_pass;   /* main_psp.c cde_selftest */
  const uint32_t B = P12_L * 64u;
  uint8_t *raw = malloc(2 * B + 4 * 64 + 64), *x, *y; uint32_t sink = 0, keep = g_cde_ok; uint64_t t0, t1; double v[16];
  if (!raw) { out("probe dcache: no memory\n"); return; }
  x = (uint8_t *)(((uintptr_t)raw + 63) & ~(uintptr_t)63); y = x + B;   /* y has 4 spare lines past its end for p12_ldpf */
  memset(x, 1, 2 * B + 4 * 64);
  sceKernelDcacheWritebackInvalidateAll(); t0 = hp_now(); sink += p12_ld((uint32_t *)x, P12_L); t1 = hp_now(); v[0] = p12_ns(t0, t1, P12_L);
  sceKernelDcacheWritebackInvalidateAll(); t0 = hp_now(); p12_st1((uint32_t *)x, P12_L); t1 = hp_now(); v[1] = p12_ns(t0, t1, P12_L);
  sceKernelDcacheWritebackInvalidateAll(); t0 = hp_now(); p12_st16((uint32_t *)x, P12_L); t1 = hp_now(); v[2] = p12_ns(t0, t1, P12_L);
  v[3] = v[14] = v[15] = -1.0;                                  /* (-1: not timed, the self-test failed) */
  if (g_cde_pass) { sceKernelDcacheWritebackInvalidateAll(); t0 = hp_now(); p12_cde16((uint32_t *)x, P12_L); t1 = hp_now(); v[3] = p12_ns(t0, t1, P12_L); }
  sceKernelDcacheWritebackInvalidateAll(); t0 = hp_now(); p12_svq((uint32_t *)x, P12_L, 0); t1 = hp_now(); v[4] = p12_ns(t0, t1, P12_L);
  sceKernelDcacheWritebackInvalidateAll(); t0 = hp_now(); p12_svq((uint32_t *)x, P12_L, 1); t1 = hp_now(); v[5] = p12_ns(t0, t1, P12_L);
  out("probe dcache (ns per 64-byte line not in the cache): 1 load %.1f, 1 store %.1f, 16 stores %.1f, cache 0x18 + 16 stores %.1f, 4 sv.q %.1f, 4 sv.q wb %.1f\n",
      v[0], v[1], v[2], v[3], v[4], v[5]);
  sceKernelDcacheWritebackInvalidateAll(); t0 = hp_now(); p12_fill((uint32_t *)y, 256); t1 = hp_now(); v[6] = p12_ns(t0, t1, 256);   /* 256 lines: all stay */
  t0 = hp_now(); sink += p12_ld((uint32_t *)y, 256); t1 = hp_now(); v[7] = p12_ns(t0, t1, 256);
  sceKernelDcacheWritebackInvalidateAll(); t0 = hp_now(); sink += p12_ldpf((uint32_t *)y, P12_L); t1 = hp_now(); v[8] = p12_ns(t0, t1, P12_L);
  sceKernelDcacheWritebackInvalidateAll(); p12_st16((uint32_t *)y, 256);        /* the cache full of dirty lines, then loads that replace them */
  t0 = hp_now(); sink += p12_ld((uint32_t *)x, 256); t1 = hp_now(); v[9] = p12_ns(t0, t1, 256);
  sceKernelDcacheWritebackInvalidateAll(); sink += p12_ld((uint32_t *)y, 256);  /* the same with clean lines replaced */
  t0 = hp_now(); sink += p12_ld((uint32_t *)x, 256); t1 = hp_now(); v[10] = p12_ns(t0, t1, 256);
  sceKernelDcacheWritebackInvalidateAll(); t0 = hp_now(); p12_st16((uint32_t *)((uintptr_t)x | 0x40000000u), P12_L); t1 = hp_now(); v[11] = p12_ns(t0, t1, P12_L);
  out("probe dcache: cache 0x1e alone %.1f, loads after it %.1f, loads with a fill 4 lines ahead %.1f; loads replacing dirty lines %.1f, clean %.1f; 16 uncached stores %.1f\n",
      v[6], v[7], v[8], v[9], v[10], v[11]);
  sceKernelDcacheWritebackInvalidateAll(); t0 = hp_now(); memcpy(y, x, 65536); t1 = hp_now(); v[12] = p12_ns(t0, t1, 1024);
  sceKernelDcacheWritebackInvalidateAll(); g_cde_ok = 0; t0 = hp_now(); rt_qcopy_nov(y, x, 65536); t1 = hp_now(); v[13] = p12_ns(t0, t1, 1024);
  if (g_cde_pass) {
    sceKernelDcacheWritebackInvalidateAll(); g_cde_ok = 1; t0 = hp_now(); rt_qcopy_nov(y, x, 65536); t1 = hp_now(); v[14] = p12_ns(t0, t1, 1024);
    sceKernelDcacheWritebackInvalidateAll(); g_cde_ok = 1; t0 = hp_now(); rt_qcopy_nov(y + 32, x, 65536 - 64); t1 = hp_now(); v[15] = p12_ns(t0, t1, 1023); }
  g_cde_ok = keep;
  out("probe dcache: 64 KB copy, lines not in the cache, per line: memcpy %.1f, lv.q / sv.q %.1f, with claimed lines %.1f (destination 32 bytes off its line %.1f)\n",
      v[12], v[13], v[14], v[15]);
  sceKernelDcacheWritebackInvalidateAll();
  if (sink == 0x12345678u) out(" \n");
  free(raw);
}

void hw_probe(OutFn out)
{
  probe_round(out);
  probe_denormal(out);
  probe_fpu_zero(out);
  probe_memory(out);
  probe_dcache(out);
  probe_icache(out);
  pspFpuSetFCR31(0x01000001u);
  probe_vfpu_special(out);
  probe_vfpu_cc(out);
  probe_vfpu_ovf(out);
  probe_vfpu_bits(out);
  probe_vfpu_arith(out);
  probe_vfpu_cost(out);
}
