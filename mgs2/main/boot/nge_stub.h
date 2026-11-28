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
#include "../ge/natp06.h"   /* step 2: NatP6, NP6_MAXGRP (the people's blocks in the carve) */
#ifdef __psp__
#pragma GCC push_options
#pragma GCC optimize("Os")   /* gl41: the cooker built for size (it runs once a mesh; its code sits in the main image) */
#endif
#include "../nge/nge_cook.c"
#ifdef __psp__
#pragma GCC pop_options
#endif
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
typedef struct { uint8_t *mem; uint32_t ge, cap, used; NgeBlk *b; int nb; uint32_t lo; } NgeReg;   /* step 2: lo, the first offset cooks may
                                                    take (the region's top cap - lo bytes until the carve after the opening, then 64) */
static NgeEnt *nge_e; static int nge_ne, nge_nzf; static int16_t *nge_bkt, *nge_zf;   /* gl41: nge_bkt in the "nge_scratch" block (not .bss) */
#define NGE_ZOMBIE 0xffffffffu   /* an entry that keeps a changed mesh's old cook until no list in flight can use it (not in the table) */
static NgeReg nge_rg[2]; static int nge_nrg;
int g_nge;                       /* 1: the arena is there (MGS2_NGE or MGS2_NGE_CYCLE named a 1 and the host gave the regions) */
static int g_nge_pic;            /* this pipeline call's switch */
static int nge_cfg_set, nge_all, nge_ncyc, nge_log = 1; static uint8_t nge_cyc[BD_CYC_N]; static uint32_t nge_kb = 256;
static volatile uint32_t *nge_fin;   /* the PSP: the GE's finishes (psp/xl_psp.c ge_fin_n); 0: x86 */
static uint32_t nge_xn[2], nge_xseq[2] = { ~0u, ~0u }, nge_used, nge_peak, nge_tpeak;
typedef struct { uint32_t vb, used, peak, ck, rc, ev, nr, nat, tod, xf; } NgeLine;
static NgeLine *nge_ring, nge_cur; static int nge_rn; static long nge_rlost, nge_rfull; static long nge_c0[NGE_C_N]; static int nge_pic_open;

static int n6_dbg;
/* gl42: the reuse content hash's probe (MGS2_NGE_HPROBE, default 1; 0: off): each n6_csum at a reuse check timed on the main CPU (the
   system's microsecond clock, two reads a check), summed into the people turn line ("reuse hash checks", "reuse hash us"; its bytes are
   "stream bytes hashed at use"), so the rain's main-CPU cost of the check can be read per step.  Measurement only: nothing it reads or
   writes is the game's, and the x86 twin's microseconds are its own clock's (only the counts compare) */
static int n6_hp = 1;
#ifdef __psp__
unsigned int sceKernelGetSystemTimeLow(void);
static inline uint32_t n6_us(void) { return sceKernelGetSystemTimeLow(); }
#else
#include <time.h>
static inline uint32_t n6_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint32_t)((uint64_t)t.tv_sec * 1000000u + (uint64_t)t.tv_nsec / 1000u); }
#endif
/* step 2, the carve after the opening: each region's KB from then (MGS2_NGE_KB2; default NGE_KB2_P6 when the people can be native, else
   no growth), the vblank from which frames begin with the larger carve (MGS2_NGE_GROW; default the bound turns' first, else 24,000), and
   whether the people can be native in some turn (their arrays then come from the carve too, on the PSP) */
#define NGE_KB2_P6 384   /* [C twin, MGS2_NGE=2 whole route]: 352 KB 4,297 ship no-room; 384 KB 0 ship, 3 people no-room, 67 re-cooks;
                            416 KB 0 / 0, but then the arena's data room is 1696 - 264 - 492 = 940 KB against 939 KB at vb 36,755-36,907 with
                            nothing native (C2 on): 384 KB leaves 972 KB, 33 KB to spare */
