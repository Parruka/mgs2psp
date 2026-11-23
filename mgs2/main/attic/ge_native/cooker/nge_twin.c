/* nge_twin.c (x86 twin only; -DNGE_TWIN): the step 0 harness of the native GE renderer's cooker (nge_cook.c, nge_list.c).
   MGS2_NGE=1.  Reads only: nothing here writes guest memory or changes what the twin converts or draws (the state hashes stay).
     nge_stub   (boot/nat_stub.h, at each 0x1214a0 call the stub records): the mesh is cooked (or its cook reused when nge_mesh_same),
                the per-mesh transform computed from the constant block (nge_xf_block), each group's record kept by its NatGrp;
     nge_conv   (ge/xconv.c, each P04+df0 call): today's input to the batch (vc_df0_rec / vc_df0, the same bytes as the batch) against the
                cooked group: vertices word by word, strips (the cooked list's VADDR / PRIM words), the transform's 14 floats, the cut;
                a differing group cooked again from today's memory: equal then = STALE (the cook is older than the data), else FORMAT;
     nge_draw   (ge/gelist.c xf_draw): the depth-pair arms (MGS2_GL_DZ) of ship batches;
     nge_relight (boot/nat_stub.h's P12cd78 wrapper): the static lighting's rewrites of the meshes' colour streams; the meshes it changed
                are cooked again at their next stub call (the design's hook);
     the census: at the first stub call of each vblank the draw queue (chanl 0x1df950; re/dg_objects.md) walked as gen/census.py does.
   "nge:" lines every 1000 vblanks and at the exit.  Game data stays in this process. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "nge_cook.h"
#include "nge_list.h"
#include "vu1conv.h"
#include "xlate.h"
#include "replay_fmt.h"
#include "natsnap.h"

extern uint8_t g_mem[];
const NatGrp *gl_ng_g;   /* the group being walked (boot/nat_x86.c, around nat_p04_group) */
extern long gl_rp_vb;
int xc_nge_off(const float *w, const int16_t *box, const VcXform *x, const void *d);   /* ge/xconv.c (NGE_TWIN) */

static int on = -1;
static NgeArena A, T;   /* the cooked arena; a scratch one for the fresh cooks */
static uint32_t A_gen = 1;
#define GE_FAKE 0x0a000000u
#define ACAP (24u << 20)

/* ---- counts ---- */
enum {
  /* stub side */
  S_CALLS, S_GROUPS, S_DF0, S_INPL, S_PKTSTREAM, S_COOK, S_REUSE, S_RECOOK_PTR, S_RECOOK_LIGHT, S_RECOOK_STALE, S_RESET, S_E_OVR, S_E_BAD, S_E_ROOM,
  S_BLK_NE, S_BLK_BAD, S_NOARR,
  /* conversion side */
  C_CALLS, C_NOGRP, C_UNMATCHED, C_GONE, C_NOTCOOKED, C_GRP_NOSTREAM, C_GRP_BIG, C_EN_NE_DN, C_CMP, C_KEPT, C_CUT, C_NEG,
  D_NV, D_NEGUV, D_POS, D_UV, D_RGB, D_ALPHA, D_STRIP, D_WORLD, D_WORLD_LATE, D_WORLD_FORM, D_PROJ, D_CUT, D_BLOB, D_ANY,
  D_STALE, D_FORMAT, D_STALE_V,
  K_FOG, K_VP, K_ALPHA, K_PICS,
  W_TODAY, W_NATIVE, W_LIST, W_BATCH, W_MESHW,
  Z_X, Z_Y, Z_W, Z_B, Z_O, Z_MISS,
  L_CALLS, L_CHG_CALLS, L_CHG_BYTES, L_MESH_CHG, L_COOKED_CHG, L_VB,
  G_PRE24, G_W00A, G_W01A, G_REC, G_LANE0, G_LANE1, G_LANEX, C_NOXF, B_SAME24, B_SPR, B_OTHER,
  NCNT };
