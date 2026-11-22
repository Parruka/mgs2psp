/* gsup.c: see gsup.h.  The transfer and CLUT load rules are tools/gsref.c's (xfer_start / xfer_bytes, clut_load), whose GS memory
   this stands in for. */
#include <stdlib.h>
#include <string.h>
#include "gsup.h"
#include "gslayout.h"
#ifdef GU_SENT
#include <stdio.h>
#endif

#define GU_K 4                 /* entries per block list */
#define GU_NOSRC 0xffffffffu
#define GU_DR 8                /* drawn areas remembered (gu_drawn marks an area again only when a transfer wrote into it since) */
/* the bits of a word a transfer writes, by lane set: bit 0 = bits 0-23, bit 1 = bits 24-27, bit 2 = bits 28-31 */
static const uint32_t LANE_BITS[8] = { 0, 0x00ffffffu, 0x0f000000u, 0x0fffffffu, 0xf0000000u, 0xf0ffffffu, 0xff000000u, 0xffffffffu };
#define FNV0 0xcbf29ce484222325ull
static uint64_t fnv(uint64_t h, const uint8_t *b, uint32_t n) { for (uint32_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; } return h; }
static uint64_t mix(uint64_t h, uint64_t v) { h = (h ^ v) * 0x9e3779b97f4a7c15ull; return h ^ h >> 31; }

typedef struct {
  uint32_t src, got, need;     /* EE address of its first byte (GU_NOSRC: no data yet), bytes received, bytes its rectangle takes */
  uint32_t bp, bw; int psm, sx, sy, w, h;
  uint8_t bad, whole;          /* bad: its words read as missing; whole: every word of the blocks that list it counts as written */
  uint8_t drawn;               /* not a transfer: an area the GS drew (gu_drawn; bad and whole) */
  int refs, next;              /* block entries, CLUT load copies and the current transfer naming it; free list */
  uint32_t no;                 /* its number among host -> local transfers (GuStats.xfers when it started) */
  uint64_t id; uint32_t idgot; /* its content identity (gu_keys; 0: not made yet) and the bytes it had then */
  uint8_t *own;                /* a local copy: its pixels as the map knew the source's when it ran (read instead of EE RAM) */
#ifdef GU_SENT
  uint8_t *sent;               /* dev: its bytes as they came (gu_keep_sent) */
#endif
} Xf;
typedef struct {
  uint32_t cbp, cbw; int cpsm, csm, cou, cov, n;
  int nb; uint16_t bn[4]; uint32_t snap[4][GU_K];   /* the lists of the blocks the CLUT entries are in, as at the load */
  uint8_t bad; int refs, next;
  uint64_t id; int idf;        /* the identity of what it loaded (gu_keys) and its GU_ID_* flags */
} Ld;
typedef struct { int ld; int e0; int hi; } Row;
typedef struct { int psm, x0, y0, x1, y1, dirty; uint32_t bp, bw, lanes; long used; } Dr;   /* an area marked drawn, its slot's use */   /* a CLUT buffer row: load, its first entry, 1: the high halves of CT32 entries */
struct GuMap {
  GuRamFn ram; void *ctx;
  uint32_t bl[16384][GU_K];    /* per block: (transfer + 1) | lanes << 24, newest first; 0 ends the list */
  Xf *x; int nx, capx, freex;
  Ld *l; int nl, capl, freel;
  int cur;                     /* the transfer taking IMAGE data, -1: none */
  Row row[32];                 /* the CLUT buffer in rows of 16 halves (rows 16-31 hold the high halves of CT32 entries 0-255) */
  uint32_t cbp0, cbp1;         /* the CBP0 / CBP1 registers of CLD 2-5 */
  GuStats st;
  GuDataFn data_fn; void *data_ctx;
  Dr dr[GU_DR];                /* the areas marked drawn last */
  uint8_t drb[16384]; long dr_clock;   /* per block: the dr slots whose area holds it */
  uint32_t ver[16384], ver_clock;      /* per block: changes with its list (gu_block_ver) */
  uint8_t lost[16384];                 /* per block: lanes whose entry was dropped from a full list (they read as not known) */
  int keys, zero_empty;                /* gu_keys, gu_zero_empty */
  struct GuSrc *srct; uint32_t nsrc, capsrc;   /* gu_keys: per upload source (EE address, size) its last key, and whether it changed */
#ifdef GU_SENT
  int keep, read_sent, watch;
  void (*trx_fn)(void *ctx, const uint64_t *gr); void *trx_ctx;
#endif
};

static uint8_t ibt32[32], ict32[64];   /* inverses of gs_bt32 / gs_ct32: block in page, word in block -> y << 3 | x */
/* the other layouts' inverses: block in page -> y << 3 | x; nibble (T4), byte (T8) or half (CT16) in block -> y << 5 | x */
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
/* the pixels per word of a transfer in another layout: 8 (T4), 4 (T8), 2 (CT16, CT16S); 0: the 32-bit layout or not modelled */
static int units(int psm) { return psm == GS_T4 ? 8 : psm == GS_T8 ? 4 : (psm == GS_CT16 || psm == GS_CT16S) ? 2 : 0; }
/* a transfer in another layout the map models: T4 and T8 need an even width (their pages are two width units wide) */
static int other_ok(int psm, uint32_t bw) { return units(psm) && !((psm == GS_T4 || psm == GS_T8) && (bw & 1)); }
/* the pixel of x's buffer that unit u (nibble, byte or half) of block rel (counted from its bp) holds; 0: no place (width 0) */
static int unit_xy(const Xf *x, uint32_t rel, int u, int *px, int *py)
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
/* n bytes at offset off of x's data: as sent (dev), its own pixels (a local copy), or EE RAM */
static const uint8_t *xf_bytes(const GuMap *m, const Xf *x, uint32_t off, uint32_t n)
{
#ifdef GU_SENT
  if (m->read_sent) return x->sent ? x->sent + off : 0;
#endif
  return x->own ? x->own + off : m->ram(m->ctx, x->src + off, n);
}

