/* gelive.c: see gelive.h.  The rules are tools/gsref.c's export (ge_tex, ge_prim, xc_run), which the replay pictures were checked with:
   a CT32 / CT24 texture at block 0 or 0x1000 is a frame buffer (GE kind 2: the one being drawn, or the other), an indexed or 16-bit
   texture comes from the texture path, anything else draws untextured. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "gelive.h"
#include "gslayout.h"
#include "texconv.h"
#include "vu1conv.h"
#include "gelist.h"
#include "fxthin.h"   /* task #114 */
#include "me_rt.h"   /* task #149: -DMGS2_ME_SAFE (ME-safe calls) */

static long gen_of(const GuMap *m)   /* moves whenever a texture source can have changed: transfers, local copies, drawn areas, CLUT loads */
{
  const GuStats *u = gu_stats(m); return u->xfers + u->xfer_local + u->drawn + u->loads;
}
static int add_tex(GlFrame *f, uint64_t key, const GerTex *t, int u0, int v0, const uint8_t *data, const uint32_t *pal, uint32_t size)
{
  uint32_t h = (uint32_t)((key * 0x9e3779b97f4a7c15ull) >> 54) & (GL_HASH - 1);
  for (int k = 0; k < GL_HASH; k++, h = (h + 1) & (GL_HASH - 1)) {
    int i = f->hash[h];
    if (i < 0) break;
    if (f->tkey[i] == key) return i;
  }
  int i = gb_tex(&f->o, t, u0, v0);
  if (i < 0) { f->n.tex_full++; return -1; }
  f->hash[h] = (int16_t)i; f->tkey[i] = key; f->tdata[i] = data; f->tpal[i] = pal; f->tsize[i] = size;
  f->n.tex_new++; if (t->kind == 1) { f->n.tbytes += size + t->npal * 4; if (t->psm == 16) f->n.tex16++; }
  return i;
}
/* the GE texture of a draw with TEX0 / CLAMP of context ctxt (or the CLAMP *clp), drawn into the frame buffer at block fbp: its index, or -1
   (untextured) */
static int gl_tex(GlFrame *f, const uint64_t *gr, int ctxt, uint32_t fbp, XcTex *xt, const uint64_t *clp)
{
  uint64_t t0 = gr[0x06 + ctxt], cl = clp ? *clp : gr[0x08 + ctxt], texa = gr[0x3b];
  int psm = (int)(t0 >> 20 & 63); uint32_t tbp = (uint32_t)(t0 & 0x3fff);
  f->n.tex_asks++;
  memset(xt, 0, sizeof *xt); xt->index = -1;
  if (psm == GS_CT32 || psm == GS_CT24) {   /* a frame buffer: the one being drawn (depth blur) or the other (motion trail) */
    if (tbp != 0 && tbp != 0x1000) { f->n.tex_unsup++; return -1; }
    GerTex t; memset(&t, 0, sizeof t); t.kind = 2; t.psm = tbp != fbp; t.tw = t.th = 512; t.bw = 512; t.rows = 272;
    int i = add_tex(f, 0xfb00000000000000ull | t.psm, &t, 0, 0, 0, 0, 0);
    if (i < 0) return -1;
    f->n.tex_fb++;
    xt->index = i; xt->kind = 2; xt->tw = xt->th = 512; xt->minalpha = 0;
    return i;
  }
  long gen = f->memo_on ? gen_of(f->map) : 0;
  if (f->memo_on && f->memo.valid && f->memo.t0 == t0 && f->memo.cl == cl && f->memo.texa == texa && f->memo.gen == gen) {
    f->n.tex_memo++;
    if (!f->memo_check) { *xt = f->memo.xt; return f->memo.ti; }
  }
  TkTex k; int kind = tk_get_um(f->tk, gr, ctxt, cl, &k);   /* sb9: through the memo by uploads (OPT2_TX_UMEMO; off: tk_get_cl, as gl12) */
  if (kind != TK_TEX) {
    if (kind == TK_RT) f->n.tex_rt++; else if (kind == TK_LOST) f->n.tex_lost++; else f->n.tex_unsup++;
    if (f->memo_check && f->memo.valid && f->memo.t0 == t0 && f->memo.cl == cl && f->memo.texa == texa && f->memo.gen == gen) f->memo_bad++;
    f->memo.valid = 0; return -1;
  }
  uint64_t key = (k.id ^ (k.pal_id * 0xff51afd7ed558ccdull)) & ~0xff00000000000000ull;
  if (f->memo_check && f->memo.valid && f->memo.t0 == t0 && f->memo.cl == cl && f->memo.texa == texa && f->memo.gen == gen
      && (f->memo.ti < 0 || f->tkey[f->memo.ti] != key || f->memo.xt.minalpha != k.minalpha)) f->memo_bad++;
  GerTex t; memset(&t, 0, sizeof t);
  const TcTex *tt = k.tp; uint32_t size = k.size;   /* sb9: the description and tc_texture_size, as tk_get_um gives them */
  t.kind = 1; t.psm = (uint8_t)tt->psm; t.repeat = (uint8_t)tt->repeat; t.minalpha = (uint8_t)k.minalpha;
  t.tw = (uint16_t)tt->tw; t.th = (uint16_t)tt->th; t.bw = (uint16_t)tt->bw; t.rows = (uint16_t)tt->rows; t.size = size; t.npal = (uint32_t)k.npal;
  int i = add_tex(f, key, &t, tt->u0, tt->v0, k.data, k.pal, size);
  if (i < 0) { f->memo.valid = 0; return -1; }
  xt->index = i; xt->kind = 1; xt->tw = tt->tw; xt->th = tt->th; xt->u0 = tt->u0; xt->v0 = tt->v0; xt->minalpha = k.minalpha;
  if (f->memo_on) { f->memo.t0 = t0; f->memo.cl = cl; f->memo.texa = texa; f->memo.gen = gen; f->memo.ti = i; f->memo.xt = *xt; f->memo.valid = 1; }
  return i;
}
int gl_tex_cb(void *ctx, const uint64_t *gr, int ctxt, XcTex *t)   /* task #149: was static xtex_cb (gelive.h) */
{
  GlFrame *f = ctx; return gl_tex(f, gr, ctxt, (uint32_t)(gr[0x4c + ctxt] & 0x1ff) * 32, t, 0) >= 0;
}

