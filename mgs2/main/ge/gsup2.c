/* gsup2.c: the texture map of gsup.h, lean (renderer_notes 11.12 "What can make it lean" 1 and "The lean texture map"; task #47).  It
   gives the same words and the same DRAWN / LOST flags as ge/gsup.c, whose rules it keeps (tools/gsref.c's transfers and CLUT loads),
   and identities that change whenever what they name changes; it is built instead of gsup.c (or beside it, renamed, for the dev check
   boot/gudual.c).  gsup.c keeps a list per 256-byte block and pushes on it for every upload, every re-marked drawn area and every CLUT
   load (its PSP cost: renderer_notes 11.12 "Step 1 on the PSP").  Here, for the PSP's costs:
   - each block has a state word per lane group (0: bits 0-23, 1: bits 24-27, 2: bits 28-31): nothing, the one record that wrote all of
     the block in that group (a transfer, a local copy or a drawn area), or a list of records, newest first, where records wrote parts of
     it.  A list never changes once made (a push makes a new head), so a CLUT load keeps the state words it read as its snapshot;
   - nothing counts references: records, list nodes and CLUT loads that nothing names any more go back to their pools in a sweep, at the
     start of a write when a pool runs low; so a write of all of a block is a store per lane group;
   - a host -> local transfer goes into the blocks when its data is all in (or when the map is read before that, or when the next
     transfer starts: gsup.c covers it at TRXDIR), so its identity is final when the blocks get it;
   - a drawn area is kept by its rectangle and marked again only after a write that is not a draw went into one of its blocks in its
     lanes (each block knows the drawn areas it is in);
   - a CLUT load takes the load kept last for its parameters (a table, which a sweep empties of the loads nothing else names) when the
     blocks it read still hold the states they held then, else it is kept as a new load with those states; the CLUT rows point at the loads;
   - an area's identity is kept by its rectangle and made again only when a block of its pages (8 KB) was written since; made again, it
     walks a block's states only where they differ from the block's before it (gu_area_memo); the identity of CLUT entries 0..255 that
     one load put in place in order is kept with the load (gu_clut_memo); a flags-only question walks each state word once
     (gu_area_flags).  All three give what the plain walks give (codec_speed.md step 1; GU_MEMO_CHECK compares them).
   Portable C; the host gives the EE RAM.  Game data passes through it: what it gives is personal use only. */
#include <stdlib.h>
#include <string.h>
#include "gsup.h"
#include "gslayout.h"
#include "me_rt.h"   /* task #149: -DMGS2_ME_SAFE (ME-safe calls) */
#if defined GU_SENT || defined GU_MEMO_CHECK
#include <stdio.h>
#endif
#ifdef GU_MEMO_CHECK   /* dev: each memo's answer against the plain walk's (counts printed at exit) */
static struct { long area, area_bad, flags, flags_bad, clut, clut_bad; } MC;
__attribute__((destructor)) static void mc_report(void)
{ fprintf(stderr, "gsup2 memo check: area identities %ld (%ld differing), area flags %ld (%ld differing), CLUT identities kept %ld (%ld differing)\n",
          MC.area, MC.area_bad, MC.flags, MC.flags_bad, MC.clut, MC.clut_bad); }
#endif
#ifdef GU_DEVCNT   /* dev: what the map's paths do (counts printed at exit) */
#include <stdio.h>
static struct { long put_full, put_part, put_draw, dtm_iter, dtm_set, dtm_already, remark, remark_new, remark_blocks, remark_touched, drawn_calls,
                     drawn_early, drawn_kept, drawn_memo, ident_calls, ident_sparse_b, ident_full_b, gc_r, gc_n, gc_l, clut_calls, clut_memo, clut_new,
                     clut_nb, trx, commits, cov_blocks, cov_pageblocks, put_page, rec_news, node_news, ld_news; } DC;
static const struct GuMap *dc_map;
static void dc_pools(const struct GuMap *m);
static struct { int psm, w, h, lanes, own; long n, blocks; } DH[64]; static int ndh;
static void dh_add(int psm, int w, int h, int lanes, int own, long blocks)
{
  int i; for (i = 0; i < ndh && !(DH[i].psm == psm && DH[i].w == w && DH[i].h == h && DH[i].lanes == lanes && DH[i].own == own); i++) {}
  if (i == ndh) { if (ndh == 64) return; ndh++; DH[i].psm = psm; DH[i].w = w; DH[i].h = h; DH[i].lanes = lanes; DH[i].own = own; }
  DH[i].n++; DH[i].blocks += blocks;
}
#define DC_(x) (DC.x++)
__attribute__((destructor)) static void dc_report(void)
{
  fprintf(stderr, "gsup2 count: put full %ld part %ld draw %ld; dtm loop %ld (set dirty %ld, already dirty %ld); areas marked again %ld (new %ld): "
          "blocks %ld, of them written since %ld; gu_drawn %ld (early out %ld, kept %ld, by the last two %ld); rec_ident %ld: sparse bytes %ld, full bytes %ld; sweeps "
          "for records %ld, nodes %ld, loads %ld; CLUT loads %ld: by parameters %ld, new %ld, blocks %ld; transfers %ld, commits %ld\n",
          DC.put_full, DC.put_part, DC.put_draw, DC.dtm_iter, DC.dtm_set, DC.dtm_already, DC.remark, DC.remark_new, DC.remark_blocks, DC.remark_touched,
          DC.drawn_calls, DC.drawn_early, DC.drawn_kept, DC.drawn_memo, DC.ident_calls, DC.ident_sparse_b, DC.ident_full_b, DC.gc_r, DC.gc_n, DC.gc_l, DC.clut_calls,
          DC.clut_memo, DC.clut_new, DC.clut_nb, DC.trx, DC.commits);
  fprintf(stderr, "gsup2 count: cover blocks %ld, in whole pages %ld; page writes %ld\n", DC.cov_blocks, DC.cov_pageblocks, DC.put_page);
  if (dc_map) dc_pools(dc_map);
  for (int i = 0; i < ndh; i++) if (DH[i].blocks > DC.cov_blocks / 200)
    fprintf(stderr, "gsup2 count:   psm %02x %dx%d lanes %d own %d: %ld commits, %ld blocks\n", DH[i].psm, DH[i].w, DH[i].h, DH[i].lanes, DH[i].own, DH[i].n, DH[i].blocks);
}
#else
#define DC_(x) ((void)0)
#endif

#define GU_NOSRC 0xffffffffu
static const uint32_t GB[3] = { 0x00ffffffu, 0x0f000000u, 0xf0000000u };   /* the bits of each lane group */
#define FNV0 0xcbf29ce484222325ull
/* content hashes by 32-bit words (cheap on the PSP: no 64-bit products): two lanes of xxHash32's round, each over every word, and its end
   mix; a last short word is padded with zeros and the length goes in at the end */
typedef struct { uint32_t a, c; } Wh;
static inline uint32_t rotl32(uint32_t v, int r) { return v << r | v >> (32 - r); }
static inline void wh_word(Wh *h, uint32_t w) { h->a = rotl32(h->a + w * 0x85ebca77u, 13) * 0x9e3779b1u; h->c = rotl32(h->c + w * 0xc2b2ae3du, 17) * 0x27d4eb2fu; }
static inline uint32_t wh_end1(uint32_t v) { v ^= v >> 15; v *= 0x85ebca77u; v ^= v >> 13; v *= 0xc2b2ae3du; return v ^ v >> 16; }
static Wh wh_start(uint64_t seed) { Wh h = { (uint32_t)seed + 0x165667b1u, (uint32_t)(seed >> 32) ^ 0x9e3779b1u }; return h; }
static uint64_t wh_end(Wh h, uint32_t n) { return (uint64_t)wh_end1(h.a ^ n) << 32 | wh_end1(h.c ^ n ^ 0x5bd1e995u); }
static uint64_t whash(uint64_t seed, const uint8_t *b, uint32_t n)
{
  Wh h = wh_start(seed); uint32_t w, nw = n >> 2;
  for (uint32_t i = 0; i < nw; i++) { memcpy(&w, b + 4 * i, 4); wh_word(&h, w); }
  if (n & 3) { w = 0; memcpy(&w, b + 4 * nw, n & 3); wh_word(&h, w); }
  return wh_end(h, n);
}
static uint64_t mix(uint64_t h, uint64_t v) { h = (h ^ v) * 0x9e3779b97f4a7c15ull; return h ^ h >> 31; }

/* a state word: 0 nothing written since the map started; S_REC(i): record i wrote all of the block in that group; S_NOD(i): list node i */
#define S_NODE 0x80000000u
#define S_REC(i) ((uint32_t)(i) + 1u)
#define S_NOD(i) (S_NODE | ((uint32_t)(i) + 1u))
#define S_IDX(s) ((int)(((s) & ~S_NODE) - 1u))

typedef struct {
  uint32_t src, got, need;     /* as gsup.c's Xf: EE address of its first byte (GU_NOSRC: no data yet), bytes received, bytes it takes */
  uint32_t bp, bw; int psm, sx, sy, w, h;
  uint8_t bad, whole, drawn;   /* as Xf */
  uint8_t lanes;               /* the lane groups it writes (bit g: group g) */
  uint8_t in;                  /* 0: not in the blocks yet; 1: in them */
  uint8_t live;                /* in use (not in the pool) */
  int next;                    /* the pool's free list */
  uint32_t mk;                 /* the last sweep that found it named */
  uint32_t no;                 /* its number among host -> local transfers */
  uint64_t id; uint32_t idgot; /* as Xf */
  uint8_t *own;                /* as Xf */
#ifdef GU_SENT
  uint8_t *sent;               /* dev: its bytes as they came */
#endif
} Rec;
typedef struct { int rec; uint32_t below, mk; int live; } Node;   /* below: the state under it; the pool's free list in rec */
typedef struct {
  uint32_t rk, rt;                            /* its parameters as the table of loads by parameters keys them (gu_clut_load) */
  uint32_t seq;                               /* the write count (GuMap seq) when its blocks were last seen to hold its states */
  uint32_t cbp, cbw, epoch; int cpsm, csm, cou, cov, n;
  int nb; uint16_t bn[4]; uint32_t s[4][3];   /* the blocks the entries are in and their state words at the load */
  uint8_t bad, live; int next; uint32_t mk;   /* in use; the pool's free list; the last sweep that found it named */
  uint64_t id; int idf;                       /* the identity of what it loaded (gu_keys) and its GU_ID_* flags */
  uint64_t cid; int cidf; int8_t ccp; uint8_t cv;   /* gu_clut_memo: cv, gu_clut_id of entries 0..255 all from it in order, kept for CPSM
                                                        ccp (cid, cidf: a function of the load alone, so never stale while it lives) */
} Ld;
/* a CLUT buffer row: 0 (never loaded) or ROW(load, its first entry / 16, 1: the high halves of CT32 entries) */
#define ROW(li, j, hi) ((uint32_t)(li + 1) << 5 | (uint32_t)(j) << 1 | (uint32_t)(hi))
#define ROW_LD(w) ((int)((w) >> 5) - 1)
#define ROW_E0(w) ((int)((w) >> 1 & 15) * 16)
#define DT_N 16u       /* drawn areas (a bit of dtm each) */
#define RT_BITS 9      /* CLUT loads by parameters: 512 */
#define AC_N 1024u     /* area identities */
/* a sweep at the start of a write when fewer records, list nodes or loads than these are free; a pool less than half free after it doubles */
#define R_LOW 64
#define N_LOW 512
#define L_LOW 16
#define TL_N 64        /* blocks a drawn area remembers as written since its last draw (more: all of it is marked again) */
typedef struct { int psm, x0, y0, x1, y1, used, dirty; uint32_t bp, bw, lanes, last; int ntl, over; uint16_t tl[TL_N]; } DtE;
#define IT_W 4         /* sb9 0b: CLUT loads kept by identity: sets of IT_W (the least recently used goes), 1 << IT_SB sets */
#define IT_SB 7
#define IT_N (IT_W << IT_SB)
typedef struct { int li; uint32_t rk, rt, use; uint64_t sig; } ItE;   /* a load (-1: none), its parameters, when last used (0: never) and
                                                                       the identity of its records */