/* ---- transfer and load records ---- */
static int xf_new(GuMap *m)
{
  int i;
  if (m->freex >= 0) { i = m->freex; m->freex = m->x[i].next; }
  else {
    if (m->nx == m->capx) { int c = m->capx ? m->capx * 2 : 256; Xf *q = realloc(m->x, (size_t)c * sizeof *q); if (!q) return -1; m->x = q; m->capx = c; }
    i = m->nx++;
  }
  memset(&m->x[i], 0, sizeof m->x[i]); m->x[i].src = GU_NOSRC; m->x[i].next = -1; m->st.live_xfers++;
  return i;
}
static void xf_unref(GuMap *m, int i)
{
  if (--m->x[i].refs) return;
#ifdef GU_SENT
  free(m->x[i].sent); m->x[i].sent = 0;
#endif
  if (m->x[i].own) { m->st.own_bytes -= m->x[i].need; free(m->x[i].own); m->x[i].own = 0; }
  m->x[i].next = m->freex; m->freex = i; m->st.live_xfers--;
}
static void ld_unref(GuMap *m, int i)
{
  Ld *L = &m->l[i];
  if (--L->refs) return;
  for (int b = 0; b < L->nb; b++) for (int k = 0; k < GU_K && L->snap[b][k]; k++) xf_unref(m, (int)(L->snap[b][k] & 0xffffff) - 1);
  L->next = m->freel; m->freel = i; m->st.live_loads--;
}
static int ld_new(GuMap *m)
{
  int i;
  if (m->freel >= 0) { i = m->freel; m->freel = m->l[i].next; }
  else {
    if (m->nl == m->capl) { int c = m->capl ? m->capl * 2 : 256; Ld *q = realloc(m->l, (size_t)c * sizeof *q); if (!q) return -1; m->l = q; m->capl = c; }
    i = m->nl++;
  }
  memset(&m->l[i], 0, sizeof m->l[i]); m->l[i].next = -1; m->st.live_loads++;
  return i;
}

GuMap *gu_new(GuRamFn ram, void *ctx)
{
  GuMap *m = calloc(1, sizeof *m); if (!m) return 0;
  inv_init();
  m->ram = ram; m->ctx = ctx; m->freex = m->freel = -1; m->cur = -1; m->cbp0 = m->cbp1 = ~0u;
#ifdef GU_SENT
  m->watch = -1;
#endif
  for (int r = 0; r < 32; r++) m->row[r].ld = -1;
  return m;
}
void gu_free(GuMap *m)
{
  if (!m) return;
  for (int i = 0; i < m->nx; i++) {
#ifdef GU_SENT
    free(m->x[i].sent);
#endif
    free(m->x[i].own);
  }
  free(m->x); free(m->l); free(m);
}
const GuStats *gu_stats(const GuMap *m) { return &m->st; }