int gl_init(GlFrame *f, TkCache *tk, GuMap *map, double zk, int max_tex)
{
  memset(f, 0, sizeof *f);
  if (zk <= 0) { f->zraw = 1; zk = 1; }
  if (gb_init(&f->o, zk, max_tex)) return -1;
  f->o.zraw = f->zraw;
  f->o.live = 1; f->max_tex = max_tex; f->tk = tk; f->map = map;
  f->tdata = calloc((size_t)max_tex, sizeof *f->tdata); f->tpal = calloc((size_t)max_tex, sizeof *f->tpal);
  f->tkey = calloc((size_t)max_tex, sizeof *f->tkey); f->tsize = calloc((size_t)max_tex, sizeof *f->tsize);
  f->held = aligned_alloc(64, GL_HOLD * sizeof *f->held); f->hold_max = GL_HOLD;   /* sb4: step c's records (64-byte lines: XcHeld.img) */
  if (f->held) memset(f->held, 0, GL_HOLD * sizeof *f->held);
  if (!f->tdata || !f->tpal || !f->tkey || !f->tsize || !f->held) return -1;
  f->h.tex = gl_tex_cb; f->h.ctx = f; f->h.zk = f->o.zk; f->h.zkf = f->o.zkf; f->h.noidx = 1; f->h.zr = &f->zr;
  f->h.cut = 1;   /* df0 calls wholly off the scissor are left out (ge/xconv.h XcHost.cut); the host may turn it off */
  f->h.clipfast = 1;   /* sb3: the effects' clip positions without needless clamps (XcHost.clipfast); the host may turn it off */
  if (f->zraw) { f->h.zlo = 1; f->h.zhi = 0; } else { f->h.zlo = 0; f->h.zhi = (float)(65535.0 / zk); }   /* raw: no window before a
                                                 frame has a range (every crossing effect triangle as the PS2's polygon) */
  f->map_lo = 1; f->map_hi = 0;
  f->memo_on = 1;
  gl_begin(f);
  return 0;
}
void gl_begin(GlFrame *f)
{
  gb_reset(&f->o); memset(f->hash, 0xff, sizeof f->hash); memset(&f->n, 0, sizeof f->n); f->memo.valid = 0; f->lit_ok = 0;
  f->o.dropped = 0; xc_zr_begin(&f->zr); f->tlo = 3.0e38f; f->thi = -1; f->tz0 = f->tztop = 0;
  f->nheld = 0; f->held_cur = -1; f->held_gone = 0; f->pend = 0; xc_hold7_reset();   /* sb4 (sb10 T2: and the P07 calls' copies) */
  f->ft_done = 0; f->h.nthin = 0;   /* task #114 */
  f->ft_n = 0; f->ft_nd = 0;       /* task #149, #180 */
}
void gl_thin(GlFrame *f, const XcThin *r, int n, int keep3, const void *d, int nd)   /* task #149, #180 */
{
  _Static_assert(FX_RANGES == 16 && FX_DROPS == 512, "GlFrame.ft_r, ft_d");
  if (n > 16) n = 16;
  for (int i = 0; i < n; i++) f->ft_r[i] = r[i];
  f->ft_n = n < 0 ? 0 : n;
  if (nd > 512) nd = 512;
  if (nd > 0) memcpy(f->ft_d, d, (size_t)nd * sizeof f->ft_d[0]);
  f->ft_nd = nd < 0 ? 0 : nd; f->ft_keep3 = keep3;
}
#ifdef MGS2_ME_SAFE
static int gl_drop_in(const GlFrame *f, uint32_t src)   /* task #180: fx_drop_has on the frame's blocks */
{
  int lo = 0, hi = f->ft_nd - 1;
  while (lo <= hi) { const int m = (lo + hi) >> 1;
    if (src < f->ft_d[m].lo) hi = m - 1; else if (src >= f->ft_d[m].hi) lo = m + 1; else return 1; }
  return 0;
}
#endif
/* zchk: what the frame's map leaves outside the GE's range (x86 checks; float, as the GE) */
static float zc_depth(const GerXform *x, const float *p, float *w)   /* GE depth before the map (Z24 x zk) of a position, its w */
{
  float e[3];
  for (int j = 0; j < 3; j++) e[j] = p[0] * x->world[j] + p[1] * x->world[3 + j] + p[2] * x->world[6 + j] + x->world[9 + j];
  *w = e[2];
  return e[2] > 0 ? x->vs[2] * (x->proj[10] * e[2] + x->proj[14]) / e[2] + x->vo[2] : 0;
}
static void gl_zchk(GlFrame *f)
{
  float zs, zo; gx_depth_map(f->map_lo, f->map_hi, f->o.zkf, &zs, &zo);   /* (task #149: (float)zk, made at init) */
  for (int i = 0; i < f->o.nb; i++) {
    const GerBatch *b = &f->o.b[i]; const GerXform *x = (const GerXform *)(f->o.d + b->first);
    if (b->kind == GERK_XFORM) {
      const VcVert *v = (const VcVert *)(x + 1); const uint16_t *runs = (const uint16_t *)(v + x->nv);
      int wr = (b->flags & GERF_ZWRITE) && b->ztst >= 2;
      for (uint32_t s = 0; s < x->nst; s++)
        for (uint32_t k = runs[2 * s] + 2u; k < (uint32_t)runs[2 * s] + runs[2 * s + 1]; k++) {
          int out = 0;
          for (uint32_t j = k - 2; j <= k; j++) {
            float p[3] = { v[j].x / 32768.0f, v[j].y / 32768.0f, v[j].z / 32768.0f }, w, g = zc_depth(x, p, &w) * zs + zo;
            if (w > 0 && (g < 0 || g > 65535)) out = 1;
          }
          if (out) { if (wr) f->n.out_w++; else f->n.out_o++; }
        }
    } else if (b->kind == GERK_LIT) {   /* each call once: its pass 1 (the passes draw the same positions) */
      const GerLit *p = (const GerLit *)(f->o.d + b->first);
      if ((p->pass & 15) != 1) continue;
      const GerXform *lx = (const GerXform *)((const uint8_t *)p + p->xf);
      if (p->pass & GERL_C1) {   /* sb11 C1: the positions skinned by the call's bones, as the GE does (a float sum: the check's) */
        const GerC1 *g = (const GerC1 *)((const uint8_t *)p + p->vtx); const uint8_t *v = (const uint8_t *)g + g->voff[0];
        const uint16_t *runs = (const uint16_t *)((const uint8_t *)p + p->run); const int nw = (int)g->nw, w2 = (nw + 1) & ~1;
        int wr = (b->flags & GERF_ZWRITE) && b->ztst >= 2;
        for (uint32_t s = 0; s < p->nst; s++)
          for (uint32_t k = runs[2 * s] + 2u; k < (uint32_t)runs[2 * s] + runs[2 * s + 1]; k++) {
            int out = 0;
            for (uint32_t j = k - 2; j <= k; j++) {
              const uint8_t *q = v + j * p->stride; int16_t h[3]; memcpy(h, q + w2 + 10, 6);
              float pp[3] = { 0, 0, 0 }, w;
              for (int e = 0; e < nw; e++) {
                if (!q[e]) continue;
                const float a = q[e] / 128.0f, *m = g->bone[e];
                for (int r = 0; r < 3; r++) pp[r] += a * (m[r] * (h[0] / 32768.0f) + m[3 + r] * (h[1] / 32768.0f) + m[6 + r] * (h[2] / 32768.0f) + m[9 + r]);
              }
              float gz = zc_depth(lx, pp, &w) * zs + zo;
              if (w > 0 && (gz < 0 || gz > 65535)) out = 1;
            }
            if (out) { if (wr) f->n.out_w++; else f->n.out_o++; }
          }
        continue;
      }
      if (p->pass & GERL_C2) {   /* sb12 C2: 16-bit positions; skinned, blended by the two 16-bit weights over bone 0 (32768 I) and the
                                    call's bone 1, as the GE does (a float sum: the check's) */
        static const float B0[12] = { 32768.0f, 0, 0, 0, 32768.0f, 0, 0, 0, 32768.0f, 0, 0, 0 };
        const GerC2 *g = (const GerC2 *)((const uint8_t *)p + p->vtx); const uint8_t *v = (const uint8_t *)g + g->voff[0];
        const uint16_t *runs = (const uint16_t *)((const uint8_t *)p + p->run); const int sk = g->skin != 0;
        const float *bm[2] = { B0, g->bone1 };
        int wr = (b->flags & GERF_ZWRITE) && b->ztst >= 2;
        for (uint32_t s = 0; s < p->nst; s++)
          for (uint32_t k = runs[2 * s] + 2u; k < (uint32_t)runs[2 * s] + runs[2 * s + 1]; k++) {
            int out = 0;
            for (uint32_t j = k - 2; j <= k; j++) {
              const uint8_t *q = v + j * p->stride; int16_t h[3]; memcpy(h, q + (sk ? 18 : 14), 6);
              float pp[3] = { h[0] / 32768.0f, h[1] / 32768.0f, h[2] / 32768.0f }, w;
              if (sk) {
                uint16_t wt[2]; memcpy(wt, q, 4); const float hp[3] = { pp[0], pp[1], pp[2] };
                pp[0] = pp[1] = pp[2] = 0;
                for (int e = 0; e < 2; e++) {
                  const float a = wt[e] / 32768.0f, *m = bm[e];
                  for (int r = 0; r < 3; r++) pp[r] += a * (m[r] * hp[0] + m[3 + r] * hp[1] + m[6 + r] * hp[2] + m[9 + r]);
                }
              }
              float gz = zc_depth(lx, pp, &w) * zs + zo;
              if (w > 0 && (gz < 0 || gz > 65535)) out = 1;
            }
            if (out) { if (wr) f->n.out_w++; else f->n.out_o++; }
          }
        continue;
      }
      const uint8_t *v = (const uint8_t *)p + p->vtx; const uint16_t *runs = (const uint16_t *)((const uint8_t *)p + p->run);
      int wr = (b->flags & GERF_ZWRITE) && b->ztst >= 2, fl = (p->vtype & 0x180) == 0x180, at = (p->vtype & 3) == 3 ? 12 : 8;
      for (uint32_t s = 0; s < p->nst; s++)
        for (uint32_t k = runs[2 * s] + 2u; k < (uint32_t)runs[2 * s] + runs[2 * s + 1]; k++) {
          int out = 0;
          for (uint32_t j = k - 2; j <= k; j++) {
            const uint8_t *q = v + j * p->stride; float pp[3], w;
            if (fl) memcpy(pp, q + at, 12); else { int16_t h[3]; memcpy(h, q + at, 6); for (int i = 0; i < 3; i++) pp[i] = h[i] / 32768.0f; }
            float g = zc_depth(lx, pp, &w) * zs + zo;
            if (w > 0 && (g < 0 || g > 65535)) out = 1;
          }
          if (out) { if (wr) f->n.out_w++; else f->n.out_o++; }
        }
    } else if (b->kind == GERK_XFORMF && b->prim != 6 && b->prim != 2) {   /* (sb15: the sprites' and streaks' batches carry no classes) */
      const VcVertF *v = (const VcVertF *)(x + 1); const uint8_t *cls = (const uint8_t *)(v + x->nv + x->nfv);
      for (uint32_t k = 2; k < x->nv; k++) {
        if (cls[k] != 3) continue;
        int out = 0;
        for (uint32_t j = k - 2; j <= k; j++) { float p[3] = { v[j].x, v[j].y, v[j].z }, w, g = zc_depth(x, p, &w) * zs + zo; if (g < 0 || g > 65535) out = 1; }
        f->n.bent += out;
      }
    }
  }
}
void gl_end(GlFrame *f)
{
  GlCount *n = &f->n; long *a = (long *)&f->all, *m = (long *)&f->max, *c = (long *)n;
  if (f->nheld) { f->hflush[3]++; gl_flush(f); }   /* sb4: (each transfer's end runs them: none expected here) */
  /* the frame's depth range: the df0 calls' and the GS draws'; the map keeps the last frame's when it has none */
  xc_zr_end(&f->zr);
  float lo = f->zr.lo, hi = f->zr.hi;
  if (f->tlo <= f->thi) { if (lo > hi) { lo = f->tlo; hi = f->thi; } else { if (f->tlo < lo) lo = f->tlo; if (f->thi > hi) hi = f->thi; } }
  f->zlo = lo; f->zhi = hi;
  if (lo <= hi) { f->map_lo = lo; f->map_hi = hi; }
  n->zr_calls = f->zr.calls; n->zr_always = f->zr.always;
  if (f->zchk && f->zraw) gl_zchk(f);
  if (f->zraw && lo <= hi) { f->h.zlo = lo; f->h.zhi = hi; }   /* the next frame's class 3 window: this frame's range, unmargined */
  n->frames = 1; n->batches = f->o.nb; n->verts = f->o.nv; n->dbytes = f->o.nd; n->ntex = f->o.nt; n->dropped = f->o.dropped;
  for (int i = 0; i < f->o.nb; i++) n->xbatches += f->o.b[i].kind != GERK_THROUGH;
  for (size_t i = 0; i < sizeof *n / sizeof (long); i++) { a[i] += c[i]; if (c[i] > m[i]) m[i] = c[i]; }
  f->all_dbytes += n->dbytes; f->all_tbytes += n->tbytes;
}

