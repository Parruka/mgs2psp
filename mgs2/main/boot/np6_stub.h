/* np6_stub.h: native step 2 (attic/ge_native/step2_results.md), the main CPU's part for the people: the 0x12e8b8 builder's stub and its
   records (ge/natp06.h).  Included by nat_stub.h after nge_stub.h (one text for the PSP and the x86 twin, as nat_stub.h): the stub's
   choices are part of the run (a stubbed call writes no packet), so they are the same on both.

   The builder 0x12e8b8 (gen: P12e8b8; one caller, 0x12f060's loop over an object's meshes, which sets SPR 0xc70 = mesh+0xF8 and SPR
   0xd34 = mesh+0xFC just before) and the template's set-up 0x12e790 (called twice at each people pass's start, 0x12f130, for the two
   template halves at SPR 0x190 and 0x260) are reached through the wrappers below (the generated functions linked as P12e8b8_orig and
   P12e790_orig).  In a full call of the draw pipeline (g_zt set, g_dg_lite clear) the builder's wrapper asks rt_nat_p06: 1 = stubbed.

   The builder's loop in C (gen P12e8b8, 0x12e990 .. 0x12f03c), per group i of the mesh's (int16) mesh+0xCC:
     - the template half flips (SPR 0xd2c = 1 - itself; half t at SPR 0x190 + 208 t, 13 quadwords);
     - each stream k (A .. F: mesh+0xDC .. +0xF0, the cursor; its quadwords desc+0x0E + k) is sent when the cursor and the count are
       not 0 (REF n, the cursor, UNPACK V4-16 / V2-16 masked to T + 0x100 + k), the cursor then moving by 16 n; else a CNT of 0 and a NOP;
     - each material j (desc+0x14 + 4 j) is sent when it is not 0 and not the half's last sent one (the template word itself: a memo
       the set-up clears to 0 at the pass's start); else a CNT of 0 and a NOP;
     - the header: desc+0x0C (byte), desc word 0; for the call's first two groups also SPR 0xc70 and mesh+0xC8 & 1 (the later groups
       keep the half's, which those two wrote: the same values), and the constant block's REF from SPR 0xd34 (q0, sent by those two only);
     - MSCAL: SPR 0xc7c + 2 idx, idx = (desc0 & 1) + 2 (stream F's cursor after the group not 0);
     - the group's 13 (or 12) quadwords go to the packet control's place (SPR 0xd40: qwc +4, cap +8, RAM destination +0x10, end +0x14),
       unless end < destination + qwc (skipped: never seen).
   The stub keeps the memo itself (np6_memo, both halves, and the half to use), cleared by the set-up's wrapper and moved by every
   group it records, so its choices are the builder's while every call of the pass is stubbed.  A call the stub gives back to the builder
   (a fallback) leaves the game's own memo behind the stub's: such a call is counted (N6_FB_MEMO when the memos differ); none on the
   routes is the requirement, read from the counts.

   MGS2_NP6=<arm> for the whole run, else the arm follows MGS2_NGE_CYCLE's digit (ge/nge stub: 2 = the people native too: arm 3; else 0):
     0  the builder runs, nothing recorded;
     3  native: the call is stubbed and its groups recorded in the array of the packet buffer the call writes; the walk draws them;
     4  stubbed and not drawn (timing only);
     1, 2  x86 only (shadow): the builder runs and the groups are recorded with their packets' addresses; 1 the walk compares each
           record's words with the packet's, 2 the walk draws the record and steps over the packet.
   A call in arm 3 runs the builder instead (returns 0, counted) when its buffer's array is full, a count is over its UNPACK's field, the
   packet control's destination is not in the buffer the pipeline call started in, its key is below the last one recorded, or the packet
   position with every stubbed group's quadwords added is within NAT_END_MARGIN of the buffer's end (as rt_nat_p04).
   Host memory only is written here (the arrays, the memo, counts); the game's state is read. */
#include "../ge/natp06.h"