/* ---- content identities (gu_keys) ---- */
/* the PSP key of n bytes (renderer_notes 11.2): 64 words spread over them (all words when fewer) */
static uint64_t sparse_key(const uint8_t *b, uint32_t n)
{
  uint32_t nw = n / 4, st = nw >= 64 ? nw / 64 : 1; uint64_t h = FNV0;
  for (uint32_t k = 0; k < 64 && k * st < nw; k++) h = fnv(h, b + 4 * (k * st), 4);
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
/* an upload's identity from the bytes it has now: format, buffer, rectangle, EE address, size, bytes received and their key.  fin: the
   transfer has ended (all its data in, or the next one started), so its source's key is compared with the source's last one: a source
   that has shown a second key is hashed in full from then on (renderer_notes 11.10 step 6: 0 stale keys on both routes) */
static void xf_ident(GuMap *m, Xf *x, int fin)
{
  uint32_t n = x->got < x->need ? x->got : x->need; uint64_t k = 0;
  const uint8_t *s = n ? m->ram(m->ctx, x->src, n) : 0;
  if (n && !s) x->bad = 1;   /* its bytes are not in EE RAM: its words read as missing */
  if (s) {
    struct GuSrc *so = src_of(m, x->src, x->need); int vol = 0;
    k = sparse_key(s, n);
    if (so) { if (fin) { if (so->keyed && so->key != k) so->vol = 1; so->key = k; so->keyed = 1; } vol = so->vol; }
    if (vol) k = fnv(FNV0 ^ 1, s, n);
  }
  uint64_t h = mix(FNV0, (uint64_t)(uint32_t)x->psm << 48 | (uint64_t)x->bp << 16 | x->bw);
  h = mix(h, (uint64_t)(uint32_t)x->sx << 48 | (uint64_t)(uint32_t)x->sy << 32 | (uint64_t)(uint32_t)x->w << 16 | (uint32_t)x->h);
  h = mix(mix(h, (uint64_t)x->src << 32 | x->need), (uint64_t)n << 32 ^ k);
  x->id = h | 1; x->idgot = x->got;
}
void gu_keys(GuMap *m, int on) { m->keys = on; }
void gu_zero_empty(GuMap *m, int on) { m->zero_empty = on; }
uint32_t gu_block_ver(const GuMap *m, uint32_t bn) { return m->ver[bn & 0x3fff]; }
static uint64_t list_id(GuMap *m, const uint32_t *L, uint32_t bn, uint32_t lanes, int *flags)
{
  uint64_t h = FNV0; int f = (m->lost[bn & 0x3fff] & lanes) ? GU_ID_LOST : 0;
  for (int k = 0; k < GU_K && L[k]; k++) {
    uint32_t ln = (L[k] >> 24 & 7) & lanes; if (!ln) continue;
    Xf *x = &m->x[(L[k] & 0xffffff) - 1];
    if (x->drawn) { f |= GU_ID_DRAWN; h = mix(h, 0xd7a300u | ln); continue; }
    if (!x->own && !x->bad && x->bw && m->keys && (!x->id || x->idgot != x->got)) xf_ident(m, x, 0);
    if (x->bad || !x->bw) { f |= GU_ID_LOST; h = mix(h, 0x10500u | ln); continue; }
    h = mix(mix(h, x->id), ln);
  }
  *flags = f; return h;
}
uint64_t gu_lanes_id(GuMap *m, uint32_t bn, uint32_t lanes, int *flags) { return list_id(m, m->bl[bn & 0x3fff], bn, lanes, flags); }

/* ---- block lists ---- */
/* transfer xi wrote lanes of block bn: all its words (full) or some */
static void bl_push(GuMap *m, uint32_t bn, int xi, uint32_t lanes, int full)
{
  uint32_t *L = m->bl[bn & 0x3fff];
  if (m->x[xi].whole) full = 1;   /* every word of the block reads as missing in its lanes (word_list), so it hides the older entries
                                     there wherever its rectangle lies: a drawn area's edge blocks need no list entry of their own */
  if (m->drb[bn & 0x3fff] && !m->x[xi].drawn) for (int k = 0; k < GU_DR; k++) if (m->drb[bn & 0x3fff] >> k & 1) m->dr[k].dirty = 1;
  if (full) {   /* older entries keep only the lanes this one does not write */
    int o = 0;
    for (int k = 0; k < GU_K && L[k]; k++) {
      uint32_t e = L[k], ln = (e >> 24 & 7) & ~lanes;
      if (ln) L[o++] = (e & 0xffffff) | ln << 24; else xf_unref(m, (int)(e & 0xffffff) - 1);
    }
    for (int k = o; k < GU_K; k++) L[k] = 0;
    m->lost[bn & 0x3fff] &= (uint8_t)~lanes;
  } else {   /* an older entry of the same format and rectangle, in these lanes, writes no word this one does not: it is hidden */
    const Xf *n = &m->x[xi]; int o = 0;
    for (int k = 0; k < GU_K && L[k]; k++) {
      uint32_t e = L[k]; const Xf *x = &m->x[(e & 0xffffff) - 1];
      if (x->psm == n->psm && x->bp == n->bp && x->bw == n->bw && x->sx == n->sx && x->sy == n->sy && x->w == n->w && x->h == n->h
          && !((e >> 24 & 7) & ~lanes)) xf_unref(m, (int)(e & 0xffffff) - 1);
      else L[o++] = e;
    }
    for (int k = o; k < GU_K; k++) L[k] = 0;
  }
  int drop = L[GU_K - 1] != 0;
  if (drop) { m->lost[bn & 0x3fff] |= (uint8_t)(L[GU_K - 1] >> 24 & 7); xf_unref(m, (int)(L[GU_K - 1] & 0xffffff) - 1); m->st.dropped++; }
  memmove(L + 1, L, (GU_K - 1) * sizeof *L);
  L[0] = (uint32_t)(xi + 1) | lanes << 24; m->x[xi].refs++;
  m->ver[bn & 0x3fff] = ++m->ver_clock;
#ifdef GU_SENT
  if ((int)(bn & 0x3fff) == m->watch) { char b[1024]; gu_why(m, (bn & 0x3fff) << 6, b, sizeof b);
    fprintf(stderr, "gsup watch: push lanes %u%s%s: %s\n", lanes, full ? " full" : "", drop ? ", the oldest dropped" : "", b); }
#endif
}
/* a word from a block list: the newest entry whose rectangle holds the word gives the bits it wrote */
static uint32_t word_list(const GuMap *m, const uint32_t *L, uint32_t wa, uint32_t *known)
{
  uint32_t v = 0, done = 0, kn = 0, bn = wa >> 6 & 0x3fff; int wi = wa & 63;
  for (int k = 0; k < GU_K && L[k] && done != 0xffffffffu; k++) {
    uint32_t e = L[k], bits = LANE_BITS[e >> 24 & 7] & ~done;
    const Xf *x = &m->x[(e & 0xffffff) - 1];
    if (!bits) continue;
    if (x->whole || !x->bw) { done |= bits; continue; }   /* its words read as missing */
    int nu = units(x->psm);
    if (nu) {   /* another layout: each nibble, byte or half of the word is a pixel of its own */
      uint32_t rel = (bn - x->bp) & 0x3fff; int ub = 32 / nu;
      for (int u = 0; u < nu; u++) {
        uint32_t um = (((1u << ub) - 1) << (u * ub)) & bits; int px, py;
        if (!um) continue;
        if (!unit_xy(x, rel, wi * nu + u, &px, &py)) { done |= um; continue; }
        if (px < x->sx || px >= x->sx + x->w || py < x->sy || py >= x->sy + x->h) continue;   /* a pixel it did not write */
        uint32_t pi = (uint32_t)(py - x->sy) * (uint32_t)x->w + (uint32_t)(px - x->sx), off = nu == 8 ? pi >> 1 : nu == 4 ? pi : 2 * pi, nb = nu == 2 ? 2 : 1;
        if (!x->bad && off + nb > x->got) continue;   /* its data stopped short (or has not come): the GS did not write the pixel */
        done |= um;
        if (x->bad) continue;
        const uint8_t *s = xf_bytes(m, x, off, nb); if (!s) continue;
        uint32_t val = nu == 8 ? (uint32_t)(s[0] >> ((pi & 1) * 4) & 15) : nu == 4 ? s[0] : (uint32_t)(s[0] | s[1] << 8);   /* T4: the first pixel in the low nibble */
        v |= val << (u * ub) & um; kn |= um;
      }
      continue;
    }
    uint32_t rel = (bn - x->bp) & 0x3fff, page = rel >> 5; int b = ibt32[rel & 31], p = ict32[wi];
    int px = (int)(page % x->bw) * 64 + (b & 7) * 8 + (p & 7), py = (int)(page / x->bw) * 32 + (b >> 3) * 8 + (p >> 3);
    if (px < x->sx || px >= x->sx + x->w || py < x->sy || py >= x->sy + x->h) continue;   /* a word it did not write */
    uint32_t pi = (uint32_t)(py - x->sy) * (uint32_t)x->w + (uint32_t)(px - x->sx), off, nb, val;
    switch (x->psm) {
      case GS_CT32: off = 4 * pi; nb = 4; break;
      case GS_CT24: off = 3 * pi; nb = 3; break;
      case GS_T8H: off = pi; nb = 1; break;
      default: off = pi >> 1; nb = 1; break;   /* T4HL, T4HH: the first pixel in the low nibble */
    }
    if (!x->bad && off + nb > x->got) continue;   /* its data stopped short (or has not come): the GS did not write the word */
    done |= bits;
    if (x->bad) continue;
    const uint8_t *s = xf_bytes(m, x, off, nb); if (!s) continue;
    switch (x->psm) {
      case GS_CT32: val = s[0] | s[1] << 8 | s[2] << 16 | (uint32_t)s[3] << 24; break;
      case GS_CT24: val = s[0] | s[1] << 8 | (uint32_t)s[2] << 16; break;
      case GS_T8H: val = (uint32_t)s[0] << 24; break;
      default: val = (uint32_t)(s[0] >> ((pi & 1) * 4) & 15) << (x->psm == GS_T4HL ? 24 : 28); break;
    }
    v |= val & bits; kn |= bits;
  }
  if (m->zero_empty) kn |= ~done & ~LANE_BITS[m->lost[bn]];   /* no entry: zero since the boot's GS clear, unless the map lost it */
  *known = kn; return v;
}
uint32_t gu_word(const GuMap *m, uint32_t wa, uint32_t *known) { return word_list(m, m->bl[wa >> 6 & 0x3fff], wa, known); }
void gu_block(const GuMap *m, uint32_t bn, uint32_t need, uint32_t v[64], uint32_t known[64])
{ for (uint32_t i = 0; i < 64; i++) { v[i] = gu_word(m, (bn & 0x3fff) << 6 | i, &known[i]); known[i] &= need; } }

/* the blocks a rectangle of a buffer covers, cell by cell of the format's block size: fn(bn, full) */
static int cell_w(int psm) { return psm == GS_T4 ? 32 : (psm == GS_CT16 || psm == GS_CT16S || psm == GS_Z16 || psm == GS_Z16S || psm == GS_T8) ? 16 : 8; }
static int cell_h(int psm) { return (psm == GS_T8 || psm == GS_T4) ? 16 : 8; }
static uint32_t cell_bn(int psm, uint32_t bp, uint32_t bw, int x, int y)
{
  switch (psm) {
    case GS_CT16: case GS_Z16: return gs_pa16(bp, bw, x, y, 0, psm == GS_Z16 ? 0x18 : 0) >> 8;
    case GS_CT16S: case GS_Z16S: return gs_pa16(bp, bw, x, y, 1, psm == GS_Z16S ? 0x18 : 0) >> 8;
    case GS_T8: return gs_pa8(bp, bw, x, y) >> 8;
    case GS_T4: return gs_pa4(bp, bw, x, y) >> 9;
    default: return gs_pa32(bp, bw, x, y, (psm == GS_Z32 || psm == GS_Z24) ? 0x18 : 0) >> 8;
  }
}
static void cover(GuMap *m, int xi, int psm, uint32_t bp, uint32_t bw, int sx, int sy, int w, int h, uint32_t lanes)
{
  int cw = cell_w(psm), ch = cell_h(psm), x1 = sx + w > 2048 ? 2048 : sx + w, y1 = sy + h > 2048 ? 2048 : sy + h;
  for (int cy = sy & -ch; cy < y1; cy += ch)
    for (int cx = sx & -cw; cx < x1; cx += cw)
      bl_push(m, cell_bn(psm, bp, bw, cx, cy), xi, lanes, cx >= sx && cx + cw <= sx + w && cy >= sy && cy + ch <= sy + h);
}

/* ---- transfers ---- */
static int psm_bits(int psm)
{
  switch (psm) { case GS_CT32: case GS_Z32: return 32; case GS_CT24: case GS_Z24: return 24; case GS_CT16: case GS_CT16S: case GS_Z16: case GS_Z16S: return 16;
                 case GS_T8: case GS_T8H: return 8; default: return 4; }
}
/* the word lanes a transfer in the 32-bit layout writes (LANE_BITS); 0: another layout */
static uint32_t psm_lanes(int psm)
{ return psm == GS_CT32 ? 7 : psm == GS_CT24 ? 1 : psm == GS_T8H ? 6 : psm == GS_T4HL ? 2 : psm == GS_T4HH ? 4 : 0; }
uint64_t gu_area_id(GuMap *m, int psm, uint32_t bp, uint32_t bw, int x0, int y0, int x1, int y1, int *flags)
{
  uint32_t lanes = psm == GS_Z24 ? 1 : psm_lanes(psm); int cw = cell_w(psm), ch = cell_h(psm), f = 0; uint64_t h = FNV0;
  if (!lanes) lanes = 7;   /* the T4, T8 and 16-bit layouts, Z32: whole words of their own blocks */
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > 2047) x1 = 2047;
  if (y1 > 2047) y1 = 2047;
  for (int cy = y0 & -ch; cy <= y1; cy += ch)
    for (int cx = x0 & -cw; cx <= x1; cx += cw) { int ff; h = mix(h, gu_lanes_id(m, cell_bn(psm, bp, bw, cx, cy), lanes, &ff)); f |= ff; }
  *flags = f; return h;
}
/* a local -> local transfer (x: its record, destination rectangle and format set): each pixel as the map knows the source pixel when
   the copy runs, in the bytes a host transfer of that format would send.  The GS copies pixel values; only formats of one size are
   taken (T4 / T4HL / T4HH, T8 / T8H, CT16 / CT16S, CT24, CT32).  Returns the pixels (malloc'd), or 0 when a source pixel is not known
   (the GS drew it, or no modelled transfer wrote it). */