typedef struct { uint64_t id; int psm, x0, y0, x1, y1, flags, pg0, npg, used; uint32_t bp, bw, seq, epoch; } AcE;
typedef struct { int k, psm, x0, y0, x1, y1; uint32_t bp, bw, bits; } DlE;
struct GuMap {
  GuRamFn ram; void *ctx;
  uint32_t bs[16384][3];       /* per block and lane group: its own state word */
  uint32_t ps[512][3], pm[512][3];   /* per page (32 blocks) and lane group: the state of the last write of all of the page; the blocks
                                        written since (bit j: block 32 p + j), whose state is their own */
  uint16_t pdt[512];           /* per page: drawn areas that may have blocks in it (an or of their dtm, maybe more) */
  Rec *r; int nr, capr, freer, nfreer;
  Node *nd; int nn, capn, freen, nfreen;
  Ld *l; int nl, capl, freel, nfreel;
  uint32_t gcn;                /* sweeps so far */
  int cur;                     /* the transfer taking IMAGE data, -1: none */
  uint32_t row[32];            /* as gsup.c's rows, packed (ROW) */
  int canon, canon32;          /* 1 + the load whose rows() put CLUT entries 0..255 in place in order (0: none); and whether the high halves too */
  uint8_t memo, cmemo;         /* gu_area_memo, gu_clut_memo */
  uint32_t cbp0, cbp1;
  GuStats st;
  GuDataFn data_fn; void *data_ctx;
  uint32_t seq;                /* counts the writes into the blocks */
  uint32_t pg[512];            /* per page (32 blocks): seq of the last write into it */
  uint32_t epoch;              /* changes when what a record holds changes without a write (its data came after it went in) */
  DtE dt[DT_N]; int rt[1 << RT_BITS]; AcE ac[AC_N];
  uint16_t dtm[16384];         /* per block: the drawn areas (bits of dt) it is in */

  int dt_hit[2]; uint32_t dt_clock;   /* the two drawn areas found last; a clock for the least recently drawn */
  DlE dl[2];                   /* gu_drawn's last two calls with an area, as given, and the area (-1: none) */
  int keys, zero_empty;
  struct GuSrc *srct; uint32_t nsrc, capsrc;
  int ldid; uint32_t itclk; ItE it[IT_N];   /* sb9 0b: gu_ld_ident's mode, a clock for the table's uses and the loads kept by identity */
#ifdef GU_SENT
  int keep, read_sent, watch;
  void (*trx_fn)(void *ctx, const uint64_t *gr); void *trx_ctx;
#endif
};

static uint8_t ibt32[32], ict32[64];   /* inverses of gs_bt32 / gs_ct32: block in page, word in block -> y << 3 | x */
static uint8_t ibt4[32], ibt8[32], ibt16[32], ibt16s[32]; static uint16_t ict4[512], ict8[256], ict16[128];
static void inv_init(void)
{
  for (int y = 0; y < 4; y++) for (int x = 0; x < 8; x++) ibt32[gs_bt32[y][x]] = (uint8_t)(y << 3 | x);
  for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) ict32[gs_ct32[y][x]] = (uint8_t)(y << 3 | x);
  for (int y = 0; y < 8; y++) for (int x = 0; x < 4; x++) {
    ibt4[gs_bt4[y][x]] = (uint8_t)(y << 3 | x); ibt16[gs_bt16[y][x]] = (uint8_t)(y << 3 | x); ibt16s[gs_bt16s[y][x]] = (uint8_t)(y << 3 | x); }
  for (int y = 0; y < 4; y++) for (int x = 0; x < 8; x++) ibt8[gs_bt8[y][x]] = (uint8_t)(y << 3 | x);
  for (int y = 0; y < 16; y++) for (int x = 0; x < 32; x++) ict4[gs_ct4[y][x]] = (uint16_t)(y << 5 | x);
  for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) ict8[gs_ct8[y][x]] = (uint16_t)(y << 5 | x);
  for (int y = 0; y < 8; y++) for (int x = 0; x < 16; x++) ict16[gs_ct16[y][x]] = (uint16_t)(y << 5 | x);
}

/* ---- the blocks of a layout: gslayout.h's gs_pa* without the place in the block ---- */
typedef struct { uint8_t pws, phs, cws, chs, half, cols, rows, zx; const uint8_t *bt; } Lay;   /* shifts of the page and cell sizes; half:
                                                                                                    pages per row bw / 2; the block table */
static const Lay L32 = { 6, 5, 3, 3, 0, 8, 4, 0, &gs_bt32[0][0] }, LZ32 = { 6, 5, 3, 3, 0, 8, 4, 0x18, &gs_bt32[0][0] },
                 L16 = { 6, 6, 4, 3, 0, 4, 8, 0, &gs_bt16[0][0] }, LZ16 = { 6, 6, 4, 3, 0, 4, 8, 0x18, &gs_bt16[0][0] },
                 L16S = { 6, 6, 4, 3, 0, 4, 8, 0, &gs_bt16s[0][0] }, LZ16S = { 6, 6, 4, 3, 0, 4, 8, 0x18, &gs_bt16s[0][0] },
                 L8 = { 7, 6, 4, 4, 1, 8, 4, 0, &gs_bt8[0][0] }, L4 = { 7, 7, 5, 4, 1, 4, 8, 0, &gs_bt4[0][0] };
static const Lay *lay(int psm)
{
  switch (psm) {
    case GS_CT16: return &L16;
    case GS_Z16: return &LZ16;
    case GS_CT16S: return &L16S;
    case GS_Z16S: return &LZ16S;
    case GS_T8: return &L8;
    case GS_T4: return &L4;
    case GS_Z32: case GS_Z24: return &LZ32;
    default: return &L32;
  }
}
/* the block of pixel (x, y), 0 <= x, y < 2048, of the buffer at bp, width bw */
static inline uint32_t blk(const Lay *L, uint32_t bp, uint32_t bw, int x, int y)
{
  uint32_t ppr = L->half ? bw >> 1 : bw;
  return ((bp + (((uint32_t)(y >> L->phs) * ppr + (uint32_t)(x >> L->pws)) << 5)
           + L->bt[((y >> L->chs) & (L->rows - 1)) * L->cols + ((x >> L->cws) & (L->cols - 1))]) ^ L->zx) & 0x3fff;
}
static int units(int psm) { return psm == GS_T4 ? 8 : psm == GS_T8 ? 4 : (psm == GS_CT16 || psm == GS_CT16S) ? 2 : 0; }
static int other_ok(int psm, uint32_t bw) { return units(psm) && !((psm == GS_T4 || psm == GS_T8) && (bw & 1)); }
static int unit_xy(const Rec *x, uint32_t rel, int u, int *px, int *py)
{
  uint32_t page = rel >> 5, ppr; int b, p, pw, ph, cw, ch;
  switch (x->psm) {
    case GS_T4: b = ibt4[rel & 31]; p = ict4[u]; pw = 128; ph = 128; cw = 32; ch = 16; ppr = x->bw >> 1; break;
    case GS_T8: b = ibt8[rel & 31]; p = ict8[u]; pw = 128; ph = 64; cw = 16; ch = 16; ppr = x->bw >> 1; break;
    default: b = (x->psm == GS_CT16S ? ibt16s : ibt16)[rel & 31]; p = ict16[u]; pw = 64; ph = 64; cw = 16; ch = 8; ppr = x->bw; break;
  }
  if (!ppr) return 0;
  *px = (int)(page % ppr) * pw + (b & 7) * cw + (p & 31); *py = (int)(page / ppr) * ph + (b >> 3) * ch + (p >> 5);
  return 1;
}
static const uint8_t *rec_bytes(const GuMap *m, const Rec *x, uint32_t off, uint32_t n)
{
#ifdef GU_SENT
  if (m->read_sent) return x->sent ? x->sent + off : 0;
#endif
  return x->own ? x->own + off : ME_CALLP(m->ram)(m->ctx, x->src + off, n);
}

/* ---- records, list nodes, CLUT loads: pools with free lists, swept ---- */
static int rec_new(GuMap *m)
{
  int i;
  if (m->freer >= 0) { i = m->freer; m->freer = m->r[i].next; m->nfreer--; }
  else {
    if (m->nr == m->capr) { int c = m->capr ? m->capr * 2 : 1024; Rec *q = realloc(m->r, (size_t)c * sizeof *q); if (!q) return -1; m->r = q; m->capr = c; }
    i = m->nr++;
  }
  memset(&m->r[i], 0, sizeof m->r[i]); m->r[i].src = GU_NOSRC; m->r[i].next = -1; m->r[i].live = 1; m->st.live_xfers++; DC_(rec_news);
  return i;
}
static void rec_free(GuMap *m, int i)
{
  Rec *x = &m->r[i];
#ifdef GU_SENT
  free(x->sent); x->sent = 0;
#endif
  if (x->own) { m->st.own_bytes -= x->need; free(x->own); x->own = 0; }
  x->live = 0; x->next = m->freer; m->freer = i; m->nfreer++; m->st.live_xfers--;
}
/* a node for record rec over state below */
static int node_new(GuMap *m, int rec, uint32_t below)
{
  int i;
  if (m->freen >= 0) { i = m->freen; m->freen = m->nd[i].rec; m->nfreen--; }
  else {
    if (m->nn == m->capn) { int c = m->capn ? m->capn * 2 : 2048; Node *q = realloc(m->nd, (size_t)c * sizeof *q); if (!q) return -1; m->nd = q; m->capn = c; }
    i = m->nn++;
  }
  m->nd[i].rec = rec; m->nd[i].below = below; m->nd[i].mk = 0; m->nd[i].live = 1; DC_(node_news);
  return i;
}
static int ld_new(GuMap *m)
{
  int i;
  if (m->freel >= 0) { i = m->freel; m->freel = m->l[i].next; m->nfreel--; }
  else {
    if (m->nl == m->capl) { int c = m->capl ? m->capl * 2 : 1024; Ld *q = realloc(m->l, (size_t)c * sizeof *q); if (!q) return -1; m->l = q; m->capl = c; }
    i = m->nl++;
  }
  m->st.live_loads++; DC_(ld_news);
  return i;
}
/* the next record of a state walk: its index; *s moves to the state below it */
static int st_next(const GuMap *m, uint32_t *s)
{
  uint32_t t = *s;
  if (t & S_NODE) { const Node *n = &m->nd[S_IDX(t)]; *s = n->below; return n->rec; }
  *s = 0; return S_IDX(t);
}
/* the state of block bn in lane group g: its own, or its page's if a write of all of the page came after the block's own */
static inline uint32_t st_of(const GuMap *m, uint32_t bn, int g) { return (m->pm[bn >> 5][g] >> (bn & 31) & 1) ? m->bs[bn][g] : m->ps[bn >> 5][g]; }
static inline void st3(const GuMap *m, uint32_t bn, uint32_t *s3) { s3[0] = st_of(m, bn, 0); s3[1] = st_of(m, bn, 1); s3[2] = st_of(m, bn, 2); }
static inline void st_set(GuMap *m, uint32_t bn, int g, uint32_t v) { m->bs[bn][g] = v; m->pm[bn >> 5][g] |= 1u << (bn & 31); }
/* the sweep: what the blocks, the CLUT rows and the current transfer name stays; the rest goes back to the pools, and the table of
   loads forgets the loads only it named */
static void gc_state(GuMap *m, uint32_t s)
{
  while (s) {
    if (!(s & S_NODE)) { m->r[S_IDX(s)].mk = m->gcn; return; }
    Node *n = &m->nd[S_IDX(s)];
    if (n->mk == m->gcn) return;   /* found already, with what is under it */
    n->mk = m->gcn; m->r[n->rec].mk = m->gcn; s = n->below;
  }
}
static void gc_ld(GuMap *m, int li)
{
  if (li < 0) return;
  Ld *L = &m->l[li];
  if (L->mk == m->gcn) return;
  L->mk = m->gcn;
  for (int b = 0; b < L->nb; b++) for (int g = 0; g < 3; g++) gc_state(m, L->s[b][g]);
}
static void gc(GuMap *m)
{
  uint32_t last = 0, v;
  m->gcn++; m->st.gcs++;
  for (int p = 0; p < 512; p++) for (int g = 0; g < 3; g++) {   /* the blocks' states (the same word as the one before: found already) */
    uint32_t own = m->pm[p][g];
    if (~own && (v = m->ps[p][g]) != last) { last = v; if (v) gc_state(m, v); }
    for (; own; own &= own - 1) if ((v = m->bs[p * 32 + __builtin_ctz(own)][g]) != last) { last = v; if (v) gc_state(m, v); }
  }
  for (int r = 0; r < 32; r++) if (m->row[r]) gc_ld(m, ROW_LD(m->row[r]));
  for (int k = 0; k < IT_N; k++) if (m->it[k].li >= 0) gc_ld(m, m->it[k].li);   /* sb9 0b: the loads kept by identity, and their records */
  if (m->cur >= 0) m->r[m->cur].mk = m->gcn;
  for (uint32_t k = 0; k < 1u << RT_BITS; k++) if (m->rt[k] >= 0 && m->l[m->rt[k]].mk != m->gcn) m->rt[k] = -1;
  for (int i = 0; i < m->nr; i++) if (m->r[i].live && m->r[i].mk != m->gcn) rec_free(m, i);
  for (int i = 0; i < m->nn; i++) if (m->nd[i].live && m->nd[i].mk != m->gcn) { m->nd[i].live = 0; m->nd[i].rec = m->freen; m->freen = i; m->nfreen++; }
  for (int i = 0; i < m->nl; i++)
    if (m->l[i].live && m->l[i].mk != m->gcn) { m->l[i].live = 0; m->l[i].next = m->freel; m->freel = i; m->nfreel++; m->st.live_loads--; }
}
static int pool_low(int cap, int n, int nfree, int low) { return cap && cap - n + nfree < low; }
static void *pool_grow(void *p, int *cap, size_t sz) { void *q = realloc(p, (size_t)*cap * 2 * sz); if (!q) return p; *cap *= 2; return q; }
/* at the start of a write (never in one: nothing outside the pools may hold a state word then) */
static void gc_maybe(GuMap *m)
{
  if (!pool_low(m->capr, m->nr, m->nfreer, R_LOW) && !pool_low(m->capn, m->nn, m->nfreen, N_LOW) && !pool_low(m->capl, m->nl, m->nfreel, L_LOW)) return;
#ifdef GU_DEVCNT
  if (pool_low(m->capr, m->nr, m->nfreer, R_LOW)) DC.gc_r++;
  if (pool_low(m->capn, m->nn, m->nfreen, N_LOW)) DC.gc_n++;
  if (pool_low(m->capl, m->nl, m->nfreel, L_LOW)) DC.gc_l++;
#endif
  gc(m);
  if (2 * (m->capr - m->nr + m->nfreer) < m->capr) m->r = pool_grow(m->r, &m->capr, sizeof *m->r);
  if (2 * (m->capn - m->nn + m->nfreen) < m->capn) m->nd = pool_grow(m->nd, &m->capn, sizeof *m->nd);
  if (2 * (m->capl - m->nl + m->nfreel) < m->capl) m->l = pool_grow(m->l, &m->capl, sizeof *m->l);
}

