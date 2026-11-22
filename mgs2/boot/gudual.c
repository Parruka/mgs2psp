/* gudual.c - dev, x86 check builds only (tools/dev/build_xlive.sh with GU=dual): the texture map of gsup.h given by two maps side by
   side, fed the same calls: ge/gsup.c (the reference, its names renamed gr_ by ge/gu_ren.h) and ge/gsup2.c (the lean form, gl_)
   (renderer_notes 11.12, task #47).  Words, indices, texels and CLUT entries come from both and are compared; the reference's are given.
   Identities and their flags come from both; the lean map's are given (so ge/texcache.c keys its cache by them and boot/xlive_chk.c's
   texture path check, which compares the cache with the map's texels, checks them), and for each area and CLUT range the changes of the
   two identities are counted side by side: the lean one must change whenever the reference's does, unless what it names did not change.
   GuStats: the reference's.  At the end of the run (a destructor) a summary on stderr; the first differences are printed as they come.
   Game data passes through it: personal use only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GU_RP gr_
#define GU_RT Gr
#include "../ge/gu_ren.h"
#include "../ge/gsup.h"
#include "../ge/gu_unren.h"
#undef GU_RP
#undef GU_RT
#define GU_RP gl_
#define GU_RT Gl
#include "../ge/gu_ren.h"
#include "../ge/gsup.h"
#include "../ge/gu_unren.h"
#undef GU_RP
#undef GU_RT
#include "../ge/gsup.h"

struct GuMap { GrMap *a; GlMap *b; };
static GuMap *the;   /* the last map made (the summary's) */
static struct {
  long words, wdiff, texels, tdiff, cluts, cdiff, sent_words, sent_wdiff;
  long ids[2], fdiff[2], tr[2][2][2];   /* by kind (0 area, 1 CLUT): calls, flag differences, [reference changed][lean changed] */
  long aflags, aflags_diff;             /* gu_area_flags: calls, differences */
} D;
static int shown;
#define SHOW(...) do { if (shown++ < 40) fprintf(stderr, __VA_ARGS__); } while (0)
static int read_sent_on;

/* the last identities of each area / CLUT range asked for */
typedef struct { uint64_t k1, k2, ra, la; uint8_t used, kind; } Last;
#define LAST_N (1u << 16)
static Last *last;
static void id_pair(int kind, uint64_t k1, uint64_t k2, uint64_t ra, uint64_t la, int fa, int fb)
{
  D.ids[kind]++;
  if (fa != fb) { D.fdiff[kind]++; SHOW("gudual: %s %016llx %016llx: flags %d (reference) / %d (lean)\n", kind ? "CLUT" : "area", (unsigned long long)k1, (unsigned long long)k2, fa, fb); }
  if (!last && !(last = calloc(LAST_N, sizeof *last))) return;
  uint32_t i = (uint32_t)((k1 * 0x9e3779b97f4a7c15ull ^ k2 * 0xc2b2ae3d27d4eb4full ^ (uint64_t)kind) >> 40) & (LAST_N - 1);
  for (int n = 0; n < 64; n++, i = (i + 1) & (LAST_N - 1)) {
    Last *e = &last[i];
    if (!e->used) { e->used = 1; e->kind = (uint8_t)kind; e->k1 = k1; e->k2 = k2; e->ra = ra; e->la = la; return; }
    if (e->kind == kind && e->k1 == k1 && e->k2 == k2) {
      int rc = e->ra != ra, lc = e->la != la;
      D.tr[kind][rc][lc]++;
      if (rc && !lc && D.tr[kind][1][0] <= 20)
        fprintf(stderr, "gudual: %s %016llx %016llx: the reference's identity changed, the lean one did not (%ld so far)\n", kind ? "CLUT" : "area",
                (unsigned long long)k1, (unsigned long long)k2, D.tr[kind][1][0]);
      if (!rc && lc && D.tr[kind][0][1] <= 20)
        fprintf(stderr, "gudual: %s %016llx %016llx: the lean identity changed (%016llx to %016llx), the reference's did not (%016llx) (%ld so far)\n",
                kind ? "CLUT" : "area", (unsigned long long)k1, (unsigned long long)k2, (unsigned long long)e->la, (unsigned long long)la,
                (unsigned long long)ra, D.tr[kind][0][1]);
      e->ra = ra; e->la = la; return;
    }
  }
}

