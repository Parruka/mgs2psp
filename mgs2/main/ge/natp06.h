/* natp06.h: native step 2 (attic/ge_native/step2_results.md): people meshes, the 0x12e8b8 builder's groups as records.  The format
   both CPUs share (guest addresses and words, no host pointers) and the walk's front end: a record to lane 2's translator entry
   (ge/xlate.h xl_lane_p06) with the record's pointers and words in place of the packet's, so the converters get the same call as from
   the packet (runtime/ee_io.h lane_p06).  The main CPU (boot/np6_stub.h) writes the groups inside the pipeline call, one array per
   packet buffer, beside the ship's (ge/natsnap.h); the walks (boot/nat_x86.c on the twin; ge/xlme.c on the PSP) draw them by the
   ship's rule: before a top-level tag of the groups' packet buffer, every group whose key is at or below the tag's address.
   A header for the walk that includes it, which defines NATP_PTR and NATP_SPAN before it (as ge/natp04.h).  ME-safe: no C library, no
   system call, no static data; every guest pointer goes through NATP_PTR and every count is capped before use.
   Game data passes through it: personal use only. */
#ifndef MGS2_NATP06_H
#define MGS2_NATP06_H
#include <stdint.h>
#include "natsnap.h"
#include "xlate.h"

#define NP6_MAXGRP 1024          /* people groups a picture holds (more: the builder runs; the same on the twin and the PSP) */
/* the template's words (0x12e790 + 0x18ec40 at the pass's start; re/mesh_packets.md 6) */
#define NP6T_S11 0x01000101u     /* STCYCL 1 / 1: the constant block, the materials, the header's CNT */
#define NP6T_S61 0x01000106u     /* STCYCL 6 / 1: the six streams */
#define NP6T_BLK 0x6c188000u     /* UNPACK V4-32 24 -> T + 0 */
#define NP6T_MAT 0x6c0880e8u     /* UNPACK V4-32 8 -> T + 0xe8 (+ 8 a material) */
#define NP6T_V4 0x6d008100u      /* UNPACK V4-16 2 n -> T + 0x100 + k (A, B, F; | 2 n << 16) */
#define NP6T_V2 0x75008100u      /* UNPACK V2-16 masked 4 n -> T + 0x100 + k (C, D, E; | 4 n << 16) */
#define NP6T_HDR 0x6c018014u     /* UNPACK V4-32 1 -> T + 0x14 */
enum { NP6G_FULL = 1,            /* the constant block's REF first (the call's first two groups) */
       NP6G_PKT = 2 };           /* x86 shadow arms: the builder ran too, the group's packet lies at key */
typedef struct {                 /* 72 B */
  uint32_t key;                  /* the order key: the packet position (RAM) of the next item at the call (every group of a call has its
                                    call's), or the group's own first tag (x86 shadow arms) */
  uint32_t blk;                  /* SPR 0xd34 = mesh+0xFC: the constant block (NP6G_FULL only) */
  uint32_t mat[3];               /* desc+0x14, +0x18, +0x1C when sent (the builder's memo: not 0 and not the last sent in the template's
                                    half), else 0: a CNT of 0 and a NOP */
  uint32_t s[6];                 /* streams A .. F (the mesh's cursors), 0: not sent */
  uint8_t n[6];                  /* their quadwords: desc+0x0E .. +0x13 */
  uint8_t flags, pad;            /* NP6G_* */
  uint32_t hdr[4];               /* the header quadword: desc+0x0C the count, desc word 0, SPR 0xc70 (mesh+0xF8, the fog word), mesh+0xC8 & 1 */
  uint32_t mscal;                /* 0x14000000 | the VU1 entry's halfword (SPR 0xc7c + 2 idx) */
} NatP6;
typedef struct {
  NatP6 *g;                      /* NP6_MAXGRP entries, or 0 */
  uint32_t n, cur, draw, seq, vq, last_key;   /* as ge/natsnap.h NatBuf */
} NatP6Buf;
/* the stub's counts (boot/np6_stub.h g_np6_c) */
enum { N6_REC, N6_GRP, N6_STUBC, N6_EMPTY, N6_FB_FULL, N6_FB_ORDER, N6_FB_END, N6_FB_OTHER, N6_NOMEM, N6_PICS, N6_GRP_MAX, N6_LEFT, N6_MEMO_RESET,
       N6_FB_MEMO, N6_NOARR, N6_LATE, N6_N };   /* step 2: N6_NOARR pipeline calls in arm 3 before the people's arrays were there or before the
                                                    people's start (the builder ran); N6_LATE those of them from the start on (the growth late) */