GuMap *gu_new(GuRamFn ram, void *ctx)
{
  GuMap *m = calloc(1, sizeof *m); if (!m) return 0;
  inv_init();
  m->ram = ram; m->ctx = ctx; m->freer = m->freen = m->freel = -1; m->cur = -1; m->cbp0 = m->cbp1 = ~0u; m->memo = m->cmemo = 1;
#ifdef GU_DEVCNT
  dc_map = m;
#endif
#ifdef GU_SENT
  m->watch = -1;
#endif
  for (uint32_t k = 0; k < 1u << RT_BITS; k++) m->rt[k] = -1;
  m->dt_hit[0] = m->dt_hit[1] = -1; m->dl[0].k = m->dl[1].k = -1;
  m->ldid = 1; for (int k = 0; k < IT_N; k++) { m->it[k].li = -1; m->it[k].use = 0; }   /* sb9 0b: on by default (OPT2_GU_LDID) */
  return m;
}
void gu_free(GuMap *m)
{
  if (!m) return;
  for (int i = 0; i < m->nr; i++) {
#ifdef GU_SENT
    free(m->r[i].sent);
#endif
    free(m->r[i].own);
  }
  free(m->r); free(m->nd); free(m->l); free(m->srct); free(m);
}
const GuStats *gu_stats(const GuMap *m) { return &m->st; }
#ifdef GU_DEVCNT
static void dc_pools(const GuMap *m)
{
  fprintf(stderr, "gsup2 count: pools at the end: records %d made, capacity %d, %d free, %d live (%ld taken in all); nodes %d / %d / %d (%ld); "
          "loads %d / %d / %d, %d live (%ld); sweeps %ld\n", m->nr, m->capr, m->nfreer, m->st.live_xfers, DC.rec_news, m->nn, m->capn, m->nfreen, DC.node_news,
          m->nl, m->capl, m->nfreel, m->st.live_loads, DC.ld_news, m->st.gcs);
}
#endif

/* ---- content identities (gu_keys): gsup.c's ---- */
static uint64_t sparse_key(const uint8_t *b, uint32_t n)
{
  uint32_t nw = n / 4, st = nw >= 64 ? nw / 64 : 1; uint64_t h = FNV0;
  Wh wh = wh_start(h); uint32_t w, k;
  for (k = 0; k < 64 && k * st < nw; k++) { memcpy(&w, b + 4 * (k * st), 4); wh_word(&wh, w); }
  h = wh_end(wh, k);
  return h;
}
struct GuSrc { uint32_t src, n; uint64_t key; uint8_t used, keyed, vol; };
static uint32_t src_slot(uint32_t src, uint32_t n, uint32_t cap) { return (src * 2654435761u ^ n * 40503u) & (cap - 1); }
static struct GuSrc *src_of(GuMap *m, uint32_t src, uint32_t n)
{
  if (2 * (m->nsrc + 1) > m->capsrc) {
    uint32_t c = m->capsrc ? 2 * m->capsrc : 256; struct GuSrc *t = calloc(c, sizeof *t); if (!t) return 0;
    for (uint32_t i = 0; i < m->capsrc; i++) if (m->srct[i].used) {
      uint32_t j = src_slot(m->srct[i].src, m->srct[i].n, c); while (t[j].used) j = (j + 1) & (c - 1); t[j] = m->srct[i]; }
    free(m->srct); m->srct = t; m->capsrc = c;
  }
  uint32_t j = src_slot(src, n, m->capsrc);
  while (m->srct[j].used && (m->srct[j].src != src || m->srct[j].n != n)) j = (j + 1) & (m->capsrc - 1);
  if (!m->srct[j].used) { m->srct[j].used = 1; m->srct[j].src = src; m->srct[j].n = n; m->nsrc++; }
  return &m->srct[j];
}
static void rec_ident(GuMap *m, Rec *x, int fin)
{
  uint32_t n = x->got < x->need ? x->got : x->need; uint64_t k = 0;
  const uint8_t *s = n ? ME_CALLP(m->ram)(m->ctx, x->src, n) : 0;
  DC_(ident_calls);
  if (n && !s) x->bad = 1;
  if (s) {
    struct GuSrc *so = src_of(m, x->src, x->need); int vol = 0;
    k = sparse_key(s, n);
#ifdef GU_DEVCNT
    DC.ident_sparse_b += n / 4 >= 64 ? 256 : (n / 4) * 4;
#endif
    if (so) { if (fin) { if (so->keyed && so->key != k) so->vol = 1; so->key = k; so->keyed = 1; } vol = so->vol; }
    if (vol) k = whash(FNV0 ^ 1, s, n);
#ifdef GU_DEVCNT
    if (vol) DC.ident_full_b += n;
#endif
  }
  uint64_t h = mix(FNV0, (uint64_t)(uint32_t)x->psm << 48 | (uint64_t)x->bp << 16 | x->bw);
  h = mix(h, (uint64_t)(uint32_t)x->sx << 48 | (uint64_t)(uint32_t)x->sy << 32 | (uint64_t)(uint32_t)x->w << 16 | (uint32_t)x->h);
  h = mix(mix(h, (uint64_t)x->src << 32 | x->need), (uint64_t)n << 32 ^ k);
  x->id = h | 1; x->idgot = x->got;
}
void gu_keys(GuMap *m, int on) { m->keys = on; }
void gu_area_memo(GuMap *m, int on) { m->memo = (uint8_t)(on != 0); }
void gu_clut_memo(GuMap *m, int on) { m->cmemo = (uint8_t)(on != 0); }
void gu_zero_empty(GuMap *m, int on) { m->zero_empty = on; }
uint32_t gu_block_ver(const GuMap *m, uint32_t bn) { return m->pg[(bn & 0x3fff) >> 5]; }
/* the identity of a state: its records newest first (a drawn area and a lost one by kind) */
static uint64_t state_id(GuMap *m, uint32_t s, int *f)
{
  uint64_t h = FNV0;
  while (s) {
    Rec *x = &m->r[st_next(m, &s)];
    if (x->drawn) { *f |= GU_ID_DRAWN; h = mix(h, 0xd7a300u); continue; }
    if (!x->own && !x->bad && x->bw && m->keys && (!x->id || x->idgot != x->got)) rec_ident(m, x, 0);
    if (x->bad || !x->bw) { *f |= GU_ID_LOST; h = mix(h, 0x10500u); continue; }
    h = mix(h, x->id);
  }
  return h;
}
static uint64_t block_id(GuMap *m, uint32_t bn, uint32_t lanes, int *f)
{
  uint64_t h = FNV0;
  for (int g = 0; g < 3; g++) if (lanes >> g & 1) h = mix(mix(h, state_id(m, st_of(m, bn & 0x3fff, g), f)), (uint64_t)g);
  return h;
}
/* block_id of a block whose state words in lanes are s3 */
static uint64_t states_id(GuMap *m, const uint32_t *s3, uint32_t lanes, int *f)
{
  uint64_t h = FNV0;
  for (int g = 0; g < 3; g++) if (lanes >> g & 1) h = mix(mix(h, state_id(m, s3[g], f)), (uint64_t)g);
  return h;
}

/* ---- writes into the blocks ---- */
static int same_rect(const Rec *a, const Rec *b)
{ return a->psm == b->psm && a->bp == b->bp && a->bw == b->bw && a->sx == b->sx && a->sy == b->sy && a->w == b->w && a->h == b->h; }
/* state s without the records of x's rectangle, which x hides (gsup.c's rule for a write of part of a block) */
static uint32_t unhide(GuMap *m, uint32_t s, const Rec *x)
{
  uint32_t ch[16]; int n = 0, last = -1;
  for (uint32_t t = s; t && n < 16; n++) { ch[n] = t; int i = st_next(m, &t); if (same_rect(&m->r[i], x)) last = n; }
  if (last < 0) return s;
  uint32_t base = (ch[last] & S_NODE) ? m->nd[S_IDX(ch[last])].below : 0;
  for (int j = last - 1; j >= 0; j--) {   /* the nodes above the last hidden one, again, less the hidden ones */
    int i = m->nd[S_IDX(ch[j])].rec;
    if (same_rect(&m->r[i], x)) continue;
    int k = node_new(m, i, base); if (k < 0) return s;   /* no memory: the list stays as it was */
    base = S_NOD(k);
  }
  return base;
}
/* a write that is not a draw, in lanes ln of block bn: the drawn areas whose lanes it takes there take the block back at their next draw,
   and stop watching it till then */
