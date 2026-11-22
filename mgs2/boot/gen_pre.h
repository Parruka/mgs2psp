/* Prelude of the generated units of the split x86 probe build (gen.c: the ELF translation and the call dispatch; gen_stage.c: one stage
   image each).  The machine state (registers, guest memory) is defined by the runtime unit (boot.c, guest.h GSV); memory accesses take
   the plain RAM path inline and call the runtime for the register space, the scratchpad, or while a hardware register write is pending
   (ee_io.h mm_map).  The generated functions are made global (`static` defined away around them) so the units call each other. */
#define GUEST_SPLIT_GEN
#define GUEST_MM_EXTERN
#define GUEST_HW_EXTERN
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GUEST_SIZE (1 << 25)
extern uint32_t g_io_arm, g_io_pend;
extern uint8_t g_mem[GUEST_SIZE];
uint8_t *rt_mm(uint32_t a); uint32_t rt_wr_note(uint32_t a); void rt_poll(void);
uint32_t rt_hwr32(uint32_t a); void rt_hww32(uint32_t a, uint32_t v);
void rt_syscall(uint32_t pc); void rt_break(uint32_t pc);
uint32_t rt_cop0r(int r, int s); void rt_cop0w(int r, int s, uint32_t v); void rt_cop0op(int f, uint32_t pc);
#ifdef GEN_MM_FAST
/* the PSP access path, checked on x86 with the same hashes: an address in the 32 MB of RAM (kuseg) is used as it is; every other address
   (kseg, uncached views, scratchpad, the register space) goes to the runtime.  A store outside RAM goes to rt_store, which applies a
   register write's effect at once (ee_io.h io_flush), so no write is ever left pending for the next access to complete. */
#if defined(__psp__) && defined(GEN_PIN23)
/* lessons A3: the guest memory base in $s7 for the whole program (every unit built with -ffixed-23; main sets it before any guest code) */
register uint8_t *g_memb asm("$23");
#define GEN_BASE g_memb
#else
#define GEN_BASE g_mem
#endif
#ifdef GUEST_SPR0
/* with the scratchpad at RAM 0 (ee_io.h GUEST_SPR0) one mask covers RAM, its kseg0 / kseg1 / uncached views and the scratchpad; only the
   register space 0x1xxxxxxx goes to the runtime.  GEN_MM_CHECK (x86 dev): reports an address the mask would get wrong (kseg1 registers
   0xbxxxxxxx, unmapped segments, the scratchpad past 16 KB) */
#ifdef GEN_DG_TRACE            /* dev (boot.c MGS2_DG_TRACE): every RAM read and write goes to the runtime, to see which words the draw
                                  pipeline writes that the rest of the game then reads (the GE renderer's cut) */
void rt_dgt_rd(uint32_t a); void rt_dgt_wr(uint32_t a, int n); uint32_t rt_dgt_wrh(uint32_t a, int n);
#define GEN_ODD(a) rt_dgt_rd(a)
#define GEN_ODD_W(a, n) rt_dgt_wr(a, n)
#define GUEST_WR_HOOK(a, n) rt_dgt_wrh((uint32_t)(a), n)   /* a store through MM (VSQC2): the next MM is the write, not a read */
#elif defined(GEN_MM_CHECK)
void rt_mm_odd(uint32_t a);
#define GEN_ODD(a) ((((0x58du >> ((a) >> 28)) & 1) && !((a) >> 28 == 7 && ((a) & 0x0fffc000u))) ? (void)0 : rt_mm_odd(a))
#else
#define GEN_ODD(a) ((void)0)
#endif
#ifndef GEN_ODD_W
#define GEN_ODD_W(a, n) GEN_ODD(a)
#endif
#define GEN_FAST(a) (__builtin_expect((a) >> 28 != 1, 1))
#define GEN_RAM(a) (GEN_BASE + ((a) & (GUEST_SIZE - 1)))
#else
#define GEN_ODD(a) ((void)0)
#define GEN_FAST(a) (__builtin_expect((a) < GUEST_SIZE, 1))
#define GEN_RAM(a) (GEN_BASE + (a))
#endif
static inline uint8_t *gen_mm(uint32_t a) { if (GEN_FAST(a)) { GEN_ODD(a); return GEN_RAM(a); } return rt_mm(a); }
void rt_store(uint32_t a, const void *p, int n);
void rt_st8(uint32_t a, uint32_t v); void rt_st16(uint32_t a, uint32_t v); void rt_st32(uint32_t a, uint32_t v); void rt_st64(uint32_t a, uint64_t v);
static inline void rt_stf(uint32_t a, float v) { uint32_t u; memcpy(&u, &v, 4); rt_st32(a, u); }
/* the slow path takes the value in registers (no stack copy at every store site) */
#define GEN_ST_SLOW(T, a, v) _Generic((T)0, uint8_t: rt_st8, uint16_t: rt_st16, uint32_t: rt_st32, uint64_t: rt_st64, float: rt_stf)(a, v)
#define GUEST_STORE(T, a, v) ({ uint32_t a_ = (uint32_t)(a); T v_ = (T)(v); \
  if (GEN_FAST(a_)) { GEN_ODD_W(a_, sizeof(T)); *(T *)GEN_RAM(a_) = v_; } else GEN_ST_SLOW(T, a_, v_); (void)0; })
