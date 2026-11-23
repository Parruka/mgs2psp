/* nge_stub.h: native step 1 (MGS2_NGE; attic/ge_native/step1_results.md), the main CPU's part, included by nat_stub.h (one text for the
   PSP and the x86 twin, as nat_stub.h).  In rt_nat_p04, after a stubbed call's groups are recorded, nge_rec:
     - finds the mesh's cook (a table by the mesh's guest address) and checks it at each use (nge_mesh_same: the descriptor array, the
       three stream bases, the group count, the pre-lit bit, the descriptors' sum; and no +0xD0 override this frame); a miss, or a
       check that fails, cooks it again into the arena (host memory only);
     - computes the call's WORLD and projection terms from its constant block (SPR 0xc54) into its buffer's transform array;
     - gives each group drawn by P04+df0 with its three streams in RAM outside packet memory (NATG_INPL) and a cook that can be drawn
       (pre-lit, strips, no flag) a reference to its cooked group; every other group keeps 0: today's path.
   The arena: one or two regions the host carves from its frame memories (psp/xl_psp.c: the tail of each frame memory's arena;
   boot/xlive_chk.c the same sizes), each within one 16 MB window of the GE's address space.  Meshes are placed first fit; when nothing
   fits, the least recently used mesh not used by the last NGE_GUARD pipeline calls (and, on the PSP, not before the GE's last 3
   finishes) is evicted, until it fits or nothing can go (then today's path for the call).  A fill line for each picture (pipeline call):
   "nge f" (x86: at once; the PSP: kept in a ring and written with the turn lines).
   MGS2_NGE=1: on for the whole run (default 0: off, nothing is cooked or referenced, every path today's); MGS2_NGE_CYCLE=<digits 0/1>:
   a digit a turn by the bound turns' rule (MGS2_OPT_CYCLE's from and n, as MGS2_NAT_F1_CYCLE; past the list MGS2_NGE's); MGS2_NGE_KB=<n>:
   each region's KB (default 256); MGS2_NGE_LOG=0: no fill lines.  Nothing here writes the game's memory: the choices of the stub
   (rt_nat_p04's return) do not depend on it. */
#include "../ge/nge.h"
#include "../nge/nge_cook.c"
#include "../nge/nge_list.c"

#define NGE_NENT 768             /* entries: meshes with a cook in the arena, old cooks kept (zombies), and dead entries (re-used when the
                                    table is full); the route's twin run: about 400 meshes live at 448 KB a region, 829 cooked in all */
#define NGE_NBKT 1024
#define NGE_NBLK 512             /* blocks a region holds */
#define NGE_GUARD 8              /* pipeline calls a mesh is kept after its last use */
#define NGE_RING 128             /* the PSP's fill lines between two turn lines (a turn of 100 vblanks: about 50) */
enum { NGE_C_PICS, NGE_C_CALLS, NGE_C_CALLS_NAT, NGE_C_REUSE, NGE_C_COOK, NGE_C_RECOOK_CHG, NGE_C_RECOOK_EV, NGE_C_EVICT, NGE_C_NOROOM,
       NGE_C_BAD, NGE_C_OVR, NGE_C_NOBLK, NGE_C_XFULL, NGE_C_TFULL, NGE_C_GRP, NGE_C_GRP_NAT, NGE_C_GRP_NOTDF0, NGE_C_GRP_NOTINPL,
       NGE_C_GRP_FLAG, NGE_C_GRP_NOCALL, NGE_C_EV_HELD, NGE_C_N };
static const char *const nge_cn[NGE_C_N] = { "pictures", "calls", "calls native", "reused", "cooked", "re-cooked (changed)",
  "re-cooked (evicted before)", "evicted", "no room", "not cookable", "+0xD0 override", "constant block not in RAM", "transform array full",
  "table full", "groups", "groups native", "groups not df0 (today)", "groups df0 not in place (today)", "groups df0 with a cook flag (today)",
  "groups df0 of a call not native (today)", "evictions held by the GE" };