static uint8_t *local_copy(GuMap *m, const Xf *x, uint64_t b, uint64_t p)
{
  int spsm = (int)(b >> 24 & 63), ssx = (int)(p & 0x7ff), ssy = (int)(p >> 16 & 0x7ff), bits = psm_bits(x->psm);
  uint32_t sbp = (uint32_t)(b & 0x3fff), sbw = (uint32_t)(b >> 16 & 63);
  int sidx = spsm == GS_T8 || spsm == GS_T4 || spsm == GS_T8H || spsm == GS_T4HL || spsm == GS_T4HH, s16 = spsm == GS_CT16 || spsm == GS_CT16S;
  if (psm_bits(spsm) != bits || (!sidx && !s16 && spsm != GS_CT32 && spsm != GS_CT24) || !sbw) return 0;
  uint8_t *o = calloc(1, x->need ? x->need : 1); if (!o) return 0;
  for (int j = 0; j < x->h; j++)
    for (int i = 0; i < x->w; i++) {
      uint32_t pi = (uint32_t)j * (uint32_t)x->w + (uint32_t)i, v, kn; int sx = (ssx + i) & 2047, sy = (ssy + j) & 2047;
      if (sidx || s16) { int64_t k = gu_texel(m, spsm, sbp, sbw, sx, sy); if (k < 0) { free(o); return 0; } v = (uint32_t)k; kn = 0; }
      else { v = gu_word(m, gs_pa32(sbp, sbw, sx, sy, 0) >> 2, &kn); if ((kn & (spsm == GS_CT24 ? 0xffffffu : 0xffffffffu)) != (spsm == GS_CT24 ? 0xffffffu : 0xffffffffu)) { free(o); return 0; } }
      switch (x->psm) {
        case GS_CT32: memcpy(o + 4 * pi, &v, 4); break;
        case GS_CT24: o[3 * pi] = (uint8_t)v; o[3 * pi + 1] = (uint8_t)(v >> 8); o[3 * pi + 2] = (uint8_t)(v >> 16); break;
        case GS_T8H: case GS_T8: o[pi] = (uint8_t)v; break;
        case GS_CT16: case GS_CT16S: o[2 * pi] = (uint8_t)v; o[2 * pi + 1] = (uint8_t)(v >> 8); break;
        default: o[pi >> 1] |= (uint8_t)((v & 15) << ((pi & 1) * 4)); break;   /* T4, T4HL, T4HH: the first pixel in the low nibble */
      }
    }
  return o;
}
void gu_trxdir(GuMap *m, const uint64_t *gr)
{
  uint64_t b = gr[0x50], p = gr[0x51], r = gr[0x52]; int dir = (int)(gr[0x53] & 3), w = (int)(r & 0xfff), h = (int)(r >> 32 & 0xfff);
  if (m->cur >= 0) { Xf *c = &m->x[m->cur]; if (m->keys && !c->bad && c->got && c->got < c->need) xf_ident(m, c, 1); xf_unref(m, m->cur); m->cur = -1; }
  if (dir == 1 || dir == 3) return;   /* local -> host, off: nothing written */
  int psm = (int)(b >> 56 & 63), sx = (int)(p >> 32 & 0x7ff), sy = (int)(p >> 48 & 0x7ff); uint32_t bp = (uint32_t)(b >> 32 & 0x3fff), bw = (uint32_t)(b >> 48 & 63);
  int i = xf_new(m); if (i < 0) return;
  Xf *x = &m->x[i];
  x->bp = bp; x->bw = bw; x->psm = psm; x->sx = sx; x->sy = sy; x->w = w; x->h = h; x->need = (uint32_t)(((uint64_t)w * h * psm_bits(psm) + 7) / 8);
  x->refs = 1;
#ifdef GU_SENT
  if (m->trx_fn) m->trx_fn(m->trx_ctx, gr);
#endif
  if (dir == 2) {   /* a local copy: resolved from the map now (before its own blocks change), in a modelled layout; else not known */
    uint32_t ln = psm_lanes(psm);
    m->st.xfer_local++;
    if (w && h) {
      uint8_t *o = (ln || other_ok(psm, bw)) && bw && sx + w <= 2048 && sy + h <= 2048 ? local_copy(m, x, b, p) : 0;
#ifdef GU_SENT
      if (o && m->keep) { int r = m->read_sent; m->read_sent = 1; x->sent = local_copy(m, x, b, p); m->read_sent = r; }   /* dev: as the GS copied */
#endif
      if (o) { x->own = o; x->got = x->need; m->st.own_bytes += x->need;
        if (m->keys) { x->id = mix(mix(fnv(FNV0 ^ 2, o, x->need), (uint64_t)(uint32_t)psm << 48 | (uint64_t)bp << 16 | bw),
                                   (uint64_t)(uint32_t)sx << 48 | (uint64_t)(uint32_t)sy << 32 | (uint64_t)(uint32_t)w << 16 | (uint32_t)h) | 1; x->idgot = x->got; } }
      else {   /* not known; a copy of a drawn area is drawn too (on the PSP the GE copies its own render target) */
        int f = 0; gu_area_id(m, (int)(b >> 24 & 63), (uint32_t)(b & 0x3fff), (uint32_t)(b >> 16 & 63), (int)(p & 0x7ff), (int)(p >> 16 & 0x7ff),
                              (int)(p & 0x7ff) + w - 1, (int)(p >> 16 & 0x7ff) + h - 1, &f);
        if (f & GU_ID_DRAWN) { x->drawn = 1; m->st.local_drawn++; }
        m->st.local_unknown++; x->bad = x->whole = 1; }
      cover(m, i, psm, bp, bw, sx, sy, w, h, ln ? ln : 7);
    }
    xf_unref(m, i); return;
  }
  m->st.xfers++; m->cur = i; x->no = (uint32_t)m->st.xfers;
  if (!w || !h) return;
  uint32_t lanes = psm_lanes(psm);
  if (!lanes) { m->st.xfer_other++; lanes = 7; if (!other_ok(psm, bw)) { m->st.xfer_unmod++; x->bad = x->whole = 1; } }
  if (!x->bad && (!bw || sx + w > 2048 || sy + h > 2048)) { m->st.xfer_wrap++; x->bad = x->whole = 1; }
  cover(m, i, psm, bp, bw, sx, sy, w, h, lanes);
#ifdef GU_SENT
  if (m->keep && !x->bad) x->sent = calloc(1, x->need);   /* dev: filled as its IMAGE data comes */
#endif
}
void gu_image(GuMap *m, uint32_t ee, uint32_t n)
{
  if (m->cur < 0) { m->st.image_orphan++; return; }
  Xf *x = &m->x[m->cur];
  if (x->got >= x->need) { m->st.image_orphan++; return; }
  if (x->src == GU_NOSRC) x->src = ee;
  else if (ee != x->src + x->got && !x->bad) { x->bad = 1; m->st.xfer_split++; }
#ifdef GU_SENT
  if (x->sent && !x->bad) { const uint8_t *s = m->ram(m->ctx, ee, n); uint32_t k = x->need - x->got < n ? x->need - x->got : n; if (s) memcpy(x->sent + x->got, s, k); }
#endif
  x->got += n;
  if (m->keys && !x->bad && x->got >= x->need && x->got - n < x->need) xf_ident(m, x, 1);
  if (m->data_fn) m->data_fn(m->data_ctx, x->no, ee, n, x->got, x->need);
}
void gu_image_run(GuMap *m, uint32_t ee, uint32_t n)
{
  while (n) {
    if (m->cur < 0 || m->x[m->cur].got >= m->x[m->cur].need) { m->st.image_orphan += n / 16; return; }
    const Xf *x = &m->x[m->cur]; uint32_t k = (x->need - x->got + 15) & ~15u;
    if (k > n) k = n;
    gu_image(m, ee, k); ee += k; n -= k;
  }
}
void gu_on_data(GuMap *m, GuDataFn fn, void *ctx) { m->data_fn = fn; m->data_ctx = ctx; }
#ifdef GU_SENT
void gu_keep_sent(GuMap *m, int on) { m->keep = on; }
void gu_read_sent(GuMap *m, int on) { m->read_sent = on; }
long gu_sent_scan(const GuMap *m, void (*fn)(void *ctx, uint32_t xfer, uint32_t ee, uint32_t got, uint32_t first, uint32_t ndiff), void *ctx)
{
  long n = 0;
  for (int i = 0; i < m->nx; i++) {
    const Xf *x = &m->x[i]; uint32_t got = x->got < x->need ? x->got : x->need;
    if (!x->refs || !x->sent || x->bad || x->src == GU_NOSRC || !got) continue;
    const uint8_t *s = m->ram(m->ctx, x->src, got); if (!s) continue;
    n++;
    if (!memcmp(s, x->sent, got)) continue;
    uint32_t first = ~0u, nd = 0;
    for (uint32_t k = 0; k < got; k++) if (s[k] != x->sent[k]) { if (first == ~0u) first = k; nd++; }
    fn(ctx, x->no, x->src, got, first, nd);
  }
  return n;
}
void gu_on_trx(GuMap *m, void (*fn)(void *ctx, const uint64_t *gr), void *ctx) { m->trx_fn = fn; m->trx_ctx = ctx; }
void gu_watch(GuMap *m, int bn) { m->watch = bn; }
int gu_why(const GuMap *m, uint32_t wa, char *buf, int n)
{
  const uint32_t *L = m->bl[wa >> 6 & 0x3fff]; int o = snprintf(buf, (size_t)n, "block %04x:", wa >> 6 & 0x3fff);
  if (!L[0]) o += snprintf(buf + o, (size_t)(n > o ? n - o : 0), " no transfer");
  for (int k = 0; k < GU_K && L[k] && o < n; k++) {
    const Xf *x = &m->x[(L[k] & 0xffffff) - 1];
    o += snprintf(buf + o, (size_t)(n - o), " [%u psm %02x bp %04x bw %u at %d,%d %dx%d lanes %u%s%s%s got %u/%u]", x->no, x->psm, x->bp, x->bw, x->sx, x->sy, x->w,
                  x->h, L[k] >> 24 & 7, x->drawn ? " drawn" : x->bad ? " bad" : "", x->whole && !x->drawn ? " whole" : "", x->own ? " own" : x->src == GU_NOSRC && !x->drawn ? " nosrc" : "", x->got, x->need);
  }
  return o;
}
#endif
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