#define GUEST_STORE16(a, p) ({ uint32_t a_ = (uint32_t)(a); \
  if (GEN_FAST(a_)) { GEN_ODD_W(a_, 16); memcpy(__builtin_assume_aligned(GEN_RAM(a_), 16), (p), 16); } else rt_store(a_, (p), 16); (void)0; })
#define MM(a) gen_mm((uint32_t)(a))
#else
static inline uint8_t *gen_mm(uint32_t a)
{
  uint32_t top = a >> 28, p;
  if ((g_io_pend | g_io_arm) || top == 7) return rt_mm(a);
  if (top == 2 || top == 3) return g_mem + (a & 0x0fffffffu & (GUEST_SIZE - 1));
  p = a & 0x1fffffffu;
  if (p >= 0x10000000u && p < 0x14000000u) return rt_mm(a);
  return g_mem + (p & (GUEST_SIZE - 1));
}
static inline uint32_t gen_wr_note(uint32_t a)
{
  uint32_t top = a >> 28, p = a & 0x1fffffffu;
  if (top != 7 && top != 2 && top != 3 && p >= 0x10000000u && p < 0x14000000u) return rt_wr_note(a);
  return a;
}
#define MM(a) gen_mm((uint32_t)(a))
#define GUEST_WR_HOOK(a, n) gen_wr_note((uint32_t)(a))
#endif
/* recomp/lean_plan.md step 3: the host address of a guest base as one addu GCC cannot reassociate.  Written plainly, GCC folds
   (GEN_BASE + x) + k into GEN_BASE + (x + k): an addiu and an addu at every access (and x + k kept live) instead of k as the offset of
   the host load / store.  A constant address keeps the plain form, which folds into the access. */
#if defined(GEN_MM_FAST) && defined(__psp__) && defined(GEN_PIN23)
static inline __attribute__((always_inline)) uint8_t *gen_hb(uint32_t a)
{ uint8_t *h; if (__builtin_constant_p(a)) return GEN_BASE + (a & (GUEST_SIZE - 1));
  __asm__("addu %0, $23, %1" : "=r"(h) : "r"(a & (GUEST_SIZE - 1))); return h; }
#elif defined(GEN_MM_FAST)
static inline uint8_t *gen_hb(uint32_t a) { return GEN_BASE + (a & (GUEST_SIZE - 1)); }
#endif
/* tools/sp_fast.py: a guest stack access ($sp + k): $sp never points into the register space and a frame never crosses the ends of the
   32 MB, so the host address is the host base of $sp (computed once while $sp is unchanged) + k.  GEN_SP_CHECK (x86 dev) reports an
   access that breaks the invariant; the dev tracers and the address check build take the general path.  GEN_HB_SP (step 3): the host
   base through gen_hb */