long g_nge_c[NGE_C_N];
typedef struct { uint32_t mesh, off, size, last, fin; int16_t next, reg; uint8_t live, seen, pad[2]; NgeMesh m; } NgeEnt;
typedef struct { uint32_t off, size; int16_t e, pad; } NgeBlk;
typedef struct { uint8_t *mem; uint32_t ge, cap, used; NgeBlk *b; int nb; } NgeReg;
static NgeEnt *nge_e; static int nge_ne, nge_nzf; static int16_t nge_bkt[NGE_NBKT], *nge_zf;
#define NGE_ZOMBIE 0xffffffffu   /* an entry that keeps a changed mesh's old cook until no list in flight can use it (not in the table) */
static NgeReg nge_rg[2]; static int nge_nrg;
int g_nge;                       /* 1: the arena is there (MGS2_NGE or MGS2_NGE_CYCLE named a 1 and the host gave the regions) */
static int g_nge_pic;            /* this pipeline call's switch */
static int nge_cfg_set, nge_all, nge_ncyc, nge_log = 1; static uint8_t nge_cyc[BD_CYC_N]; static uint32_t nge_kb = 256;
static volatile uint32_t *nge_fin;   /* the PSP: the GE's finishes (psp/xl_psp.c ge_fin_n); 0: x86 */
static uint32_t nge_xn[2], nge_xseq[2] = { ~0u, ~0u }, nge_used, nge_peak, nge_tpeak;
typedef struct { uint32_t vb, used, peak, ck, rc, ev, nr, nat, tod, xf; } NgeLine;
static NgeLine *nge_ring, nge_cur; static int nge_rn; static long nge_rlost, nge_rfull; static long nge_c0[NGE_C_N]; static int nge_pic_open;

static void nge_cfg(void)
{
  const char *e, *c = getenv("MGS2_OPT_CYCLE"); long f = -1, n = 0;
  nge_cfg_set = 1;
  nge_all = (e = getenv("MGS2_NGE")) ? atoi(e) & 1 : 0;
  if ((e = getenv("MGS2_NGE_CYCLE")) && c && sscanf(c, "%ld:%ld", &f, &n) == 2 && n > 0 && f >= 0)
    for (; nge_ncyc < BD_CYC_N && (*e == '0' || *e == '1'); e++) nge_cyc[nge_ncyc++] = (uint8_t)(*e - '0');
  if ((e = getenv("MGS2_NGE_KB")) && atoi(e) >= 64 && atoi(e) <= 4096) nge_kb = (uint32_t)atoi(e);
  if ((e = getenv("MGS2_NGE_LOG"))) nge_log = atoi(e) != 0;
}
/* the bytes of each region the host should carve (0: MGS2_NGE off in every turn) */
uint32_t nge_cfg_bytes(void)
{
  int on;
  if (!nge_cfg_set) nge_cfg();
  on = nge_all; for (int i = 0; i < nge_ncyc; i++) if (nge_cyc[i]) on = 1;
  return on ? nge_kb << 10 : 0;
}
int rt_nge_at(long vb)   /* the switch for the frame that begins at hook vb (the bound turns' rule) */
{
  if (!nge_cfg_set) nge_cfg();
  if (!bd_set) bd_setup();
  if (nge_ncyc && vb >= bd_from) { long t = (vb - bd_from) / bd_turn; if (t < nge_ncyc) return nge_cyc[t]; }
  return nge_all;
}
/* the host's: its regions (m1 0: one), their GE addresses, each region's bytes; fin: the GE's finish count (0 on x86); al: the host's
   allocator for the tables (the PSP: blocks of the system's memory outside newlib's heap, whose headroom is about 64 KB; x86: malloc),
   each block on a 64-byte line.  Three blocks, the largest first: the table (entries, zombie stack, both regions' blocks, the fill ring)
   and each packet buffer's references and transforms */
