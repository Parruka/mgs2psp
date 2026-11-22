/* Guest-machine shim for statically recompiled original functions (model/recomp.py).  Registers + a small private memory arena that holds the
   original data structures (room records etc.); generated code reads/writes it at the original struct offsets. */
#ifndef GUEST_H
#define GUEST_H
#include <stdint.h>
#include <string.h>
#include <math.h>
#if defined(__psp__) && !defined(ME_IMAGE)
/* the FPU instructions, whatever the optimisation level (at -Os gcc calls the library sqrtf instead of inlining sqrt.s; same results) */
static inline __attribute__((always_inline)) float tt_sqrtf(float x) { float r; __asm__("sqrt.s %0, %1" : "=f"(r) : "f"(x)); return r; }
static inline __attribute__((always_inline)) float tt_fabsf(float x) { float r; __asm__("abs.s %0, %1" : "=f"(r) : "f"(x)); return r; }
#define sqrtf tt_sqrtf
#define fabsf tt_fabsf
#endif
#ifndef GUEST_SIZE
#define GUEST_SIZE (1 << 25)                     /* the PS2 main memory: 32 MB (heap up to _sf_heap_end 0x2000000) */
#endif
/* MGS2 x86 probe built as two translation units (boot/gen.c: the generated code; boot.c: the runtime): GSV makes the machine state
   shared, defined by the runtime unit (GUEST_SPLIT_RT) and extern in the generated one (GUEST_SPLIT_GEN); one unit: static */
#if defined(GUEST_SPLIT_GEN)
#define GSV extern
#elif defined(GUEST_SPLIT_RT)
#define GSV
#else
#define GSV static
#endif
#ifndef ME_IMAGE
GSV uint8_t g_mem[GUEST_SIZE] __attribute__((aligned(64)));   /* 64: the Media Engine protocol blocks in it are cache-line aligned */
#endif
#if defined(__psp__) && defined(GUEST_PINREG) && !defined(ME_IMAGE)
/* PSP code generation (exact, no semantic change): the guest memory base lives in $s7 for the whole program (every file built with
   -ffixed-23, main() sets it before any guest access; s7 is callee-saved, so library code preserves it), so a guest access is
   addu + lw instead of lui + addiu + addu + lw with a relocated constant per field offset; the guest register file and the VU0 state are
   small data, one $gp-relative instruction per access.  Code that may run with another s7 (interrupt / other threads) must not use MM. */
register uint8_t *g_memb asm("$23");
#define GUEST_PIN_INIT() (g_memb = g_mem)
#define GREG_SD __attribute__((section(".sdata")))
#else
#define GUEST_PIN_INIT() ((void)0)
#define GREG_SD
#endif
#if defined(__psp__) && defined(GUEST_SPRSTATE) && !defined(ME_IMAGE)
/* v18 (PSP): the hot machine state (registers, VU0 state, poll counters) lives in the PSP scratchpad: psp/spr_state.ld gives the
   addresses, main_psp.c zeroes it before the game starts; same code, only the addresses differ (no D-cache line spent on it) */