static void told(GuMap *m, uint32_t bn, uint32_t ln)
{
  for (uint32_t mk = m->dtm[bn]; mk; mk &= mk - 1) {
    int k = __builtin_ctz(mk); DtE *e = &m->dt[k]; DC_(dtm_iter);
    if (!(e->lanes & ln)) continue;
#ifdef GU_DEVCNT
    if (e->dirty) DC.dtm_already++; else DC.dtm_set++;
#endif
    e->dirty = 1; m->dtm[bn] &= (uint16_t)~(1u << k);
    if (e->ntl < TL_N) e->tl[e->ntl++] = (uint16_t)bn; else e->over = 1;
  }
}
/* record xi writes block bn in its lane groups: all of the block in a group (full, or a whole record) or part of it */
static void put(GuMap *m, uint32_t bn, int xi, int full)
{
  const Rec *x = &m->r[xi]; uint32_t ln = x->lanes;
#ifdef GU_DEVCNT
  if (x->drawn) DC.put_draw++; else if (full || x->whole) DC.put_full++; else DC.put_part++;
#endif
  if (m->dtm[bn] && !x->drawn) told(m, bn, ln);
  if (full || x->whole) { uint32_t v = S_REC(xi); for (int g = 0; g < 3; g++) if (ln >> g & 1) st_set(m, bn, g, v); }
  else for (int g = 0; g < 3; g++) {
    if (!(ln >> g & 1)) continue;
    uint32_t b = unhide(m, st_of(m, bn, g), x); int k = node_new(m, xi, b);
    if (k < 0) { st_set(m, bn, g, b); m->st.dropped++; continue; }   /* no memory for the node: the block keeps what it had */
    st_set(m, bn, g, S_NOD(k));
  }
  m->pg[bn >> 5] = m->seq;
#ifdef GU_SENT
  if ((int)bn == m->watch) { char buf[1024]; gu_why(m, bn << 6, buf, sizeof buf);
    fprintf(stderr, "gsup2 watch: record %d (transfer %u%s) %s: %s\n", xi, x->no, x->drawn ? ", drawn" : "", full ? "full" : "part", buf); }
#endif
}
/* record xi writes all of page p (the blocks of a page of its rectangle, at a page-aligned buffer) */
static void put_page(GuMap *m, uint32_t p, int xi)
{
  const Rec *x = &m->r[xi]; uint32_t ln = x->lanes, v = S_REC(xi);
  DC_(put_page);
  if (m->pdt[p] && !x->drawn) {   /* drawn areas may have blocks here */
    uint16_t any = 0;
    for (uint32_t j = 0; j < 32; j++) { uint32_t bn = p * 32 + j; if (m->dtm[bn]) told(m, bn, ln); any |= m->dtm[bn]; }
    m->pdt[p] = any;
  }
  for (int g = 0; g < 3; g++) if (ln >> g & 1) { m->ps[p][g] = v; m->pm[p][g] = 0; }
  m->pg[p] = m->seq;
#ifdef GU_SENT
  if (m->watch >= 0 && (uint32_t)m->watch >> 5 == p) { char buf[1024]; gu_why(m, (uint32_t)m->watch << 6, buf, sizeof buf);
    fprintf(stderr, "gsup2 watch: record %d (transfer %u%s) page: %s\n", xi, x->no, x->drawn ? ", drawn" : "", buf); }
#endif
}
static void cover(GuMap *m, int xi)
{
  const Rec *x = &m->r[xi]; const Lay L = *lay(x->psm); int sx = x->sx, sy = x->sy, w = x->w, h = x->h; uint32_t bp = x->bp, bw = x->bw;
  int cw = 1 << L.cws, ch = 1 << L.chs, pw = 1 << L.pws, ph = 1 << L.phs, x1 = sx + w > 2048 ? 2048 : sx + w, y1 = sy + h > 2048 ? 2048 : sy + h;
#ifdef GU_DEVCNT
  { int pw = 1 << L.pws, ph = 1 << L.phs, fx0 = (sx + pw - 1) & -pw, fx1 = x1 & -pw, fy0 = (sy + ph - 1) & -ph, fy1 = y1 & -ph; long nb = 0;
    for (int cy = sy & -ch; cy < y1; cy += ch) for (int cx = sx & -cw; cx < x1; cx += cw) nb++;
    DC.cov_blocks += nb; if (fx1 > fx0 && fy1 > fy0) DC.cov_pageblocks += (long)((fx1 - fx0) >> L.pws) * ((fy1 - fy0) >> L.phs) * 32;
    dh_add(x->psm, w, h, x->lanes, x->own ? 1 : x->bad ? 2 : 0, nb); }
#endif
  for (int py = sy & -ph; py < y1; py += ph) {   /* page by page: a page all in the rectangle in one write */
    int pfy = py >= sy && py + ph <= sy + h, cy0 = py > (sy & -ch) ? py : sy & -ch, cy1 = py + ph < y1 ? py + ph : y1;
    for (int px = sx & -pw; px < x1; px += pw) {
      if (pfy && !(bp & 31) && px >= sx && px + pw <= sx + w) { put_page(m, blk(&L, bp, bw, px, py) >> 5, xi); continue; }
      int cx0 = px > (sx & -cw) ? px : sx & -cw, cx1 = px + pw < x1 ? px + pw : x1;
      for (int cy = cy0; cy < cy1; cy += ch) {
        int fy = cy >= sy && cy + ch <= sy + h;
        for (int cx = cx0; cx < cx1; cx += cw) put(m, blk(&L, bp, bw, cx, cy), xi, fy && cx >= sx && cx + cw <= sx + w);
      }
    }
  }
}
/* the pages (32 blocks) a rectangle of a buffer can touch: pg0 and how many (512: all) */
static void area_pages(int psm, uint32_t bp, uint32_t bw, int x0, int y0, int x1, int y1, int *pg0, int *npg)
{
  int pw = 64, ph = 32; uint32_t ppr = bw;
  switch (psm) {
    case GS_T4: pw = 128; ph = 128; ppr = bw >> 1; break;
    case GS_T8: pw = 128; ph = 64; ppr = bw >> 1; break;
    case GS_CT16: case GS_CT16S: case GS_Z16: case GS_Z16S: ph = 64; break;
  }
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > 2047) x1 = 2047;
  if (y1 > 2047) y1 = 2047;
  if (x1 < x0 || y1 < y0) { *pg0 = (int)(bp >> 5); *npg = 1; return; }
  uint32_t a = (uint32_t)(y0 / ph) * ppr + (uint32_t)(x0 / pw), b = (uint32_t)(y1 / ph) * ppr + (uint32_t)(x1 / pw);
  uint32_t lo = (bp >> 5) + a, hi = ((bp + 31) >> 5) + b;
  if (hi - lo >= 511) { *pg0 = 0; *npg = 512; return; }
  *pg0 = (int)(lo & 511); *npg = (int)(hi - lo + 1);
}
/* transfer xi goes into the blocks (gsup.c covered it at TRXDIR) */
static void commit(GuMap *m, int xi)
{
  Rec *x = &m->r[xi];
  if (x->in) return;
  if (x->got < x->need && !x->bad) m->st.early++;
  x->in = 1; m->seq++; DC_(commits);
  cover(m, xi);
}
/* the current transfer is not in the blocks yet and the map is read or written: it goes in as gsup.c would hold it now */
static void settle(GuMap *m) { if (m->cur >= 0 && !m->r[m->cur].in) commit(m, m->cur); }

/* ---- words ---- */
/* the bits of word wa that record x holds, of bits (not done yet): gsup.c's word_list for one entry */
static void entry(const GuMap *m, const Rec *x, uint32_t bits, uint32_t wa, uint32_t *v, uint32_t *kn, uint32_t *done)
{
  uint32_t bn = wa >> 6 & 0x3fff; int wi = wa & 63;
  if (!bits) return;
  if (x->whole || !x->bw) { *done |= bits; return; }   /* its words read as missing */
  int nu = units(x->psm);
  if (nu) {   /* another layout: each nibble, byte or half of the word is a pixel of its own */
    uint32_t rel = (bn - x->bp) & 0x3fff; int ub = 32 / nu;
    for (int u = 0; u < nu; u++) {
      uint32_t um = (((1u << ub) - 1) << (u * ub)) & bits; int px, py;
      if (!um) continue;
      if (!unit_xy(x, rel, wi * nu + u, &px, &py)) { *done |= um; continue; }
      if (px < x->sx || px >= x->sx + x->w || py < x->sy || py >= x->sy + x->h) continue;
      uint32_t pi = (uint32_t)(py - x->sy) * (uint32_t)x->w + (uint32_t)(px - x->sx), off = nu == 8 ? pi >> 1 : nu == 4 ? pi : 2 * pi, nb = nu == 2 ? 2 : 1;
      if (!x->bad && off + nb > x->got) continue;
      *done |= um;
      if (x->bad) continue;
      const uint8_t *s = rec_bytes(m, x, off, nb); if (!s) continue;
      uint32_t val = nu == 8 ? (uint32_t)(s[0] >> ((pi & 1) * 4) & 15) : nu == 4 ? s[0] : (uint32_t)(s[0] | s[1] << 8);
      *v |= val << (u * ub) & um; *kn |= um;
    }
    return;
  }
  uint32_t rel = (bn - x->bp) & 0x3fff, page = rel >> 5; int b = ibt32[rel & 31], p = ict32[wi];
  int px = (int)(page % x->bw) * 64 + (b & 7) * 8 + (p & 7), py = (int)(page / x->bw) * 32 + (b >> 3) * 8 + (p >> 3);
  if (px < x->sx || px >= x->sx + x->w || py < x->sy || py >= x->sy + x->h) return;
  uint32_t pi = (uint32_t)(py - x->sy) * (uint32_t)x->w + (uint32_t)(px - x->sx), off, nb, val;
  switch (x->psm) {
    case GS_CT32: off = 4 * pi; nb = 4; break;
    case GS_CT24: off = 3 * pi; nb = 3; break;
    case GS_T8H: off = pi; nb = 1; break;
    default: off = pi >> 1; nb = 1; break;
  }
  if (!x->bad && off + nb > x->got) return;
  *done |= bits;
  if (x->bad) return;
  const uint8_t *s = rec_bytes(m, x, off, nb); if (!s) return;
  switch (x->psm) {
    case GS_CT32: val = s[0] | s[1] << 8 | s[2] << 16 | (uint32_t)s[3] << 24; break;
    case GS_CT24: val = s[0] | s[1] << 8 | (uint32_t)s[2] << 16; break;
    case GS_T8H: val = (uint32_t)s[0] << 24; break;
    default: val = (uint32_t)(s[0] >> ((pi & 1) * 4) & 15) << (x->psm == GS_T4HL ? 24 : 28); break;
  }
  *v |= val & bits; *kn |= bits;
}
/* word wa through the three state words of its block (now, or as a CLUT load kept them) */
static uint32_t words3(const GuMap *m, const uint32_t *s3, uint32_t wa, uint32_t *known)
{
  uint32_t v = 0, kn = 0, done = 0;
  if (s3[0] && s3[0] == s3[1] && s3[0] == s3[2] && !(s3[0] & S_NODE)) entry(m, &m->r[S_IDX(s3[0])], 0xffffffffu, wa, &v, &kn, &done);
  else for (int g = 0; g < 3; g++)
    for (uint32_t s = s3[g]; s && (done & GB[g]) != GB[g]; ) { int i = st_next(m, &s); entry(m, &m->r[i], GB[g] & ~done, wa, &v, &kn, &done); }
  if (m->zero_empty) kn |= ~done;   /* no record: zero since the boot's GS clear (this map never loses a record) */
  *known = kn; return v;
}
uint32_t gu_word(const GuMap *cm, uint32_t wa, uint32_t *known)
{ GuMap *m = (GuMap *)cm; uint32_t s3[3]; settle(m); st3(m, wa >> 6 & 0x3fff, s3); return words3(m, s3, wa, known); }
/* bits gb of block bn's 64 words from record x, when x alone gives them all: a CT32-layout upload (CT32, CT24, T8H, T4HL, T4HH) that
   wrote all of the block and has all its bytes, so entry would mark every word done from x.  entry's values, 8 pixel rows at a time
   (rec_bytes is linear: own, sent and the guest RAM callbacks).  0 (nothing written): not this case */
/* blk_rec for a CT16 / CT16S upload that wrote all of the block and has all its bytes (task #83): entry's "another layout" values, half
   by half (unit 2i + h is half h of word i, pixel ict16[unit] of the block), from the block's 8 rows of 16 pixels */