/* ---- areas the GS drew ---- */
void gu_drawn(GuMap *m, int psm, uint32_t bp, uint32_t bw, uint32_t bits, int x0, int y0, int x1, int y1)
{
  uint32_t lanes;   /* the lanes of the words it wrote: by format (CT and Z alike) and the bits not masked */
  switch (psm) {
    case GS_CT32: case GS_Z32: lanes = (bits & 0xffffffu ? 1 : 0) | (bits & 0x0f000000u ? 2 : 0) | (bits & 0xf0000000u ? 4 : 0); break;
    case GS_CT24: case GS_Z24: lanes = bits & 0xffffffu ? 1 : 0; break;   /* bits 24-31 are left as they were */
    case GS_CT16: case GS_CT16S: case GS_Z16: case GS_Z16S: lanes = bits & 0xffffu ? 7 : 0; break;
    default: return;   /* not a frame or depth format */
  }
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > 2047) x1 = 2047;
  if (y1 > 2047) y1 = 2047;
  if (!lanes || !bw || x0 > x1 || y0 > y1) return;
  int k, o;
  for (k = 0; k < GU_DR; k++) { const Dr *d = &m->dr[k];
    if (d->used && d->psm == psm && d->bp == bp && d->bw == bw && d->lanes == lanes && d->x0 == x0 && d->y0 == y0 && d->x1 == x1 && d->y1 == y1) break; }
  if (k < GU_DR) { m->dr[k].used = ++m->dr_clock; if (!m->dr[k].dirty) { m->st.drawn_kept++; return; } }
  else {   /* the least recently used slot; its blocks lose its bit */
    for (k = 0, o = 1; o < GU_DR; o++) if (m->dr[o].used < m->dr[k].used) k = o;
    if (m->dr[k].used) for (int b = 0; b < 16384; b++) m->drb[b] &= (uint8_t)~(1u << k);
    m->dr[k].psm = psm; m->dr[k].bp = bp; m->dr[k].bw = bw; m->dr[k].lanes = lanes; m->dr[k].x0 = x0; m->dr[k].y0 = y0; m->dr[k].x1 = x1; m->dr[k].y1 = y1;
    m->dr[k].used = ++m->dr_clock;
  }
  m->dr[k].dirty = 0;
  int i = xf_new(m); if (i < 0) return;
  Xf *x = &m->x[i];
  x->bp = bp; x->bw = bw; x->psm = psm; x->sx = x0; x->sy = y0; x->w = x1 - x0 + 1; x->h = y1 - y0 + 1; x->bad = x->whole = x->drawn = 1; x->refs = 1;
  m->st.drawn++;
  int cw = cell_w(psm), ch = cell_h(psm);
  for (int cy = y0 & -ch; cy <= y1; cy += ch)
    for (int cx = x0 & -cw; cx <= x1; cx += cw) {
      uint32_t bn = cell_bn(psm, bp, bw, cx, cy);
      bl_push(m, bn, i, lanes, cx >= x0 && cx + cw <= x1 + 1 && cy >= y0 && cy + ch <= y1 + 1);
      m->drb[bn & 0x3fff] |= (uint8_t)(1u << k);
    }
  xf_unref(m, i);
}