static const char *const CN[NCNT] = {
  "stub_calls", "stub_groups", "df0_groups", "inpl_groups", "pkt_stream_groups", "cooks", "reuse", "recook_ptr", "recook_light", "recook_stale", "arena_resets",
  "e_ovr", "e_bad", "e_room", "blk_ne_mesh_fc", "blk_bad", "no_array",
  "conv_calls", "no_group", "unmatched", "gone", "not_cooked", "grp_nostream", "grp_big", "en_ne_dn", "compared", "kept", "cut", "neg_count",
  "d_nv", "d_neguv", "d_pos", "d_uv", "d_rgb", "d_alpha", "d_strip", "d_world", "d_world_late", "d_world_form", "d_proj", "d_cut", "d_blob", "d_any",
  "d_stale", "d_format", "d_stale_verts",
  "k_fog", "k_vp", "k_alpha", "k_pics",
  "w_today", "w_native", "w_list", "w_batches", "w_mesh_world",
  "z_x", "z_y", "z_w", "z_b", "z_o", "z_miss",
  "l_calls", "l_chg_calls", "l_chg_bytes", "l_mesh_chg", "l_cooked_chg", "l_vblanks",
  "nogrp_vb_lt24000", "nogrp_w00a", "nogrp_w01a", "nogrp_rec", "nogrp_lane0", "nogrp_lane1", "nogrp_lane_other", "compared_no_xf",
  "blk_same_low25", "blk_spr", "blk_other" };
static long N[NCNT], N0[NCNT];
static uint32_t ms_e[16]; static long ms_n[16];   /* stub groups by their VU1 entry (MSCAL halfword) */
static uint32_t blk_ex[4][2]; static int blk_exn;   /* a few (mesh, blk) pairs */

/* ---- cooked meshes by guest address ---- */
#define MT_N 16384
typedef struct { uint32_t mesh, off, gen, stale; } Mt;
static Mt mt[MT_N];
static Mt *mt_get(uint32_t mesh, int add)
{
  uint32_t h = (mesh * 0x9e3779b1u) >> 18;
  for (int k = 0; k < MT_N; k++, h = (h + 1) & (MT_N - 1)) {
    Mt *e = &mt[h];
    if (e->mesh == mesh) return e;
    if (!e->mesh) { if (!add) return 0; e->mesh = mesh; e->off = 0; e->gen = 0; e->stale = 0; return e; }
  }
  return 0;
}
/* ---- groups by their NatGrp (side records) ---- */
#define SD_N 32768
typedef struct { const NatGrp *p; NatGrp copy; uint32_t mesh, moff, gen, blk; uint16_t gi; uint8_t xok, df0; NgeXf xf; } Sd;
static Sd sd[SD_N];
static Sd *sd_get(const NatGrp *p, int add)
{
  uint32_t h = (uint32_t)(((uintptr_t)p >> 4) * 0x9e3779b1u) >> 17;
  for (int k = 0; k < SD_N; k++, h = (h + 1) & (SD_N - 1)) {
    Sd *e = &sd[h];
    if (e->p == p) return e;
    if (!e->p) { if (!add) return 0; e->p = p; return e; }
  }
  return 0;
}
/* ---- ship batches by their GerXform (the depth-pair arms) ---- */
#define SB_N 16384
static const void *sb[SB_N];
static void sb_put(const void *x) { uint32_t h = (uint32_t)(((uintptr_t)x >> 4) * 0x9e3779b1u) >> 18; sb[h] = x; }
static int sb_has(const void *x) { uint32_t h = (uint32_t)(((uintptr_t)x >> 4) * 0x9e3779b1u) >> 18; return sb[h] == x; }

/* ---- the census ---- */
enum { CE_OBJ, CE_OBJ_DRAWN, CE_P04_MESH, CE_P04_MESH_DRAWN, CE_P04_PRE, CE_P04_LIT, CE_P04_GRP, CE_P04_V, CE_P04_V_DRAWN, CE_P06_MESH,
       CE_P06_V, CE_DEFS, CE_JOINTS, CE_JOINT_MAX, CE_PAGES, CE_PAGECOUNT, CE_MATS, CE_COOK_ALL, CE_COOK_DRAWN, CE_STUB_MESH, CE_STUB_GRP,
       CE_STUB_V, CE_STUB_COOK, CE_COOK_PRE, CE_COOK_LIT, CE_COOK_LIT_MODEL, CE_COOK_PRE_DRAWN, CE_COOK_LIT_DRAWN, CE_N };