static int blk_rec16(const GuMap *m, const Rec *x, uint32_t bn, uint32_t gb, uint32_t *v, uint32_t *kn, uint32_t *done)
{
  if (x->whole || !x->bw || x->bad) return 0;
  uint32_t rel = (bn - x->bp) & 0x3fff, page = rel >> 5; int b = (x->psm == GS_CT16S ? ibt16s : ibt16)[rel & 31];
  int px = (int)(page % x->bw) * 64 + (b & 7) * 16, py = (int)(page / x->bw) * 64 + (b >> 3) * 8;
  if (px < x->sx || px + 16 > x->sx + x->w || py < x->sy || py + 8 > x->sy + x->h) return 0;
  uint32_t p0 = (uint32_t)(py - x->sy) * (uint32_t)x->w + (uint32_t)(px - x->sx), pr = (uint32_t)x->w, pl = p0 + 7 * pr + 15;   /* pixels */
  if (2 * pl + 2 > x->got) return 0;   /* entry's off + nb <= got, for the last pixel */
  const uint8_t *r[8];
  for (int k = 0; k < 8; k++) if (!(r[k] = rec_bytes(m, x, 2 * (p0 + (uint32_t)k * pr), 32))) return 0;
  for (int i = 0; i < 64; i++) {
    int p = ict16[2 * i], q = ict16[2 * i + 1]; const uint8_t *s = r[p >> 5] + 2 * (p & 31), *u = r[q >> 5] + 2 * (q & 31);
    uint32_t val = (uint32_t)(s[0] | s[1] << 8) | (uint32_t)(u[0] | u[1] << 8) << 16;
    v[i] |= val & gb; kn[i] |= gb; done[i] |= gb;
  }
  return 1;
}
static int blk_rec(const GuMap *m, const Rec *x, uint32_t bn, uint32_t gb, uint32_t *v, uint32_t *kn, uint32_t *done)
{
  int sz;   /* bytes a pixel; 0: a nibble */
  switch (x->psm) { case GS_CT32: sz = 4; break; case GS_CT24: sz = 3; break; case GS_T8H: sz = 1; break; case GS_T4HL: case GS_T4HH: sz = 0; break;
    case GS_CT16: case GS_CT16S: return blk_rec16(m, x, bn, gb, v, kn, done);
    default: return 0; }
  if (x->whole || !x->bw || x->bad) return 0;
  uint32_t rel = (bn - x->bp) & 0x3fff, page = rel >> 5; int b = ibt32[rel & 31];
  int px = (int)(page % x->bw) * 64 + (b & 7) * 8, py = (int)(page / x->bw) * 32 + (b >> 3) * 8;
  if (px < x->sx || px + 8 > x->sx + x->w || py < x->sy || py + 8 > x->sy + x->h) return 0;
  uint32_t p0 = (uint32_t)(py - x->sy) * (uint32_t)x->w + (uint32_t)(px - x->sx), pr = (uint32_t)x->w, pl = p0 + 7 * pr + 7;   /* pixels */
  if ((sz ? sz * pl + sz : (pl >> 1) + 1) > x->got) return 0;   /* entry's off + nb <= got, for the last pixel */
  const uint8_t *r[8];
  for (int k = 0; k < 8; k++) { uint32_t q = p0 + (uint32_t)k * pr;
    if (!(r[k] = sz ? rec_bytes(m, x, sz * q, 8 * (uint32_t)sz) : rec_bytes(m, x, q >> 1, ((q + 7) >> 1) - (q >> 1) + 1))) return 0; }
  int t4 = x->psm == GS_T4HL ? 24 : 28;
  for (int i = 0; i < 64; i++) {
    int j = ict32[i] & 7, k = ict32[i] >> 3; const uint8_t *s = r[k] + sz * j; uint32_t val;
    switch (sz) {
      case 4: val = s[0] | s[1] << 8 | s[2] << 16 | (uint32_t)s[3] << 24; break;
      case 3: val = s[0] | s[1] << 8 | (uint32_t)s[2] << 16; break;
      case 1: val = (uint32_t)s[0] << 24; break;
      default: { uint32_t q = p0 + (uint32_t)k * pr, pi = q + (uint32_t)j; val = (uint32_t)(r[k][(pi >> 1) - (q >> 1)] >> ((pi & 1) * 4) & 15) << t4; }
    }
    v[i] |= val & gb; kn[i] |= gb; done[i] |= gb;
  }
  return 1;
}
void gu_block(const GuMap *cm, uint32_t bn, uint32_t need, uint32_t v[64], uint32_t known[64])
{
  GuMap *m = (GuMap *)cm; uint32_t s3[3], done[64]; int one = 1; bn &= 0x3fff; settle(m); st3(m, bn, s3); m->st.blocks++;
  memset(v, 0, 64 * sizeof *v); memset(known, 0, 64 * sizeof *known); memset(done, 0, sizeof done);
  int all = s3[0] == s3[1] && s3[0] == s3[2];   /* one state for every lane: one pass with all the bits (words3's) */
  for (int g = 0; g < 3; g++) {   /* the lane groups apart: each its own state's records, top first, as words3 */
    uint32_t gb = (all ? ~0u : GB[g]) & need, s = s3[all ? 0 : g];
    if (gb && s && !blk_rec(m, &m->r[(s & S_NODE) ? m->nd[S_IDX(s)].rec : S_IDX(s)], bn, gb, v, known, done)) {   /* (a top record that
                                                                                     gives every word ends the list's walk) */
      one = 0;
      for (uint32_t i = 0; i < 64; i++)
        for (uint32_t t = s; t && (done[i] & gb) != gb; ) { int ri = st_next(m, &t); entry(m, &m->r[ri], gb & ~done[i], bn << 6 | i, &v[i], &known[i], &done[i]); }
    }
    if (all) break;
  }
  for (int i = 0; i < 64; i++) known[i] = (m->zero_empty ? known[i] | ~done[i] : known[i]) & need;
  if (one) m->st.blocks_one++;
}
int gu_index(const GuMap *m, int psm, uint32_t tbp, uint32_t tbw, int x, int y)
{
  uint32_t a, wa, kn; int sh, nb;
  x &= 2047; y &= 2047;
  switch (psm) {
    case GS_T8: a = gs_pa8(tbp, tbw, x, y); wa = a >> 2; sh = (int)(a & 3) * 8; nb = 8; break;
    case GS_T4: a = gs_pa4(tbp, tbw, x, y); wa = a >> 3; sh = (int)(a & 7) * 4; nb = 4; break;
    case GS_T8H: wa = gs_pa32(tbp, tbw, x, y, 0) >> 2; sh = 24; nb = 8; break;
    case GS_T4HL: wa = gs_pa32(tbp, tbw, x, y, 0) >> 2; sh = 24; nb = 4; break;
    case GS_T4HH: wa = gs_pa32(tbp, tbw, x, y, 0) >> 2; sh = 28; nb = 4; break;
    default: return -1;
  }
  uint32_t v = gu_word(m, wa, &kn), mk = ((1u << nb) - 1) << sh;
  return (kn & mk) == mk ? (int)((v & mk) >> sh) : -1;
}
int64_t gu_texel(const GuMap *m, int psm, uint32_t tbp, uint32_t tbw, int x, int y)
{
  uint32_t a, v, kn, mk; int sh;
  x &= 2047; y &= 2047;
  switch (psm) {
    case GS_CT32: case GS_CT24: v = gu_word(m, gs_pa32(tbp, tbw, x, y, 0) >> 2, &kn); mk = psm == GS_CT32 ? 0xffffffffu : 0xffffffu;
      return (kn & mk) == mk ? (int64_t)(v & mk) : -1;
    case GS_CT16: case GS_CT16S: a = gs_pa16(tbp, tbw, x, y, psm == GS_CT16S, 0); v = gu_word(m, a >> 2, &kn); sh = (a & 2) ? 16 : 0; mk = 0xffffu << sh;
      return (kn & mk) == mk ? (int64_t)((v & mk) >> sh) : -1;
    default: return gu_index(m, psm, tbp, tbw, x, y);
  }
}

/* ---- identities of areas ---- */
static int psm_bits(int psm)
{
  switch (psm) { case GS_CT32: case GS_Z32: return 32; case GS_CT24: case GS_Z24: return 24; case GS_CT16: case GS_CT16S: case GS_Z16: case GS_Z16S: return 16;
                 case GS_T8: case GS_T8H: return 8; default: return 4; }
}
static uint32_t psm_lanes(int psm)
{ return psm == GS_CT32 ? 7 : psm == GS_CT24 ? 1 : psm == GS_T8H ? 6 : psm == GS_T4HL ? 2 : psm == GS_T4HH ? 4 : 0; }
uint64_t gu_lanes_id(GuMap *m, uint32_t bn, uint32_t lanes, int *flags) { int f = 0; settle(m); uint64_t h = block_id(m, bn, lanes, &f); *flags = f; return h; }
uint64_t gu_area_id(GuMap *m, int psm, uint32_t bp, uint32_t bw, int x0, int y0, int x1, int y1, int *flags)
{
  uint32_t lanes = psm == GS_Z24 ? 1 : psm_lanes(psm); const Lay L = *lay(psm); int cw = 1 << L.cws, ch = 1 << L.chs, f = 0; uint64_t h = FNV0;
  if (!lanes) lanes = 7;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > 2047) x1 = 2047;
  if (y1 > 2047) y1 = 2047;
  settle(m);
  uint64_t kh = mix(mix(FNV0, (uint64_t)(uint32_t)psm << 48 | (uint64_t)bp << 16 | bw), (uint64_t)(uint32_t)x0 << 48 | (uint64_t)(uint32_t)y0 << 32 | (uint64_t)(uint32_t)x1 << 16 | (uint32_t)y1);
  AcE *e = &m->ac[(kh >> 40) & (AC_N - 1)];
  if (e->used && e->psm == psm && e->bp == bp && e->bw == bw && e->x0 == x0 && e->y0 == y0 && e->x1 == x1 && e->y1 == y1 && e->epoch == m->epoch) {
    int k; for (k = 0; k < e->npg && (int32_t)(m->pg[(e->pg0 + k) & 511] - e->seq) <= 0; k++) {}
    if (k == e->npg) { m->st.areas_kept++; *flags = e->flags; return e->id; }
  }
  if (m->memo) {   /* a block whose state words are the cell before's has its identity and flags (block_id is a function of them; a
                      state's identity, once made, stays until a write replaces the state, and making it again changes nothing) */
    uint32_t ls[3] = { ~0u, ~0u, ~0u }, s3[3]; uint64_t lb = 0; int lf = 0;
    for (int cy = y0 & -ch; cy <= y1; cy += ch)
      for (int cx = x0 & -cw; cx <= x1; cx += cw) {
        uint32_t bn = blk(&L, bp, bw, cx, cy);
        s3[0] = (lanes & 1) ? st_of(m, bn, 0) : 0; s3[1] = (lanes & 2) ? st_of(m, bn, 1) : 0; s3[2] = (lanes & 4) ? st_of(m, bn, 2) : 0;
        if (s3[0] != ls[0] || s3[1] != ls[1] || s3[2] != ls[2]) { lf = 0; lb = states_id(m, s3, lanes, &lf); ls[0] = s3[0]; ls[1] = s3[1]; ls[2] = s3[2]; }
        f |= lf; h = mix(h, lb);
      }
#ifdef GU_MEMO_CHECK
    { int f2 = 0; uint64_t h2 = FNV0;
      for (int cy = y0 & -ch; cy <= y1; cy += ch) for (int cx = x0 & -cw; cx <= x1; cx += cw) h2 = mix(h2, block_id(m, blk(&L, bp, bw, cx, cy), lanes, &f2));
      MC.area++; MC.area_bad += h2 != h || f2 != f; }
#endif
  } else
    for (int cy = y0 & -ch; cy <= y1; cy += ch)
      for (int cx = x0 & -cw; cx <= x1; cx += cw) h = mix(h, block_id(m, blk(&L, bp, bw, cx, cy), lanes, &f));
  e->used = 1; e->psm = psm; e->bp = bp; e->bw = bw; e->x0 = x0; e->y0 = y0; e->x1 = x1; e->y1 = y1; e->epoch = m->epoch; e->seq = m->seq;
  e->id = h; e->flags = f; area_pages(psm, bp, bw, x0, y0, x1, y1, &e->pg0, &e->npg); m->st.areas_made++;
  *flags = f; return h;
}
/* the flags a state's records give (state_id's, its identities made as state_id makes them) */
static int state_flags(GuMap *m, uint32_t s)
{
  int f = 0;
  while (s) {
    Rec *x = &m->r[st_next(m, &s)];
    if (x->drawn) { f |= GU_ID_DRAWN; continue; }
    if (!x->own && !x->bad && x->bw && m->keys && (!x->id || x->idgot != x->got)) rec_ident(m, x, 0);
    if (x->bad || !x->bw) f |= GU_ID_LOST;
  }
  return f;
}
int gu_area_flags(GuMap *m, int psm, uint32_t bp, uint32_t bw, int x0, int y0, int x1, int y1)
{
  uint32_t lanes = psm == GS_Z24 ? 1 : psm_lanes(psm); const Lay L = *lay(psm);
  int cw = 1 << L.cws, ch = 1 << L.chs, pw = 1 << L.pws, ph = 1 << L.phs, f = 0;
  if (!lanes) lanes = 7;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > 2047) x1 = 2047;
  if (y1 > 2047) y1 = 2047;
  settle(m);
  int cx0 = x0 & -cw, cy0 = y0 & -ch, cx1 = (x1 & -cw) + cw, cy1 = (y1 & -ch) + ch;   /* the cells' union: [cx0, cx1) x [cy0, cy1) */
  uint32_t ls[3] = { ~0u, ~0u, ~0u };   /* the state words seen last in each lane group (their flags are in f already) */
  for (int py = cy0 & -ph; py < cy1; py += ph)
    for (int px = cx0 & -pw; px < cx1; px += pw) {
      if (!(bp & 31) && px >= cx0 && px + pw <= cx1 && py >= cy0 && py + ph <= cy1) {   /* all 32 blocks of page p */
        uint32_t p = blk(&L, bp, bw, px, py) >> 5;
        for (int g = 0; g < 3; g++) {
          if (!(lanes >> g & 1)) continue;
          uint32_t own = m->pm[p][g], v;
          if (~own && (v = m->ps[p][g]) != ls[g]) { ls[g] = v; f |= state_flags(m, v); }   /* the page's state: blocks without their own */
          for (; own; own &= own - 1) if ((v = m->bs[p * 32 + __builtin_ctz(own)][g]) != ls[g]) { ls[g] = v; f |= state_flags(m, v); }
        }
        continue;
      }
      int ya = py > cy0 ? py : cy0, yb = py + ph < cy1 ? py + ph : cy1, xa = px > cx0 ? px : cx0, xb = px + pw < cx1 ? px + pw : cx1;
      for (int cy = ya; cy < yb; cy += ch)
        for (int cx = xa; cx < xb; cx += cw) {
          uint32_t bn = blk(&L, bp, bw, cx, cy), v;
          for (int g = 0; g < 3; g++) if ((lanes >> g & 1) && (v = st_of(m, bn, g)) != ls[g]) { ls[g] = v; f |= state_flags(m, v); }
        }
    }
#ifdef GU_MEMO_CHECK
  { int f2 = 0; for (int cy = y0 & -ch; cy <= y1; cy += ch) for (int cx = x0 & -cw; cx <= x1; cx += cw) block_id(m, blk(&L, bp, bw, cx, cy), lanes, &f2);
    MC.flags++; MC.flags_bad += f2 != f; }
#endif
  return f;
}

/* ---- sb9 step 0a (stagec_notes.md 0.3): what tells a texture's region by its upload records ---- */
void gu_settle(GuMap *m) { settle(m); }
/* the pages (32 blocks) of the cells gu_area_id walks for that area, with the blocks of each it reads (bit j: block 32 p + j), and its
   lane groups: how many pages (at most max), or -1 (more, or none) */