GuMap *gu_new(GuRamFn ram, void *ctx)
{
  GuMap *m = calloc(1, sizeof *m); if (!m) return 0;
  m->a = gr_new(ram, ctx); m->b = gl_new(ram, ctx);
  if (!m->a || !m->b) { gr_free(m->a); gl_free(m->b); free(m); return 0; }
  return the = m;
}
void gu_free(GuMap *m) { if (!m) return; if (the == m) the = 0; gr_free(m->a); gl_free(m->b); free(m); }
void gu_trxdir(GuMap *m, const uint64_t *gr) { gr_trxdir(m->a, gr); gl_trxdir(m->b, gr); }
void gu_image(GuMap *m, uint32_t ee, uint32_t n) { gr_image(m->a, ee, n); gl_image(m->b, ee, n); }
void gu_image_run(GuMap *m, uint32_t ee, uint32_t n) { gr_image_run(m->a, ee, n); gl_image_run(m->b, ee, n); }
void gu_clut_load(GuMap *m, const uint64_t *gr, int ctxt) { gr_clut_load(m->a, gr, ctxt); gl_clut_load(m->b, gr, ctxt); }
void gu_drawn(GuMap *m, int psm, uint32_t bp, uint32_t bw, uint32_t bits, int x0, int y0, int x1, int y1)
{ gr_drawn(m->a, psm, bp, bw, bits, x0, y0, x1, y1); gl_drawn(m->b, psm, bp, bw, bits, x0, y0, x1, y1); }
void gu_zero_empty(GuMap *m, int on) { gr_zero_empty(m->a, on); gl_zero_empty(m->b, on); }
void gu_keys(GuMap *m, int on) { gr_keys(m->a, on); gl_keys(m->b, on); }
void gu_on_data(GuMap *m, GuDataFn fn, void *ctx) { gr_on_data(m->a, fn, ctx); }
const GuStats *gu_stats(const GuMap *m) { return (const GuStats *)gr_stats(m->a); }

uint32_t gu_word(const GuMap *m, uint32_t wa, uint32_t *known)
{
  uint32_t ka, kb, va = gr_word(m->a, wa, &ka), vb = gl_word(m->b, wa, &kb);
  if (read_sent_on) D.sent_words++; else D.words++;
  if (ka != kb || (va & ka) != (vb & kb)) { if (read_sent_on) D.sent_wdiff++; else D.wdiff++;
    SHOW("gudual: word %06x%s: %08x known %08x (reference) / %08x known %08x (lean)\n", wa, read_sent_on ? " (as sent)" : "", va & ka, ka, vb & kb, kb); }
  *known = ka; return va;
}
void gu_block(const GuMap *m, uint32_t bn, uint32_t need, uint32_t v[64], uint32_t known[64])
{ for (uint32_t i = 0; i < 64; i++) { v[i] = gu_word(m, (bn & 0x3fff) << 6 | i, &known[i]); known[i] &= need; } }
int gu_index(const GuMap *m, int psm, uint32_t tbp, uint32_t tbw, int x, int y)
{
  int a = gr_index(m->a, psm, tbp, tbw, x, y), b = gl_index(m->b, psm, tbp, tbw, x, y);
  D.texels++;
  if (a != b) { D.tdiff++; SHOW("gudual: index psm %02x tbp %04x tbw %u (%d, %d): %d / %d\n", psm, tbp, tbw, x, y, a, b); }
  return a;
}
int64_t gu_texel(const GuMap *m, int psm, uint32_t tbp, uint32_t tbw, int x, int y)
{
  int64_t a = gr_texel(m->a, psm, tbp, tbw, x, y), b = gl_texel(m->b, psm, tbp, tbw, x, y);
  D.texels++;
  if (a != b) { D.tdiff++; SHOW("gudual: texel psm %02x tbp %04x tbw %u (%d, %d)%s: %lld / %lld\n", psm, tbp, tbw, x, y, read_sent_on ? " (as sent)" : "",
                                (long long)a, (long long)b); }
  return a;
}
int64_t gu_clut(const GuMap *m, int cpsm, int k)
{
  int64_t a = gr_clut(m->a, cpsm, k), b = gl_clut(m->b, cpsm, k);
  D.cluts++;
  if (a != b) { D.cdiff++; SHOW("gudual: CLUT cpsm %x entry %d%s: %lld / %lld\n", cpsm, k, read_sent_on ? " (as sent)" : "", (long long)a, (long long)b); }
  return a;
}
#ifdef GU_SENT
#include "../ge/gslayout.h"
/* GUDUAL_AREA=<psm>:<bp>:<bw>:<x0>:<y0>:<x1>:<y1> (hex): each time that area's identity is asked, both identities and, for each cell, both
   maps' account of its block (the T4 / T8 / 16-bit / 32-bit layouts as the maps' cells) */