static const char *const CEN[CE_N] = { "objects", "objects_drawn", "p04_meshes", "p04_meshes_drawn", "p04_prelit_meshes", "p04_lit_meshes",
  "p04_groups", "p04_verts", "p04_verts_drawn", "p06_meshes", "p06_verts", "models", "joints_sum", "joints_max", "model_pages", "page_count",
  "materials", "cooked_all", "cooked_drawn", "stub_meshes", "stub_groups", "stub_verts", "stub_cooked", "cooked_prelit", "cooked_lit", "cooked_lit_per_model", "cooked_prelit_drawn", "cooked_lit_drawn" };
typedef struct { long pics; long max[CE_N]; double sum[CE_N]; } CeAgg;
static CeAgg agg[2];   /* 0: w00a (vb < 31400), 1: the rest */
static long cur[CE_N];
static long cur_vb = -1;
/* the stage's distinct meshes (every one the queue held, and every one stubbed) and their cooked bytes */
#define DM_N 65536
typedef struct { uint32_t mesh, bytes, nv, ng, stubbed; } Dm;
static Dm dm[2][DM_N]; static long dm_n[2];
static Dm *dm_get(int st, uint32_t mesh)
{
  uint32_t h = (mesh * 0x9e3779b1u) >> 16;
  for (int k = 0; k < DM_N; k++, h = (h + 1) & (DM_N - 1)) {
    Dm *e = &dm[st][h];
    if (e->mesh == mesh) return e;
    if (!e->mesh) { e->mesh = mesh; dm_n[st]++; return e; }
  }
  return 0;
}
static inline uint32_t r32(const uint8_t *ram, uint32_t a) { uint32_t v; memcpy(&v, ram + (a & 0x1fffffcu), 4); return v; }
static inline int r16s(const uint8_t *ram, uint32_t a) { int16_t v; memcpy(&v, ram + (a & 0x1fffffeu), 2); return v; }
static inline int okp(uint32_t p) { return (p >> 28) < 4 && (p & 0x0fffffffu) >= 0x100000u && (p & 0x0fffffffu) < 0x2000000u; }
static void pic_end(void)
{
  if (cur_vb < 0) return;
  CeAgg *g = &agg[cur_vb >= 31400];
  g->pics++;
  for (int k = 0; k < CE_N; k++) { if (cur[k] > g->max[k]) g->max[k] = cur[k]; g->sum[k] += (double)cur[k]; }
}
static void census(const uint8_t *ram, long vb)
{
  memset(cur, 0, sizeof cur);
  const int st = vb >= 31400;
  const uint32_t CH = 0x1df950u, gm = r32(ram, CH + 0x288), Q = r32(ram, CH + 0x28c);
  if (!okp(Q)) return;
  const uint32_t cnt = r32(ram, Q + 4), arr = r32(ram, Q + 8);
  if (!okp(arr) || cnt > 4096) return;
  enum { DN = 1024 }; static uint32_t defs[DN], pages[DN], mats[8192]; static uint64_t lk[8192]; int nd = 0, np = 0, nm = 0, nlk = 0;
  for (uint32_t i = 0; i < cnt; i++) {
    const uint32_t o = r32(ram, arr + 4 * i);
    if (!okp(o)) continue;
    const uint32_t fl = r32(ram, o + 0x4c), gg = r32(ram, o + 0x50), d = r32(ram, o + 0x44); const int n = r16s(ram, o + 0x58), cull = r16s(ram, o + 0x5c);
    if (!okp(d) || n < 0 || n > 512) continue;
    cur[CE_OBJ]++;
    const int drawn_o = (gg & gm) && !(fl & 0x0a000080u) && !(fl & 0x1000u) && cull != 2, p06 = (fl & 0x10000u) != 0;
    cur[CE_OBJ_DRAWN] += drawn_o;
    int j; for (j = 0; j < nd && defs[j] != d; j++) {}
    if (j == nd && nd < DN) {
      defs[nd++] = d; const uint32_t jn = r32(ram, d + 4), pg = r32(ram, d + 0xc);
      if (jn < 4096) { cur[CE_JOINTS] += jn; if ((long)jn > cur[CE_JOINT_MAX]) cur[CE_JOINT_MAX] = jn; }
      int q; for (q = 0; q < np && pages[q] != pg; q++) {} if (q == np && np < DN) pages[np++] = pg;
    }
    for (int k = 0; k < n; k++) {
      const uint32_t m = o + 0xc0 + 0x130u * (uint32_t)k, mfl = r32(ram, m + 0xc0), desc = r32(ram, m + 0xf4); const int mc = r16s(ram, m + 0xc8), ng = r16s(ram, m + 0xcc);
      const int drawn = drawn_o && !(mfl & 0x1000u) && !(mc & 2);
      if (ng <= 0 || ng > 1024 || !okp(desc)) continue;
      uint32_t v = 0; for (int g = 0; g < ng; g++) v += ram[(desc + 32u * (uint32_t)g + 12) & 0x1ffffffu];
      if (p06) { cur[CE_P06_MESH]++; cur[CE_P06_V] += v; continue; }
      cur[CE_P04_MESH]++; cur[CE_P04_MESH_DRAWN] += drawn; cur[(mfl & 1) ? CE_P04_PRE : CE_P04_LIT]++; cur[CE_P04_GRP] += ng; cur[CE_P04_V] += v;
      if (drawn) cur[CE_P04_V_DRAWN] += v;
      for (int g = 0; g < ng; g++) {
        const uint32_t ma = r32(ram, desc + 32u * (uint32_t)g + 20); int q; for (q = 0; q < nm && mats[q] != ma; q++) {}
        if (q == nm && nm < 8192) mats[nm++] = ma;
      }
      uint32_t nv, ngr, nlit; const uint32_t b = nge_mesh_bytes(ram, m, &nv, &ngr, &nlit);
      cur[CE_COOK_ALL] += b; if (drawn) cur[CE_COOK_DRAWN] += b;
      if (mfl & 1) { cur[CE_COOK_PRE] += b; if (drawn) cur[CE_COOK_PRE_DRAWN] += b; }
      else { cur[CE_COOK_LIT] += b; if (drawn) cur[CE_COOK_LIT_DRAWN] += b;
        const uint64_t key = (uint64_t)d << 16 | (uint32_t)k; int q; for (q = 0; q < nlk && lk[q] != key; q++) {}
        if (q == nlk && nlk < 8192) { lk[nlk++] = key; cur[CE_COOK_LIT_MODEL] += b; } }
      Dm *e = dm_get(st, m); if (e) { e->bytes = b; e->nv = v; e->ng = (uint32_t)ng; }
    }
  }
  cur[CE_DEFS] = nd; cur[CE_PAGES] = np; cur[CE_MATS] = nm; cur[CE_PAGECOUNT] = r32(ram, 0x18e8f0);
}