int gu_area_pages(int psm, uint32_t bp, uint32_t bw, int x0, int y0, int x1, int y1, uint16_t *pg, uint32_t *mk, int max, uint32_t *lanesp)
{
  uint32_t lanes = psm == GS_Z24 ? 1 : psm_lanes(psm); const Lay L = *lay(psm); int cw = 1 << L.cws, ch = 1 << L.chs, n = 0;
  if (!lanes) lanes = 7;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > 2047) x1 = 2047;
  if (y1 > 2047) y1 = 2047;
  for (int cy = y0 & -ch; cy <= y1; cy += ch)
    for (int cx = x0 & -cw; cx <= x1; cx += cw) {
      uint32_t bn = blk(&L, bp, bw, cx, cy), p = bn >> 5; int k;
      for (k = 0; k < n && pg[k] != p; k++) {}
      if (k == n) { if (n == max) return -1; pg[n] = (uint16_t)p; mk[n] = 0; n++; }
      mk[k] |= 1u << (bn & 31);
    }
  *lanesp = lanes; return n ? n : -1;
}
/* the identity of what those blocks hold, after settling the map: 0 and *sig when, in each page and lane group, every block read holds
   the same state and that state is empty or one record that is plain (not drawn, not lost) and final (its identity made from all its
   bytes, or a local copy); then gu_area_id's identity of any area whose cells are those blocks is a function of the area and *sig, its
   flags 0, and making it would make no record's identity.  Else why not: 1 keys off, 2 a page's blocks hold different states, 3 a list
   of records, 4 a drawn area, 5 a lost record, 6 a record whose identity is not final */
int gu_pages_sig(GuMap *m, uint32_t lanes, int n, const uint16_t *pg, const uint32_t *mk, uint64_t *sig)
{
  uint64_t h = FNV0;
  if (!m->keys) return 1;
  settle(m);
  for (int k = 0; k < n; k++) {
    uint32_t p = pg[k], msk = mk[k];
    for (int g = 0; g < 3; g++) {
      if (!(lanes >> g & 1)) continue;
      uint32_t own = m->pm[p][g] & msk, s;
      if (!own) s = m->ps[p][g];
      else if (own != msk) return 2;
      else { uint32_t b = msk; s = m->bs[p * 32 + (uint32_t)__builtin_ctz(b)][g];
             for (b &= b - 1; b; b &= b - 1) if (m->bs[p * 32 + (uint32_t)__builtin_ctz(b)][g] != s) return 2; }
      uint64_t v = 0;
      if (s) {
        if (s & S_NODE) return 3;
        const Rec *x = &m->r[S_IDX(s)];
        if (x->drawn) return 4;
        if (x->bad || !x->bw) return 5;
        if (!x->own && (!x->id || x->idgot != x->got || x->got < x->need)) return 6;
        v = x->id;
      }
      h = mix(h, v);
    }
  }
  *sig = h; return 0;
}

/* ---- transfers ---- */
/* one texel of a local copy's source as local_copy reads it (an index or 16-bit texel, or a 32-bit word): 0 when the map does not know
   all of its bits */
static inline int lc_src(const GuMap *m, int spsm, uint32_t sbp, uint32_t sbw, int texel, int sx, int sy, uint32_t *v)
{
  if (texel) { int64_t k = gu_texel(m, spsm, sbp, sbw, sx, sy); if (k < 0) return 0; *v = (uint32_t)k; return 1; }
  uint32_t kn, mk = spsm == GS_CT24 ? 0xffffffu : 0xffffffffu; *v = gu_word(m, gs_pa32(sbp, sbw, sx, sy, 0) >> 2, &kn); return (kn & mk) == mk;
}
static uint8_t *local_copy(GuMap *m, const Rec *x, uint64_t b, uint64_t p)
{
  int spsm = (int)(b >> 24 & 63), ssx = (int)(p & 0x7ff), ssy = (int)(p >> 16 & 0x7ff), bits = psm_bits(x->psm);
  uint32_t sbp = (uint32_t)(b & 0x3fff), sbw = (uint32_t)(b >> 16 & 63);
  int sidx = spsm == GS_T8 || spsm == GS_T4 || spsm == GS_T8H || spsm == GS_T4HL || spsm == GS_T4HH, s16 = spsm == GS_CT16 || spsm == GS_CT16S;
  if (psm_bits(spsm) != bits || (!sidx && !s16 && spsm != GS_CT32 && spsm != GS_CT24) || !sbw) return 0;
  /* task #149: every texel of the source first, the buffer only when the map knows them all (most local copies read a drawn area) */
  int tx = sidx || s16; uint32_t v;
  for (int j = 0; j < x->h; j++) {
    ME_BEAT();
    for (int i = 0; i < x->w; i++) if (!lc_src(m, spsm, sbp, sbw, tx, (ssx + i) & 2047, (ssy + j) & 2047, &v)) return 0; }
  uint8_t *o = calloc(1, x->need ? x->need : 1); if (!o) return 0;
  for (int j = 0; j < x->h; j++) {
    ME_BEAT();
    for (int i = 0; i < x->w; i++) {
      uint32_t pi = (uint32_t)j * (uint32_t)x->w + (uint32_t)i;
      if (!lc_src(m, spsm, sbp, sbw, tx, (ssx + i) & 2047, (ssy + j) & 2047, &v)) { free(o); return 0; }   /* (cannot happen) */
      switch (x->psm) {
        case GS_CT32: memcpy(o + 4 * pi, &v, 4); break;
        case GS_CT24: o[3 * pi] = (uint8_t)v; o[3 * pi + 1] = (uint8_t)(v >> 8); o[3 * pi + 2] = (uint8_t)(v >> 16); break;
        case GS_T8H: case GS_T8: o[pi] = (uint8_t)v; break;
        case GS_CT16: case GS_CT16S: o[2 * pi] = (uint8_t)v; o[2 * pi + 1] = (uint8_t)(v >> 8); break;
        default: o[pi >> 1] |= (uint8_t)((v & 15) << ((pi & 1) * 4)); break;
      }
    } }
  return o;
}
void gu_trxdir(GuMap *m, const uint64_t *gr)
{
  uint64_t b = gr[0x50], p = gr[0x51], r = gr[0x52]; int dir = (int)(gr[0x53] & 3), w = (int)(r & 0xfff), h = (int)(r >> 32 & 0xfff);
  if (m->cur >= 0) {   /* the transfer before ends; one whose data did not all come goes into the blocks as gsup.c holds it */
    commit(m, m->cur);
    Rec *c = &m->r[m->cur];
    if (m->keys && !c->bad && c->got && c->got < c->need) { rec_ident(m, c, 1); m->epoch++; }
    m->cur = -1;
  }
  if (dir == 1 || dir == 3) return;
  DC_(trx);
  gc_maybe(m);
  int psm = (int)(b >> 56 & 63), sx = (int)(p >> 32 & 0x7ff), sy = (int)(p >> 48 & 0x7ff); uint32_t bp = (uint32_t)(b >> 32 & 0x3fff), bw = (uint32_t)(b >> 48 & 63);
  int i = rec_new(m); if (i < 0) return;
  Rec *x = &m->r[i];
  x->bp = bp; x->bw = bw; x->psm = psm; x->sx = sx; x->sy = sy; x->w = w; x->h = h; x->need = (uint32_t)(((uint64_t)w * h * psm_bits(psm) + 7) / 8);
#ifdef GU_SENT
  if (m->trx_fn) ME_CALLP(m->trx_fn)(m->trx_ctx, gr);
#endif
  if (dir == 2) {   /* a local copy: resolved from the map now, in a modelled layout; else not known */
    uint32_t ln = psm_lanes(psm);
    m->st.xfer_local++;
    if (w && h) {
      uint8_t *o = (ln || other_ok(psm, bw)) && bw && sx + w <= 2048 && sy + h <= 2048 ? local_copy(m, x, b, p) : 0;
      x = &m->r[i];
#ifdef GU_SENT
      if (o && m->keep) { int rs = m->read_sent; m->read_sent = 1; x->sent = local_copy(m, x, b, p); m->read_sent = rs; x = &m->r[i]; }
#endif
      if (o) { x->own = o; x->got = x->need; m->st.own_bytes += x->need;
        if (m->keys) { x->id = mix(mix(whash(FNV0 ^ 2, o, x->need), (uint64_t)(uint32_t)psm << 48 | (uint64_t)bp << 16 | bw),
                                   (uint64_t)(uint32_t)sx << 48 | (uint64_t)(uint32_t)sy << 32 | (uint64_t)(uint32_t)w << 16 | (uint32_t)h) | 1; x->idgot = x->got; } }
      else {
        int f = 0; gu_area_id(m, (int)(b >> 24 & 63), (uint32_t)(b & 0x3fff), (uint32_t)(b >> 16 & 63), (int)(p & 0x7ff), (int)(p >> 16 & 0x7ff),
                              (int)(p & 0x7ff) + w - 1, (int)(p >> 16 & 0x7ff) + h - 1, &f);
        x = &m->r[i];
        if (f & GU_ID_DRAWN) { x->drawn = 1; m->st.local_drawn++; }
        m->st.local_unknown++; x->bad = x->whole = 1; }
      x->lanes = (uint8_t)(ln ? ln : 7);
      commit(m, i);
    }
    return;
  }
  m->st.xfers++; m->cur = i; x->no = (uint32_t)m->st.xfers;
  if (!w || !h) { x->in = 1; return; }   /* nothing to write (gsup.c covers nothing) */
  uint32_t lanes = psm_lanes(psm);
  if (!lanes) { m->st.xfer_other++; lanes = 7; if (!other_ok(psm, bw)) { m->st.xfer_unmod++; x->bad = x->whole = 1; } }
  if (!x->bad && (!bw || sx + w > 2048 || sy + h > 2048)) { m->st.xfer_wrap++; x->bad = x->whole = 1; }
  x->lanes = (uint8_t)lanes;
#ifdef GU_SENT
  if (m->keep && !x->bad) x->sent = calloc(1, x->need);
#endif
  if (x->bad) commit(m, i);   /* its words read as missing from now on */
}
void gu_image(GuMap *m, uint32_t ee, uint32_t n)
{
  if (m->cur < 0) { m->st.image_orphan++; return; }
  Rec *x = &m->r[m->cur];
  if (x->got >= x->need) { m->st.image_orphan++; return; }
  int was_bad = x->bad;
  if (x->src == GU_NOSRC) x->src = ee;
  else if (ee != x->src + x->got && !x->bad) { x->bad = 1; m->st.xfer_split++; }
#ifdef GU_SENT
  if (x->sent && !x->bad) { const uint8_t *s = ME_CALLP(m->ram)(m->ctx, ee, n); uint32_t k = x->need - x->got < n ? x->need - x->got : n; if (s) memcpy(x->sent + x->got, s, k); }
#endif
  x->got += n;
  if (x->in == 1 && !was_bad) m->epoch++;   /* in the blocks before its data was all in: what they hold changes */
  if (m->keys && !x->bad && x->got >= x->need && x->got - n < x->need) rec_ident(m, x, 1);
  uint32_t no = x->no, got = x->got, need = x->need;
  if (!x->in && got >= need) commit(m, m->cur);   /* its data is all in */
  if (m->data_fn) ME_CALLP(m->data_fn)(m->data_ctx, no, ee, n, got, need);
}
void gu_image_run(GuMap *m, uint32_t ee, uint32_t n)
{
  while (n) {
    if (m->cur < 0 || m->r[m->cur].got >= m->r[m->cur].need) { m->st.image_orphan += n / 16; return; }
    const Rec *x = &m->r[m->cur]; uint32_t k = (x->need - x->got + 15) & ~15u;
    if (k > n) k = n;
    gu_image(m, ee, k); ee += k; n -= k;
  }
}
void gu_on_data(GuMap *m, GuDataFn fn, void *ctx) { m->data_fn = fn; m->data_ctx = ctx; }

/* ---- areas the GS drew ---- */
/* the drawn area that is (psm .. y1), or -1 */
static int dt_find(GuMap *m, int psm, uint32_t bp, uint32_t bw, uint32_t lanes, int x0, int y0, int x1, int y1)
{
#define DT_IS(e) ((e)->used && (e)->psm == psm && (e)->bp == bp && (e)->bw == bw && (e)->lanes == lanes && (e)->x0 == x0 && (e)->y0 == y0 \
                  && (e)->x1 == x1 && (e)->y1 == y1)
  int h0 = m->dt_hit[0], h1 = m->dt_hit[1];
  if (h0 >= 0 && DT_IS(&m->dt[h0])) return h0;
  if (h1 >= 0 && DT_IS(&m->dt[h1])) { m->dt_hit[0] = h1; m->dt_hit[1] = h0; return h1; }
  for (int k = 0; k < (int)DT_N; k++) if (DT_IS(&m->dt[k])) { m->dt_hit[1] = h0; m->dt_hit[0] = k; return k; }
  return -1;
#undef DT_IS
}
/* each block of drawn area e: with draw (a drawn record's index + 1) that record goes in and the block is marked in e's area (dtm);
   without, the block's dtm bits are and'ed with keep */