#if defined(GEN_MM_FAST) && defined(GUEST_SPR0) && !defined(GEN_DG_TRACE) && !defined(GEN_MM_CHECK)
#ifdef GEN_SP_CHECK
void rt_sp_bad(uint32_t sp, int k);
#define MMS(sp, k) ({ uint32_t s_ = (uint32_t)(sp); int32_t o_ = (int32_t)(s_ & (GUEST_SIZE - 1)) + (k); \
  if (s_ >> 28 == 1 || o_ < 0 || o_ >= GUEST_SIZE) rt_sp_bad(s_, k); GEN_BASE + (s_ & (GUEST_SIZE - 1)) + (k); })
#elif defined(GEN_HB_SP)
#define MMS(sp, k) (gen_hb((uint32_t)(sp)) + (k))
#else
#define MMS(sp, k) (GEN_BASE + ((uint32_t)(sp) & (GUEST_SIZE - 1)) + (k))
#endif
#else
#define MMS(sp, k) MM((uint32_t)(sp) + (k))
#endif
#define RD8S(sp, k) (*(uint8_t *)MMS(sp, k))
#define RD16S(sp, k) (*(uint16_t *)MMS(sp, k))
#define RD32S(sp, k) (*(uint32_t *)MMS(sp, k))
#define RD64S(sp, k) (*(uint64_t *)MMS(sp, k))
#define RDFS(sp, k) (*(const float *)MMS(sp, k))
#if defined(GEN_DG_TRACE) && defined(GUEST_SPR0)   /* dev: for the tracer a stack store is a write (MMS alone counts it as a read) */
#define WR8S(sp, k, v) GUEST_STORE(uint8_t, (uint32_t)(sp) + (k), v)
#define WR16S(sp, k, v) GUEST_STORE(uint16_t, (uint32_t)(sp) + (k), v)
#define WR32S(sp, k, v) GUEST_STORE(uint32_t, (uint32_t)(sp) + (k), v)
#define WR64S(sp, k, v) GUEST_STORE(uint64_t, (uint32_t)(sp) + (k), v)
#define WRFS(sp, k, v) GUEST_STORE(float, (uint32_t)(sp) + (k), v)
#else
#define WR8S(sp, k, v) ((void)(*(uint8_t *)MMS(sp, k) = (uint8_t)(v)))
#define WR16S(sp, k, v) ((void)(*(uint16_t *)MMS(sp, k) = (uint16_t)(v)))
#define WR32S(sp, k, v) ((void)(*(uint32_t *)MMS(sp, k) = (uint32_t)(v)))
#define WR64S(sp, k, v) ((void)(*(uint64_t *)MMS(sp, k) = (uint64_t)(v)))
#define WRFS(sp, k, v) ((void)(*(float *)MMS(sp, k) = (v)))
#endif
/* tools/mm_base.py (recomp/lean_plan.md step 3): a guest access through a register base + a constant offset k takes the I/O test and the
   host address from the base, not from each address, so GCC shares both between the accesses through one register and k becomes the
   offset of the host access.  Exact while base and base + k lie in the same 256 MB segment and the same 32 MB window, as every access
   of a valid pointer to a field does; GEN_MB_CHECK (x86 dev) reports an access that breaks it.  The lq / sq halves (base = the 16-byte-
   aligned address, k = 0 / 8) are exact for every address.  The dev tracers and the address check build take the general path. */