/* one axis of a DIRECT primitive's texels, a .. c (vertex texel coordinates), with the GS's wrap mode wm (MIN, MAX for the region modes)
   over size: the texels it can read, one more each side for bilinear taps, as a region lo .. hi that holds every texel the GS gives it
   there: CLAMP and REGION_CLAMP clamp into the region; REPEAT only when no tap wraps (a >= 0.5 and c < size - 0.5: a bilinear tap is
   floor(u - 0.5) and the next, a nearest one floor(u)).  The region is then taken out to whole GS blocks of g texels, inside MIN .. MAX
   (or the texture), when that stays within the GE's 512: every tap is still inside it and clamps where the GS's does, and a draw whose
   box moves a little from frame to frame (the codec's overlays read 115-141 x 176-198 texels of one unchanging image) gets a few regions
   the texture cache keeps instead of a new one each frame.  0, or -1 when the axis stays as it is */
static int prim_axis(int wm, int mn, int mx, int size, float a, float c, int g, int *lo, int *hi)
{
  int l = (int)floorf(a) - 1, h = (int)ceilf(c) + 1;
  if (wm == 0) { if (!(a >= 0.5f && c < (float)size - 0.5f)) return -1; mn = 0; mx = size - 1; }
  else if (wm == 1) { mn = 0; mx = size - 1; }
  else if (wm != 2 || mn > mx) return -1;
  *lo = l < mn ? mn : l > mx ? mx : l; *hi = h < mn ? mn : h > mx ? mx : h;
  if (*hi - *lo + 1 > 512) return -1;
  int bl = *lo / g * g, bh = *hi / g * g + g - 1;
  if (bl < mn) bl = mn;
  if (bh > mx) bh = mx;
  if (bh - bl + 1 <= 512) { *lo = bl; *hi = bh; }
  return 0;
}
/* a DIRECT primitive's texture as the region it reads (task #83): a 16-bit texture (the codec's overlays, 512 x 512 repeated, read
   127 x 180 of it) or one over the GE's 512 on an axis (its glow, 1024 x 1024 clamped, read 35 x 35) becomes a REGION_CLAMP CLAMP over
   the texels it can read, on each axis where prim_axis allows it.  1 with *cl, or 0: the texture as it is */