#define GSVS extern
#define SPRS extern
#else
#define GSVS GSV
#define SPRS
#endif
GSVS int64_t R[32] GREG_SD, RH[32] GREG_SD;   /* RH: upper 64 bits of the 128-bit EE GPRs (lq / sq / MMI / qmtc2) */ GSVS int64_t HI GREG_SD, LO GREG_SD, HI1 GREG_SD, LO1 GREG_SD; GSVS float F[32] GREG_SD, ACC GREG_SD; GSVS int CC GREG_SD;
#if defined(GUEST_MM_EXTERN)             /* dev: the including program defines MM itself (tracers) */
#elif defined(GUEST_TRACE)                /* dev: records the executable-image words (rodata etc.) the recompiled code touches, to see which pieces model/pipeline.py wcslice must carry */
static uint8_t g_touch[0x300000 / 4]; static uint8_t g_touchw[0x700000 / 4];   /* g_touchw: world blob words (WC_BASE 0x600000) */
static inline uint32_t guest_t(uint32_t a) { if (a >= 0x100000u && a < 0x381700u) g_touch[(a - 0x100000u) >> 2] = 1; else if (a >= 0x600000u && a < 0xd00000u) g_touchw[(a - 0x600000u) >> 2] = 1; return a; }
#define MM(a) (g_mem + (guest_t((uint32_t)(a)) & (GUEST_SIZE - 1)))
#elif defined(GUEST_RANGE)                 /* dev: counts the accesses outside the guest memory (they wrap under the mask) */
static unsigned long g_oor; static uint32_t g_oor_last;
static unsigned long g_oor_hi[16], g_acc_n, g_acc_lines; static uint32_t g_cur_fn, g_last_line; static inline uint32_t guest_r(uint32_t a) { g_acc_n++; if ((a >> 6) != g_last_line) { g_acc_lines++; g_last_line = a >> 6; } if (a >= GUEST_SIZE) { g_oor++; g_oor_last = a; g_oor_hi[a >> 28]++; if ((a >> 28) != 7 && g_oor_hi[a >> 28] < 2) { printf("oor %08x fn %x\n", a, g_cur_fn); } } return a; }
#define MM(a) (g_mem + (guest_r((uint32_t)(a)) & (GUEST_SIZE - 1)))
#elif defined(GUEST_NOMASK) && defined(__psp__) && defined(GUEST_PINREG) && !defined(ME_IMAGE)
#define MM(a) (g_memb + (uint32_t)(a))
#elif defined(GUEST_NOMASK)                /* PSP: every guest address the game forms is in range (checked with GUEST_RANGE: none outside) */
#define MM(a) (g_mem + (uint32_t)(a))
#else
#define MM(a) (g_mem + ((uint32_t)(a) & (GUEST_SIZE - 1)))
#endif
/* model/regcache.py: 64-bit pass-through in a function with 32-bit register locals (R[] keeps the exact value, the local its truncation) */
#define RCLD(l, n, e) ((void)(R[n] = (e)), (void)((l) = (int32_t)R[n]))
#define RCMV(l, n, src) ((void)(R[n] = R[src]), (void)((l) = (int32_t)R[n]))
#ifdef GEN_NCHK   /* dev check (regcache.py RC_NCHK=1): a 32-bit register local loaded from an R[] that holds another value than a sign-extended
                     32-bit one (the NARROW assumption; fa: the function, 0 / 1 for RCLD / RCMV, where R[] stays exact; fn: the C function's
                     name); boot.c reports them */
void rt_nchk_bad(int n, uint32_t fa, int64_t v, const char *fn);
#define NCHK(n, fa) ({ int64_t v_ = R[n]; if (__builtin_expect(v_ != (int64_t)(int32_t)v_, 0)) rt_nchk_bad(n, fa, v_, __func__); (int32_t)v_; })
#undef RCLD
#undef RCMV
#define RCLD(l, n, e) ((void)(R[n] = (e)), (void)((l) = NCHK(n, 0)))
#define RCMV(l, n, src) ((void)(R[n] = R[src]), (void)((l) = NCHK(n, 1)))
#else
#define NCHK(n, fa) ((int32_t)R[n])   /* headers made with RC_NCHK=1, built without the check */
#endif
#define RD8(a) (*(uint8_t *)MM(a))
#define RD16(a) (*(uint16_t *)MM(a))
#define RD32(a) (*(uint32_t *)MM(a))
#define RD64(a) (*(uint64_t *)MM(a))
#ifndef GUEST_WR_HOOK
#define GUEST_WR_HOOK(a, n) (a)          /* dev: a write-address hook (tracers) */
#else
#define GUEST_WR_HOOK_ON
#endif
#ifndef GUEST_STORE                      /* a guest store of a T (the including program may route stores itself: gen_pre.h GEN_MM_FAST) */
#define GUEST_STORE(T, a, v) (*(T *)MM(GUEST_WR_HOOK(a, sizeof(T))) = (T)(v))
#endif
#define WR8(a, v) GUEST_STORE(uint8_t, a, v)
#define WR16(a, v) GUEST_STORE(uint16_t, a, v)
#define WR32(a, v) GUEST_STORE(uint32_t, a, v)
#define WR64(a, v) GUEST_STORE(uint64_t, a, v)
/* model/qsr.py: the guest stack frame saves / restores of a function as one call (the same stores and loads, in order, from one hot copy;
   the I-cache is the PSP's bottleneck): table = count, then register | sq << 5 | offset / 8 << 6 */
static __attribute__((noinline)) void qsr_save(uint32_t sp, const uint16_t *t)
{ unsigned n = *t++; do { unsigned e = *t++, r = e & 31; uint32_t a = sp + ((e >> 6) << 3); WR64(a, R[r]); if (e & 32) WR64(a + 8, RH[r]); } while (--n); }
static __attribute__((noinline)) void qsr_rest(uint32_t sp, const uint16_t *t)
{ unsigned n = *t++; do { unsigned e = *t++, r = e & 31; uint32_t a = sp + ((e >> 6) << 3); R[r] = (int64_t)RD64(a); if (e & 32) RH[r] = (int64_t)RD64(a + 8); } while (--n); }
/* float accesses are 4-byte aligned (lwc1 / swc1 fault otherwise on the EE too): a plain aligned access (lwc1 / swc1 on the PSP) instead of
   a byte-wise memcpy (lwl / lwr + a stack round trip: a third of the code of vector-heavy functions) */