static int wa_on = -1; static unsigned wa_a[7];
static void area_watch(GuMap *m, int psm, uint32_t bp, uint32_t bw, int x0, int y0, int x1, int y1, uint64_t a, uint64_t b)
{
  if (wa_on < 0) { const char *e = getenv("GUDUAL_AREA"); wa_on = e && sscanf(e, "%x:%x:%x:%x:%x:%x:%x", &wa_a[0], &wa_a[1], &wa_a[2], &wa_a[3], &wa_a[4], &wa_a[5], &wa_a[6]) == 7; }
  if (!wa_on || (unsigned)psm != wa_a[0] || bp != wa_a[1] || bw != wa_a[2] || (unsigned)x0 != wa_a[3] || (unsigned)y0 != wa_a[4] || (unsigned)x1 != wa_a[5] || (unsigned)y1 != wa_a[6]) return;
  fprintf(stderr, "gudual area: asked: %016llx (reference) / %016llx (lean)\n", (unsigned long long)a, (unsigned long long)b);
  int cw = psm == 0x14 ? 32 : (psm == 0x02 || psm == 0x0a || psm == 0x13) ? 16 : 8, ch = (psm == 0x13 || psm == 0x14) ? 16 : 8; char buf[2048];
  for (int cy = y0 & -ch; cy <= y1; cy += ch)
    for (int cx = x0 & -cw; cx <= x1; cx += cw) {
      uint32_t bn = psm == 0x14 ? gs_pa4(bp, bw, cx, cy) >> 9 : psm == 0x13 ? gs_pa8(bp, bw, cx, cy) >> 8 : gs_pa32(bp, bw, cx, cy, 0) >> 8;
      gr_why(m->a, bn << 6, buf, sizeof buf); fprintf(stderr, "gudual area:   cell %d,%d reference %s\n", cx, cy, buf);
      gl_why(m->b, bn << 6, buf, sizeof buf); fprintf(stderr, "gudual area:   cell %d,%d lean      %s\n", cx, cy, buf);
    }
}
#endif
uint64_t gu_area_id(GuMap *m, int psm, uint32_t bp, uint32_t bw, int x0, int y0, int x1, int y1, int *flags)
{
  int fa, fb; uint64_t a = gr_area_id(m->a, psm, bp, bw, x0, y0, x1, y1, &fa), b = gl_area_id(m->b, psm, bp, bw, x0, y0, x1, y1, &fb);
#ifdef GU_SENT
  area_watch(m, psm, bp, bw, x0, y0, x1, y1, a, b);
#endif
  id_pair(0, (uint64_t)(uint32_t)psm << 48 | (uint64_t)bp << 16 | bw, (uint64_t)(uint16_t)x0 << 48 | (uint64_t)(uint16_t)y0 << 32 | (uint64_t)(uint16_t)x1 << 16 | (uint16_t)y1, a, b, fa, fb);
  *flags = fb; return b;
}
uint64_t gu_clut_id(const GuMap *m, int cpsm, int k0, int n, int *flags)
{
  int fa, fb; uint64_t a = gr_clut_id(m->a, cpsm, k0, n, &fa), b = gl_clut_id(m->b, cpsm, k0, n, &fb);
  id_pair(1, (uint64_t)(uint32_t)cpsm, (uint64_t)(uint32_t)k0 << 32 | (uint32_t)n, a, b, fa, fb);
  *flags = fb; return b;
}
int gu_area_flags(GuMap *m, int psm, uint32_t bp, uint32_t bw, int x0, int y0, int x1, int y1)
{
  int fa = gr_area_flags(m->a, psm, bp, bw, x0, y0, x1, y1), fb = gl_area_flags(m->b, psm, bp, bw, x0, y0, x1, y1);
  D.aflags++;
  if (fa != fb) { D.aflags_diff++; SHOW("gudual: area flags psm %02x bp %04x bw %u (%d,%d)-(%d,%d): %d (reference) / %d (lean)\n", psm, bp, bw, x0, y0, x1, y1, fa, fb); }
  return fb;
}
void gu_area_memo(GuMap *m, int on) { gr_area_memo(m->a, on); gl_area_memo(m->b, on); }
void gu_clut_memo(GuMap *m, int on) { gr_clut_memo(m->a, on); gl_clut_memo(m->b, on); }
uint64_t gu_lanes_id(GuMap *m, uint32_t bn, uint32_t lanes, int *flags) { return gl_lanes_id(m->b, bn, lanes, flags); }
uint32_t gu_block_ver(const GuMap *m, uint32_t bn) { return gl_block_ver(m->b, bn); }