static int prim_clamp(const GfDraw *d, const GfVtx *const *v, int n, uint64_t cl0, uint64_t *cl)
{
  int tw = 1 << d->tw, th = 1 << d->th, lo[2], hi[2];
  if (!(d->tpsm == GS_CT16 || d->tpsm == GS_CT16S || tw > 512 || th > 512) || d->mxl) return 0;
  float a = 1e30f, b = 1e30f, c = -1e30f, e = -1e30f;
  for (int i = 0; i < n; i++) {
    float s, t;
    if (d->fst) { s = v[i]->u / 16.0f; t = v[i]->v / 16.0f; }
    else { if (!(v[i]->q > 0)) return 0; s = v[i]->s / v[i]->q * (float)tw; t = v[i]->t / v[i]->q * (float)th; }
    if (!(s > -4096 && s < 4096 && t > -4096 && t < 4096)) return 0;
    if (s < a) a = s;
    if (s > c) c = s;
    if (t < b) b = t;
    if (t > e) e = t;
  }
  /* a GS block's texels: 16 x 8 in CT16 / CT16S, 16 x 16 in T8, 32 x 16 in T4, 8 x 8 in the CT32 layout (CT32, CT24, T8H, T4HL, T4HH) */
  int p = d->tpsm, gu = p == GS_CT16 || p == GS_CT16S || p == GS_T8 ? 16 : p == GS_T4 ? 32 : 8, gv = p == GS_T8 || p == GS_T4 ? 16 : 8;
  int ru = !prim_axis(d->wms, d->minu, d->maxu, tw, a, c, gu, &lo[0], &hi[0]), rv = !prim_axis(d->wmt, d->minv, d->maxv, th, b, e, gv, &lo[1], &hi[1]);
  if (!ru && !rv) return 0;
  *cl = cl0;
  if (ru) *cl = (*cl & ~(3ull | 0xfffffull << 4)) | 2 | (uint64_t)lo[0] << 4 | (uint64_t)hi[0] << 14;
  if (rv) *cl = (*cl & ~(3ull << 2 | 0xfffffull << 24)) | 2 << 2 | (uint64_t)lo[1] << 24 | (uint64_t)hi[1] << 34;
  return 1;
}
void gl_prim(GlFrame *f, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n)
{
  if (g->path != 2 && g->path != 3) { f->n.path1++; return; }
  if (g->unk_used) { f->n.uncertain++; return; }
  int ti = -1;
  if (d->tme) { XcTex xt; uint64_t cl; ti = gl_tex(f, g->gr, d->ctxt, d->fbp, &xt, tk_t16_on(f->tk) && prim_clamp(d, v, n, g->gr[0x08 + d->ctxt], &cl) ? &cl : 0); f->n.prims_tex++; }
  GerBatch b; int u = gb_state(d, ti, 0, &b);
  for (int k = 0; k < 4; k++) if (u >> k & 1) f->n.unrep[k]++;
  if (gb_prim(&f->o, &b, d, v, n)) return;
  f->n.prims++;
  if ((b.flags & GERF_ZWRITE) && b.ztst >= 2) {   /* a depth-tested depth writer (GERF_ZWRITE has the test on): the frame's range */
    uint32_t top = d->zpsm == 0x31 ? 0xffffffu : (d->zpsm & 2) ? 0xffffu : 0xffffffffu;
    for (int i = 0; i < n; i++) {
      uint32_t z = v[i]->z < top ? v[i]->z : top;
      if (!z) f->tz0++; else if (z == top) f->tztop++; else { float q = (float)z; if (q < f->tlo) f->tlo = q; if (q > f->thi) f->thi = q; }
    }
  }
  if ((d->zpsm == 0x32 || d->zpsm == 0x3a) && (b.flags & GERF_ZTEST)) f->n.z16++;
}
int gl_need, gl_need_chk; long gl_need_n, gl_need_diff;   /* sb1 (gelive.h) */
/* task #158 (Ffm 2026-11-03 13:27, "remove the rain"): MGS2_GL_STREAKS=0 leaves a streak call whose data lies in one of the rain's ranges
   out whole, as C4 off leaves out every streak call (before its decode); the other streak calls (spark.c, usp.c, the opening's without a
   block) are drawn.  MGS2_GL_STREAK_CALLS=N (fxthin.h) leaves out the calls whose block's place in the range is not a multiple of N */