#if defined(GEN_MM_FAST) && defined(GUEST_SPR0) && !defined(GEN_DG_TRACE) && !defined(GEN_MM_CHECK)
#ifdef GEN_MB_CHECK
void rt_mb_bad(uint32_t b, int k);
#define GEN_MB_CK(b, k) ({ uint32_t c_ = (b) + (uint32_t)(k); int32_t o_ = (int32_t)((b) & (GUEST_SIZE - 1)) + (k); \
  if (((b) >> 28 == 1) != (c_ >> 28 == 1) || ((b) >> 28 != 1 && (o_ < 0 || o_ >= GUEST_SIZE))) rt_mb_bad(b, k); (void)0; })
#else
#define GEN_MB_CK(b, k) ((void)0)
#endif
#define GEN_LDB(T, b, k) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_MB_CK(b_, k); \
  GEN_FAST(b_) ? *(T *)(h_ + (k)) : *(T *)rt_mm(b_ + (uint32_t)(k)); })
#define GEN_STB(T, b, k, v) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); T v_ = (T)(v); GEN_MB_CK(b_, k); \
  if (GEN_FAST(b_)) *(T *)(h_ + (k)) = v_; else GEN_ST_SLOW(T, b_ + (uint32_t)(k), v_); (void)0; })
#define RD8B(b, k) GEN_LDB(uint8_t, b, k)
#define RD16B(b, k) GEN_LDB(uint16_t, b, k)
#define RD32B(b, k) GEN_LDB(uint32_t, b, k)
#define RD64B(b, k) GEN_LDB(uint64_t, b, k)
#define RDFB(b, k) GEN_LDB(float, b, k)
#define WR8B(b, k, v) GEN_STB(uint8_t, b, k, v)
#define WR16B(b, k, v) GEN_STB(uint16_t, b, k, v)
#define WR32B(b, k, v) GEN_STB(uint32_t, b, k, v)
#define WR64B(b, k, v) GEN_STB(uint64_t, b, k, v)
#define WRFB(b, k, v) GEN_STB(float, b, k, v)
/* tools/ldu_fuse.py's 8-byte unaligned pair through a base register, inline: with the base outside the register space the 8 bytes are
   one unaligned host access (the invariant above for k and k + 7 puts both ends in the base's 32 MB view), else GLDU / GSDU
   (boot/gen_dispatch.c), which run the pair as before */
#define GLDUB(r, b, k, rl) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); int64_t r_; GEN_MB_CK(b_, k); GEN_MB_CK(b_, (k) + 7); \
  if (GEN_FAST(b_)) { uint64_t v_; memcpy(&v_, h_ + (k), 8); r_ = (int64_t)v_; } else r_ = GLDU(r, b_ + (uint32_t)(k), rl); r_; })
#define GSDUB(r, b, k, rl) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); uint64_t v_ = (uint64_t)(r); GEN_MB_CK(b_, k); \
  GEN_MB_CK(b_, (k) + 7); if (GEN_FAST(b_)) memcpy(h_ + (k), &v_, 8); else GSDU((int64_t)v_, b_ + (uint32_t)(k), rl); (void)0; })
/* lqc2 / sqc2 (runtime/vu0.h VLQC2 / VSQC2) at a multiple of 16 from a register: (r + k) & ~15 = (r & ~15) + k, so the base is the
   register's 16-byte-aligned value (the invariant above for k and k + 15) */
#define VLQC2B(ft, b, k) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_MB_CK(b_, k); GEN_MB_CK(b_, (k) + 15); \
  if (ft) { if (GEN_FAST(b_)) VU_CP16(VF[ft].u, h_ + (k)); else VU_CP16(VF[ft].u, rt_mm(b_ + (uint32_t)(k))); } (void)0; })
#define VSQC2B(ft, b, k) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_MB_CK(b_, k); GEN_MB_CK(b_, (k) + 15); \
  if (GEN_FAST(b_)) VU_CP16(h_ + (k), VF[ft].u); else rt_store(b_ + (uint32_t)(k), VF[ft].u, 16); (void)0; })