int nge_host_setup(uint8_t *m0, uint32_t ge0, uint8_t *m1, uint32_t ge1, uint32_t cap, volatile uint32_t *fin, void *(*al)(uint32_t n, const char *name))
{
  uint8_t *m[2] = { m0, m1 }; uint32_t ge[2] = { ge0, ge1 }; int n = 0;
  const uint32_t tn = (uint32_t)(NGE_NENT * sizeof(NgeEnt) + NGE_NENT * sizeof(int16_t) + 2 * NGE_NBLK * sizeof(NgeBlk) + NGE_RING * sizeof(NgeLine) + 63) & ~63u,
                 bn = (uint32_t)(NAT_MAXGRP * sizeof(NgeRef) + NGE_MAXCALL * sizeof(NgeXf) + 63) & ~63u;
  uint8_t *tb, *bb[2];
  if (!nge_cfg_set) nge_cfg();
  for (int k = 0; k < 2; k++) {
    if (!m[k] || cap < 4096) continue;
    if ((ge[k] >> 24) != ((ge[k] + cap - 1) >> 24)) {   /* one BASE serves a region: not across a 16 MB boundary of the GE's space */
      fprintf(stderr, "nge: region %d (GE %08x, %u KB) crosses a 16 MB boundary: not used\n", k, (unsigned)ge[k], (unsigned)(cap >> 10)); continue; }
    nge_rg[n].mem = m[k]; nge_rg[n].ge = ge[k]; nge_rg[n].cap = cap; nge_rg[n].used = 0; nge_rg[n].nb = 0;
    xc_nge_mem[n] = m[k]; xc_nge_ge[n] = ge[k]; n++;
  }
  tb = n ? al(tn, "nge_table") : 0; bb[0] = tb ? al(bn, "nge_buf0") : 0; bb[1] = bb[0] ? al(bn, "nge_buf1") : 0;
  if (!n || !tb || !bb[0] || !bb[1] || ((uintptr_t)tb | (uintptr_t)bb[0] | (uintptr_t)bb[1]) & 63) {
    xc_nge_ref[0] = xc_nge_ref[1] = 0; xc_nge_mem[0] = xc_nge_mem[1] = 0;
    fprintf(stderr, "nge: native step 1 NOT ON: %s (regions %d; blocks of %u B and 2 x %u B: %08x %08x %08x); today's path everywhere\n",
            !n ? "no region" : "no memory for the tables", n, (unsigned)tn, (unsigned)bn, (unsigned)(uintptr_t)tb, (unsigned)(uintptr_t)bb[0],
            (unsigned)(uintptr_t)bb[1]);
    return -1; }
  memset(tb, 0, tn); memset(bb[0], 0, bn); memset(bb[1], 0, bn);
  nge_e = (NgeEnt *)(void *)tb; nge_zf = (int16_t *)(void *)(tb + NGE_NENT * sizeof(NgeEnt));
  nge_rg[0].b = (NgeBlk *)(void *)(tb + NGE_NENT * sizeof(NgeEnt) + NGE_NENT * sizeof(int16_t)); nge_rg[1].b = nge_rg[0].b + NGE_NBLK;
  nge_ring = (NgeLine *)(void *)(nge_rg[1].b + NGE_NBLK);
  for (int b = 0; b < 2; b++) { xc_nge_ref[b] = (NgeRef *)(void *)bb[b]; xc_nge_xfa[b] = (NgeXf *)(void *)(bb[b] + NAT_MAXGRP * sizeof(NgeRef)); }
  for (int k = 0; k < NGE_NBKT; k++) nge_bkt[k] = -1;
  nge_nrg = n; nge_fin = fin; g_nge = 1;
  fprintf(stderr, "nge: native step 1 (MGS2_NGE %d, MGS2_NGE_CYCLE %d digits): %d region(s) of %u KB at %08x (GE %08x)%s%08x (GE %08x); "
          "tables %u B (%d entries of %u B) at %08x, references and transforms %u B a packet buffer at %08x and %08x; a mesh kept %d pipeline "
          "calls after its last use%s; fill lines %s\n",
          nge_all, nge_ncyc, n, (unsigned)(cap >> 10), (unsigned)(uintptr_t)nge_rg[0].mem, (unsigned)nge_rg[0].ge, n > 1 ? " and " : " (",
          (unsigned)(uintptr_t)(n > 1 ? nge_rg[1].mem : 0), (unsigned)(n > 1 ? nge_rg[1].ge : 0), (unsigned)tn, NGE_NENT, (unsigned)sizeof(NgeEnt),
          (unsigned)(uintptr_t)tb, (unsigned)bn, (unsigned)(uintptr_t)bb[0], (unsigned)(uintptr_t)bb[1], NGE_GUARD,
          fin ? " and the GE's 3 finishes" : "", nge_log ? "on" : "off");
  return 0;
}