static int gl_line_none(const XcHost *h, uint32_t src)
{
  for (int k = 0; k < h->nthin; k++) { const XcThin *r = &h->thin[k];
    if (r->line && src - r->lo < r->hi - r->lo && (!r->keep || (r->kcall > 1 && (src - r->lo) / FX_STREAK_BLOCK % (uint32_t)r->kcall))) return 1; }
  return 0;
}
void gl_call(GlFrame *f, const XlCall *c)
{
  f->lit_ok = 0; f->held_cur = -1; f->held_gone = 0;
  if ((!gl_conv_entry(c->entry) && !(f->h.c4 && gl_c4_entry(c->entry))) || c->cont) { f->n.other++; return; }   /* (sb15: step 4's) */
  f->n.calls++;
#ifndef __psp__
  fx_who_note(f->ram, c->entry, c->src);   /* sb15 x86 census (MGS2_FX_WHO=1) */
#endif
#ifdef MGS2_ME_SAFE
  /* task #149 / #180: the ranges and the rain's sprite blocks the host's scan found (gl_thin), taken where the scan ran without
     -DMGS2_ME_SAFE; the calls left out counted on the frame */
  if (f->ft_on && !f->ft_done && (c->entry == XL_FX || c->entry == XL_FX2 || c->entry == VC_LINE || (c->entry == VC_SPR && !f->ft_keep3))) {
    f->ft_done = 1; f->h.nthin = f->ft_n; f->h.thin = f->ft_r; }
  if (c->entry == VC_LINE && f->h.nthin && gl_line_none(&f->h, c->src)) { f->n.calls--; f->n.other++; f->line_none++; return; }   /* task #158 */
  if (c->entry == VC_SPR && f->ft_on && f->ft_done && f->ft_nd && gl_drop_in(f, c->src)) { f->n.calls--; f->n.other++; f->spr_none++; return; }   /* task #161 */
#else
  if (f->ft && !f->ft_done && (c->entry == XL_FX || c->entry == XL_FX2 || c->entry == VC_LINE || (c->entry == VC_SPR && !f->ft->keep[3]))) {   /* task #114: the frame's thinned
                                                                     effects, as they are now (task #154: the streaks too, C4 on) */
    f->ft_done = 1; f->h.nthin = fx_thin_scan(f->ft, f->ram, f->h.c4); f->h.thin = f->ft->r; }
  if (c->entry == VC_LINE && f->h.nthin && gl_line_none(&f->h, c->src)) { f->n.calls--; f->n.other++; f->ft->line_none++; return; }   /* task #158 */
  if (c->entry == VC_SPR && f->ft && f->ft->nd && fx_drop_has(f->ft, c->src)) { f->n.calls--; f->n.other++; f->ft->spr_none++; return; }   /* task #161 */
#endif
  /* sb4, step c (renderer_notes 11.29): a call xc_hold_test takes is held when the frame has a record, its batch slots and data room
     beyond what the held calls may take (pend); the rest run here, after the held calls when the room they need is not certain */
  uint32_t bound; int npd;
  if (c->rec && (c->rec->lane == 4 ? f->stepc7 : f->stepc) && xc_hold_test(c, &bound, &npd)) {   /* (sb10 T2: lane 4 by its own switch) */
    if (f->h.c1 && c->rec->lane == 4) bound = xc_c1_hold_bound(c, bound);   /* sb11 C1: the room a C1 call may take */
    if (f->h.c2 && c->rec->lane == 2) bound = xc_c2_hold_bound(c, bound);   /* sb12 C2: the room a C2 call may take */
    if (f->nheld >= f->hold_max) { f->hflush[2]++; gl_flush(f); }
    if (c->rec->lane == 4 && !xc_hold7_room()) { f->hflush_log++; gl_flush(f); }   /* sb10 T2: the room for the call's copies */
    uint32_t left = gb_left(&f->o), need = (bound + 63) & ~63u;
    if (left >= f->pend && left - f->pend >= need && (!f->o.fixed || f->o.nb + 1 + npd <= f->o.cap_b)) {
      XcHeld *r = &f->held[f->nheld];
      xc_hold(c, &f->h, r);
      r->slot = gb_slot(&f->o);
      f->pend += need; f->held_cur = f->nheld++; f->held_n++; f->held7_n += c->rec->lane == 4;
      f->n.conv++; f->n.lit_calls++;   /* (xc_hold_test: the call converts and has a strip) */
      return;
    }
  }
  if (f->nheld) { uint32_t l0 = gb_left(&f->o); if (l0 < f->pend || l0 - f->pend < GL_XCAP) { f->hflush[1]++; gl_flush(f); } }
  uint32_t left = gb_left(&f->o), cap = left < GL_XCAP ? left : GL_XCAP;
  /* the converter reads the call's input in the VU memory model: every quadword decoded (sb1: a lane's P04+df0 call reads its streams
     from the game's data, the rest the translator decoded; gl_need: only the quadwords the converter reads, xl_vm_need) */
#ifndef __psp__
  if (gl_need && gl_need_chk) {   /* sb1 check: the call converted from the memory xl_vm_need leaves, then from every quadword decoded */
    static uint8_t va[16384] __attribute__((aligned(64))), d1[GL_XCAP] __attribute__((aligned(64))), d2[GL_XCAP] __attribute__((aligned(64)));
    xl_vm_need(c); memcpy(va, c->vm, sizeof va);
    xl_vm_sync();
    XlCall c2 = *c; c2.vm = va; GerBatch b1, b2; uint32_t s1 = 0, s2 = 0; memset(&b1, 0, sizeof b1); memset(&b2, 0, sizeof b2);
    int r1 = xc_call(&c2, &f->h, &b1, d1, cap, &s1), r2 = xc_call(c, &f->h, &b2, d2, cap, &s2);
    gl_need_n++;
    if (r1 != r2 || s1 != s2 || memcmp(&b1, &b2, sizeof b1) || (r1 > 0 && memcmp(d1, d2, s1))) gl_need_diff++;
  } else
#endif
  if (gl_need) xl_vm_need(c);
  else if (!c->rec) xl_vm_sync();
  uint8_t *blob = gb_room(&f->o, cap);
  if (!blob) { f->n.conv_full++; return; }
  GerBatch b; uint32_t size = 0;
  int r = xc_call(c, &f->h, &b, blob, cap, &size);
  if (r == -5) { f->n.conv_full++; return; }
  if (r < 0) { f->n.conv_bad++; return; }
  if (!r) { f->n.conv_empty++; return; }
  if (gb_xform_in(&f->o, &b, size)) { f->n.conv_full++; return; }
  f->n.conv++;
  if (b.kind == GERK_LIT) { f->lit_ok = 1; f->lit_base = f->o.room; f->n.lit_calls++; }
}
void gl_pass(GlFrame *f, const XlCall *c, int pass)
{
  if (f->held_gone) { f->held_late++; return; }   /* sb4: a pass of a held call that has run (none expected) */
  if (f->held_cur >= 0) {   /* sb4: a held call's pass: its look-up now, its batch slot where gb_xform_at would put it */
    XcHeld *r = &f->held[f->held_cur];
    if (!xc_hold_pass(c, pass, &f->h, r)) { f->n.lit_nopass++; return; }
    r->pslot[r->np - 1] = gb_slot(&f->o);
    f->n.lit_passes++;
    return;
  }
  if (!f->lit_ok) return;   /* the call's pass 1 is not in the frame */
  GerBatch b;
  if (!xc_pass(c, pass, &f->h, &b, f->o.d + f->lit_base)) { f->n.lit_nopass++; return; }
  if (gb_xform_at(&f->o, &b, f->lit_base)) { f->n.conv_full++; return; }
  f->n.lit_passes++;
}