static uint32_t nge_kb2; static long nge_gvb = -1, nge_p6lag = 100; static int nge_p6, nge_grown; static long nge_grown_vb = -1;
extern int g_np6_ok; extern NatP6Buf g_np6[2];
static void nge_cfg(void)
{
  const char *e, *c = getenv("MGS2_OPT_CYCLE"); long f = -1, n = 0;
  nge_cfg_set = 1;
  nge_all = (e = getenv("MGS2_NGE")) ? atoi(e) : 0; if (nge_all < 0 || nge_all > 2) nge_all = 0;   /* step 2: 2 = the people native too */
  if ((e = getenv("MGS2_NGE_CYCLE")) && c && sscanf(c, "%ld:%ld", &f, &n) == 2 && n > 0 && f >= 0)
    for (; nge_ncyc < BD_CYC_N && *e >= '0' && *e <= '2'; e++) nge_cyc[nge_ncyc++] = (uint8_t)(*e - '0');
  if ((e = getenv("MGS2_NGE_KB")) && atoi(e) >= 64 && atoi(e) <= 4096) nge_kb = (uint32_t)atoi(e);
  if ((e = getenv("MGS2_NGE_LOG"))) nge_log = atoi(e) != 0;
  nge_p6 = nge_all == 2 || ((e = getenv("MGS2_NP6")) && atoi(e) == 3); for (int i = 0; i < nge_ncyc; i++) if (nge_cyc[i] == 2) nge_p6 = 1;
  nge_kb2 = nge_p6 ? NGE_KB2_P6 : 0;
  if ((e = getenv("MGS2_NGE_KB2"))) nge_kb2 = atoi(e) >= 64 && atoi(e) <= 4096 ? (uint32_t)atoi(e) : 0;
  if (nge_kb2 <= nge_kb) nge_kb2 = 0;   /* (no growth) */
  if ((e = getenv("MGS2_NGE_P6LAG")) && atol(e) >= 0) nge_p6lag = atol(e);
  if ((e = getenv("MGS2_NGE_GROW"))) nge_gvb = atol(e);
  else nge_gvb = f >= 0 && n > 0 ? f : 24000;
  if ((e = getenv("MGS2_NGE_HPROBE"))) n6_hp = atoi(e) != 0;   /* gl42 */
#ifndef __psp__
  if ((e = getenv("MGS2_NGE6_DBG"))) n6_dbg = atoi(e);   /* step 2, x86: a line for each people re-cook (what changed) */
#endif
}
uint32_t nge_cfg_bytes(void);
/* step 2: each region's bytes after the carve after the opening (nge_cfg_bytes when there is none) */
uint32_t nge_cfg_bytes2(void) { if (!nge_cfg_set) nge_cfg(); return nge_cfg_bytes() && nge_kb2 ? nge_kb2 << 10 : nge_cfg_bytes(); }
/* step 2: the people's block for a packet buffer (its array, then its references, NGE_P6REF words), each on a 64-byte line: in the carve
   after the opening (the PSP's), 0 when the people cannot be native or there is no growth */
uint32_t nge_np6_bytes(void)
{
  if (!nge_cfg_set) nge_cfg();
  return nge_p6 && nge_cfg_bytes2() > nge_cfg_bytes() ? (((uint32_t)(NP6_MAXGRP * sizeof(NatP6)) + 63u) & ~63u) + ((NGE_P6REF * 4u + 63u) & ~63u) : 0;
}
long nge_grow_vb(void) { if (!nge_cfg_set) nge_cfg(); return nge_cfg_bytes2() > nge_cfg_bytes() ? nge_gvb : -1; }
/* step 2: the first vblank whose picture may have native people when they wait for the carve: the growth's vb + MGS2_NGE_P6LAG (default
   100, one turn), the same on the twin and the PSP whenever the PSP has grown by then, so that the game's state (the people's packets
   the builder no longer writes) does not hang on when the GE finished; -1: no wait */
long nge_p6_vb(void) { if (!nge_cfg_set) nge_cfg(); return nge_np6_bytes() ? nge_gvb + nge_p6lag : -1; }
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
/* the host's: its regions (m1 0: one), their GE addresses, each region's bytes (cap; step 2: the top cap0 of them only until nge_grow;
   p6r: the people's references for each packet buffer in its blocks in the carve, 0: in the tables' blocks); fin: the GE's finish count (0 on x86); al: the host's
   allocator for the tables (the PSP: blocks of the system's memory outside newlib's heap, whose headroom is about 64 KB; x86: malloc),
   each block on a 64-byte line.  Three blocks, the largest first: the table (entries, zombie stack, both regions' blocks, the fill ring)
   and each packet buffer's references and transforms */