/* ---- the arena ---- */
static int nge_held(const NgeEnt *e) { return g_dg_seq - e->last < NGE_GUARD || (nge_fin && *nge_fin - e->fin < 3); }
static void nge_free(int k)   /* the mesh's block back to its region */
{
  NgeEnt *e = &nge_e[k]; NgeReg *r = &nge_rg[e->reg];
  for (int j = 0; j < r->nb; j++) if (r->b[j].off == e->off) {
    memmove(&r->b[j], &r->b[j + 1], (size_t)(r->nb - j - 1) * sizeof(NgeBlk)); r->nb--; break; }
  r->used -= e->size; nge_used -= e->size; e->live = 0;
  if (e->mesh == NGE_ZOMBIE) nge_zf[nge_nzf++] = (int16_t)k;
}
/* a free entry: a dead zombie's, a new one, else a dead entry's (taken out of its bucket); -1: none */
static int nge_slot(void)
{
  if (nge_nzf) return nge_zf[--nge_nzf];
  if (nge_ne < NGE_NENT) return nge_ne++;
  for (int j = 0; j < nge_ne; j++) if (!nge_e[j].live && nge_e[j].mesh != NGE_ZOMBIE) {
    for (int16_t *p = &nge_bkt[nge_e[j].mesh >> 4 & (NGE_NBKT - 1)]; *p >= 0; p = &nge_e[*p].next) if (*p == j) { *p = nge_e[j].next; break; }
    return j; }
  return -1;
}
/* a mesh whose RAM changed: its old cook is freed now when no list in flight can use it, else kept as a zombie entry (evicted later as
   any old mesh); 0: no entry for it (the caller keeps the old cook and draws the call today's way) */
static int nge_retire(int k)
{
  NgeEnt *e = &nge_e[k];
  if (!nge_held(e)) { nge_free(k); return 1; }
  const int z = nge_slot();
  if (z < 0 || z == k) return 0;
  NgeReg *r = &nge_rg[e->reg];
  nge_e[z] = *e; nge_e[z].mesh = NGE_ZOMBIE; nge_e[z].next = -1; nge_e[z].seen = 0;
  for (int j = 0; j < r->nb; j++) if (r->b[j].off == e->off) { r->b[j].e = (int16_t)z; break; }
  e->live = 0; return 1;
}
static int nge_fit(NgeReg *r, uint32_t size, uint32_t *off, int *pos)
{
  uint32_t at = 64;
  if (r->nb >= NGE_NBLK) return 0;
  for (int j = 0; j <= r->nb; j++) {
    const uint32_t end = j < r->nb ? r->b[j].off : r->cap;
    if (end >= at && end - at >= size) { *off = at; *pos = j; return 1; }
    if (j < r->nb) at = r->b[j].off + r->b[j].size;
  }
  return 0;
}
static int nge_place(int k, uint32_t size)   /* a block for entry k (first fit; else evict the least recently used old mesh) */
{
  for (;;) {
    for (int g = 0; g < nge_nrg; g++) {
      uint32_t off; int pos; NgeReg *r = &nge_rg[g];
      if (!nge_fit(r, size, &off, &pos)) continue;
      memmove(&r->b[pos + 1], &r->b[pos], (size_t)(r->nb - pos) * sizeof(NgeBlk));
      r->b[pos].off = off; r->b[pos].size = size; r->b[pos].e = (int16_t)k; r->nb++; r->used += size;
      nge_e[k].reg = (int16_t)g; nge_e[k].off = off; nge_e[k].size = size;
      nge_used += size; if (nge_used > nge_peak) nge_peak = nge_used; if (nge_used > nge_tpeak) nge_tpeak = nge_used;
      return 1;
    }
    int best = -1, held = 0;
    for (int j = 0; j < nge_ne; j++) {
      const NgeEnt *e = &nge_e[j];
      if (!e->live || g_dg_seq - e->last < NGE_GUARD) continue;
      if (nge_fin && *nge_fin - e->fin < 3) { held = 1; continue; }   /* (the PSP: a list the GE has not finished may use it) */
      if (best < 0 || (int32_t)(e->last - nge_e[best].last) < 0) best = j;
    }
    if (best < 0) { if (held) g_nge_c[NGE_C_EV_HELD]++; return 0; }
    nge_free(best); g_nge_c[NGE_C_EVICT]++; nge_cur.ev++;
  }
}
/* the mesh's entry with a live cook that matches RAM now (cooked on a miss), or -1 */
static int nge_get(uint32_t mesh)
{
  int k = nge_bkt[mesh >> 4 & (NGE_NBKT - 1)];
  while (k >= 0 && nge_e[k].mesh != mesh) k = nge_e[k].next;
  if (nat_r32(mesh + 0xd0) & 0xfu) { g_nge_c[NGE_C_OVR]++; return -1; }
  if (k >= 0 && nge_e[k].live) {
    if (nge_mesh_same(&nge_e[k].m, g_mem)) { g_nge_c[NGE_C_REUSE]++; return k; }
    if (!nge_retire(k)) { g_nge_c[NGE_C_TFULL]++; return -1; }
    g_nge_c[NGE_C_RECOOK_CHG]++; nge_cur.rc++;
  } else if (k >= 0 && nge_e[k].seen) { g_nge_c[NGE_C_RECOOK_EV]++; nge_cur.rc++; }
  NgeArena a; NgeMesh m;
  nge_arena_at(&a, 0, ~0u, 0, 0);
  int rc = nge_cook_mesh(&a, g_mem, mesh, 0xff, &m, 0);
  if (rc != NGE_OK) { g_nge_c[rc == NGE_E_OVR ? NGE_C_OVR : NGE_C_BAD]++; return -1; }
  if (k < 0) {
    if ((k = nge_slot()) < 0) { g_nge_c[NGE_C_TFULL]++; return -1; }
    nge_e[k].mesh = mesh; nge_e[k].live = 0; nge_e[k].seen = 0;
    nge_e[k].next = nge_bkt[mesh >> 4 & (NGE_NBKT - 1)]; nge_bkt[mesh >> 4 & (NGE_NBKT - 1)] = (int16_t)k;
  }
  const uint32_t size = (a.used + 63) & ~63u;
  if (!nge_place(k, size)) { g_nge_c[NGE_C_NOROOM]++; nge_cur.nr++; return -1; }
  NgeReg *r = &nge_rg[nge_e[k].reg];
  nge_arena_at(&a, r->mem + nge_e[k].off, size, nge_e[k].off, r->ge);
  if (nge_cook_mesh(&a, g_mem, mesh, 0xff, &nge_e[k].m, (uint8_t)nge_e[k].reg) != NGE_OK) { nge_free(k); g_nge_c[NGE_C_BAD]++; return -1; }
  nge_e[k].live = 1; nge_e[k].seen = 1; g_nge_c[NGE_C_COOK]++; nge_cur.ck++;
  return k;
}