void gl_flush(GlFrame *f)
{
  if (!f->nheld) return;
  xc_held_begin();
  for (int i = 0; i < f->nheld; i++) {
    const XcHeld *r = &f->held[i];
    uint32_t left = gb_left(&f->o), cap = left < GL_XCAP ? left : GL_XCAP, size = 0;
    uint8_t *blob = gb_room(&f->o, cap);
    GerBatch b;
    if (!blob || xc_held_run(r, &f->h, &b, blob, cap, &size) != 1 || gb_fill_in(&f->o, r->slot, &b, size)) { f->held_bad++; continue; }
    uint32_t base = f->o.room;
    for (int k = 0; k < r->np; k++) { GerBatch pb; xc_held_pass(r, k, &pb, f->o.d + base); if (gb_fill_at(&f->o, r->pslot[k], &pb, base)) f->held_bad++; }
  }
  xc_held_end();
  f->nheld = 0; f->pend = 0;
  if (f->held_cur >= 0) { f->held_cur = -1; f->held_gone = 1; }
}

int gl_write(const GlFrame *f, FILE *fp, uint32_t vb)
{
  const GbOut *o = &f->o; static const uint8_t zero[64];
  GerTex *t = malloc((size_t)(o->nt ? o->nt : 1) * sizeof *t); if (!t) return -1;
  uint32_t at = (o->nd + 63) & ~63u;   /* the textures' texels and palettes after the frame's own data */
  for (int i = 0; i < o->nt; i++) {
    t[i] = o->t[i];
    if (t[i].kind != 1) continue;
    t[i].data = at; at = (at + t[i].size + 63) & ~63u;
    t[i].pal = at; at = (at + t[i].npal * 4 + 63) & ~63u;
  }
  GerHeader hd = { GER_MAGIC, vb, (uint32_t)o->nb, (uint32_t)o->nv, (uint32_t)o->nt, at, 0, 0, 0, 0, 480, 272, (float)o->zk,
                   f->zraw ? f->map_lo : 0, f->zraw ? f->map_hi : 0 };
  hd.off_batch = 64; hd.off_vert = (hd.off_batch + o->nb * (uint32_t)sizeof(GerBatch) + 63) & ~63u;
  hd.off_tex = (hd.off_vert + o->nv * (uint32_t)sizeof(GerVert) + 63) & ~63u;
  hd.off_data = (hd.off_tex + o->nt * (uint32_t)sizeof(GerTex) + 63) & ~63u;
  fwrite(&hd, sizeof hd, 1, fp); fwrite(zero, 64 - sizeof hd, 1, fp);
  fwrite(o->b, sizeof(GerBatch), (size_t)o->nb, fp); fwrite(zero, hd.off_vert - hd.off_batch - o->nb * sizeof(GerBatch), 1, fp);
  fwrite(o->v, sizeof(GerVert), (size_t)o->nv, fp); fwrite(zero, hd.off_tex - hd.off_vert - o->nv * sizeof(GerVert), 1, fp);
  fwrite(t, sizeof(GerTex), (size_t)o->nt, fp); fwrite(zero, hd.off_data - hd.off_tex - o->nt * sizeof(GerTex), 1, fp);
  fwrite(o->d, 1, o->nd, fp);
  uint32_t pos = o->nd;
  for (int i = 0; i < o->nt; i++) {
    if (t[i].kind != 1) continue;
    fwrite(zero, t[i].data - pos, 1, fp); fwrite(f->tdata[i], 1, t[i].size, fp); pos = t[i].data + t[i].size;
    fwrite(zero, t[i].pal - pos, 1, fp); if (t[i].npal) fwrite(f->tpal[i], 4, t[i].npal, fp); pos = t[i].pal + t[i].npal * 4;
  }
  fwrite(zero, at - pos, 1, fp);
  free(t);
  return ferror(fp) ? -1 : 0;
}

