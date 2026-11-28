/* natp04.h: the native front end for ship meshes, step 1 (native_renderer_design.md 6.1): a 0x1214a0 group from its record
   (ge/natsnap.h NatGrp) to lane 1's converter core (ge/xlate.h xl_p04_core), with the record's pointers and header words in place of
   the packet's.  A header for the walk that includes it (ge/xlme.c: the PSP's late walk, on the ME and the main CPU; boot/nat_x86.c: the
   x86 twin's runtime walk), which defines before it:
     NATP_PTR(a, n)        n quadwords at guest address a in one piece, as const uint32_t *, or 0
     NATP_SPAN(p, nw, a)   nw words at p, which came from guest address a, to the translator (xl_vif1_span)
   ME-safe: no C library, no system call, no static data, no call through a pointer; every guest pointer goes through NATP_PTR (range-
   checked there) and every count is capped before use (a bad group is dropped and its code returned).  The group's header words
   (g->hdr) must stay where they are until the transfer ends: the translator may keep the pointer. */
#ifndef MGS2_NATP04_H
#define MGS2_NATP04_H
#include <stdint.h>
#include "natsnap.h"
#include "xlate.h"

/* the template's constant words (0x1213b0 / 0x18dcf0 copied to SPR 0x190 / 0x210 at the pass's start; re/mesh_packets.md 3) */
#define NATT_S11 0x01000101u       /* STCYCL 1 / 1 */
#define NATT_S31 0x01000103u       /* STCYCL 3 / 1 */
#define NATT_BLK 0x6c188000u       /* UNPACK V4-32 24 -> T + 0 */
#define NATT_MAT 0x6c0880e8u       /* UNPACK V4-32 8 -> T + 0xe8 */
#define NATT_A 0x6d008100u         /* UNPACK V4-16 2 n -> T + 0x100 (| 2 n << 16) */
#define NATT_B 0x6d008101u         /* -> T + 0x101 */
#define NATT_C 0x75008102u         /* UNPACK V2-16 masked 4 n -> T + 0x102 (| 4 n << 16) */
#define NATT_HDR 0x6c018014u       /* UNPACK V4-32 1 -> T + 0x14 */

enum { NATP_CORE = 1,            /* the converter core took the group (xl_p04_core) */
       NATP_FED = 2,             /* a stream not sent, or the core declined: fed word by word (the generic walk's order) */
       NATP_BAD_CNT = -2,        /* a count over its cap */
       NATP_BAD_PTR = -3 };      /* a pointer out of EE RAM / the scratchpad, or not in one piece */

#ifdef NATP_PTRH                   /* (a walk with a view of a copy: ge/xlme.c) */
#define NATP_PF(a, n, s) (fh ? NATP_PTRH((a), (n), (s)) : NATP_PTR((a), (n)))
#else
#define NATP_PF(a, n, s) ((void)fh, NATP_PTR((a), (n)))
#endif
static inline __attribute__((always_inline)) int nat_p04_group(const NatGrp *g)
{
  int q0 = (g->flags & NATG_FULL) != 0, nr = 4 + q0, all = 1, j, k; XlP04 x; uint32_t key = g->key, ad[5];
  const int fh = (g->flags & NATG_FAST) != 0;   /* gl34: the pointers by NATP_PTRH (a hint per kind of pointer; the same answers) */
  if (g->n[0] > 128u || g->n[1] > 128u || g->n[2] > 64u) return NATP_BAD_CNT;   /* the UNPACK's count field: 2 n, 2 n, 4 n <= 256 */
  if (q0) { x.stc[0] = NATT_S11; x.unp[0] = NATT_BLK; x.qwc[0] = 24; ad[0] = g->blk & 0xfffffff0u; if (!(x.dp[0] = NATP_PF(ad[0], 24, 0))) return NATP_BAD_PTR; }
  x.stc[q0] = NATT_S11; x.unp[q0] = NATT_MAT; x.qwc[q0] = 8; ad[q0] = g->mat & 0xfffffff0u; if (!(x.dp[q0] = NATP_PF(ad[q0], 8, 1))) return NATP_BAD_PTR;
  for (k = 0; k < 3; k++) {
    uint32_t n = g->n[k]; j = q0 + 1 + k; x.stc[j] = NATT_S31;
    if (g->s[k] && n) {
      x.unp[j] = k == 2 ? NATT_C | ((4 * n) & 0xffu) << 16 : (k ? NATT_B : NATT_A) | ((2 * n) & 0xffu) << 16; x.qwc[j] = n; ad[j] = g->s[k] & 0xfffffff0u;
      if (!(x.dp[j] = NATP_PF(ad[j], n, 2 + k))) return NATP_BAD_PTR; }
    else { x.unp[j] = 0; x.qwc[j] = 0; x.dp[j] = 0; ad[j] = 0; all = 0; }   /* not sent: CNT 0, the slot's STCYCL and a NOP */
  }
  x.hunp = NATT_HDR; x.mscal = g->mscal; x.hdr = g->hdr; x.tadr = key; x.q0 = q0; x.keep = g->flags >> 4;
#ifndef __psp__
  { extern const NatGrp *gl_rp_g; gl_rp_g = g; }   /* gl36: the twin's probe (ge/reuse_probe.h) */
#endif
  if (all && (g->flags & NATG_FAST) && xl_p04_fast(&x)) return NATP_CORE;   /* gl34: the fast front end (0: not its form) */
  if (all && xl_p04_core(&x)) return NATP_CORE;   /* lane 1's case: every slot a REF (runtime/ee_io.h lane_p04's tests hold by construction) */
  /* the generic walk's order: each tag's TTE words, then its data (a REF's where it lies; the CNT's: the header, MSCAL and three NOPs) */
  for (j = 0; j < nr; j++) {
    uint32_t tte[2]; tte[0] = x.stc[j]; tte[1] = x.unp[j];
    NATP_SPAN(tte, 2, key + 16u * (uint32_t)j + 8u);
    if (x.qwc[j]) NATP_SPAN(x.dp[j], 4 * x.qwc[j], ad[j]);
  }
  { uint32_t tte[2], tail[4]; tte[0] = NATT_S11; tte[1] = NATT_HDR; tail[0] = g->mscal; tail[1] = tail[2] = tail[3] = 0;
    NATP_SPAN(tte, 2, key + 16u * (uint32_t)nr + 8u);
    NATP_SPAN(g->hdr, 4, key + 16u * (uint32_t)(nr + 1));
    NATP_SPAN(tail, 4, key + 16u * (uint32_t)(nr + 2)); }
  return NATP_FED;
}
#endif