static void dt_cells(GuMap *m, const DtE *e, uint16_t keep, int draw)
{
  const Lay L = *lay(e->psm); int cw = 1 << L.cws, ch = 1 << L.chs, x0 = e->x0, y0 = e->y0, x1 = e->x1, y1 = e->y1; uint32_t bp = e->bp, bw = e->bw;
  uint16_t bit = (uint16_t)(1u << (e - m->dt));
  for (int cy = y0 & -ch; cy <= y1; cy += ch)
    for (int cx = x0 & -cw; cx <= x1; cx += cw) {
      uint32_t bn = blk(&L, bp, bw, cx, cy);
      if (draw) { DC_(remark_blocks); put(m, bn, draw - 1, 1); m->dtm[bn] |= bit; m->pdt[bn >> 5] |= bit; } else m->dtm[bn] &= keep;
    }
}
/* call d (with its area) becomes the newest of the last two */
static void dl_put(GuMap *m, const DlE *d)
{
  for (int j = 0; j < 2; j++) {
    DlE *e = &m->dl[j];
    if (e->bp == d->bp && e->psm == d->psm && e->x0 == d->x0 && e->y0 == d->y0 && e->x1 == d->x1 && e->y1 == d->y1 && e->bw == d->bw && e->bits == d->bits) {
      e->k = d->k; return; }
  }
  m->dl[1] = m->dl[0]; m->dl[0] = *d;
}
void gu_drawn(GuMap *m, int psm, uint32_t bp, uint32_t bw, uint32_t bits, int x0, int y0, int x1, int y1)
{
  uint32_t lanes; int ax0 = x0, ay0 = y0, ax1 = x1, ay1 = y1;
  DC_(drawn_calls);
  for (int j = 0; j < 2; j++) {   /* one of the last two calls again (a frame and a depth buffer): its area, unless a write went into it since */
    const DlE *d = &m->dl[j];
    if (d->k < 0 || d->bp != bp || d->psm != psm || d->x0 != x0 || d->y0 != y0 || d->x1 != x1 || d->y1 != y1 || d->bw != bw || d->bits != bits) continue;
    settle(m);
    DtE *e = &m->dt[d->k];
    if (!e->dirty) { m->st.drawn_kept++; DC_(drawn_kept); DC_(drawn_memo); e->last = ++m->dt_clock; return; }
    break;
  }
  switch (psm) {
    case GS_CT32: case GS_Z32: lanes = (bits & 0xffffffu ? 1 : 0) | (bits & 0x0f000000u ? 2 : 0) | (bits & 0xf0000000u ? 4 : 0); break;
    case GS_CT24: case GS_Z24: lanes = bits & 0xffffffu ? 1 : 0; break;
    case GS_CT16: case GS_CT16S: case GS_Z16: case GS_Z16S: lanes = bits & 0xffffu ? 7 : 0; break;
    default: return;
  }
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > 2047) x1 = 2047;
  if (y1 > 2047) y1 = 2047;
  if (!lanes || !bw || x0 > x1 || y0 > y1) { DC_(drawn_early); return; }
  settle(m);
  int k = dt_find(m, psm, bp, bw, lanes, x0, y0, x1, y1), whole = 1;
  DlE call = { -1, psm, ax0, ay0, ax1, ay1, bp, bw, bits };
  if (k >= 0 && !m->dt[k].dirty) { m->st.drawn_kept++; DC_(drawn_kept); m->dt[k].last = ++m->dt_clock; call.k = k; dl_put(m, &call); return; }
  DC_(remark); if (k < 0) DC_(remark_new);
  gc_maybe(m);
  if (k >= 0) { m->st.dt_dirty++; whole = m->dt[k].over; }
  else {   /* a new area: in the place of an unused or the least recently drawn one, whose blocks are told it is gone */
    m->st.dt_new++; k = 0;
    for (int j = 1; j < (int)DT_N && m->dt[k].used; j++) if (!m->dt[j].used || m->dt[j].last < m->dt[k].last) k = j;
    if (m->dt[k].used) dt_cells(m, &m->dt[k], (uint16_t)~(1u << k), 0);
    for (int j = 0; j < 2; j++) if (m->dl[j].k == k) m->dl[j].k = -1;
    m->dt_hit[1] = m->dt_hit[0]; m->dt_hit[0] = k;
  }
  DtE *e = &m->dt[k];
  int i = rec_new(m);
  if (i < 0) { dt_cells(m, e, (uint16_t)~(1u << k), 0); e->used = 0; e->ntl = e->over = 0; for (int j = 0; j < 2; j++) if (m->dl[j].k == k) m->dl[j].k = -1;
               m->st.dt_fail++; return; }   /* sb10: counted (texcache.c's front reads it with the generations) */
  Rec *x = &m->r[i];
  x->bp = bp; x->bw = bw; x->psm = psm; x->sx = x0; x->sy = y0; x->w = x1 - x0 + 1; x->h = y1 - y0 + 1; x->bad = x->whole = x->drawn = 1;
  x->lanes = (uint8_t)lanes; x->in = 1;
  m->st.drawn++; m->seq++;
  e->used = 1; e->dirty = 0; e->psm = psm; e->bp = bp; e->bw = bw; e->lanes = lanes; e->x0 = x0; e->y0 = y0; e->x1 = x1; e->y1 = y1;
  e->last = ++m->dt_clock; call.k = k; dl_put(m, &call);
  /* the area's blocks get the new draw: all of them for a new area (or one that was written in too many blocks); else only the blocks
     written since its last draw, as the others still hold a drawn area in its lanes, which reads (missing) and names (drawn) the same */
  if (whole) dt_cells(m, e, 0xffff, i + 1);
  else for (int t = 0; t < e->ntl; t++) { uint32_t bn = e->tl[t]; DC_(remark_blocks); put(m, bn, i, 1); m->dtm[bn] |= (uint16_t)(1u << k); m->pdt[bn >> 5] |= (uint16_t)(1u << k); }
#ifdef GU_DEVCNT
  DC.remark_touched += e->ntl;
#endif
  e->ntl = e->over = 0;
}

/* ---- the CLUT buffer ---- */
static int is_idx(int psm) { return psm == GS_T8 || psm == GS_T4 || psm == GS_T8H || psm == GS_T4HL || psm == GS_T4HH; }
static void ld_xy(const Ld *L, int e, int *x, int *y, uint32_t *bw)
{
  *bw = 1;
  if (L->csm) { *bw = L->cbw; *x = L->cou * 16 + e; *y = L->cov; }
  else if (L->n == 256) { int p = (e & 0xe7) | (e & 8) << 1 | (e & 0x10) >> 1; *x = p & 15; *y = p >> 4; }
  else { *x = e & 7; *y = e >> 3; }
  *x &= 2047; *y &= 2047;
}
/* load L as kept for parameters rk, rt holds what a load with them would hold now: the same epoch and its blocks' states */
static int ld_now(const GuMap *m, Ld *L, uint32_t rk, uint32_t rt)
{
  int b;
  if (L->rk != rk || L->rt != rt || L->epoch != m->epoch) return 0;
  for (b = 0; b < L->nb && (int32_t)(m->pg[L->bn[b] >> 5] - L->seq) <= 0; b++) {}   /* nothing written into their pages since */
  if (b == L->nb) return 1;
  for (b = 0; b < L->nb; b++) { uint32_t bn = L->bn[b]; if (st_of(m, bn, 0) != L->s[b][0] || st_of(m, bn, 1) != L->s[b][1] || st_of(m, bn, 2) != L->s[b][2]) return 0; }
  L->seq = m->seq; return 1;
}
/* the CLUT rows a load of n entries at CSA csa fills (a CT32 / CT24 entry fills a row of low and a row of high halves) take load li */
static void rows(GuMap *m, int li, int csa, int n, int c32)
{
  m->canon = n == 256 && !csa ? li + 1 : 0; m->canon32 = c32;
  if (c32 && n == 256 && !csa) { uint32_t v = ROW(li, 0, 0); for (int j = 0; j < 16; j++, v += 2) { m->row[j] = v; m->row[16 + j] = v | 1; } return; }
  if (c32) for (int j = 0; j < n / 16; j++) { int r = (csa + j) & 15; m->row[r] = ROW(li, j, 0); m->row[r + 16] = ROW(li, j, 1); }
  else for (int j = 0; j < n / 16; j++) m->row[(csa + j) & 31] = ROW(li, j, 0);
}
static int clut_new(GuMap *m, const uint64_t *gr, uint64_t t0, int *rs, uint32_t rk, uint32_t rt, int n, int c32);
/* ---- sb9 step 0b (stagec_notes.md 0.4): CLUT loads kept by identity ---- */
void gu_ld_ident(GuMap *m, int mode) { if (mode != m->ldid) { m->ldid = mode; for (int k = 0; k < IT_N; k++) { m->it[k].li = -1; m->it[k].use = 0; } } }
/* the identity of the records the blocks of load L hold now (L's blocks are those of any load with its parameters): 0, with *sig and each
   block's and lane group's record identity in v (0: empty), when each state is empty or one plain final record (as gu_pages_sig); else 1 */