static uint64_t hw(uint64_t h, const void *p, uint32_t n)   /* n bytes as 32-bit words, the last ones a byte at a time */
{
  const uint8_t *b = p; uint32_t i = 0, w;
  for (; i + 4 <= n; i += 4) { memcpy(&w, b + i, 4); h ^= w; h *= 0x100000001b3ull; }
  for (; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; }
  return h;
}
#ifndef __psp__
uint64_t gl_digest_c(const GlFrame *f)
{
  const GbOut *o = &f->o; uint64_t h = 0xcbf29ce484222325ull;
  uint32_t n[3] = { (uint32_t)o->nb, (uint32_t)o->nv, (uint32_t)o->nt };
  h = hw(h, n, sizeof n);
  for (int i = 0; i < o->nb; i++) {
    GerBatch b = o->b[i];
    if (b.kind == GERK_THROUGH) { h = hw(h, &b, sizeof b); continue; }   /* (first: its vertices' index, not the data's offset) */
    const uint8_t *p = o->d + b.first; b.first = 0; h = hw(h, &b, sizeof b);
    if (b.kind == GERK_LIT) {
      const GerLit *l = (const GerLit *)p;
      h = hw(h, l, sizeof *l); h = hw(h, p + l->xf, sizeof(GerXform));
      if (l->pass & GERL_C1) {   /* sb11 C1: its GerC1, then the pass's vertices */
        const GerC1 *g = (const GerC1 *)(p + l->vtx);
        h = hw(h, g, GERC1_SIZE(g->nw)); h = hw(h, (const uint8_t *)g + g->voff[(l->pass & 15) - 1], l->nv * l->stride);
      } else if (l->pass & GERL_C2) {   /* sb12 C2: its GerC2, then the pass's vertices */
        const GerC2 *g = (const GerC2 *)(p + l->vtx);
        h = hw(h, g, GERC2_SIZE(g->skin)); h = hw(h, (const uint8_t *)g + g->voff[(l->pass & 15) - 1], l->nv * l->stride);
      } else h = hw(h, p + l->vtx, l->nv * l->stride);
      h = hw(h, p + l->run, l->nst * 4);
    } else {
      const GerXform *x = (const GerXform *)p;
      uint32_t sz = b.kind == GERK_XFORM ? (uint32_t)sizeof *x + x->nv * (uint32_t)sizeof(VcVert) + x->nst * 4 + x->nidx * 2 :
                    (uint32_t)sizeof *x + (x->nv + x->nfv) * (uint32_t)sizeof(VcVertF) + ((x->nv + 3) & ~3u) + x->nfan * 4;
      h = hw(h, p, sz);
    }
  }
  h = hw(h, o->v, (uint32_t)o->nv * (uint32_t)sizeof *o->v);
  for (int i = 0; i < o->nt; i++) {
    h = hw(h, &o->t[i], sizeof o->t[i]);
    if (o->t[i].kind != 1) continue;
    if (f->tdata[i]) h = hw(h, f->tdata[i], f->tsize[i]);
    if (f->tpal[i]) h = hw(h, f->tpal[i], o->t[i].npal * 4);
  }
  float z[4] = { f->zlo, f->zhi, f->map_lo, f->map_hi }; h = hw(h, z, sizeof z);
  return h;
}
#endif
uint64_t gl_digest(const GlFrame *f, const GxList *l)
{
  const GbOut *o = &f->o; uint64_t h = 0xcbf29ce484222325ull;
  uint32_t n[5] = { (uint32_t)o->nb, (uint32_t)o->nv, o->nd, l->naux, (uint32_t)o->nt };
  h = hw(h, &l->dig, 8); h = hw(h, n, sizeof n);
  h = hw(h, o->b, n[0] * (uint32_t)sizeof *o->b); h = hw(h, o->v, n[1] * (uint32_t)sizeof *o->v); h = hw(h, o->d, o->nd);
  h = hw(h, l->aux + l->alow, l->naux);   /* (aux is taken from the top down: gelist.h) */
  for (int i = 0; i < o->nt; i++) {
    h = hw(h, &o->t[i], sizeof o->t[i]);
    if (o->t[i].kind != 1) continue;
    if (f->tdata[i]) h = hw(h, f->tdata[i], f->tsize[i]);
    if (f->tpal[i]) h = hw(h, f->tpal[i], o->t[i].npal * 4);
  }
  return h;
}