static void line(long vb, int last)
{
  fprintf(stderr, "nge: vb %ld%s:", vb, last ? " (exit)" : "");
  for (int k = 0; k < NCNT; k++) if (N[k] - N0[k]) fprintf(stderr, " %s=%ld", CN[k], N[k] - N0[k]);
  fprintf(stderr, " | arena used %u peak %u\n", A.used, A.peak);
  memcpy(N0, N, sizeof N);
}
static void fin(void)
{
  pic_end(); cur_vb = -1;
  line(gl_rp_vb, 1);
  fprintf(stderr, "nge: totals:"); for (int k = 0; k < NCNT; k++) fprintf(stderr, " %s=%ld", CN[k], N[k]); fputc('\n', stderr);
  for (int s = 0; s < 2; s++) {
    if (!agg[s].pics) continue;
    fprintf(stderr, "nge: census %s pictures %ld:", s ? "w01a+" : "w00a", agg[s].pics);
    for (int k = 0; k < CE_N; k++) fprintf(stderr, " %s=%ld/%.1f", CEN[k], agg[s].max[k], agg[s].sum[k] / (double)agg[s].pics);
    fputc('\n', stderr);
    double b = 0, bs = 0, v = 0, vs = 0; long n = 0, ns = 0;
    for (int i = 0; i < DM_N; i++) { const Dm *e = &dm[s][i]; if (!e->mesh) continue; n++; b += e->bytes; v += e->nv; if (e->stubbed) { ns++; bs += e->bytes; vs += e->nv; } }
    fprintf(stderr, "nge: stage %s distinct meshes %ld (verts %.0f, cooked %.0f B); stubbed %ld (verts %.0f, cooked %.0f B)\n", s ? "w01a+" : "w00a", n, v, b, ns, vs, bs);
  }
  fprintf(stderr, "nge: arena used %u peak %u cap %u resets %ld\n", A.used, A.peak, A.cap, N[S_RESET]);
  fprintf(stderr, "nge: stub groups by entry:"); for (int k = 0; k < 16 && ms_e[k]; k++) fprintf(stderr, " %04x=%ld", ms_e[k], ms_n[k]); fputc('\n', stderr);
  fprintf(stderr, "nge: blk examples:"); for (int k = 0; k < blk_exn; k++) fprintf(stderr, " mesh %08x blk %08x;", blk_ex[k][0], blk_ex[k][1]); fputc('\n', stderr);
}
static int setup(void)
{
  if (on >= 0) return on;
  const char *e = getenv("MGS2_NGE"); on = e ? atoi(e) : 0;
  if (!on) return 0;
  void *m = 0, *t = 0;
  if (posix_memalign(&m, 64, ACAP) || posix_memalign(&t, 64, 1u << 20)) { on = 0; return 0; }
  nge_arena_init(&A, m, ACAP, GE_FAKE); nge_arena_init(&T, t, 1u << 20, GE_FAKE + 0x2000000u);
  atexit(fin);
  return on;
}
static void tick(const uint8_t *ram, long vb)
{
  if (vb == cur_vb) return;
  pic_end();
  static long vb0 = -1;
  if (vb0 < 0) vb0 = vb - vb % 1000;
  if (vb >= vb0 + 1000) { line(vb, 0); vb0 = vb - vb % 1000; }
  cur_vb = vb; if (ram) census(ram, vb);
}