int nge_host_setup(uint8_t *m0, uint32_t ge0, uint8_t *m1, uint32_t ge1, uint32_t cap, uint32_t cap0, uint32_t *const p6r[2],
                   volatile uint32_t *fin, void *(*al)(uint32_t n, const char *name))
{
  uint8_t *m[2] = { m0, m1 }; uint32_t ge[2] = { ge0, ge1 }; int n = 0;
  const int r6 = p6r && p6r[0] && p6r[1];   /* step 2: the people's references in the carve (the PSP's), else in the tables' blocks */
  const uint32_t tn = (uint32_t)(NGE_NENT * sizeof(NgeEnt) + NGE_NENT * sizeof(int16_t) + 2 * NGE_NBLK * sizeof(NgeBlk) + NGE_RING * sizeof(NgeLine) + 63) & ~63u,
                 bn = (uint32_t)(NAT_MAXGRP * sizeof(NgeRef) + NGE_MAXCALL * sizeof(NgeXf) + (r6 ? 0 : NGE_P6REF * 4) + 63) & ~63u;
  uint8_t *tb, *bb[2];
  if (!nge_cfg_set) nge_cfg();
  for (int k = 0; k < 2; k++) {
    if (!m[k] || cap < 4096) continue;
    if ((ge[k] >> 24) != ((ge[k] + cap - 1) >> 24)) {   /* one BASE serves a region: not across a 16 MB boundary of the GE's space */
      fprintf(stderr, "nge: region %d (GE %08x, %u KB) crosses a 16 MB boundary: not used\n", k, (unsigned)ge[k], (unsigned)(cap >> 10)); continue; }
    nge_rg[n].mem = m[k]; nge_rg[n].ge = ge[k]; nge_rg[n].cap = cap; nge_rg[n].used = 0; nge_rg[n].nb = 0;
    nge_rg[n].lo = cap0 && cap0 < cap ? cap - cap0 : 0;
    xc_nge_mem[n] = m[k]; xc_nge_ge[n] = ge[k]; n++;
  }
  const uint32_t sn = (uint32_t)(NGE_NBKT * sizeof(int16_t) + NC_SCR + 63) & ~63u;   /* gl41: the bucket heads and the cooker's scratch */
  uint8_t *sb;
  tb = n ? al(tn, "nge_table") : 0; bb[0] = tb ? al(bn, "nge_buf0") : 0; bb[1] = bb[0] ? al(bn, "nge_buf1") : 0; sb = bb[1] ? al(sn, "nge_scratch") : 0;
  if (!n || !tb || !bb[0] || !bb[1] || !sb || ((uintptr_t)tb | (uintptr_t)bb[0] | (uintptr_t)bb[1] | (uintptr_t)sb) & 63) {
    xc_nge_ref[0] = xc_nge_ref[1] = 0; xc_nge_mem[0] = xc_nge_mem[1] = 0; xc_nge_ref6[0] = xc_nge_ref6[1] = 0;   /* (gl42: step 2's too) */
    fprintf(stderr, "nge: native step 1 NOT ON: %s (regions %d; blocks of %u B, 2 x %u B and %u B: %08x %08x %08x %08x); today's path everywhere\n",
            !n ? "no region" : "no memory for the tables", n, (unsigned)tn, (unsigned)bn, (unsigned)sn, (unsigned)(uintptr_t)tb, (unsigned)(uintptr_t)bb[0],
            (unsigned)(uintptr_t)bb[1], (unsigned)(uintptr_t)sb);
    return -1; }
  memset(tb, 0, tn); memset(bb[0], 0, bn); memset(bb[1], 0, bn); memset(sb, 0, sn);
  nge_bkt = (int16_t *)(void *)sb; nge_cook_scratch(sb + NGE_NBKT * sizeof(int16_t));
  nge_e = (NgeEnt *)(void *)tb; nge_zf = (int16_t *)(void *)(tb + NGE_NENT * sizeof(NgeEnt));
  nge_rg[0].b = (NgeBlk *)(void *)(tb + NGE_NENT * sizeof(NgeEnt) + NGE_NENT * sizeof(int16_t)); nge_rg[1].b = nge_rg[0].b + NGE_NBLK;
  nge_ring = (NgeLine *)(void *)(nge_rg[1].b + NGE_NBLK);
  for (int b = 0; b < 2; b++) { xc_nge_ref[b] = (NgeRef *)(void *)bb[b]; xc_nge_xfa[b] = (NgeXf *)(void *)(bb[b] + NAT_MAXGRP * sizeof(NgeRef));
    xc_nge_ref6[b] = r6 ? p6r[b] : (uint32_t *)(void *)(bb[b] + NAT_MAXGRP * sizeof(NgeRef) + NGE_MAXCALL * sizeof(NgeXf)); }   /* native step 2 */
  xc_nge_cap = cap;
  for (int k = 0; k < NGE_NBKT; k++) nge_bkt[k] = -1;
  nge_nrg = n; nge_fin = fin; g_nge = 1;
  if (nge_rg[0].lo) fprintf(stderr, "nge: step 2: each region %u KB until vb %ld, then %u KB (the carve after the opening)%s\n", (unsigned)(cap0 >> 10),
                            nge_grow_vb(), (unsigned)(cap >> 10), nge_np6_bytes() ? ", with the people's arrays and references beside it" : "");
  fprintf(stderr, "nge: native step 1 (MGS2_NGE %d, MGS2_NGE_CYCLE %d digits): %d region(s) of %u KB at %08x (GE %08x)%s%08x (GE %08x); "
          "tables %u B (%d entries of %u B) at %08x, references and transforms %u B a packet buffer at %08x and %08x; gl41: the bucket heads and the "
          "cooker's scratch %u B at %08x (not .bss); a mesh kept %d pipeline calls after its last use%s; fill lines %s\n",
          nge_all, nge_ncyc, n, (unsigned)(cap >> 10), (unsigned)(uintptr_t)nge_rg[0].mem, (unsigned)nge_rg[0].ge, n > 1 ? " and " : " (",
          (unsigned)(uintptr_t)(n > 1 ? nge_rg[1].mem : 0), (unsigned)(n > 1 ? nge_rg[1].ge : 0), (unsigned)tn, NGE_NENT, (unsigned)sizeof(NgeEnt),
          (unsigned)(uintptr_t)tb, (unsigned)bn, (unsigned)(uintptr_t)bb[0], (unsigned)(uintptr_t)bb[1], (unsigned)sn, (unsigned)(uintptr_t)sb, NGE_GUARD,
          fin ? " and the GE's 3 finishes" : "", nge_log ? "on" : "off");
  return 0;
}