/* the group's quadwords as the builder copies them to the packet (13 from q0 with NP6G_FULL, else 12 from q1).  A slot not sent: a CNT of
   0 whose ADDR the builder leaves as it was (here 0; the shadow compare skips it) */
static inline int np6_words(const NatP6 *g, uint32_t *w)
{
  int q = 0;
#define Q4(a, b, c, d) do { w[4 * q] = (a); w[4 * q + 1] = (b); w[4 * q + 2] = (c); w[4 * q + 3] = (d); q++; } while (0)
  if (g->flags & NP6G_FULL) Q4(0x30000018u, g->blk, NP6T_S11, NP6T_BLK);
  for (int j = 0; j < 3; j++) { if (g->mat[j]) Q4(0x30000008u, g->mat[j], NP6T_S11, NP6T_MAT + 8u * (uint32_t)j); else Q4(0x10000000u, 0, NP6T_S11, 0); }
  for (int k = 0; k < 6; k++) { uint32_t n = g->n[k], v2 = k >= 2 && k <= 4;
    if (g->s[k] && n) Q4(0x30000000u | n, g->s[k], NP6T_S61, (v2 ? NP6T_V2 | ((4 * n) & 0xffu) << 16 : NP6T_V4 | ((2 * n) & 0xffu) << 16) + (uint32_t)k);
    else Q4(0x10000000u, 0, NP6T_S61, 0); }
  Q4(0x10000002u, 0, NP6T_S11, NP6T_HDR);
  Q4(g->hdr[0], g->hdr[1], g->hdr[2], g->hdr[3]);
  Q4(g->mscal, 0, 0, 0);
#undef Q4
  return q;
}

#endif
#if defined(NATP_PTR) && !defined(MGS2_NATP06_WALK)   /* the walk's part: after ge/natp04.h, with NATP_PTR and NATP_SPAN defined */
#define MGS2_NATP06_WALK
/* the group to the translator: lane 2's entry with the packet's words made from the record (its data pointers from NATP_PTR), else (the
   translator declines lane 2: data waiting, or an UNPACK it would not take at once) the generic walk's order, word by word.  NATP_CORE:
   lane 2 took it; NATP_FED; NATP_BAD_*: dropped */
static inline __attribute__((always_inline)) int nat_p06_group(const NatP6 *g)
{
  uint32_t w[4 * 16]; const uint32_t *dp[10]; const int q0 = (g->flags & NP6G_FULL) != 0, nr = 9 + q0; int nq, j;
  for (j = 0; j < 6; j++) if (g->n[j] > 128u) return NATP_BAD_CNT;   /* the UNPACK's count field: 2 n (4 n for C, D, E: 64) <= 256 */
  if (g->n[2] > 64u || g->n[3] > 64u || g->n[4] > 64u) return NATP_BAD_CNT;
  nq = np6_words(g, w);
  for (j = 0; j < nr; j++) {
    const uint32_t *t = w + 4 * j, n = t[0] & 0xffffu;
    dp[j] = 0;
    if (t[0] >> 28 == 3 && !(dp[j] = NATP_PTR(t[1] & 0xfffffff0u, n))) return NATP_BAD_PTR;
  }
  if (xl_lane_p06(w, g->key, q0, dp)) return NATP_CORE;
  for (j = 0; j < nr; j++) {   /* each tag's TTE words, then a sent slot's data where it lies */
    NATP_SPAN(w + 4 * j + 2, 2, g->key + 16u * (uint32_t)j + 8u);
    if (dp[j]) NATP_SPAN(dp[j], 4u * (w[4 * j] & 0xffffu), w[4 * j + 1] & 0xfffffff0u);
  }
  NATP_SPAN(w + 4 * nr + 2, 2, g->key + 16u * (uint32_t)nr + 8u);   /* the CNT: STCYCL, the header's UNPACK, its quadword, MSCAL and NOPs */
  NATP_SPAN(w + 4 * (nr + 1), 4, g->key + 16u * (uint32_t)(nr + 1));
  NATP_SPAN(w + 4 * (nr + 2), 4, g->key + 16u * (uint32_t)(nr + 2));
  (void)nq;
  return NATP_FED;
}
#endif