/* the stub: mesh's groups g[0 .. ng - 1] (0: no array), blk the constant block (SPR 0xc54) */
void nge_stub(const uint8_t *ram, uint32_t mesh, const NatGrp *g, int ng, uint32_t blk, long vb);
void nge_stub(const uint8_t *ram, uint32_t mesh, const NatGrp *g, int ng, uint32_t blk, long vb)
{
  if (!setup()) return;
  tick(ram, vb);
  N[S_CALLS]++; N[S_GROUPS] += ng;
  if (!g) { N[S_NOARR]++; return; }
  if (blk != mesh + 0xfc) N[S_BLK_NE]++;
  if (blk_exn < 4 && (blk ^ (mesh + 0xfc)) & 0x1ffffffu) { blk_ex[blk_exn][0] = mesh; blk_ex[blk_exn][1] = blk; blk_exn++; }
  if (!((blk ^ (mesh + 0xfc)) & 0x1ffffffu)) N[B_SAME24]++; else if ((blk >> 28) == 7) N[B_SPR]++; else N[B_OTHER]++;
  for (int i = 0; i < ng && g; i++) { const uint32_t e = g[i].mscal & 0xffffu; int k; for (k = 0; k < 16 && ms_e[k] && ms_e[k] != e; k++) {} if (k < 16) { ms_e[k] = e; ms_n[k]++; } }
  Mt *t = mt_get(mesh, 1); if (!t) return;
  uint32_t off = 0; int rc = NGE_OK;
  if (t->off && t->gen == A_gen && !t->stale && nge_mesh_same(nge_mesh_at(&A, t->off), ram)) { off = t->off; N[S_REUSE]++; }
  else {
    if (t->off && t->gen == A_gen) { N[t->stale == 1 ? S_RECOOK_LIGHT : t->stale ? S_RECOOK_STALE : S_RECOOK_PTR]++; nge_mesh_at(&A, t->off)->live = 0; }
    static uint32_t alpha = 0xff;   /* the vertex alpha: the last one a ship call had (nge_conv) */
    extern uint32_t nge_alpha_seen; if (nge_alpha_seen) alpha = nge_alpha_seen & 0xff;
    rc = nge_cook_mesh(&A, ram, mesh, alpha, &off);
    if (rc == NGE_E_ROOM) {   /* the arena full: everything cooked again from here (counted) */
      N[S_RESET]++; nge_arena_reset(&A); A_gen++; rc = nge_cook_mesh(&A, ram, mesh, alpha, &off); }
    if (rc == NGE_OK) { N[S_COOK]++; t->off = off; t->gen = A_gen; t->stale = 0; }
    else { N[rc == NGE_E_OVR ? S_E_OVR : rc == NGE_E_BAD ? S_E_BAD : S_E_ROOM]++; t->off = 0; off = 0; }
  }
  NgeXf xf; const int xok = nge_xf_block(ram, blk, &xf);
  if (!xok) N[S_BLK_BAD]++;
  const NgeMesh *m = off ? nge_mesh_at(&A, off) : 0;
  long v = 0;
  for (int i = 0; i < ng; i++) {
    Sd *s = sd_get(&g[i], 1); if (!s) continue;
    s->copy = g[i]; s->mesh = mesh; s->moff = off; s->gen = A_gen; s->blk = blk; s->gi = (uint16_t)i; s->xok = (uint8_t)xok; s->xf = xf;
    s->df0 = g[i].mscal == NAT_DF0_MSCAL;
    N[S_DF0] += s->df0; N[S_INPL] += (g[i].flags & NATG_INPL) != 0;
    if (s->df0 && !(g[i].flags & NATG_INPL)) N[S_PKTSTREAM]++;
    v += g[i].hdr[0];
  }
  cur[CE_STUB_MESH]++; cur[CE_STUB_GRP] += ng; cur[CE_STUB_V] += v; if (m) cur[CE_STUB_COOK] += m->bytes;
  Dm *e = dm_get(vb >= 31400, mesh); if (e) { e->stubbed = 1; if (m) e->bytes = m->bytes; if (!e->nv) { e->nv = (uint32_t)v; e->ng = (uint32_t)ng; } }
}