#else
#define RD8B(b, k) RD8((uint32_t)(b) + (uint32_t)(k))
#define RD16B(b, k) RD16((uint32_t)(b) + (uint32_t)(k))
#define RD32B(b, k) RD32((uint32_t)(b) + (uint32_t)(k))
#define RD64B(b, k) RD64((uint32_t)(b) + (uint32_t)(k))
#define RDFB(b, k) RDF((uint32_t)(b) + (uint32_t)(k))
#define WR8B(b, k, v) WR8((uint32_t)(b) + (uint32_t)(k), v)
#define WR16B(b, k, v) WR16((uint32_t)(b) + (uint32_t)(k), v)
#define WR32B(b, k, v) WR32((uint32_t)(b) + (uint32_t)(k), v)
#define WR64B(b, k, v) WR64((uint32_t)(b) + (uint32_t)(k), v)
#define WRFB(b, k, v) WRF((uint32_t)(b) + (uint32_t)(k), v)
#define GLDUB(r, b, k, rl) GLDU(r, (uint32_t)(b) + (uint32_t)(k), rl)
#define GSDUB(r, b, k, rl) GSDU(r, (uint32_t)(b) + (uint32_t)(k), rl)
#define VLQC2B(ft, b, k) VLQC2(ft, (uint32_t)(b) + (uint32_t)(k))
#define VSQC2B(ft, b, k) VSQC2(ft, (uint32_t)(b) + (uint32_t)(k))
#endif
#define IRQ_POLL() rt_poll()
/* tools/poll_fast.py: a loop back-edge advances the virtual time inline; only from kernel.h k_poll_at (an event due, an interrupt pending,
   a thread switch wanted) are the cached registers stored and the rest of the poll run */
#ifdef K_COUNTDOWN   /* runtime/ee_io.h: the time to k_poll_at as one 32-bit countdown (rt_poll_slow re-arms it when it was clamped) */
extern int32_t k_left; void rt_poll_slow(void);
#define IRQ_POLLF(flush) ((k_left -= K_POLL_CYCLES) <= 0 ? ((void)(flush), rt_poll_slow()) : (void)0)
#define IRQ_POLLV(flush, after) ((k_left -= K_POLL_CYCLES) <= 0 ? ((void)(flush), rt_poll_slow(), (void)(after)) : (void)0)
#else
extern uint64_t k_time, k_poll_at; void rt_poll_slow(void);
#define IRQ_POLLF(flush) ((k_time += K_POLL_CYCLES) >= k_poll_at ? ((void)(flush), rt_poll_slow()) : (void)0)
#define IRQ_POLLV(flush, after) ((k_time += K_POLL_CYCLES) >= k_poll_at ? ((void)(flush), rt_poll_slow(), (void)(after)) : (void)0)
#endif   /* tools/vu_hoist.py: redo the loop's hoisted VU0 input clamps after the handlers */
extern uint32_t g_vu_ext;   /* boot.c: counts rt_store and rt_poll_slow; VH_CHK redoes a loop's hoisted clamps when it moved */
#define VH_CHK(e, redo) (__builtin_expect(g_vu_ext != (e), 0) ? ((e) = g_vu_ext, (void)(redo)) : (void)0)
void rt_missing(uint32_t a);    /* tools/prune_cov.py: a function the reduced build left out */
struct GenFn { uint32_t a; void (*f)(void); };   /* tools/jalr_tab.py: a jalr target and its translated function */
/* recomp/idioms.py: a library routine the runtime runs natively when it can reproduce the translation exactly (boot.c rt_idiom_*) */
#if defined(GEN_MM_FAST) && defined(GUEST_SPR0) && !defined(GEN_DG_TRACE) && !defined(GEN_NO_IDIOMS)
int rt_idiom_qcopy(void);
#define GEN_IDIOM_QCOPY() do { if (rt_idiom_qcopy()) return; } while (0)
#endif
#ifdef GEN_COVER                 /* dev: which translated functions run (boot.c rt_cov; GEN_UNIT names the unit: "elf" or the stage file) */
#ifndef GEN_UNIT
#define GEN_UNIT "elf"
#endif
void rt_cov(const char *u, uint32_t a);
#define FN_ENTER(a) rt_cov(GEN_UNIT, a)
#endif
#if defined(DG_LITE) && !defined(GEN_COVER)   /* dev (x86, boot.c MGS2_DG_LITE): on a reduced draw-pipeline call the listed functions return at once */
extern int g_dgx_lite; int rt_dg_lite(uint32_t a);
#define FN_ENTER(a) do { if (g_dgx_lite && rt_dg_lite(a)) return; } while (0)
#endif
#if defined(GEN_LITE) && !defined(DG_LITE) && !defined(GEN_COVER)   /* the reduced draw pipeline on steps that are not shown (dg_lite.h, boot.c rt_dg_call) */
#include "dg_lite.h"
extern int g_dg_lite;
#define FN_ENTER(a) do { if (GEN_LITE_FN(a) && g_dg_lite) return; } while (0)
#endif
#include "guest.h"
#ifdef GEN_DG_TRACE   /* dev (boot.c MGS2_READ_HASH): each load gives the tracer the bytes it uses (g_dgt_w: width, 0 = a word; g_dgt_o: offset
                         from the address MM gets; mmi.h's partial loads and stores set them too, width -1 for a partial store's merge read) */