/* step 2: the carve after the opening, once the host has seen every frame begun with the smaller one done (the GE's finishes on the
   PSP): each region whole; the people's arrays (p6[b], nge_np6_bytes() each, zeroed; their references after them: xc_nge_ref6, set at
   nge_host_setup) from then, when the people can be native (p6 0: the host's own arrays, x86) */
void nge_grow(uint8_t *const p6[2], long vb)
{
  if (nge_grown) return;
  nge_grown = 1; nge_grown_vb = vb;
  for (int g = 0; g < nge_nrg; g++) nge_rg[g].lo = 0;
  if (p6 && p6[0] && p6[1]) for (int b = 0; b < 2; b++) { memset(p6[b], 0, nge_np6_bytes()); g_np6[b].g = (NatP6 *)(void *)p6[b]; }
  if (nge_p6) g_np6_ok = 1;
  fprintf(stderr, "nge: step 2: the carve after the opening at vb %ld: %d region(s) of %u KB now whole%s\n", vb, nge_nrg,
          (unsigned)(nge_nrg ? nge_rg[0].cap >> 10 : 0), nge_p6 ? "; the people's arrays there from now" : "");
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
  uint32_t at = r->lo > 64 ? r->lo : 64;
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

/* ---- native step 2: the people's cook (ge/nge.h NgeP6G; the stub boot/np6_stub.h calls nge6_rec for a stubbed call's groups) ----
   A people mesh (0x12e8b8's R[4]) is cooked whole on a miss into the arena, in the ship's table under the key mesh | 1: an NgeP6G a
   group, then each drawable group's vertices (ge/vu1conv.c vc_p06_c2v on its streams, at alpha 255 and without ADC: the converter's
   own loop), strips (vc_strips of the stream's flags), the blended vertices' indices and the box.  The check at each use: the
   descriptor array, the six stream bases, the group count and the descriptors' sum.  A group is drawn natively (ok) when today's C2 path
   could take it on the data alone: no factor stream (TINT), 1..64 vertices, pass 3 not the light ramp and drawn with pass 2 or neither,
   streams A, B, C (and D with pass 2) sent and long enough, in RAM, and vc_p06_c2v takes it (UVs and weights in range) */
#include "../ge/vu1conv.h"
enum { N6K_CALLS, N6K_GRP, N6K_REF, N6K_REUSE, N6K_COOK, N6K_RECOOK, N6K_NOROOM, N6K_BAD, N6K_TFULL, N6K_GRP_NOT, N6K_BYTES,
       N6K_W_DESC, N6K_W_BASE, N6K_W_SUM, N6K_W_DATA, N6K_HASHED, N6K_HCHK, N6K_HUS, N6K_N };   /* gl42: N6K_HCHK, N6K_HUS (n6_hp) */
static const char *const n6k_cn[N6K_N] = { "people calls", "people groups", "people groups referenced", "people meshes reused",
  "people meshes cooked", "people meshes re-cooked (changed)", "people meshes not cooked: no room", "people meshes not cookable",
  "people table full", "people groups not drawn natively (today's path)", "people bytes cooked",
  "changed: descriptor array or count", "changed: a base of streams A B C", "changed: descriptor words or a base of D E F",
  "changed: stream contents only", "stream bytes hashed at use", "reuse hash checks", "reuse hash us" };
long g_np6k[N6K_N];
static uint8_t n6_v[2 * 64 * 24 + 32]; static uint16_t n6_fl[64], n6_runs[2 * 128];   /* (main CPU only) */
static uint32_t n6_mix(uint32_t s, uint32_t w) { return ((s << 5) | (s >> 27)) + w * 0x9e3779b1u; }
/* the check's last part: a hash of the words of streams A B C D the mesh's groups take from the cursors cur (0x12e8b8's order: each
   group nq quadwords of each stream it sends, so each stream one run from its cursor), the words its cook read.  A mesh whose vertices
   the game rewrites in place keeps its descriptors and bases (the opening, vb 8,755: 33 groups' normals) */
typedef uint32_t __attribute__((may_alias)) n6w32;
static uint32_t n6_csum(const uint8_t *ram, uint32_t desc, int ng, const uint32_t *cur, long *nbytes)
{
  uint32_t q[4] = { 0, 0, 0, 0 }, h = 0x2545f491u;
  for (int i = 0; i < ng; i++) for (int k = 0; k < 4; k++) q[k] += nc_rd8(ram, desc + 32u * (uint32_t)i + 14u + (uint32_t)k);
  for (int k = 0; k < 4; k++) {
    if (!cur[k] || !q[k]) continue;
    const uint32_t a = cur[k] & ~15u, nb = 16u * q[k];
    if (!nc_in_ram(a, nb)) { h = n6_mix(h, 0xdead0000u + (uint32_t)k); continue; }
    const n6w32 *w = (const n6w32 *)(const void *)(ram + (a & NGE_RAM_MASK));
    uint32_t sum = 0, x = 0;
    for (uint32_t j = 0; j < nb / 4u; j++) { sum += w[j]; x = ((x << 5) | (x >> 27)) ^ w[j]; }
    h = n6_mix(n6_mix(h, sum), x);
    if (nbytes) *nbytes += (long)nb;
  }
  return h;
}
static int n6_cook(NgeArena *a, const uint8_t *ram, uint32_t mesh, NgeMesh *m, uint8_t reg)
{
  const int ng = (int16_t)nc_rd16(ram, mesh + 0xcc);
  const uint32_t desc = nc_rd32(ram, mesh + 0xf4);
  uint32_t cur[6];
  for (int k = 0; k < 6; k++) cur[k] = nc_rd32(ram, mesh + 0xdc + 4u * (uint32_t)k);
  if (ng <= 0 || ng > 1024 || !nc_in_ram(desc, 32u * (uint32_t)ng)) return NGE_E_BAD;
  m->csum = n6_csum(ram, desc, ng, cur, 0);
  a->used = 0;
  const uint32_t go = nc_take(a, (uint32_t)ng * (uint32_t)sizeof(NgeP6G), 16);
  if (go == ~0u) return NGE_E_ROOM;
  m->mesh = mesh | 1u; m->desc = desc; m->sa = cur[0]; m->sb = cur[1]; m->sc = cur[2];
  m->dsum = n6_mix(n6_mix(n6_mix(nge_desc_sum(ram, desc, ng), cur[3]), cur[4]), cur[5]); m->ng = (uint16_t)ng; m->pre = 2; m->alpha = 0xff;
  const int tint = cur[5] != 0;   /* (the builder's MSCAL: idx (desc0 & 1) + 2 when stream F's cursor is not 0) */
  for (int i = 0; i < ng; i++) {
    const uint32_t dd = desc + 32u * (uint32_t)i, d0 = nc_rd32(ram, dd), n = nc_rd8(ram, dd + 12), hy = d0 & 0xffffu;
    uint32_t sa[6], nq[6];
    for (int k = 0; k < 6; k++) {
      nq[k] = nc_rd8(ram, dd + 14 + (uint32_t)k);
      if (cur[k] && nq[k]) { sa[k] = cur[k] & ~15u; cur[k] += 16u * nq[k]; } else { sa[k] = 0; nq[k] = 0; }
    }
    NgeP6G q; memset(&q, 0, sizeof q);
    q.n = (uint16_t)n; q.hy = (uint16_t)hy; q.skin = (uint8_t)(d0 & 1); q.st = q.skin ? 24 : 20; q.reg = reg;
    const int chk2 = (hy & 0x10) != 0, p3 = (hy & 0x100) ? -1 : (hy & 0x600) ? ((hy & 0x400) ? 2 : 1) : 0;
    /* the streams vc_p06_c2 reads (A, B V4-16: 2 vertices a quadword; C, D V2-16: 4), sent, long enough and in RAM */
#define N6S(k, per) (sa[k] && (per) * nq[k] >= n && nc_in_ram(sa[k], 16u * nq[k]))
    int ok = !tint && n >= 1 && n <= 64 && p3 != 2 && (p3 == 1) == chk2 && N6S(0, 2) && N6S(1, 2) && N6S(2, 4) && (!chk2 || N6S(3, 4));
#undef N6S
    if (ok) {
      const void *p[6]; VcLit L; VcP6C2 k;
      for (int j = 0; j < 6; j++) p[j] = sa[j] ? (const void *)(ram + (sa[j] & NGE_RAM_MASK)) : 0;
      memset(&L, 0, sizeof L); memset(&k, 0, sizeof k);
      L.n = (int)n; L.hy = hy; k.skin = q.skin; k.chk2 = chk2; k.a2 = 0xffu;   /* (no factor, no ADC: the call's parts are done by the converter) */
      ok = vc_p06_c2v(p, &L, &k, n6_v, n6_fl, 0, 0, 0) == (int)n;
      if (ok) {
        const int nst = vc_strips(n6_fl, (int)n, n6_runs, 128), uv2 = L.uv2;
        const uint32_t a1 = (n * q.st + 15u) & ~15u, vbn = uv2 == 2 ? 2 * a1 : a1;
        uint8_t bl[64]; uint32_t nb = 0;
        if (q.skin) { const int16_t *A = (const int16_t *)p[0]; for (uint32_t v = 0; v < n; v++) if (A[4 * v + 3] != 4096) bl[nb++] = (uint8_t)v; }
        vc_p06_c2box(p, (int)n, &k);
        const uint32_t vo = nc_take(a, vbn, 16), ro = vo == ~0u ? ~0u : nc_take(a, 4u * (uint32_t)nst, 4), bo = ro == ~0u ? ~0u : nc_take(a, nb, 1);
        if (bo == ~0u) return NGE_E_ROOM;
        q.vb = a->base + vo; q.runs = a->base + ro; q.bl = nb ? a->base + bo : 0; q.nst = (uint16_t)nst; q.nb = (uint16_t)nb;
        q.uv2 = (uint8_t)uv2; q.du = L.du; q.dv = L.dv; q.cull = (uint8_t)L.cull; q.ok = 1;
        for (int j = 0; j < 3; j++) { q.blo[j] = k.blo[j]; q.bhi[j] = k.bhi[j]; }
        if (a->mem) { memcpy(a->mem + vo, n6_v, vbn); memcpy(a->mem + ro, n6_runs, 4u * (uint32_t)nst); if (nb) memcpy(a->mem + bo, bl, nb); }
      }
    }
    if (!ok) g_np6k[N6K_GRP_NOT] += a->mem != 0;
    if (a->mem) memcpy(a->mem + go + (uint32_t)i * sizeof(NgeP6G), &q, sizeof q);
  }
  return NGE_OK;
}
/* N6K_REUSE: the cook matches RAM now; else what changed first (N6K_W_*) */
static int n6_same(const NgeMesh *m, const uint8_t *ram)
{
  const uint32_t mesh = m->mesh & ~1u; uint32_t c[6];
  if (nc_rd32(ram, mesh + 0xf4) != m->desc || (int16_t)nc_rd16(ram, mesh + 0xcc) != (int)m->ng) return N6K_W_DESC;
  for (int k = 0; k < 6; k++) c[k] = nc_rd32(ram, mesh + 0xdc + 4u * (uint32_t)k);
  if (c[0] != m->sa || c[1] != m->sb || c[2] != m->sc) return N6K_W_BASE;
  if (n6_mix(n6_mix(n6_mix(nge_desc_sum(ram, m->desc, m->ng), c[3]), c[4]), c[5]) != m->dsum) return N6K_W_SUM;
  if (!n6_hp) return n6_csum(ram, m->desc, m->ng, c, &g_np6k[N6K_HASHED]) == m->csum ? N6K_REUSE : N6K_W_DATA;
  const uint32_t t0 = n6_us(), h = n6_csum(ram, m->desc, m->ng, c, &g_np6k[N6K_HASHED]);   /* gl42: the probe (n6_hp) */
  g_np6k[N6K_HUS] += (long)(n6_us() - t0); g_np6k[N6K_HCHK]++;
  return h == m->csum ? N6K_REUSE : N6K_W_DATA;
}
/* the people mesh's entry with a live cook that matches RAM now (cooked on a miss), or -1 (nge_get's, with key mesh | 1) */
static int nge6_get(uint32_t mesh)
{
  const uint32_t key = mesh | 1u;
  int k = nge_bkt[key >> 4 & (NGE_NBKT - 1)];
  while (k >= 0 && nge_e[k].mesh != key) k = nge_e[k].next;
  if (k >= 0 && nge_e[k].live) {
    const int why = n6_same(&nge_e[k].m, g_mem);
    g_np6k[why]++;
#ifndef __psp__
    if (n6_dbg && why != N6K_REUSE) { const NgeMesh *o = &nge_e[k].m; const uint32_t mm = mesh & ~1u;
      fprintf(stderr, "nge6 re vb %ld why %d mesh %08x ng %d desc %08x/%08x a %08x/%08x b %08x/%08x c %08x/%08x\n", (long)k_vblanks, why,
              (unsigned)mm, (int)o->ng, (unsigned)o->desc, (unsigned)nc_rd32(g_mem, mm + 0xf4), (unsigned)o->sa, (unsigned)nc_rd32(g_mem, mm + 0xdc),
              (unsigned)o->sb, (unsigned)nc_rd32(g_mem, mm + 0xe0), (unsigned)o->sc, (unsigned)nc_rd32(g_mem, mm + 0xe4)); }
#endif
    if (why == N6K_REUSE) return k;
    if (!nge_retire(k)) { g_np6k[N6K_TFULL]++; return -1; }
    g_np6k[N6K_RECOOK]++; nge_cur.rc++;
  } else if (k >= 0 && nge_e[k].seen) { g_nge_c[NGE_C_RECOOK_EV]++; nge_cur.rc++; }
  NgeArena a; NgeMesh m;
  nge_arena_at(&a, 0, ~0u, 0, 0);
  if (n6_cook(&a, g_mem, mesh, &m, 0) != NGE_OK) { g_np6k[N6K_BAD]++; return -1; }
  if (k < 0) {
    if ((k = nge_slot()) < 0) { g_np6k[N6K_TFULL]++; return -1; }
    nge_e[k].mesh = key; nge_e[k].live = 0; nge_e[k].seen = 0;
    nge_e[k].next = nge_bkt[key >> 4 & (NGE_NBKT - 1)]; nge_bkt[key >> 4 & (NGE_NBKT - 1)] = (int16_t)k;
  }
  const uint32_t size = (a.used + 63) & ~63u;
  if (!nge_place(k, size)) { g_np6k[N6K_NOROOM]++; nge_cur.nr++; return -1; }
  NgeReg *r = &nge_rg[nge_e[k].reg];
  nge_arena_at(&a, r->mem + nge_e[k].off, size, nge_e[k].off, r->ge);
  if (n6_cook(&a, g_mem, mesh, &nge_e[k].m, (uint8_t)nge_e[k].reg) != NGE_OK) { nge_free(k); g_np6k[N6K_BAD]++; return -1; }
  nge_e[k].live = 1; nge_e[k].seen = 1; g_np6k[N6K_COOK]++; g_np6k[N6K_BYTES] += size; nge_cur.ck++;
  return k;
}
/* a stubbed people call's ng groups, recorded from index n0 of buffer b: each group's reference (0: today's path) */
static void nge6_rec(int b, uint32_t n0, int ng, uint32_t mesh)
{
  uint32_t *rf = xc_nge_ref6[b] + n0; int i, nat = 0;
  g_np6k[N6K_CALLS]++; g_np6k[N6K_GRP] += ng;
  const int k = nge6_get(mesh);
  if (k < 0 || nge_e[k].m.ng != (uint16_t)ng) { for (i = 0; i < ng; i++) rf[i] = 0; return; }
  NgeEnt *e = &nge_e[k]; const NgeP6G *q = (const NgeP6G *)(void *)(nge_rg[e->reg].mem + e->off);
  for (i = 0; i < ng; i++) { rf[i] = q[i].ok ? NGE_RO(e->reg, e->off + (uint32_t)i * (uint32_t)sizeof(NgeP6G)) : 0u; nat += q[i].ok; }
  e->last = g_dg_seq; e->fin = nge_fin ? *nge_fin : 0;
  g_np6k[N6K_REF] += nat; nge_cur.nat += (uint32_t)nat;
}
static void nge6_line(const char *what, const long *c, const long *c0)
{
  fprintf(stderr, "nge %s:", what);
  for (int i = 0; i < N6K_N; i++) fprintf(stderr, "%s %s %ld", i ? ";" : "", n6k_cn[i], c[i] - (c0 ? c0[i] : 0));
  fprintf(stderr, "\n");
}
static long g_np6k0[N6K_N];

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
  if (g_np6k[N6K_CALLS] != g_np6k0[N6K_CALLS]) { nge6_line("people turn", g_np6k, g_np6k0); memcpy(g_np6k0, g_np6k, sizeof g_np6k0); }   /* step 2 */
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
  nge6_line("people totals", g_np6k, 0);   /* step 2 */
  { static const char *const nn[NGN_N] = NGN_NAMES;   /* the converter's (ge/xconv.c; on the PSP the ME's words as RAM holds them) */
    fprintf(stderr, "nge converter:");
    for (int i = 0; i < NGN_N; i++) fprintf(stderr, "%s %s %ld", i ? ";" : "", nn[i], xc_nge_n[i]);
    fprintf(stderr, "\n"); }
  { static const char *const nn[NG6_N] = NG6_NAMES;   /* step 2: the people's (ge/xconv.c xc_lit_c2) */
    fprintf(stderr, "nge converter people:");
    for (int i = 0; i < NG6_N; i++) fprintf(stderr, "%s %s %ld", i ? ";" : "", nn[i], xc_nge6_n[i]);
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
  const uint16_t xh = nge_xf_h16(xf->world, xf->pa, xf->pb);   /* gl41: the call's transform digest, in each reference (ge/nge.h) */
  for (i = 0; i < ng; i++) {
    if (g[i].mscal != NAT_DF0_MSCAL || !(g[i].flags & NATG_INPL)) continue;
    if (q[i].kind != NGE_K_PRE || (q[i].flags & NGE_GF_ANY) || !q[i].nst) { g_nge_c[NGE_C_GRP_FLAG]++; nge_cur.tod++; continue; }
    rf[i].g = NGE_RO(e->reg, e->off + (uint32_t)i * (uint32_t)sizeof(NgeGroup)); rf[i].xf = (uint16_t)nge_xn[b]; rf[i].pad = xh; nat++;
  }
  nge_xn[b]++; if (nge_xn[b] > nge_cur.xf) nge_cur.xf = nge_xn[b]; e->last = g_dg_seq; e->fin = nge_fin ? *nge_fin : 0;
  g_nge_c[NGE_C_GRP_NAT] += nat; g_nge_c[NGE_C_CALLS_NAT]++; nge_cur.nat += (uint32_t)nat;
}