uint32_t nge_alpha_seen;
static uint32_t a2(int a) { return a >= 0x80 ? 0xffu : (uint32_t)(a < 0 ? 0 : a * 2); }   /* ge/vu1conv.c vc_a2 */
/* the cooked list's strips: (first vertex, count) from its VADDR / PRIM words */
static int list_runs(const NgeArena *a, const NgeGroup *q, uint16_t *runs, int max)
{
  const uint32_t *w = (const uint32_t *)(const void *)(a->mem + q->list), vs = q->kind == NGE_K_PRE ? 16u : q->kind == NGE_K_LIT ? 20u : 24u;
  uint32_t base = 0, at = 0; int n = 0;
  for (uint32_t k = 0; k < q->nw; k++) {
    const uint32_t c = w[k] >> 24, p = w[k] & 0xffffffu;
    if (c == NGE_C_BASE) base = (p & 0xf0000u) << 8;
    else if (c == NGE_C_VADDR) at = ((base | p) - (a->ge + q->vb)) / vs;
    else if (c == NGE_C_PRIM) { if (n == max) return -1; runs[2 * n] = (uint16_t)at; runs[2 * n + 1] = (uint16_t)(p & 0xffff); n++; at += p & 0xffff; }
    else if (c == NGE_C_RET) break;
  }
  return n;
}
/* which parts of n cooked vertices differ from today's: bit 0 positions, 1 UVs, 2 colours, 3 alpha; *nd vertices that differ */
static int vdiff(const uint8_t *ck, const VcVert *ev, int n, long *nd)
{
  int f = 0;
  for (int i = 0; i < n; i++) {
    uint32_t a[4], b[4]; memcpy(a, ck + 16 * i, 16); memcpy(b, &ev[i], 16);
    int g = 0;
    if (a[2] != b[2] || a[3] != b[3]) g |= 1;
    if (a[0] != b[0]) g |= 2;
    if ((a[1] ^ b[1]) & 0xffffffu) g |= 4;
    if ((a[1] ^ b[1]) >> 24) g |= 8;
    if (g && nd) (*nd)++;
    f |= g;
  }
  return f;
}
static struct { long vb; int have; float fog_a, fog_b, vs[3], vo[3]; int alpha; float lastw[12]; int lastw_ok; } P;