/* ---- the fill lines ---- */
static void nge_flush(void)
{
  for (int i = 0; i < nge_rn; i++) { const NgeLine *l = &nge_ring[i];
    fprintf(stderr, "nge f vb %u used %u peak %u ck %u rc %u ev %u nr %u nat %u tod %u xf %u\n", (unsigned)l->vb, (unsigned)l->used, (unsigned)l->peak,
            (unsigned)l->ck, (unsigned)l->rc, (unsigned)l->ev, (unsigned)l->nr, (unsigned)l->nat, (unsigned)l->tod, (unsigned)l->xf); }
  if (nge_rlost) fprintf(stderr, "nge: %ld fill lines not kept (the ring's %d full)\n", nge_rlost, NGE_RING);
  nge_rn = 0; nge_rlost = 0;
}
static void nge_pic_begin(void)   /* a pipeline call's first stub call: the last picture's line, this one's switch */
{
  if (nge_pic_open) {
    nge_cur.used = nge_used; nge_cur.peak = nge_peak;
    if (nge_log) { if (nge_rn >= NGE_RING) { nge_rfull++; nge_flush(); }   /* (no turn line came: a run without ME turns) */
                   nge_ring[nge_rn++] = nge_cur; }
#ifndef __psp__
    nge_flush();
#endif
  }
  memset(&nge_cur, 0, sizeof nge_cur); nge_peak = nge_used; nge_cur.vb = (uint32_t)k_vblanks;
  g_nge_pic = g_nge && rt_nge_at(k_vblanks + 1); nge_pic_open = g_nge_pic; if (g_nge_pic) g_nge_c[NGE_C_PICS]++;
}
/* the host's turn line (the PSP: psp/xl_psp.c with its nat turn lines): the counts since the last one, the ring's lines */
void nge_turn_line(long vb0, long vb1)
{
  if (!g_nge) return;
  nge_flush();
  fprintf(stderr, "nge turn vb %ld-%ld:", vb0, vb1);
  for (int i = 0; i < NGE_C_N; i++) fprintf(stderr, "%s %s %ld", i ? ";" : "", nge_cn[i], g_nge_c[i] - nge_c0[i]);
  fprintf(stderr, "; arena used %u KB, most %u KB, of %u KB in %d region(s)\n", (unsigned)(nge_used >> 10), (unsigned)(nge_tpeak >> 10),
          (unsigned)(nge_nrg * nge_rg[0].cap >> 10), nge_nrg);
  memcpy(nge_c0, g_nge_c, sizeof nge_c0); nge_tpeak = nge_used;
}
void nge_end_line(void)
{
  if (!g_nge) return;
  nge_flush();
  if (nge_rfull) fprintf(stderr, "nge: the fill ring (%d lines) was full and written out %ld times between turn lines (a run without ME turns)\n", NGE_RING, nge_rfull);
  fprintf(stderr, "nge totals:");
  for (int i = 0; i < NGE_C_N; i++) fprintf(stderr, "%s %s %ld", i ? ";" : "", nge_cn[i], g_nge_c[i]);
  fprintf(stderr, "; arena used now %u KB of %u KB in %d region(s)\n", (unsigned)(nge_used >> 10), (unsigned)(nge_nrg * nge_rg[0].cap >> 10), nge_nrg);
  { static const char *const nn[NGN_N] = NGN_NAMES;   /* the converter's (ge/xconv.c; on the PSP the ME's words as RAM holds them) */
    fprintf(stderr, "nge converter:");
    for (int i = 0; i < NGN_N; i++) fprintf(stderr, "%s %s %ld", i ? ";" : "", nn[i], xc_nge_n[i]);
    fprintf(stderr, "\n"); }
}