/* ---- the CLUT buffer ---- */
static int is_idx(int psm) { return psm == GS_T8 || psm == GS_T4 || psm == GS_T8H || psm == GS_T4HL || psm == GS_T4HH; }
static void row_set(GuMap *m, int r, int li, int e0, int hi)
{
  Row *w = &m->row[r];
  m->l[li].refs++;
  if (w->ld >= 0) ld_unref(m, w->ld);
  w->ld = li; w->e0 = e0; w->hi = hi;
}
/* entry e of load L: its place in the CLUT's buffer (gsref clut_load) */
static void ld_xy(const Ld *L, int e, int *x, int *y, uint32_t *bw)
{
  *bw = 1;
  if (L->csm) { *bw = L->cbw; *x = L->cou * 16 + e; *y = L->cov; }
  else if (L->n == 256) { int p = (e & 0xe7) | (e & 8) << 1 | (e & 0x10) >> 1; *x = p & 15; *y = p >> 4; }   /* CSM1: entries 8-15 and 16-23 swapped */
  else { *x = e & 7; *y = e >> 3; }
  *x &= 2047; *y &= 2047;
}
void gu_clut_load(GuMap *m, const uint64_t *gr, int ctxt)
{
  uint64_t t0 = gr[0x06 + ctxt];
  int psm = (int)(t0 >> 20 & 63), cpsm = (int)(t0 >> 51 & 15), csm = (int)(t0 >> 55 & 1), csa = (int)(t0 >> 56 & 31), cld = (int)(t0 >> 61 & 7);
  uint32_t cbp = (uint32_t)(t0 >> 37 & 0x3fff);
  if (!is_idx(psm)) return;
  switch (cld) { case 1: break; case 2: m->cbp0 = cbp; break; case 3: m->cbp1 = cbp; break;
    case 4: if (cbp == m->cbp0) return; m->cbp0 = cbp; break; case 5: if (cbp == m->cbp1) return; m->cbp1 = cbp; break; default: return; }
  int li = ld_new(m); if (li < 0) return;
  Ld *L = &m->l[li]; int n = (psm == GS_T8 || psm == GS_T8H) ? 256 : 16, c32 = cpsm == GS_CT32 || cpsm == GS_CT24;
  m->st.loads++;
  L->cbp = cbp; L->cpsm = cpsm; L->csm = csm; L->n = n;
  if (csm) { uint64_t tc = gr[0x1c]; L->cbw = (uint32_t)(tc & 63); L->cou = (int)(tc >> 6 & 63); L->cov = (int)(tc >> 12 & 1023); }
  /* the blocks the entries are in: the first and last entry's rows and columns, cell by cell */
  int fmt = c32 ? GS_CT32 : cpsm == GS_CT16S ? GS_CT16S : GS_CT16, cw = cell_w(fmt), ch = cell_h(fmt), x0, y0, x1, y1; uint32_t bw;
  if (csm) { ld_xy(L, 0, &x0, &y0, &bw); x1 = x0 + n - 1; y1 = y0; }
  else { x0 = y0 = 0; x1 = n == 256 ? 15 : 7; y1 = n == 256 ? 15 : 1; bw = 1; }
  for (int cy = y0 & -ch; cy <= y1 && !L->bad; cy += ch)
    for (int cx = x0 & -cw; cx <= x1 && !L->bad; cx += cw) {
      uint32_t bn = cell_bn(fmt, cbp, bw, cx & 2047, cy & 2047); int k;
      for (k = 0; k < L->nb && L->bn[k] != bn; k++) {}
      if (k < L->nb) continue;
      if (L->nb == 4) { L->bad = 1; m->st.load_wide++; break; }
      L->bn[L->nb] = (uint16_t)bn; memcpy(L->snap[L->nb], m->bl[bn], sizeof L->snap[0]);
      for (int j = 0; j < GU_K && L->snap[L->nb][j]; j++) m->x[(L->snap[L->nb][j] & 0xffffff) - 1].refs++;
      L->nb++;
    }
  if (m->keys) {   /* the identity of what it loads: its parameters and its blocks' lists as at the load */
    uint64_t h = mix(mix(FNV0, (uint64_t)cbp << 32 | (uint64_t)(uint32_t)cpsm << 8 | (uint64_t)(uint32_t)csm << 4 | (uint64_t)(n == 256)),
                     (uint64_t)L->cbw << 32 | (uint64_t)(uint32_t)L->cou << 16 | (uint32_t)L->cov);
    int f = L->bad ? GU_ID_LOST : 0;
    for (int b = 0; b < L->nb; b++) { int ff; h = mix(h, list_id(m, L->snap[b], L->bn[b], 7, &ff)); f |= ff; }
    m->l[li].id = h; m->l[li].idf = f;
  }
  for (int j = 0; j < n / 16; j++) {
    if (c32) { int r = ((csa * 16 + j * 16) & 255) >> 4; row_set(m, r, li, j * 16, 0); row_set(m, r + 16, li, j * 16, 1); }
    else row_set(m, ((csa * 16 + j * 16) & 511) >> 4, li, j * 16, 0);
  }
  if (!m->l[li].refs) { m->l[li].refs = 1; ld_unref(m, li); }
}
/* half h (0..511) of the CLUT buffer */
static int half(const GuMap *m, int h)
{
  const Row *w = &m->row[h >> 4]; if (w->ld < 0) return -1;
  const Ld *L = &m->l[w->ld]; if (L->bad) return -1;
  int x, y, e = w->e0 + (h & 15); uint32_t bw, wa, sh, kn;
  ld_xy(L, e, &x, &y, &bw);
  if (L->cpsm == GS_CT32 || L->cpsm == GS_CT24) { wa = gs_pa32(L->cbp, bw, x, y, 0) >> 2; sh = w->hi ? 16 : 0; }
  else { uint32_t a = gs_pa16(L->cbp, bw, x, y, L->cpsm == GS_CT16S, 0); wa = a >> 2; sh = (a & 2) ? 16 : 0; }
  int k; for (k = 0; k < L->nb && L->bn[k] != (wa >> 6 & 0x3fff); k++) {}
  if (k == L->nb) return -1;
  uint32_t v = word_list(m, L->snap[k], wa, &kn), mk = 0xffffu << sh;
  return (kn & mk) == mk ? (int)((v & mk) >> sh) : -1;
}
uint64_t gu_clut_id(const GuMap *m, int cpsm, int k0, int n, int *flags)
{
  int c32 = cpsm == GS_CT32 || cpsm == GS_CT24, f = 0; uint64_t h = mix(FNV0, (uint64_t)(uint32_t)cpsm << 1 | (uint64_t)c32);
  for (int k = k0 & ~15; k < k0 + n; k += 16)
    for (int part = 0; part < (c32 ? 2 : 1); part++) {
      const Row *w = &m->row[c32 ? ((k & 255) >> 4) + 16 * part : (k & 511) >> 4];
      if (w->ld < 0) { f |= GU_ID_LOST; h = mix(h, 0xe0e0u); continue; }   /* a row no load filled */
      const Ld *L = &m->l[w->ld];
      f |= L->idf | (L->bad ? GU_ID_LOST : 0); h = mix(mix(h, L->id), (uint64_t)(uint32_t)w->e0 << 8 | (uint64_t)(uint32_t)w->hi);
    }
  *flags = f; return h;
}
/* gsup.h: the flags of gu_area_id; the reference has no memos to switch */
int gu_area_flags(GuMap *m, int psm, uint32_t bp, uint32_t bw, int x0, int y0, int x1, int y1) { int f = 0; gu_area_id(m, psm, bp, bw, x0, y0, x1, y1, &f); return f; }
void gu_area_memo(GuMap *m, int on) { (void)m; (void)on; }
void gu_clut_memo(GuMap *m, int on) { (void)m; (void)on; }
int64_t gu_clut(const GuMap *m, int cpsm, int k)
{
  if (cpsm == GS_CT32 || cpsm == GS_CT24) { int lo = half(m, k & 255), hi = half(m, (k & 255) + 256); return lo < 0 || hi < 0 ? -1 : (int64_t)((uint32_t)lo | (uint32_t)hi << 16); }
  return half(m, k & 511);
}