void nge_conv(const XlCall *c, const void *d, const VcXform *Dx, int Dn, int cut_on, int off, int r, const GerBatch *b, const uint8_t *blob);
void nge_conv(const XlCall *c, const void *d, const VcXform *Dx, int Dn, int cut_on, int off, int r, const GerBatch *b, const uint8_t *blob)
{
  if (!setup()) return;
  const long vb = gl_rp_vb;
  N[C_CALLS]++;
  static VcVert Ev[1024]; static uint16_t Efl[1024], er[2048], cr[2048]; VcXform Ex;
  const int En = c->rec ? vc_df0_rec(c->vm, c->top, c->rec->p, c->rec->kind, c->rec->row, &Ex, Ev, Efl, 1024) : vc_df0(c->vm, c->top, &Ex, Ev, Efl, 1024);
  nge_alpha_seen = 0x100u | a2(Ex.alpha);
  if (En != Dn) N[C_EN_NE_DN]++;
  /* the abs constants: one value a picture? */
  if (P.vb != vb) { P.vb = vb; P.have = 0; P.lastw_ok = 0; N[K_PICS]++; }
  if (!P.have) { P.have = 1; P.fog_a = Ex.fog_a; P.fog_b = Ex.fog_b; memcpy(P.vs, Ex.vs, 12); memcpy(P.vo, Ex.vo, 12); P.alpha = Ex.alpha; }
  else {
    if (memcmp(&P.fog_a, &Ex.fog_a, 4) || memcmp(&P.fog_b, &Ex.fog_b, 4)) N[K_FOG]++;
    if (memcmp(P.vs, Ex.vs, 12) || memcmp(P.vo, Ex.vo, 12)) N[K_VP]++;
    if (P.alpha != Ex.alpha) N[K_ALPHA]++;
  }
  const NatGrp *g = gl_ng_g;
  if (!g) { N[C_NOGRP]++; N[vb < 24000 ? G_PRE24 : vb < 31400 ? G_W00A : G_W01A]++; N[G_REC] += c->rec != 0; N[c->lane == 0 ? G_LANE0 : c->lane == 1 ? G_LANE1 : G_LANEX]++; return; }
  Sd *s = sd_get(g, 0);
  if (!s || memcmp(&s->copy, g, sizeof *g)) { N[C_UNMATCHED]++; return; }
  if (!s->moff) { N[C_NOTCOOKED]++; return; }
  if (s->gen != A_gen) { N[C_GONE]++; return; }
  const NgeMesh *m = nge_mesh_at(&A, s->moff);
  if (s->gi >= m->ng) { N[C_UNMATCHED]++; return; }
  const NgeGroup *q = nge_groups(&A, m) + s->gi;
  N[C_CMP]++;
  if (off) N[C_CUT]++;
  if (r > 0) N[C_KEPT]++;
  int any = 0;
  if (En < 0) {
    N[C_NEG]++;
    if ((En == -3) != ((q->flags & NGE_GF_NEGUV) != 0)) { N[D_NEGUV]++; any = 1; }
  } else {
    if (q->flags & NGE_GF_NOSTREAM) { N[C_GRP_NOSTREAM]++; return; }
    if (q->flags & NGE_GF_BIG) { N[C_GRP_BIG]++; return; }
    if (q->nv != En) { N[D_NV]++; any = 1; }
    else {
      long nd = 0; const int f = En ? vdiff(A.mem + q->vb, Ev, En, &nd) : 0;
      if (f & 1) N[D_POS]++;
      if (f & 2) N[D_UV]++;
      if (f & 4) N[D_RGB]++;
      if (f & 8) N[D_ALPHA]++;
      const int ne = vc_strips(Efl, En, er, 1024), nc = q->list ? list_runs(&A, q, cr, 1024) : 0;
      const int ds = ne != nc || memcmp(er, cr, 4u * (uint32_t)(ne > 0 ? ne : 0));
      if (ds) N[D_STRIP]++;
      if (f || ds) {
        any = 1;
        /* cooked again from today's memory: STALE when that matches */
        nge_arena_reset(&T); uint32_t to = 0; int ok = 0;
        if (nge_cook_mesh(&T, g_mem, s->mesh, a2(Ex.alpha), &to) == NGE_OK) {
          const NgeMesh *tm = nge_mesh_at(&T, to);
          if (s->gi < tm->ng) {
            const NgeGroup *tq = nge_groups(&T, tm) + s->gi; uint16_t tr[2048];
            const int tn = tq->list ? list_runs(&T, tq, tr, 1024) : 0;
            ok = tq->nv == En && !(tq->flags & (NGE_GF_NOSTREAM | NGE_GF_BIG)) && !vdiff(T.mem + tq->vb, Ev, En, 0) && tn == ne && !memcmp(tr, er, 4u * (uint32_t)(ne > 0 ? ne : 0));
          }
        }
        if (ok) { N[D_STALE]++; N[D_STALE_V] += nd; Mt *t = mt_get(s->mesh, 0); if (t && t->gen == A_gen && !t->stale) t->stale = 2; }   /* cooked again at its next stub call */
        else N[D_FORMAT]++;
      }
    }
  }
  /* the transform */
  if (!s->xok) N[C_NOXF]++;
  if (s->xok) {
    const int dw = memcmp(s->xf.world, Ex.world, sizeof Ex.world) != 0, dp = memcmp(&s->xf.pa, &Ex.pa, 4) || memcmp(&s->xf.pb, &Ex.pb, 4);
    if (dw) {
      N[D_WORLD]++; any = 1;
      NgeXf now; if (nge_xf_block(g_mem, s->blk, &now) && !memcmp(now.world, Ex.world, sizeof Ex.world)) N[D_WORLD_LATE]++; else N[D_WORLD_FORM]++;
    }
    if (dp) { N[D_PROJ]++; any = 1; }
    /* the cut by the cooked box and the stub's transform (the viewport and scissor as today's) */
    if (cut_on && En > 0 && d) { const int no = xc_nge_off(s->xf.world, q->box, Dx, d); if (no != off) { N[D_CUT]++; any = 1; } }
  }
  if (r > 0 && b && blob && b->kind == GERK_XFORM) {   /* the batch itself against the reference (sanity) and the words */
    const GerXform *gx = (const GerXform *)(const void *)blob; const VcVert *bv = (const VcVert *)(gx + 1); const uint16_t *br = (const uint16_t *)(bv + gx->nv);
    if ((int)gx->nv != En || memcmp(bv, Ev, 16u * gx->nv) || memcmp(gx->world, Ex.world, sizeof Ex.world)) N[D_BLOB]++;
    sb_put(gx);
    const int passes = b->twopass ? 2 : 1; uint32_t at = 0xffffffffu, w = 0;
    for (uint32_t k = 0; k < gx->nst; k++) { if (br[2 * k] != at) w++; w++; at = (uint32_t)br[2 * k] + br[2 * k + 1]; }
    N[W_TODAY] += 13 + (long)w * passes; N[W_BATCH]++;
    if (!P.lastw_ok || memcmp(P.lastw, s->xf.world, sizeof P.lastw)) { N[W_NATIVE] += 13; N[W_MESHW]++; memcpy(P.lastw, s->xf.world, sizeof P.lastw); P.lastw_ok = 1; }
    N[W_NATIVE] += 3L * passes; N[W_LIST] += (long)q->nw * passes;
  }
  if (any) N[D_ANY]++;
}

