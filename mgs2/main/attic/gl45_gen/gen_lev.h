/* gl45 study, batch 3 (attic/gl45_gen/gl45_gen.md): the access forms of tools/gen_levers.py's three generator levers.  Included at the
   end of gen_pre.h (boot/ and boot_g0/); active only with -DGEN_LEV on the fast access configuration.  With GEN_LEV and a header the
   pass has not touched every macro below expands to what gen_pre.h's own form expands to (the file-scope GEN_KC and GEN_HS are 0).
   (c) the clock in a local: a converted function opens with GEN_KC_ON, which hides the file-scope GEN_KC = 0 and kl_ (the way a G0
       body hides gen_g0_fn).  IRQ_POLLF then counts on kl_ with the translation's own test; kl_ is written back before and read after
       every way out of the function's own code: the poll's slow path, a memory access's slow path (GEN_KS0 / GEN_KS1 below), and,
       put there by the pass, every call and every return.
   (b) host pointers: GEN_HS_ON hides GEN_HS = 0 and hs_ (the host address of the guest $sp, set again by the pass after every
       assignment to $sp); the _H forms take the base register's host pointer from a local the pass maintains.
   (a) no I/O test: the _N / _HN forms, used only in a function copy that runs behind the entry guard (GEN_FAST of the same value). */
#ifndef GEN_LEV_H
#define GEN_LEV_H
#if defined(GEN_LEV) && defined(GEN_MM_FAST) && defined(GUEST_SPR0) && !defined(GEN_DG_TRACE) && !defined(GEN_MM_CHECK) && !defined(GEN_MB_CHECK) && !defined(GEN_SP_CHECK)
#ifndef GEN_G0_PRE
#define GEN_G0_PRE(a, n) ((void)0)
#endif
enum { GEN_KC = 0, GEN_HS = 0 };
#ifdef K_COUNTDOWN
typedef int32_t gen_k_t;
#define GEN_KNOW k_left
#define GEN_KDUE ((kl_ -= K_POLL_CYCLES) <= 0)
#define GEN_GDUE ((k_left -= K_POLL_CYCLES) <= 0)
#else
typedef uint64_t gen_k_t;
#define GEN_KNOW k_time
#define GEN_KDUE ((kl_ += K_POLL_CYCLES) >= k_poll_at)
#define GEN_GDUE ((k_time += K_POLL_CYCLES) >= k_poll_at)
#endif
static gen_k_t kl_; static uint8_t *hs_;                 /* never read: GEN_KC and GEN_HS are 0 wherever these are in scope */
#define GEN_KGET ((void)(kl_ = GEN_KNOW))
#define GEN_KPUT ((void)(GEN_KNOW = kl_))
#define GEN_KC_ON enum { GEN_KC = 1 }; gen_k_t kl_ = GEN_KNOW
#define GEN_HS_ON enum { GEN_HS = 1 }; uint8_t *hs_
#define GEN_KS0 (GEN_KC ? GEN_KPUT : (void)0)
#define GEN_KS1 (GEN_KC ? GEN_KGET : (void)0)
#define GEN_KCALL(call) (GEN_KPUT, (call), GEN_KGET)     /* the pass: a call out of a converted function */
#define GEN_KCALLV(call) ({ __typeof__(call) x_; GEN_KPUT; x_ = (call); GEN_KGET; x_; })   /* the same around a call with a value */
#undef IRQ_POLLF
#define IRQ_POLLF(flush) (GEN_KC ? (GEN_KDUE ? (GEN_KPUT, (void)(flush), rt_poll_slow(), GEN_KGET) : (void)0) \
                                 : (GEN_GDUE ? ((void)(flush), rt_poll_slow()) : (void)0))
#undef MMS
#ifdef GEN_HB_SP
#define MMS(sp, k) (GEN_HS ? hs_ + (k) : gen_hb((uint32_t)(sp)) + (k))
#else
#define MMS(sp, k) (GEN_HS ? hs_ + (k) : GEN_BASE + ((uint32_t)(sp) & (GUEST_SIZE - 1)) + (k))
#endif
/* (a)'s entry guard, as the pass writes it in a guarded function's wrapper.  -DGEN_LEV_FBTEST=<n> (a test of the fallback, twin only):
   every n-th guarded call whose guard holds takes the untouched copy all the same, and both kinds are counted */