static int ld_sig(const GuMap *m, const Ld *L, uint64_t *sig, uint64_t *v)
{
  uint64_t h = mix(FNV0 ^ 3, (uint64_t)L->rk << 32 | L->rt);
  for (int b = 0; b < L->nb; b++)
    for (int g = 0; g < 3; g++) {
      uint32_t s = st_of(m, L->bn[b], g); uint64_t id = 0;
      if (s) {
        if (s & S_NODE) return 1;
        const Rec *x = &m->r[S_IDX(s)];
        if (x->drawn || x->bad || !x->bw || (!x->own && (!x->id || x->idgot != x->got || x->got < x->need))) return 1;
        id = x->id;
      }
      v[b * 3 + g] = id; h = mix(h, id);
    }
  *sig = h; return 0;
}
static ItE *it_set(GuMap *m, uint64_t sig) { return &m->it[((uint32_t)(sig >> 32) >> (32 - IT_SB)) * IT_W]; }   /* sig holds the parameters too */
/* the load kept by identity that holds what a new load with the parameters of load r0 would hold now (the same identity, flags 0), or -1 */
#ifdef UM_SIM
void um_sim_ld(uint64_t sig, uint32_t rk, uint32_t rt, int put); void um_sim_vfail(void); void um_sim_mem(int capr, size_t szr, int capn, size_t szn, int capl, size_t szl, long own);
static int it_check(GuMap *m, const ItE *e, const Ld *L0, const uint64_t *v, uint32_t rk, uint32_t rt);
#endif
static int it_find(GuMap *m, int r0, uint32_t rk, uint32_t rt)
{
  const Ld *L0 = &m->l[r0]; uint64_t sig, v[12];
  if (L0->rk != rk || L0->rt != rt || L0->bad) { m->st.ld_ident_nolist++; return -1; }   /* the table of loads by parameters holds another's */
  if (ld_sig(m, L0, &sig, v)) { m->st.ld_ident_nosig++; return -1; }
#ifdef UM_SIM
  um_sim_ld(sig, rk, rt, 0);
#endif
  ItE *e = it_set(m, sig), *ee = e + IT_W;
  for (; e < ee && (e->li < 0 || e->sig != sig || e->rk != rk || e->rt != rt); e++) ;
  if (e == ee) return -1;
  e->use = ++m->itclk;   /* a set holds each identity once (it_put) */
#ifdef UM_SIM
  { int r = it_check(m, e, L0, v, rk, rt); if (r < 0) um_sim_vfail(); return r; }
}
static int it_check(GuMap *m, const ItE *e, const Ld *L0, const uint64_t *v, uint32_t rk, uint32_t rt)
{
#endif
  const Ld *L = &m->l[e->li];
  if (!L->live || L->rk != rk || L->rt != rt || L->bad || L->idf || L->nb != L0->nb) return -1;
  for (int b = 0; b < L->nb; b++) {   /* the same blocks, and in each lane group a record with the same identity (a kept load's states are
                                         empty or one record, which the sweep keeps while the table names the load) */
    if (L->bn[b] != L0->bn[b]) return -1;
    for (int g = 0; g < 3; g++) { uint32_t s = L->s[b][g]; if ((s ? m->r[S_IDX(s)].id : 0) != v[b * 3 + g]) return -1; }
  }
  return e->li;
}
/* a new load li joins the table if what it loaded qualifies */
static void it_put(GuMap *m, int li)
{
  const Ld *L = &m->l[li]; uint64_t sig, v[12];
  if (L->bad || L->idf || !m->keys || ld_sig(m, L, &sig, v)) return;
#ifdef UM_SIM
  um_sim_ld(sig, L->rk, L->rt, 1); um_sim_mem(m->capr, sizeof *m->r, m->capn, sizeof *m->nd, m->capl, sizeof *m->l, m->st.own_bytes);
#endif
  ItE *e = it_set(m, sig), *u = e, *ee = e + IT_W;   /* the way that holds this identity, else the least recently used (empty ones: 0) */
  for (; e < ee && (e->li < 0 || e->sig != sig || e->rk != L->rk || e->rt != L->rt); e++) if (e->use < u->use) u = e;
  if (e == ee) e = u;
  e->li = li; e->rk = L->rk; e->rt = L->rt; e->sig = sig; e->use = ++m->itclk; m->st.ld_ident_put++;
}
void gu_clut_load(GuMap *m, const uint64_t *gr, int ctxt)
{
  uint64_t t0 = gr[0x06 + ctxt];
  int psm = (int)(t0 >> 20 & 63), cpsm = (int)(t0 >> 51 & 15), csm = (int)(t0 >> 55 & 1), csa = (int)(t0 >> 56 & 31), cld = (int)(t0 >> 61 & 7);
  uint32_t cbp = (uint32_t)(t0 >> 37 & 0x3fff);
  if (!is_idx(psm)) return;
  switch (cld) { case 1: break; case 2: m->cbp0 = cbp; break; case 3: m->cbp1 = cbp; break;
    case 4: if (cbp == m->cbp0) return; m->cbp0 = cbp; break; case 5: if (cbp == m->cbp1) return; m->cbp1 = cbp; break; default: return; }
  settle(m);
  int n = (psm == GS_T8 || psm == GS_T8H) ? 256 : 16, c32 = cpsm == GS_CT32 || cpsm == GS_CT24;
  m->st.loads++; DC_(clut_calls);
  /* the load kept last for these parameters (CBP, CPSM, CSM, its size and TEXCLUT), if its blocks hold the states they held then */
  uint32_t rk = (uint32_t)(t0 >> 37 & 0x7ffff) << 1 | (uint32_t)(n == 256), rt = csm ? (uint32_t)(gr[0x1c] & 0x3fffff) : 0;
  int *rs = &m->rt[(rk * 2654435761u ^ rt * 2246822519u) >> (32 - RT_BITS)];
  if (*rs >= 0 && ld_now(m, &m->l[*rs], rk, rt)) { m->st.ld_memo++; DC_(clut_memo); rows(m, *rs, csa, n, c32); return; }
  int ci = m->ldid && *rs >= 0 ? it_find(m, *rs, rk, rt) : -1;
  if (ci >= 0 && m->ldid == 1) {   /* sb9 0b: that load again, with the blocks' states as they are now */
    Ld *L = &m->l[ci];
    for (int b = 0; b < L->nb; b++) st3(m, L->bn[b], L->s[b]);
    L->seq = m->seq; L->epoch = m->epoch; *rs = ci; m->st.ld_ident++;
    rows(m, ci, csa, n, c32); return;
  }
  int li = clut_new(m, gr, t0, rs, rk, rt, n, c32);
  if (ci >= 0 && li >= 0) {   /* the check mode: the new load against the kept one */
    const Ld *L = &m->l[ci], *N = &m->l[li]; int same = N->id == L->id && N->idf == L->idf && N->bad == L->bad && N->nb == L->nb
      && N->cbp == L->cbp && N->cbw == L->cbw && N->cpsm == L->cpsm && N->csm == L->csm && N->cou == L->cou && N->cov == L->cov && N->n == L->n;
    for (int b = 0; same && b < N->nb; b++) same = N->bn[b] == L->bn[b];
    m->st.ld_ident_chk++; if (!same) m->st.ld_ident_bad++;
  }
}
/* gu_clut_load's load that the table of loads by parameters did not have (apart: the PSP's instruction cache keeps the common path) */
static __attribute__((noinline)) int clut_new(GuMap *m, const uint64_t *gr, uint64_t t0, int *rs, uint32_t rk, uint32_t rt, int n, int c32)
{
  int cpsm = (int)(t0 >> 51 & 15), csm = (int)(t0 >> 55 & 1), csa = (int)(t0 >> 56 & 31); uint32_t cbp = (uint32_t)(t0 >> 37 & 0x3fff);
  gc_maybe(m);
  Ld k; memset(&k, 0, sizeof k);
  k.rk = rk; k.rt = rt; k.seq = m->seq; k.cbp = cbp; k.cpsm = cpsm; k.csm = csm; k.n = n; k.epoch = m->epoch;
  if (csm) { uint64_t t = gr[0x1c]; k.cbw = (uint32_t)(t & 63); k.cou = (int)(t >> 6 & 63); k.cov = (int)(t >> 12 & 1023); }
  int fmt = c32 ? GS_CT32 : cpsm == GS_CT16S ? GS_CT16S : GS_CT16, x0, y0, x1, y1; uint32_t bw;
  const Lay L = *lay(fmt); int cw = 1 << L.cws, ch = 1 << L.chs;
  if (csm) { ld_xy(&k, 0, &x0, &y0, &bw); x1 = x0 + n - 1; y1 = y0; }
  else { x0 = y0 = 0; x1 = n == 256 ? 15 : 7; y1 = n == 256 ? 15 : 1; bw = 1; }
  for (int cy = y0 & -ch; cy <= y1 && !k.bad; cy += ch)
    for (int cx = x0 & -cw; cx <= x1 && !k.bad; cx += cw) {
      uint32_t bn = blk(&L, cbp, bw, cx & 2047, cy & 2047); int j;
      for (j = 0; j < k.nb && k.bn[j] != bn; j++) {}
      if (j < k.nb) continue;
      if (k.nb == 4) { k.bad = 1; m->st.load_wide++; break; }
      k.bn[k.nb] = (uint16_t)bn; st3(m, bn, k.s[k.nb]); k.nb++;
    }
  /* a new load, kept with the state words it read (a state never comes back once replaced: an older load equal to it is only one the table
     lost to other parameters, and it has the same identity) */
  int li = ld_new(m); if (li < 0) return -1;
  Ld *Ln = &m->l[li]; *Ln = k; Ln->live = 1; Ln->next = -1; Ln->mk = 0; m->st.ld_made++;
#ifdef GU_DEVCNT
  DC.clut_nb += k.nb; DC.clut_new++;
#endif
  if (m->keys) {   /* the identity of what it loads: its parameters and its blocks' states (gsup.c: their lists) as at the load */
    uint64_t h = mix(mix(FNV0, (uint64_t)cbp << 32 | (uint64_t)(uint32_t)cpsm << 8 | (uint64_t)(uint32_t)csm << 4 | (uint64_t)(n == 256)),
                     (uint64_t)Ln->cbw << 32 | (uint64_t)(uint32_t)Ln->cou << 16 | (uint32_t)Ln->cov);
    int f = Ln->bad ? GU_ID_LOST : 0;
    for (int b = 0; b < Ln->nb; b++) for (int g = 0; g < 3; g++) h = mix(mix(h, state_id(m, Ln->s[b][g], &f)), (uint64_t)g);
    Ln->id = h; Ln->idf = f;
  }
  if (!k.bad) *rs = li;   /* a load over too many blocks: not kept by its parameters */
  if (m->ldid) it_put(m, li);   /* sb9 0b */
  rows(m, li, csa, n, c32);
  return li;
}
static int half(const GuMap *m, int h)
{
  uint32_t w = m->row[h >> 4]; if (!w) return -1;
  const Ld *L = &m->l[ROW_LD(w)]; if (L->bad) return -1;
  int x, y, e = ROW_E0(w) + (h & 15); uint32_t bw, wa, sh, kn;
  ld_xy(L, e, &x, &y, &bw);
  if (L->cpsm == GS_CT32 || L->cpsm == GS_CT24) { wa = gs_pa32(L->cbp, bw, x, y, 0) >> 2; sh = (w & 1) ? 16 : 0; }
  else { uint32_t a = gs_pa16(L->cbp, bw, x, y, L->cpsm == GS_CT16S, 0); wa = a >> 2; sh = (a & 2) ? 16 : 0; }
  int k; for (k = 0; k < L->nb && L->bn[k] != (wa >> 6 & 0x3fff); k++) {}
  if (k == L->nb) return -1;
  uint32_t v = words3(m, L->s[k], wa, &kn), mk = 0xffffu << sh;
  return (kn & mk) == mk ? (int)((v & mk) >> sh) : -1;
}
uint64_t gu_clut_id(const GuMap *m, int cpsm, int k0, int n, int *flags)
{
  int c32 = cpsm == GS_CT32 || cpsm == GS_CT24, f = 0; uint64_t h = mix(FNV0, (uint64_t)(uint32_t)cpsm << 1 | (uint64_t)c32);
  /* entries 0..255 all from one load, in order (the rows a whole load at CSA 0 put in place): the identity is a function of that load alone
     (its identity and flags never change while a row names it), so it is kept with the load */
  Ld *Lc = !k0 && n == 256 && m->canon && (m->canon32 || !c32) ? (Ld *)&m->l[m->canon - 1] : 0;
  int hit = Lc && Lc->cv && Lc->ccp == cpsm;
#ifndef GU_MEMO_CHECK
  if (hit && m->cmemo) { *flags = Lc->cidf; return Lc->cid; }
#endif
  for (int k = k0 & ~15; k < k0 + n; k += 16)
    for (int part = 0; part < (c32 ? 2 : 1); part++) {
      uint32_t w = m->row[c32 ? ((k & 255) >> 4) + 16 * part : (k & 511) >> 4];
      if (!w) { f |= GU_ID_LOST; h = mix(h, 0xe0e0u); continue; }
      const Ld *L = &m->l[ROW_LD(w)];
      f |= L->idf | (L->bad ? GU_ID_LOST : 0); h = mix(mix(h, L->id), (uint64_t)(uint32_t)ROW_E0(w) << 8 | (uint64_t)(w & 1));
    }
#ifdef GU_MEMO_CHECK
  if (hit && m->cmemo) { MC.clut++; MC.clut_bad += Lc->cid != h || Lc->cidf != f; }
#endif
  if (Lc && m->cmemo) { Lc->cv = 1; Lc->ccp = (int8_t)cpsm; Lc->cid = h; Lc->cidf = f; }
  *flags = f; return h;
}
int64_t gu_clut(const GuMap *m, int cpsm, int k)
{
  if (cpsm == GS_CT32 || cpsm == GS_CT24) { int lo = half(m, k & 255), hi = half(m, (k & 255) + 256); return lo < 0 || hi < 0 ? -1 : (int64_t)((uint32_t)lo | (uint32_t)hi << 16); }
  return half(m, k & 511);
}

#ifdef GU_SENT
void gu_keep_sent(GuMap *m, int on) { m->keep = on; }
void gu_read_sent(GuMap *m, int on) { m->read_sent = on; }
long gu_sent_scan(const GuMap *cm, void (*fn)(void *ctx, uint32_t xfer, uint32_t ee, uint32_t got, uint32_t first, uint32_t ndiff), void *ctx)
{
  GuMap *m = (GuMap *)cm; long n = 0;
  gc(m);   /* the records the map can still read: what a sweep keeps */
  for (int i = 0; i < m->nr; i++) {
    const Rec *x = &m->r[i]; uint32_t got = x->got < x->need ? x->got : x->need;
    if (!x->live || !x->sent || x->bad || x->src == GU_NOSRC || !got) continue;
    const uint8_t *s = ME_CALLP(m->ram)(m->ctx, x->src, got); if (!s) continue;
    n++;
    if (!memcmp(s, x->sent, got)) continue;
    uint32_t first = ~0u, nd = 0;
    for (uint32_t k = 0; k < got; k++) if (s[k] != x->sent[k]) { if (first == ~0u) first = k; nd++; }
    ME_CALLP(fn)(ctx, x->no, x->src, got, first, nd);
  }
  return n;
}
void gu_on_trx(GuMap *m, void (*fn)(void *ctx, const uint64_t *gr), void *ctx) { m->trx_fn = fn; m->trx_ctx = ctx; }
void gu_watch(GuMap *m, int bn) { m->watch = bn; }
static void why_state(const GuMap *m, uint32_t s, char *buf, int n, int *o)
{
  if (!s) *o += snprintf(buf + *o, (size_t)(n > *o ? n - *o : 0), " nothing");
  while (s && *o < n) {
    const Rec *x = &m->r[st_next(m, &s)];
    *o += snprintf(buf + *o, (size_t)(n - *o), " [%u psm %02x bp %04x bw %u at %d,%d %dx%d%s%s%s got %u/%u]", x->no, x->psm, x->bp, x->bw, x->sx, x->sy,
                   x->w, x->h, x->drawn ? " drawn" : x->bad ? " bad" : "", x->whole && !x->drawn ? " whole" : "", x->own ? " own" : "", x->got, x->need);
  }
}
int gu_why(const GuMap *m, uint32_t wa, char *buf, int n)
{
  uint32_t bn = wa >> 6 & 0x3fff; int o = snprintf(buf, (size_t)n, "block %04x:", bn);
  for (int g = 0; g < 3 && o < n; g++) { o += snprintf(buf + o, (size_t)(n - o), " lanes %d:", g); why_state(m, st_of(m, bn, g), buf, n, &o); }
  return o;
}
#endif