/* ---- the stub's ---- */
static void nge_rec(int b, NatBuf *s, uint32_t n0, int ng, uint32_t mesh, uint32_t blk)
{
  NgeRef *rf = xc_nge_ref[b] + n0; const NatGrp *g = s->g + n0; int any = 0, i;
  for (i = 0; i < ng; i++) {
    rf[i].g = 0;
    if (g[i].mscal != NAT_DF0_MSCAL) { g_nge_c[NGE_C_GRP_NOTDF0]++; continue; }
    if (!(g[i].flags & NATG_INPL)) { g_nge_c[NGE_C_GRP_NOTINPL]++; continue; }
    any++;
  }
  g_nge_c[NGE_C_GRP] += ng; g_nge_c[NGE_C_CALLS]++; nge_cur.tod += (uint32_t)(ng - any);
  if (!any) return;
  if (nge_xseq[b] != s->seq) { nge_xseq[b] = s->seq; nge_xn[b] = 0; }
  NgeXf *xf = &xc_nge_xfa[b][nge_xn[b]]; int k;
  if (nge_xn[b] >= NGE_MAXCALL) { g_nge_c[NGE_C_XFULL]++; k = -1; }
  else if (!nge_xf_block(g_mem, blk, xf)) { g_nge_c[NGE_C_NOBLK]++; k = -1; }
  else k = nge_get(mesh);
  if (k < 0 || nge_e[k].m.ng != (uint16_t)ng) { g_nge_c[NGE_C_GRP_NOCALL] += any; nge_cur.tod += (uint32_t)any; return; }
  NgeEnt *e = &nge_e[k]; const NgeGroup *q = (const NgeGroup *)(void *)(nge_rg[e->reg].mem + e->off); int nat = 0;
  for (i = 0; i < ng; i++) {
    if (g[i].mscal != NAT_DF0_MSCAL || !(g[i].flags & NATG_INPL)) continue;
    if (q[i].kind != NGE_K_PRE || (q[i].flags & NGE_GF_ANY) || !q[i].nst) { g_nge_c[NGE_C_GRP_FLAG]++; nge_cur.tod++; continue; }
    rf[i].g = NGE_RO(e->reg, e->off + (uint32_t)i * (uint32_t)sizeof(NgeGroup)); rf[i].xf = (uint16_t)nge_xn[b]; nat++;
  }
  nge_xn[b]++; if (nge_xn[b] > nge_cur.xf) nge_cur.xf = nge_xn[b]; e->last = g_dg_seq; e->fin = nge_fin ? *nge_fin : 0;
  g_nge_c[NGE_C_GRP_NAT] += nat; g_nge_c[NGE_C_CALLS_NAT]++; nge_cur.nat += (uint32_t)nat;
}