#ifdef GEN_LEV_FBTEST
extern long gen_fb[3];
#define GEN_GUARD(c) ((c) ? (++gen_fb[0] % GEN_LEV_FBTEST ? 1 : (gen_fb[2]++, 0)) : (gen_fb[1]++, 0))
#else
#define GEN_GUARD(c) (c)
#endif
/* loads and stores through a base register: H = the host pointer (gen_hb of the base, or the pass's local), TST = the I/O test or 1 */
#define GEN_LDX(T, H, TST, a) ((TST) ? *(T *)(H) : ({ T x_; uint32_t a_ = (a); GEN_KS0; x_ = *(T *)rt_mm(a_); GEN_KS1; x_; }))
#define GEN_STX(T, H, TST, a, v) ({ T v_ = (T)(v); uint32_t a_ = (a); GEN_G0_PRE(a_, sizeof(T)); \
  if (TST) *(T *)(H) = v_; else { GEN_KS0; GEN_ST_SLOW(T, a_, v_); GEN_KS1; } (void)0; })
#undef GEN_LDB
#undef GEN_STB
#define GEN_LDB(T, b, k) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_LDX(T, h_ + (k), GEN_FAST(b_), b_ + (uint32_t)(k)); })
#define GEN_STB(T, b, k, v) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_STX(T, h_ + (k), GEN_FAST(b_), b_ + (uint32_t)(k), v); })
#define GEN_LDB_H(T, h, b, k) ({ uint32_t b_ = (uint32_t)(b); GEN_LDX(T, (h) + (k), GEN_FAST(b_), b_ + (uint32_t)(k)); })
#define GEN_STB_H(T, h, b, k, v) ({ uint32_t b_ = (uint32_t)(b); GEN_STX(T, (h) + (k), GEN_FAST(b_), b_ + (uint32_t)(k), v); })
#define GEN_LDB_N(T, b, k) ({ uint32_t b_ = (uint32_t)(b); *(T *)(gen_hb(b_) + (k)); })
#define GEN_STB_N(T, b, k, v) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_STX(T, h_ + (k), 1, b_ + (uint32_t)(k), v); })
#define GEN_LDB_HN(T, h, b, k) (*(T *)((h) + (k)))
#define GEN_STB_HN(T, h, b, k, v) GEN_STX(T, (h) + (k), 1, (uint32_t)(b) + (uint32_t)(k), v)
#define RD8B_H(h, b, k) GEN_LDB_H(uint8_t, h, b, k)
#define RD16B_H(h, b, k) GEN_LDB_H(uint16_t, h, b, k)
#define RD32B_H(h, b, k) GEN_LDB_H(uint32_t, h, b, k)
#define RD64B_H(h, b, k) GEN_LDB_H(uint64_t, h, b, k)
#define RDFB_H(h, b, k) GEN_LDB_H(float, h, b, k)
#define WR8B_H(h, b, k, v) GEN_STB_H(uint8_t, h, b, k, v)
#define WR16B_H(h, b, k, v) GEN_STB_H(uint16_t, h, b, k, v)
#define WR32B_H(h, b, k, v) GEN_STB_H(uint32_t, h, b, k, v)
#define WR64B_H(h, b, k, v) GEN_STB_H(uint64_t, h, b, k, v)
#define WRFB_H(h, b, k, v) GEN_STB_H(float, h, b, k, v)
#define RD8B_N(b, k) GEN_LDB_N(uint8_t, b, k)
#define RD16B_N(b, k) GEN_LDB_N(uint16_t, b, k)
#define RD32B_N(b, k) GEN_LDB_N(uint32_t, b, k)
#define RD64B_N(b, k) GEN_LDB_N(uint64_t, b, k)
#define RDFB_N(b, k) GEN_LDB_N(float, b, k)
#define WR8B_N(b, k, v) GEN_STB_N(uint8_t, b, k, v)
#define WR16B_N(b, k, v) GEN_STB_N(uint16_t, b, k, v)
#define WR32B_N(b, k, v) GEN_STB_N(uint32_t, b, k, v)
#define WR64B_N(b, k, v) GEN_STB_N(uint64_t, b, k, v)
#define WRFB_N(b, k, v) GEN_STB_N(float, b, k, v)
#define RD8B_HN(h, b, k) GEN_LDB_HN(uint8_t, h, b, k)
#define RD16B_HN(h, b, k) GEN_LDB_HN(uint16_t, h, b, k)
#define RD32B_HN(h, b, k) GEN_LDB_HN(uint32_t, h, b, k)
#define RD64B_HN(h, b, k) GEN_LDB_HN(uint64_t, h, b, k)
#define RDFB_HN(h, b, k) GEN_LDB_HN(float, h, b, k)
#define WR8B_HN(h, b, k, v) GEN_STB_HN(uint8_t, h, b, k, v)
#define WR16B_HN(h, b, k, v) GEN_STB_HN(uint16_t, h, b, k, v)
#define WR32B_HN(h, b, k, v) GEN_STB_HN(uint32_t, h, b, k, v)
#define WR64B_HN(h, b, k, v) GEN_STB_HN(uint64_t, h, b, k, v)
#define WRFB_HN(h, b, k, v) GEN_STB_HN(float, h, b, k, v)
/* the unaligned 8-byte pair and lqc2 / sqc2 through a base register (gen_pre.h's forms with the same two choices) */
#define GEN_LDUX(r, H, TST, a, rl) ({ int64_t r_; if (TST) { uint64_t v_; memcpy(&v_, (H), 8); r_ = (int64_t)v_; } \
  else { GEN_KS0; r_ = GLDU(r, (a), rl); GEN_KS1; } r_; })