void nge_draw(const void *x, int dz);
void nge_draw(const void *x, int dz)
{
  if (on <= 0) return;
  if (!sb_has(x)) { if (dz == 1 || dz == 2) N[Z_MISS]++; return; }
  N[dz == 1 ? Z_X : dz == 2 ? Z_Y : dz == 3 ? Z_W : dz == 4 ? Z_B : Z_O]++;
}

/* the static lighting (0x12cd78, obj = R[4]): phase 0 before, 1 after.  Each mesh's stream B (its colours) compared */
void nge_relight(const uint8_t *ram, uint32_t obj, int phase, long vb);
void nge_relight(const uint8_t *ram, uint32_t obj, int phase, long vb)
{
  if (!setup()) return;
  enum { CAP = 1u << 20 }; static uint8_t snap[CAP]; static long lvb = -1;
  if (!okp(obj)) return;
  const int n = r16s(ram, obj + 0x58);
  if (n <= 0 || n > 512) return;
  if (!phase) { N[L_CALLS]++; if (vb != lvb) { N[L_VB]++; lvb = vb; } }
  uint32_t o = 0; long chg = 0; int mchg = 0;
  for (int k = 0; k < n; k++) {
    const uint32_t m = obj + 0xc0 + 0x130u * (uint32_t)k, desc = r32(ram, m + 0xf4), B = r32(ram, m + 0xe0); const int ng = r16s(ram, m + 0xcc);
    if (ng <= 0 || ng > 1024 || !okp(desc) || !okp(B)) continue;
    uint32_t len = 0; for (int g = 0; g < ng; g++) len += 16u * ram[(desc + 32u * (uint32_t)g + 15) & 0x1ffffffu];
    if (!len || (B & 0x1ffffffu) + len > 0x2000000u || o + len > CAP) continue;
    const uint8_t *p = ram + (B & 0x1ffffffu);
    if (!phase) { memcpy(snap + o, p, len); }
    else {
      long c = 0; for (uint32_t i = 0; i < len; i++) c += snap[o + i] != p[i];
      if (c) { chg += c; mchg++; Mt *t = mt_get(m, 0); if (t && t->off && t->gen == A_gen) { t->stale = 1; N[L_COOKED_CHG]++; } }
    }
    o += len;
  }
  if (phase) { if (chg) N[L_CHG_CALLS]++; N[L_CHG_BYTES] += chg; N[L_MESH_CHG] += mchg; }
}