#ifdef RDF_DEN   /* dev: counts denormal floats loaded (FPU operands the PS2 would treat as zero) */
static unsigned long g_rdf_den, g_rdf_n; static uint32_t g_rdf_den_last, g_rdf_den_addr;
static inline float RDF(uint32_t a) { uint32_t u = *(const uint32_t *)MM(a); g_rdf_n++; if (!(u & 0x7f800000u) && (u & 0x7fffffu)) { g_rdf_den++; g_rdf_den_last = u; g_rdf_den_addr = a; } return *(const float *)MM(a); }
static inline void WRF(uint32_t a, float f) { *(float *)MM(a) = f; }
#elif defined(RDF_CHECK)
static unsigned long g_rdf_bad;
static inline float RDF(uint32_t a) { if (a & 3) g_rdf_bad++; return *(const float *)MM(a); }
static inline void WRF(uint32_t a, float f) { if (a & 3) g_rdf_bad++; *(float *)MM(a) = f; }
#else
static inline float RDF(uint32_t a) { return *(const float *)MM(a); }
static inline void WRF(uint32_t a, float f) { GUEST_STORE(float, a, f); }
#endif
static inline uint32_t F2I(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static inline float I2F(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static inline int32_t F2INT(float f) { if (f >= 2147483648.0f) return 0x7fffffff; if (f <= -2147483648.0f) return (int32_t)0x80000000; return (int32_t)f; }
static inline float CLP(float r) { if (r > 3.4028234663852886e38f) return 3.4028234663852886e38f; if (r < -3.4028234663852886e38f) return -3.4028234663852886e38f; return r; }
static inline float FDIV(float a, float b)   /* R5900 FPU: x/0 = +-Fmax (sign = xor), overflow clamps */
{ if (b == 0.0f) return ((F2I(a) ^ F2I(b)) >> 31) ? -3.4028234663852886e38f : 3.4028234663852886e38f; return CLP(a / b); }
/* FPU add / subtract of the generated code (tools/fpu_zero.py).  The PSP FPU with FCR31 FS = 1 returns the other operand unchanged when
   exactly one operand is zero (0 - x: -x), a denormal included, where every other denormal result is flushed (TT lessons A9; PPSSPP's
   interpreter has the same rule).  The PSP: the plain operators.  x86, the reference: the rule, then the operators (MXCSR FTZ flushes the
   rest as FS does).  Without it a denormal read from a garbage word (0x0031685c, vb 15793) made x86 divide by zero where the PSP did not. */
#ifdef __psp__
#define FADD(a, b) ((a) + (b))
#define FSUB(a, b) ((a) - (b))
#else
#ifdef FPU_ZERO_TRACE   /* dev: print the sites where the rule gives other bits than the plain operator (vb of the last hash, operands, host caller) */
#include <stdio.h>
extern long g_hash_vb;
static __attribute__((noinline)) void fz_note(uint32_t a, uint32_t b, int sub)
{ static int n; volatile float x = I2F(a), y = I2F(b); float p = sub ? x - y : x + y, r = (b << 1) ? (sub ? I2F(b ^ 0x80000000u) : I2F(b)) : I2F(a);
  if (F2I(p) != F2I(r) && n++ < 400) fprintf(stderr, "fz: vb %ld %s %08x %08x: rule %08x, plain %08x at %p\n", g_hash_vb, sub ? "sub" : "add", a, b,
                                               F2I(r), F2I(p), __builtin_return_address(0)); }
#define FZ_NOTE(a, b, s) fz_note(F2I(a), F2I(b), s)
#else
#define FZ_NOTE(a, b, s) ((void)0)
#endif
/* The asm makes the kept operand opaque: otherwise gcc folds what follows at compile time when that operand is a constant, and its folding
   rounds to nearest (0 + 280.0, then / 90.0 gave 0x40471c72 where the PSP's round toward zero gives 0x40471c71: d00t P297800, vb 4250). */
static inline float FADD(float a, float b)
{ uint32_t ua = F2I(a) << 1, ub = F2I(b) << 1; if (!ua != !ub) { float r = ub ? b : a; FZ_NOTE(a, b, 0); __asm__("" : "+x"(r)); return r; } return a + b; }
static inline float FSUB(float a, float b)
{ uint32_t ua = F2I(a) << 1, ub = F2I(b) << 1; if (!ua != !ub) { float r = ub ? I2F(F2I(b) ^ 0x80000000u) : a; FZ_NOTE(a, b, 1); __asm__("" : "+x"(r)); return r; }
  return a - b; }
#endif
/* EE hardware registers reached by the recompiled code (`lui $1, 0x1000/0x1001` + lw/sw, model/recomp.py): timer 0 count (0x10000000, the
   host sets g_hw_timer = elapsed hblank ticks before ClClock::tick) and the scratchpad DMA channels D8 fromSPR (0x1000d000) / D9 toSPR (0x1000d400):
   a start (CHCR bit 8) moves QWC quadwords at once between the scratchpad (guest 0..0x3fff: the scratchpad base the game code builds,
   lui 0x7000 at 0x19e21c, is rewritten to 0 by model/regcache.py; the masked accessor mapped 0x7000xxxx there too) and MADR. */
#ifndef GUEST_HW_EXTERN   /* MGS2: the including program may model the EE registers itself (HWR32 / HWW32) */
static uint32_t g_hw[0x20000 / 4]; static uint32_t g_hw_timer;
static uint32_t HWR32(uint32_t a)
{
  if (a == 0x10000000u) return g_hw_timer;
  return g_hw[(a - 0x10000000u) / 4 & 0x7fff];
}
static void HWW32(uint32_t a, uint32_t v)
{
  if (a == 0x10000000u) { g_hw_timer = v; return; }
  g_hw[(a - 0x10000000u) / 4 & 0x7fff] = v;
  if ((a == 0x1000d000u || a == 0x1000d400u) && (v & 0x100)) {
    uint32_t b = (a - 0x10000000u) / 4, madr = g_hw[b + 4], qwc = g_hw[b + 8] & 0xffff, sadr = g_hw[b + 32] & 0x3ff0;
#ifndef SPR_BASE
#define SPR_BASE 0u   /* the scratchpad in guest memory (model/regcache.py maps 0x70000000 there; the ME image uses its own) */
#endif
    uint32_t n = qwc * 16, s0 = SPR_BASE + (sadr & 0x3fff);
    /* the common case as one block copy (the same words as the loop below when the scratchpad range does not wrap and the two ranges do
       not overlap; GUEST_WR_HOOK builds keep the word loop) */
#ifndef GUEST_WR_HOOK_ON
    if ((sadr & 0x3fff) + n <= 0x4000u && (madr + n <= s0 || s0 + n <= madr) && madr + n <= GUEST_SIZE) {
      if (a == 0x1000d000u) memcpy(MM(madr), MM(s0), n); else memcpy(MM(s0), MM(madr), n);
    } else
#endif
    for (uint32_t i = 0; i < n; i += 4) { uint32_t s = SPR_BASE + ((sadr + i) & 0x3fff);
      if (a == 0x1000d000u) WR32(madr + i, RD32(s)); else WR32(s, RD32(madr + i)); }
    g_hw[b] = v & ~0x100u; g_hw[b + 8] = 0;
  }
}
#endif
/* model/regcache.py dead save removal: a function keeps the entry value of a callee-saved register in C locals and puts it back before
   returning, instead of the original's save / restore through its stack frame */
#define DSR_SAVE(k) int64_t dsr_sv##k = R[k], dsr_svh##k = RH[k]
#define DSR_RESTORE(k) (R[k] = dsr_sv##k, RH[k] = dsr_svh##k)
#ifndef GEN_IDIOM_QCOPY
#define GEN_IDIOM_QCOPY() ((void)0)    /* recomp/idioms.py: only the GEN_MM_FAST + GUEST_SPR0 memory path has the native forms (gen_pre.h) */
#endif
#ifndef K_POLL_CYCLES
#define K_POLL_CYCLES 64          /* kernel.h: the virtual time of one loop iteration (the generated units add it inline: gen_pre.h IRQ_POLLF) */
#endif
#ifndef IRQ_POLL
#define IRQ_POLL() ((void)0)     /* MGS2 recomp/survey.py: at every loop back-edge; the host may deliver interrupts there */
#endif
#ifndef BBT
#define BBT(a)                       /* per-instruction hook (model/recomp.py RECOMP_BBT=func,..: debug) */
#endif
#ifndef FN_ENTER
#define FN_ENTER(a)                  /* entry hook of every recompiled function (debug traces define it) */
#endif
#ifdef MGS2_GEN_DUAL
#include MGS2_GEN_DUAL   /* TT: g_yo + the original versions of host-defined functions; MGS2: none yet */
#endif
#endif
#include "vu0.h"