#define GEN_SDUX(r, H, TST, a, rl) ({ uint64_t v_ = (uint64_t)(r); uint32_t a_ = (a); GEN_G0_PRE(a_, 8); \
  if (TST) memcpy((H), &v_, 8); else { GEN_KS0; (GSDU)((int64_t)v_, a_, rl); GEN_KS1; } (void)0; })
#define GEN_LQX(ft, H, TST, a) ({ if (ft) { if (TST) VU_CP16(VF[ft].u, (H)); else { GEN_KS0; VU_CP16(VF[ft].u, rt_mm(a)); GEN_KS1; } } (void)0; })
#define GEN_SQX(ft, H, TST, a) ({ uint32_t a_ = (a); GEN_G0_PRE(a_, 16); \
  if (TST) VU_CP16((H), VF[ft].u); else { GEN_KS0; rt_store(a_, VF[ft].u, 16); GEN_KS1; } (void)0; })
#undef GLDUB
#undef GSDUB
#undef VLQC2B
#undef VSQC2B
#define GLDUB(r, b, k, rl) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_LDUX(r, h_ + (k), GEN_FAST(b_), b_ + (uint32_t)(k), rl); })
#define GSDUB(r, b, k, rl) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_SDUX(r, h_ + (k), GEN_FAST(b_), b_ + (uint32_t)(k), rl); })
#define VLQC2B(ft, b, k) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_LQX(ft, h_ + (k), GEN_FAST(b_), b_ + (uint32_t)(k)); })
#define VSQC2B(ft, b, k) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_SQX(ft, h_ + (k), GEN_FAST(b_), b_ + (uint32_t)(k)); })
#define GLDUB_H(h, r, b, k, rl) ({ uint32_t b_ = (uint32_t)(b); GEN_LDUX(r, (h) + (k), GEN_FAST(b_), b_ + (uint32_t)(k), rl); })
#define GSDUB_H(h, r, b, k, rl) ({ uint32_t b_ = (uint32_t)(b); GEN_SDUX(r, (h) + (k), GEN_FAST(b_), b_ + (uint32_t)(k), rl); })
#define VLQC2B_H(h, ft, b, k) ({ uint32_t b_ = (uint32_t)(b); GEN_LQX(ft, (h) + (k), GEN_FAST(b_), b_ + (uint32_t)(k)); })
#define VSQC2B_H(h, ft, b, k) ({ uint32_t b_ = (uint32_t)(b); GEN_SQX(ft, (h) + (k), GEN_FAST(b_), b_ + (uint32_t)(k)); })
#define GLDUB_N(r, b, k, rl) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_LDUX(r, h_ + (k), 1, b_ + (uint32_t)(k), rl); })
#define GSDUB_N(r, b, k, rl) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_SDUX(r, h_ + (k), 1, b_ + (uint32_t)(k), rl); })
#define VLQC2B_N(ft, b, k) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_LQX(ft, h_ + (k), 1, b_ + (uint32_t)(k)); })
#define VSQC2B_N(ft, b, k) ({ uint32_t b_ = (uint32_t)(b); uint8_t *h_ = gen_hb(b_); GEN_SQX(ft, h_ + (k), 1, b_ + (uint32_t)(k)); })
#define GLDUB_HN(h, r, b, k, rl) GEN_LDUX(r, (h) + (k), 1, (uint32_t)(b) + (uint32_t)(k), rl)
#define GSDUB_HN(h, r, b, k, rl) GEN_SDUX(r, (h) + (k), 1, (uint32_t)(b) + (uint32_t)(k), rl)
#define VLQC2B_HN(h, ft, b, k) GEN_LQX(ft, (h) + (k), 1, (uint32_t)(b) + (uint32_t)(k))
#define VSQC2B_HN(h, ft, b, k) GEN_SQX(ft, (h) + (k), 1, (uint32_t)(b) + (uint32_t)(k))
#endif
#endif