extern int g_dgt_w, g_dgt_o;
#undef RD8
#undef RD16
#undef RD64
#undef RD8S
#undef RD16S
#undef RD64S
#define DGT_LD(T, a, w) ({ uint32_t a_ = (uint32_t)(a); T *p_; g_dgt_w = (w); p_ = (T *)MM(a_); g_dgt_w = g_dgt_o = 0; *p_; })   /* (a register
                                                                         load does not reach the tracer: the width must not outlive it) */
#define RD8(a) DGT_LD(uint8_t, a, 1)
#define RD16(a) DGT_LD(uint16_t, a, 2)
#define RD64(a) DGT_LD(uint64_t, a, 8)
#define RD8S(sp, k) RD8((uint32_t)(sp) + (k))
#define RD16S(sp, k) RD16((uint32_t)(sp) + (k))
#define RD64S(sp, k) RD64((uint32_t)(sp) + (k))
#endif
#include "mmi.h"
#define HWR32 rt_hwr32
#define HWW32 rt_hww32
#define SYSCALL rt_syscall
#define BREAK rt_break
#define COP0R rt_cop0r
#define COP0W rt_cop0w
#define COP0OP rt_cop0op
void gen_jalr(uint32_t a);
#define JALR gen_jalr
/* regcache.py RC_SPECIAL (recomp/lean_plan.md 1b): the SIF command dispatcher's two calls through its table (P175cd0 at 0x175d98 / 0x175dd4)
   are JSIF, with the union of the register sets of the ten handlers regcache.py lists (SIF_TARGETS); any other target stops the machine */
static inline void gen_jsif(uint32_t a)
{
  switch (a) {
  case 0x175780: case 0x175760: case 0x1761a8: case 0x176468: case 0x176658: case 0x176260: case 0x176f60: case 0x163bd0: case 0x15e600:
  case 0x15dd40: break;
  default: fprintf(stderr, "JSIF: target %08x is not in regcache.py's list\n", (unsigned)a); exit(7);
  }
  gen_jalr(a);
}
#define JSIF gen_jsif
/* regcache.py RC_RETLIVE=poison (the check of lean_plan.md 1c): what a return writes in place of a register store it leaves out.  GPR: high
   word 0x5a5a5aNN (NN = the register), low word 0xa5 << 24 | the function's guest address; FPU: a NaN carrying the address / 4 */
#define RETPOISON(n, fn) ((int64_t)(((uint64_t)(0x5a5a5a00u | (n)) << 32) | (uint32_t)(0xa5000000u | (uint32_t)(fn))))
static inline float rp_f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
#define RETPOISONF(n, fn) rp_f(0x7f800000u | ((uint32_t)(fn) >> 2))