NatP6Buf g_np6[2];               /* [1]: the lower packet buffer (NAT_BUF), as g_nat */
int g_np6_arm = -1;              /* the arm of the pipeline call g_np6_seq */
int g_np6_cfg;                   /* 1: MGS2_NP6 or MGS2_NGE_CYCLE can name an arm other than 0 */
int g_np6_ok;                    /* step 2: 1: the arrays are there for arm 3 (boot/nge_stub.h nge_grow, or the host's own, nge_host_setup) */
long g_np6_c[N6_N];
static int np6_set, np6_all = -1; static uint32_t np6_seq = ~0u;
static int np6_nat;              /* step 2b: this picture's people are cooked and referenced (arm 3, MGS2_NGE's digit 2, the arena there, C2 on) */
typedef char np6_refs_fit[NP6_MAXGRP == NGE_P6REF ? 1 : -1];
#ifndef __psp__
static int np6_cap_dev; long np6_gmax[2], np6_gmax_vb[2];   /* dev (x86): MGS2_NP6_CAP=<n> the array's entries; the most groups in a picture before / from vb 24,000 */
#define NP6_CAP (np6_cap_dev ? (uint32_t)np6_cap_dev : (uint32_t)NP6_MAXGRP)
#else
#define NP6_CAP ((uint32_t)NP6_MAXGRP)
#endif
static uint32_t np6_memo[2][3]; static uint32_t np6_half;   /* the stub's memo of the two halves' last materials, and the half last used */
static void np6_setup(void)
{
  const char *e = getenv("MGS2_NP6");
  np6_set = 1;
  if (e) np6_all = atoi(e);
  if (!nge_cfg_set) nge_cfg();
  g_np6_cfg = np6_all > 0; for (int i = 0; i < nge_ncyc; i++) if (nge_cyc[i] == 2) g_np6_cfg = 1; if (nge_all == 2) g_np6_cfg = 1;
#ifndef __psp__
  if ((e = getenv("MGS2_NP6_CAP"))) { np6_cap_dev = atoi(e); if (np6_cap_dev < 0 || np6_cap_dev > 8192) np6_cap_dev = 0; }
  if (g_np6_cfg && !((e = getenv("MGS2_NAT_NOMEM")) && atoi(e))) { g_np6[0].g = calloc(NP6_CAP, sizeof(NatP6)); g_np6[1].g = calloc(NP6_CAP, sizeof(NatP6)); }
#endif
  if (g_np6_cfg) fprintf(stderr, "np6: native step 2, the people: MGS2_NP6 %d (-1: by MGS2_NGE_CYCLE's digit 2), %d groups a picture, %u B each\n",
                         np6_all, NP6_MAXGRP, (unsigned)sizeof(NatP6));
}
int rt_np6_arm_at(long vb)   /* the arm for the picture the pipeline call of vblank vb makes */
{
  if (!np6_set) np6_setup();
  if (np6_all >= 0) return np6_all;
  return rt_nge_at(vb + 1) == 2 ? 3 : 0;
}
static void np6_tpl(uint32_t a)   /* the set-up's wrapper: half a (SPR 0x190 or 0x260) cleared, the halves' order back to its start */
{
  uint32_t h = (a & 0x3fff) == 0x260 ? 1u : 0u;
  np6_memo[h][0] = np6_memo[h][1] = np6_memo[h][2] = 0; np6_half = 0; g_np6_c[N6_MEMO_RESET]++;
}
static void np6_game_memo(uint32_t m[2][3], uint32_t *half)   /* the game's memo: the template words, and SPR 0xd2c */
{
  for (uint32_t h = 0; h < 2; h++) for (uint32_t j = 0; j < 3; j++) m[h][j] = nat_spr32(0x190 + 208 * h + 20 + 16 * j);
  *half = nat_spr32(0xd2c);
}
static int np6_memo_differs(void) { uint32_t m[2][3], h; np6_game_memo(m, &h); return h != np6_half || memcmp(m, np6_memo, sizeof m) != 0; }
static int np6_resync;   /* a fallback in arm 3: the stub's memo taken from the game's after the builder ran */
#ifndef __psp__
void np6_x86_rec(int b, uint32_t g0, int ng, uint32_t mesh);   /* dev: boot/nat_x86.c's counts */
#endif
/* the builder's call (R[4] the mesh): 1 = stubbed */
int rt_nat_p06(void)
{
  uint32_t mesh, desc, qwc, cap, dest, end, d, blk, fog, clip, cur[6], key = 0, vq = 0, kq, half, memo[2][3]; int32_t ng; int b, arm, i, stub; NatP6Buf *s; NatP6 *g;
  static NatP6 tmp;
  if (np6_seq != g_dg_seq) { np6_seq = g_dg_seq; g_np6_arm = rt_np6_arm_at(k_vblanks);
    if (g_np6_arm == 3 && ((!g_np6_ok && (nge_np6_bytes() || !g_np6[0].g || !g_np6[1].g)) || k_vblanks + 1 < nge_p6_vb())) {   /* step 2: no
        arrays (before the carve after the opening, or none made) or before the people's start (nge_p6_vb): the builder runs */
      g_np6_arm = 0; g_np6_c[N6_NOARR]++; if (nge_p6_vb() >= 0 && k_vblanks + 1 >= nge_p6_vb()) g_np6_c[N6_LATE]++; }
    np6_nat = g_nge && g_np6_arm == 3 && rt_nge_at(k_vblanks + 1) == 2 && (g_opt2 & OPT2_C2); }   /* step 2b: C2 cooked (nge_stub.h nge6_rec) */
  arm = g_np6_arm;
  if (arm <= 0) return 0;
  if (arm == 4) { g_np6_c[N6_STUBC]++; return 1; }
#ifdef __psp__
  if (arm != 3) return 0;
#endif
  stub = arm == 3;
  mesh = (uint32_t)R[4];
  dest = nat_spr32(0xd50); d = dest & (GUEST_SIZE - 1);
#define NP6_FB(k) do { g_np6_c[k]++; if (stub && np6_memo_differs()) g_np6_c[N6_FB_MEMO]++; np6_resync = stub; return 0; } while (0)
  if (d < NAT_PKT0 || d >= NAT_PKT1) NP6_FB(N6_FB_OTHER);
  b = NAT_BUF(d);
  if (g_nat_bseq[b] != g_dg_seq) NP6_FB(N6_FB_OTHER);
  s = &g_np6[b];
  if (s->seq != g_dg_seq) {   /* this pipeline call's first: the array starts again */
    if (s->draw && s->cur < s->n) g_np6_c[N6_LEFT] += (long)(s->n - s->cur);
    if (g_nat_sync) g_nat_sync(b);   /* (the PSP: the ME's copies of this buffer's records made before the array is written again) */
    s->seq = g_dg_seq; s->n = s->cur = s->vq = s->last_key = 0; s->draw = arm >= 2; g_np6_c[N6_PICS]++; }
  ng = (int32_t)(int16_t)nat_r16(mesh + 0xcc);
  if (ng <= 0) { g_np6_c[N6_EMPTY]++; return stub; }
  if (ng > 1024 || s->n + (uint32_t)ng > NP6_CAP) NP6_FB(ng > 1024 ? N6_FB_OTHER : N6_FB_FULL);
  clip = (uint32_t)(int16_t)nat_r16(mesh + 0xc8) & 1; desc = nat_r32(mesh + 0xf4);
  for (int k = 0; k < 6; k++) cur[k] = nat_r32(mesh + 0xdc + 4u * (uint32_t)k);
  qwc = nat_spr32(0xd44); cap = nat_spr32(0xd48); end = nat_spr32(0xd54); blk = nat_spr32(0xd34); fog = nat_spr32(0xc70);
  if (stub) {
    key = dest + 16 * qwc; kq = key & (GUEST_SIZE - 1) & ~15u;
    if (kq < s->last_key) NP6_FB(N6_FB_ORDER);
    if ((int32_t)end - (int32_t)(dest + 16u * (qwc + s->vq + 13u * (uint32_t)ng)) < NAT_END_MARGIN) NP6_FB(N6_FB_END);
    for (i = 0; i < ng; i++) {   /* the counts first: nothing is committed (the memo) before the call is known to be taken */
      uint32_t dd = desc + 32u * (uint32_t)i;
      for (int k = 0; k < 6; k++) { uint32_t n = nat_r8(dd + 14 + (uint32_t)k); if (n > ((k >= 2 && k <= 4) ? 64u : 128u)) NP6_FB(N6_FB_OTHER); }
    }
  }
  if (stub) { half = np6_half; memcpy(memo, np6_memo, sizeof memo); }
  else np6_game_memo(memo, &half);   /* (the shadow arms: the game's own memo, as the builder that runs after sees it) */
  for (i = 0; i < ng; i++) {
    uint32_t dd = desc + 32u * (uint32_t)i, d0 = nat_r32(dd), m = i < 2 ? 13u : 12u, idx;
    g = s->g ? &s->g[s->n + (uint32_t)i] : &tmp;
    half = 1u - half;
    for (int k = 0; k < 6; k++) {
      uint32_t n = nat_r8(dd + 14 + (uint32_t)k);
      if (cur[k] && n) { g->s[k] = cur[k]; g->n[k] = (uint8_t)n; cur[k] += 16 * n; } else { g->s[k] = 0; g->n[k] = 0; }
    }
    for (int j = 0; j < 3; j++) {
      uint32_t mt = nat_r32(dd + 20 + 4u * (uint32_t)j);
      if (mt && mt != memo[half][j]) { g->mat[j] = mt; memo[half][j] = mt; } else g->mat[j] = 0;
    }
    g->hdr[0] = nat_r8(dd + 12); g->hdr[1] = d0; g->hdr[2] = fog; g->hdr[3] = clip; g->blk = blk;
    idx = (d0 & 1) + (cur[5] ? 2u : 0u);
    g->mscal = (uint32_t)(int32_t)(int16_t)nat_r16(0x70000c7cu + 2 * idx) | 0x14000000u;
    g->flags = i < 2 ? NP6G_FULL : 0; g->pad = 0;
    if (stub) g->key = key;
#ifndef __psp__
    else {   /* shadow: the group's own packet address, as the builder places it (the flush moves the destination by the quadwords before) */
      if ((int32_t)end < (int32_t)(dest + qwc)) { g->flags |= 0x80; g->key = dest + 16 * qwc; continue; }   /* skipped (never seen): left out below */
      if ((int32_t)cap < (int32_t)(qwc + m)) { dest += 16 * qwc; qwc = 0; }
      g->key = dest + 16 * qwc; g->flags |= NP6G_PKT; qwc += m;
    }
#endif
    vq += m;
  }
  if (stub) { np6_half = half; memcpy(np6_memo, memo, sizeof memo); }
#ifndef __psp__
  if (!stub && s->g) { int o = 0; for (i = 0; i < ng; i++) if (!(s->g[s->n + (uint32_t)i].flags & 0x80)) s->g[s->n + (uint32_t)o++] = s->g[s->n + (uint32_t)i]; ng = o; }
  np6_x86_rec(b, s->n, ng, mesh);
#endif
  if (!s->g) g_np6_c[N6_NOMEM]++;
  else if (stub && xc_nge_ref6[b] && s->n + (uint32_t)ng <= NGE_P6REF) { if (np6_nat) nge6_rec(b, s->n, ng, mesh); else memset(xc_nge_ref6[b] + s->n, 0, 4u * (uint32_t)ng); }   /* step 2b */
  s->n += (uint32_t)ng; s->vq += vq; if (stub) s->last_key = key & (GUEST_SIZE - 1) & ~15u;
  g_np6_c[N6_REC]++; g_np6_c[N6_GRP] += ng; if ((long)s->n > g_np6_c[N6_GRP_MAX]) g_np6_c[N6_GRP_MAX] = (long)s->n;
#ifndef __psp__
  { int r = k_vblanks >= 24000; if ((long)s->n > np6_gmax[r]) { np6_gmax[r] = (long)s->n; np6_gmax_vb[r] = k_vblanks; } }
#endif
#undef NP6_FB
  return stub;
}
void P12e8b8_orig(void);
void P12e8b8(void) __attribute__((section(".text.nat_P12e8b8")));
void P12e8b8(void)
{
  if (__builtin_expect(!np6_set, 0)) np6_setup();
  if (g_np6_cfg && g_zt && !g_dg_lite && rt_nat_p06()) return;
  P12e8b8_orig();
  if (np6_resync) { np6_resync = 0; np6_game_memo(np6_memo, &np6_half); }
}
void P12e790_orig(void);
void P12e790(void) __attribute__((section(".text.nat_P12e790")));
void P12e790(void)
{
  uint32_t a = (uint32_t)R[4];
  P12e790_orig();
  np6_tpl(a);
}