#ifdef GU_SENT
void gu_keep_sent(GuMap *m, int on) { gr_keep_sent(m->a, on); gl_keep_sent(m->b, on); }
void gu_read_sent(GuMap *m, int on) { gr_read_sent(m->a, on); gl_read_sent(m->b, on); read_sent_on = on; }
long gu_sent_scan(const GuMap *m, void (*fn)(void *ctx, uint32_t xfer, uint32_t ee, uint32_t got, uint32_t first, uint32_t ndiff), void *ctx)
{ return gr_sent_scan(m->a, fn, ctx); }
int gu_why(const GuMap *m, uint32_t wa, char *buf, int n)
{
  int o = gr_why(m->a, wa, buf, n);
  if (o < n - 16) { o += snprintf(buf + o, (size_t)(n - o), " | lean:"); o += gl_why(m->b, wa, buf + o, n - o); }
  return o;
}
void gu_on_trx(GuMap *m, void (*fn)(void *ctx, const uint64_t *gr), void *ctx) { gr_on_trx(m->a, fn, ctx); }
void gu_watch(GuMap *m, int bn) { gr_watch(m->a, bn); gl_watch(m->b, bn); }
#endif

__attribute__((destructor)) static void gudual_report(void)
{
  if (!the) return;
  const GlStats *S = gl_stats(the->b); const GrStats *R = gr_stats(the->a);
  fprintf(stderr, "gudual: words %ld, %ld differ; as sent %ld, %ld differ; texels and indices %ld, %ld differ; CLUT entries %ld, %ld differ\n",
          D.words, D.wdiff, D.sent_words, D.sent_wdiff, D.texels, D.tdiff, D.cluts, D.cdiff);
  for (int k = 0; k < 2; k++)
    fprintf(stderr, "gudual: %s identities %ld, flags differ %ld; asked again: both the same %ld, both changed %ld, only the reference's changed %ld, "
            "only the lean one's changed %ld\n", k ? "CLUT" : "area", D.ids[k], D.fdiff[k], D.tr[k][0][0], D.tr[k][1][1], D.tr[k][1][0], D.tr[k][0][1]);
  fprintf(stderr, "gudual: area flags asked alone %ld, differ %ld\n", D.aflags, D.aflags_diff);
  fprintf(stderr, "gudual: lean map: %ld transfers, %ld dropped as equal to the one in place, %ld in before their data was all in, %ld CLUT loads "
          "(%ld kept), areas drawn %ld (%ld draws kept them; the reference %ld / %ld), area identities made %ld / kept %ld, node allocation "
          "failures %ld; live records %d (the reference %d), live loads %d (%d)\n", S->xfers, S->elided, S->early, S->loads, S->ld_made, S->drawn,
          S->drawn_kept, R->drawn, R->drawn_kept, S->areas_made, S->areas_kept, S->dropped, S->live_xfers, R->live_xfers, S->live_loads, R->live_loads);
  fprintf(stderr, "gudual: lean map: blocks where a dropped upload's record went back on top %ld (too deep %ld); drawn areas marked again: "
          "not in the table %ld, dirty %ld; sweeps %ld; CLUT loads found by their parameters %ld\n", S->restacked, S->restack_deep, S->dt_new,
          S->dt_dirty, S->gcs, S->ld_memo);
}
