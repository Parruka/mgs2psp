/* gelist.c: the GE list of one live frame (gelist.h; renderer_notes 11.13).  The rules are psp/ge_replay/main.c's reference mode,
   function by function: batch() -> gx_batch, xf_setup / xf_draw -> xf_setup / xf_draw, fx_index / fx_draw -> fx_draw, draw_cut ->
   cut_draw, blend() -> blend.  GE command numbers and argument layouts as pspsdk's libgu (BSD licence: guInternal.h, sceGuTexMode.c,
   sceGuTexImage.c, sceGuClutLoad.c, sceGuDrawArray.c, sceGuSetMatrix.c, sceGuClear.c, sceGuDepthRange.c, ...).  Two things sceGu does
   not do: a df0 batch sets its vertex address once and draws its strips with one PRIM each, since the GE moves the vertex address past
   the vertices a PRIM read (PPSSPP GPUCommonHW.cpp Execute_Prim: "Some games rely on this"); and a texture is flushed once per change.
   Game data passes through it: personal use only. */
#include <math.h>
#include <string.h>
#include "gelist.h"
#include "vu1conv.h"
#include "me_rt.h"   /* task #149: -DMGS2_ME_SAFE (ME-safe calls) */

enum {   /* GE commands (pspsdk guInternal.h) */
  C_VADDR = 0x01, C_IADDR = 0x02, C_PRIM = 0x04, C_RET = 0x0b, C_END = 0x0c, C_FINISH = 0x0f, C_BASE = 0x10, C_VTYPE = 0x12, C_REGION1 = 0x15,
  C_REGION2 = 0x16, C_LIGHTING = 0x17, C_CLIP = 0x1c, C_CULL = 0x1d, C_TEXTURE = 0x1e, C_FOG = 0x1f, C_BLEND = 0x21, C_ATEST = 0x22,
  C_ZTEST = 0x23, C_WORLDN = 0x3a, C_WORLDD = 0x3b, C_VIEWN = 0x3c, C_VIEWD = 0x3d, C_PROJN = 0x3e, C_PROJD = 0x3f, C_VPXS = 0x42,
  C_TSU = 0x48, C_TSV = 0x49, C_TOU = 0x4a, C_TOV = 0x4b, C_OFFX = 0x4c, C_OFFY = 0x4d, C_SHADE = 0x50, C_FBP = 0x9c, C_FBW = 0x9d,
  C_ZBP = 0x9e, C_ZBW = 0x9f, C_TBP0 = 0xa0, C_TBW0 = 0xa8, C_CBP = 0xb0, C_CBW = 0xb1, C_TSIZE0 = 0xb8, C_TMODE = 0xc2, C_TPSM = 0xc3,
  C_CLOAD = 0xc4, C_CMODE = 0xc5, C_TFILTER = 0xc6, C_TWRAP = 0xc7, C_TFUNC = 0xc9, C_TFLUSH = 0xcb, C_FOG1 = 0xcd, C_FOG2 = 0xce,
  C_FOGC = 0xcf, C_PSM = 0xd2, C_CLEAR = 0xd3, C_SCISSOR1 = 0xd4, C_SCISSOR2 = 0xd5, C_MINZ = 0xd6, C_MAXZ = 0xd7, C_AFUNC = 0xdb,
  C_ZFUNC = 0xde, C_BMODE = 0xdf, C_FIXA = 0xe0, C_FIXB = 0xe1, C_ZMASK = 0xe7,
  /* sb11 C1 (c1_draw): the GE's skinning, lighting and texture matrix */
  C_LIGHTEN0 = 0x18, C_BOFS = 0x2a, C_BONED = 0x2b, C_TGENN = 0x40, C_TGEND = 0x41, C_RNORM = 0x51, C_MATUPD = 0x53, C_EMIS = 0x54,
  C_MAMB = 0x55, C_MDIF = 0x56, C_MSPEC = 0x57, C_MALPHA = 0x58, C_AMBC = 0x5c, C_AMBA = 0x5d, C_LMODE = 0x5e, C_LTYPE0 = 0x5f,
  C_LX0 = 0x63, C_LAC0 = 0x8f, C_LDC0 = 0x90, C_LSC0 = 0x91, C_TMAP = 0xc0
};
enum {   /* pspgu.h values */
  G_NEVER = 0, G_ALWAYS = 1, G_EQUAL = 2, G_NOTEQUAL = 3, G_LESS = 4, G_LEQUAL = 5, G_GREATER = 6, G_GEQUAL = 7,
  G_ADD = 0, G_REVSUB = 2, G_SRC_ALPHA = 2, G_ONE_MINUS_SRC_ALPHA = 3, G_DST_ALPHA = 4, G_ONE_MINUS_DST_ALPHA = 5, G_FIX = 10,
  G_LINES = 1, G_LINE_STRIP = 2, G_TRIANGLES = 3, G_TRIANGLE_STRIP = 4, G_SPRITES = 6, G_PSM_5551 = 1, G_PSM_8888 = 3, G_PSM_T4 = 4, G_PSM_T8 = 5
};
#define VT_THROUGH 0x80019fu   /* GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D (GerVert) */
#define VT_DF0 0x11eu          /* GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_3D (VcVert) */
#define VT_FX 0x19fu           /* GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D (VcVertF) */
#define VT_IDX16 0x1000u       /* GU_INDEX_16BIT */
#define VT_CLEAR 0x80011cu     /* GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D (sceGuClear's vertices) */
#define DF0_ENTRY 0x1991d8u    /* a replay file's PS2 primitives of the calls it also has as transform batches (the player's rule) */
#define FX_ENTRY 0x19a9f0u
#define RESERVE 16             /* words kept for the list's end */
#define BATCH_WORDS 96         /* the most words a batch's state and set-up take (a bound, checked before each batch) */

static const uint8_t ATMAP[8] = { G_NEVER, G_ALWAYS, G_LESS, G_LEQUAL, G_EQUAL, G_GEQUAL, G_GREATER, G_NOTEQUAL };   /* PS2 ATST -> GE */
static const uint8_t ATINV[8] = { G_ALWAYS, G_NEVER, G_GEQUAL, G_GREATER, G_NOTEQUAL, G_LESS, G_LEQUAL, G_EQUAL };   /* its negation */
static const uint8_t ZTMAP[4] = { G_NEVER, G_ALWAYS, G_GEQUAL, G_GREATER };                                         /* PS2 ZTST -> GE */

/* ---- words; the digest hashes an address word as what it names (region, offset) and leaves BASE out, so it does not depend on
   where the memory lies ---- */
enum { R_NONE, R_VERT, R_DATA, R_AUX, R_TEX, R_PAL, R_VRAM };
static GxList *L; static const GxHost *H; static GxFrame *F;
static void hash(uint32_t x) { for (int k = 0; k < 4; k++) { L->dig ^= x >> 8 * k & 0xff; L->dig *= 0x100000001b3ull; } }
static void put(uint32_t c, uint32_t a) { uint32_t w = c << 24 | (a & 0xffffff); L->w[L->n++] = w; if (L->dig_on) hash(w); }
static void putf(uint32_t c, float f) { uint32_t u; memcpy(&u, &f, 4); put(c, u >> 8); }
static void putr(uint32_t c, float f) { uint32_t u; memcpy(&u, &f, 4); put(c, (u + 0x80u) >> 8); }   /* sb11 C1: rounded to the nearest */
static void put_sym(uint32_t c, uint32_t a, int region, uint32_t off)
{
  L->w[L->n++] = c << 24 | (a & 0xffffff);
  if (L->dig_on) { hash(c << 24 | (uint32_t)region << 20); hash(off); }
}
static uint32_t room(void) { return (L->shared ? L->alow / 4 : L->cap) - L->n; }   /* (shared: aux_get keeps alow / 4 >= n) */
/* bytes of aux below what is taken, at a 16-byte boundary (the gap above them zeroed: the x86 / PSP check hashes aux), leaving room for
   more words of the list and its end when the two share a block; 0 when they do not fit */
static uint8_t *aux_get(uint32_t bytes, uint32_t more)
{
  if (bytes > L->alow) return 0;
  uint32_t at = (L->alow - bytes) & ~15u, floor = L->shared ? 4 * (L->n + more + RESERVE) : 0;
  if (at < floor) return 0;
  memset(L->aux + at + bytes, 0, L->alow - at - bytes); L->alow = at; L->naux = L->cap_aux - at;
  return L->aux + at;
}
/* what a pointer of a draw names: the through vertices, the list's aux or the data block */
static void sym(const void *p, int *region, uint32_t *off)
{
  const uint8_t *q = (const uint8_t *)p;
  if (F->v && q >= (const uint8_t *)F->v && q <= (const uint8_t *)(F->v + F->nv)) { *region = R_VERT; *off = (uint32_t)(q - (const uint8_t *)F->v); }
  else if (L->aux && q >= L->aux + L->alow && q <= L->aux + L->cap_aux) { *region = R_AUX; *off = (uint32_t)(q - L->aux); }
  else if (F->d && q >= F->d) { *region = R_DATA; *off = (uint32_t)(q - F->d); }
  else { *region = R_NONE; *off = 0; }
}
/* BASE (when its bits change: not hashed) and VADDR / IADDR of p (sceGuDrawArray's words) */
static void addr_cmd(uint32_t c, const void *p)
{
  uint32_t a = ME_CALLP(H->addr)(H->ctx, p), base = (a >> 8) & 0xf0000;
  if (L->s_base != base) { L->w[L->n++] = (uint32_t)C_BASE << 24 | base; L->s_base = base; }
  if (L->dig_on) { int r; uint32_t o; sym(p, &r, &o); put_sym(c, a, r, o); } else L->w[L->n++] = c << 24 | (a & 0xffffff);
  if (c == C_VADDR) L->c.vaddr++;
}
static void vtype(uint32_t t) { if ((uint32_t)L->s_vtype != t) { put(C_VTYPE, t); L->s_vtype = (int)t; } }
static void en(uint32_t c, int on, int *cache) { if (*cache != on) { put(c, (uint32_t)on); *cache = on; } }
static int get_exp(int v) { int i; for (i = 9; i > 0 && !((v >> i) & 1); --i) ; return i; }   /* libgu's getExp */

/* ---- state ---- */
static void blend(const GerBatch *b)
{
  int sig = b->ba | b->bb << 2 | b->bc << 4 | b->bd << 6 | b->fix << 8;
  if (L->s_bsig == sig) return;
  L->s_bsig = sig;
  unsigned f = b->fix >= 0x80 ? 0xff : b->fix * 2u, W = 0xffffff, op = G_ADD, src = G_SRC_ALPHA, dst = G_ONE_MINUS_SRC_ALPHA, fa = 0, fb = 0;
  switch (sig & 0xff) {   /* D << 6 | C << 4 | B << 2 | A; A, B, D: 0 Cs 1 Cd 2 zero; C: 0 As 1 Ad 2 FIX (the player's table) */
    case 0x44: break;                                                                   /* (Cs - Cd) As + Cd */
    case 0x48: dst = G_FIX; fb = W; break;                                              /* (Cs - 0) As + Cd */
    case 0x42: op = G_REVSUB; dst = G_FIX; fb = W; break;                               /* (0 - Cs) As + Cd */
    case 0x58: src = G_DST_ALPHA; dst = G_FIX; fb = W; break;                           /* (Cs - 0) Ad + Cd */
    case 0x54: src = G_DST_ALPHA; dst = G_ONE_MINUS_DST_ALPHA; break;                   /* (Cs - Cd) Ad + Cd */
    case 0x64: src = G_FIX; dst = G_FIX; fa = f | f << 8 | f << 16; fb = (255 - f) * 0x010101u; break;   /* (Cs - Cd) FIX + Cd */
    case 0x68: src = G_FIX; dst = G_FIX; fa = f | f << 8 | f << 16; fb = W; break;           /* (Cs - 0) FIX + Cd: the codec subtitle */
    case 0xa9: src = G_FIX; dst = G_FIX; fb = f | f << 8 | f << 16; break;                  /* (Cd - 0) FIX + 0: the codec scanlines */
    case 0x19: src = G_FIX; dst = G_FIX; fa = W; fb = W; break;   /* (Cd - 0) Ad + Cs: the codec glow, with Ad as 1.0 (exact where the PS2's Ad is
                                                                     0x80 or Cd is black: the glow only draws on the cleared background) */
    default: L->c.blend_other++; break;
  }
  put(C_BMODE, src | dst << 4 | op << 8); put(C_FIXA, fa); put(C_FIXB, fb);
}
static void texture(int ti)
{
  if (L->s_tex == ti) return;
  L->s_tex = ti; L->c.tex_set++;
  const GerTex *t = &F->t[ti];
  if (t->kind == 2) {   /* a frame buffer: the one being drawn, or the other (the previous frame) */
    uint32_t rel = H->fb[t->psm ? !L->s_draw : L->s_draw], a = 0x04000000u + rel;
    put(C_TMODE, 0); put(C_TPSM, G_PSM_8888);
    put_sym(C_TBP0, a, R_VRAM, rel); put_sym(C_TBW0, (a >> 8 & 0x0f0000) | 512, R_VRAM, 512);
    put(C_TSIZE0, 9 << 8 | 9);
  } else if (t->psm == 16) {   /* direct colour: the PS2's CT16 halves as the GE's 5551, no palette (task #83) */
    uint32_t a = ME_CALLP(H->addr)(H->ctx, F->tdata[ti]);
    put(C_TMODE, 1); put(C_TPSM, G_PSM_5551);
    put_sym(C_TBP0, a, R_TEX, (uint32_t)ti); put_sym(C_TBW0, (a >> 8 & 0x0f0000) | t->bw, R_TEX, t->bw);
    put(C_TSIZE0, (uint32_t)(get_exp(t->th) << 8 | get_exp(t->tw)));
  } else {
    uint32_t a = ME_CALLP(H->addr)(H->ctx, F->tdata[ti]), p = ME_CALLP(H->addr)(H->ctx, F->tpal[ti]);
    put(C_TMODE, 1); put(C_TPSM, t->psm == 8 ? G_PSM_T8 : G_PSM_T4);
    put_sym(C_TBP0, a, R_TEX, (uint32_t)ti); put_sym(C_TBW0, (a >> 8 & 0x0f0000) | t->bw, R_TEX, t->bw);
    put(C_TSIZE0, (uint32_t)(get_exp(t->th) << 8 | get_exp(t->tw)));
    put(C_CMODE, G_PSM_8888 | 0xff << 8);
    put_sym(C_CBP, p, R_PAL, (uint32_t)ti); put_sym(C_CBW, p >> 8 & 0xf0000, R_PAL, 0); put(C_CLOAD, t->npal / 8);
  }
  put(C_TFLUSH, 0);
  put(C_TWRAP, (t->repeat & 1 ? 0u : 1u) | (t->repeat & 2 ? 0u : 1u) << 8);   /* GU_REPEAT 0, GU_CLAMP 1 */
}
/* a transform batch's matrices, viewport, fog and (ts: 0 for none) texture scale and offset; the world matrix only when the last batch
   set another GerXform's (a P06 call's passes share theirs; a two-pass batch's second pass) */
static void xf_rest(const GerXform *x, int fog, const float *ts);
static void xf_setup(const GerXform *x, int fog, const float *ts)
{
  if (L->s_xf != x) { put(C_WORLDN, 0); for (int k = 0; k < 12; k++) putf(C_WORLDD, x->world[k]); L->s_xf = x; }
  xf_rest(x, fog, ts);
}
static void xf_rest(const GerXform *x, int fog, const float *ts)   /* (xf_setup's after the world matrix; sb11 C1's too) */
{
  if (!L->s_have_proj || memcmp(L->s_proj, x->proj, sizeof L->s_proj)) {
    put(C_PROJN, 0); for (int k = 0; k < 16; k++) putf(C_PROJD, x->proj[k]);
    memcpy(L->s_proj, x->proj, sizeof L->s_proj); L->s_have_proj = 1;
  }
  float vp[6] = { x->vs[0], x->vs[1], x->vs[2] * F->zs, x->vo[0], x->vo[1], x->vo[2] * F->zs + F->zo };   /* VIEWPORT X1 Y1 Z1 X2 Y2 Z2 */
  if (!L->s_have_vp || memcmp(L->s_vp, vp, sizeof vp)) { for (int k = 0; k < 6; k++) putf(C_VPXS + (uint32_t)k, vp[k]); memcpy(L->s_vp, vp, sizeof vp); L->s_have_vp = 1; }
  en(C_FOG, fog, &L->s_fog);
  if (fog) {
    uint32_t fp[3], fc = x->fogcol & 0xffffff; memcpy(&fp[0], &x->fog1, 4); memcpy(&fp[1], &x->fog2, 4); fp[2] = fc;
    if (!L->s_have_fogp || memcmp(L->s_fogp, fp, sizeof fp)) { putf(C_FOG1, x->fog1); putf(C_FOG2, x->fog2); put(C_FOGC, fc); memcpy(L->s_fogp, fp, sizeof fp); L->s_have_fogp = 1; }
  }
  if (ts && (!L->s_have_ts || memcmp(L->s_ts, ts, sizeof L->s_ts))) {
    putf(C_TSU, ts[0]); putf(C_TSV, ts[1]); putf(C_TOU, ts[2]); putf(C_TOV, ts[3]); memcpy(L->s_ts, ts, sizeof L->s_ts); L->s_have_ts = 1;
  }
}
/* a df0 call: its strips one PRIM each; the vertex address again only where a strip does not follow the one before */
static int xf_draw(const GerBatch *b, int tex)
{
  const GerXform *x = (const GerXform *)(F->d + b->first);
  const VcVert *v = (const VcVert *)(x + 1); const uint16_t *runs = (const uint16_t *)(v + x->nv);
  if (room() < BATCH_WORDS + 3 * x->nst + RESERVE) return -1;
  float ts[4] = { x->su, x->sv, x->tu, x->tv };
  xf_setup(x, (b->flags & GERF_FOG) != 0, tex ? ts : 0);
  vtype(VT_DF0);
  uint32_t at = 0xffffffffu;
  for (uint32_t k = 0; k < x->nst; k++) {
    if (runs[2 * k] != at) addr_cmd(C_VADDR, v + runs[2 * k]);
    put(C_PRIM, (uint32_t)G_TRIANGLE_STRIP << 16 | runs[2 * k + 1]); at = (uint32_t)runs[2 * k] + runs[2 * k + 1];
    L->c.prims++; L->c.xf_strips++;
  }
  return 0;
}
/* an effect-mesh call (GERK_XFORMF): the runs of class 1 triangles from the strip, then the triangles across the screen edge as one
   indexed list in the PS2 clipper's order: class 3 whole (the GE's scissor cuts it), class 4 as the PS2's polygon (the player's fx 1) */
static int fx_draw(const GerBatch *b, int tex)
{
  const GerXform *x = (const GerXform *)(F->d + b->first);
  uint32_t nv = x->nv, nfv = x->nfv;
  const VcVertF *v = (const VcVertF *)(x + 1);
  const uint8_t *cls = (const uint8_t *)(v + nv + nfv); const uint16_t *fan = (const uint16_t *)(cls + ((nv + 3) & ~3u));
  if (b->prim == G_SPRITES || b->prim == G_LINE_STRIP) {   /* sb15, Stage C step 4 (ge/xconv.c xc_c4), no classes: P04+3740's sprites,
                                                               the corner pairs in one PRIM; P04+2b60's streaks, the strip's pairs and
                                                               the clipper's in GU_LINES, each run of joined segments a line strip */
    const uint32_t nst = b->prim == G_LINE_STRIP ? x->nst : 0, tot = nv + nfv;
    if (room() < BATCH_WORDS + 2 * nst + 8 + RESERVE) return -1;
    float ts[4] = { x->su, x->sv, x->tu, x->tv };
    xf_setup(x, (b->flags & GERF_FOG) != 0, tex ? ts : 0);
    vtype(VT_FX);
    addr_cmd(C_VADDR, v);
    if (b->prim == G_SPRITES) { put(C_PRIM, (uint32_t)G_SPRITES << 16 | nv); L->c.prims++; L->c.fx_spr += nv / 2; return 0; }
    const uint16_t *run = (const uint16_t *)(v + tot);
    for (uint32_t r = 0, at = 0; r <= nst; r++) {   /* the GE's vertex address follows each PRIM's last vertex */
      const uint32_t next = r < nst ? run[2 * r] : tot;
      if (next > at) { put(C_PRIM, (uint32_t)G_LINES << 16 | (next - at)); L->c.prims++; L->c.fx_lines += (next - at) / 2; at = next; }
      if (r < nst) { put(C_PRIM, (uint32_t)G_LINE_STRIP << 16 | run[2 * r + 1]); L->c.prims++; L->c.fx_lruns++; at += run[2 * r + 1]; }
    }
    return 0;
  }
  uint32_t nruns = 0, ni = 0, p = 0;   /* the index list: 3 a class 3 triangle and 3 x (polygon vertices - 2) a class 4 one */
  for (uint32_t k = 2; k < nv; k++) {
    uint32_t cnt = 0;
    if (cls[k] == 1 && cls[k - 1] != 1) nruns++;
    if (p < x->nfan && fan[2 * p] == k) { cnt = fan[2 * p + 1]; p++; }
    if (cls[k] == 3) ni += 3; else if (cls[k] > 3 && cnt > 2) ni += 3 * (cnt - 2);
  }
  if (room() < BATCH_WORDS + 3 * nruns + RESERVE) return -1;
  uint16_t *t = 0; uint32_t nt = 0, base = nv;
  if (ni && !(t = (uint16_t *)aux_get(2 * ni, BATCH_WORDS + 3 * nruns))) return -1;
  p = 0;
  for (uint32_t k = 2; k < nv; k++) {
    uint32_t cnt = 0, b0 = base;
    if (p < x->nfan && fan[2 * p] == k) { cnt = fan[2 * p + 1]; base += cnt; p++; }   /* the polygon the PS2 drew for triangle k */
    if (cls[k] == 3) { t[nt++] = (uint16_t)(k - 2); t[nt++] = (uint16_t)(k - 1); t[nt++] = (uint16_t)k; }
    else if (cls[k] > 3) for (uint32_t j = 1; j + 1 < cnt; j++) { t[nt++] = (uint16_t)b0; t[nt++] = (uint16_t)(b0 + j); t[nt++] = (uint16_t)(b0 + j + 1); }
  }
  float ts[4] = { x->su, x->sv, x->tu, x->tv };
  xf_setup(x, (b->flags & GERF_FOG) != 0, tex ? ts : 0);
  vtype(VT_FX);
  for (uint32_t k = 2; k < nv; ) {   /* runs of class 1 triangles k..e = vertices k-2..e */
    if (cls[k] != 1) { k++; continue; }
    uint32_t e = k; while (e + 1 < nv && cls[e + 1] == 1) e++;
    addr_cmd(C_VADDR, v + (k - 2)); put(C_PRIM, (uint32_t)G_TRIANGLE_STRIP << 16 | (e - k + 3)); L->c.prims++; L->c.fx_runs++;
    k = e + 1;
  }
  if (nt) {
    vtype(VT_FX | VT_IDX16); addr_cmd(C_IADDR, t); addr_cmd(C_VADDR, v);
    put(C_PRIM, (uint32_t)G_TRIANGLES << 16 | nt); L->c.prims++; L->c.fx_tris += nt / 3;
  }
  return 0;
}
/* a P06 or P07 pass (GERK_LIT): its call's transform, the pass's texture scale and vertices, the strips one PRIM each as xf_draw */
static int lit_draw(const GerBatch *b, int tex)
{
  const uint8_t *p0 = F->d + b->first; const GerLit *p = (const GerLit *)p0;
  const GerXform *x = (const GerXform *)(p0 + p->xf);
  const uint8_t *v = p0 + p->vtx; const uint16_t *runs = (const uint16_t *)(p0 + p->run);
  if (room() < BATCH_WORDS + 3 * p->nst + RESERVE) return -1;
  float ts[4] = { p->su, p->sv, p->tu, p->tv };
  xf_setup(x, (b->flags & GERF_FOG) != 0, tex ? ts : 0);
  vtype(p->vtype);
  uint32_t at = 0xffffffffu;
  for (uint32_t k = 0; k < p->nst; k++) {
    if (runs[2 * k] != at) addr_cmd(C_VADDR, v + (uint32_t)runs[2 * k] * p->stride);
    put(C_PRIM, (uint32_t)G_TRIANGLE_STRIP << 16 | runs[2 * k + 1]); at = (uint32_t)runs[2 * k] + runs[2 * k + 1];
    L->c.prims++; L->c.lit_strips++;
  }
  L->c.lit_passes++;
  return 0;
}
/* ---- sb11 C1 (stagec_notes.md 1.9; replay_fmt.h GERL_C1): a P07 pass the GE skins, lights and maps (pass 3) itself.  WORLD identity and
   VIEW the call's world matrix, so the GE lights in the skinned space as the VU program does; the call's bones from bone 0 (its slots
   packed, xconv.c xc_lit_c1); its lights directional and diffuse only (no light ambient or specular) and the global ambient on a white
   material, no vertex colour; pass 3's texture coordinates from the texture matrix on the normalized normal, the pass's texture scale
   and offset folded into it (the GE does not apply them in that mode); passes 1 and 2 by their UVs as today.  Bones, light vectors and
   the texture matrix rounded to the nearest float24 (putr); VIEW as WORLD is elsewhere (putf, the same bits as the exact form's world).
   Each part is sent when it differs from what the list set; a batch that is not C1 sets LIGHTING 0, texture map mode 0 and VIEW identity
   again (c1_off), and the frame's end the first two: a frame without C1 passes has the words it had before ---- */
static const uint8_t IDW = 0;   /* L->s_xf: WORLD identity */
typedef uint32_t __attribute__((may_alias)) gu32a;
static int same_w(const void *a, const void *b, int n)   /* c1opt: n words equal (memcmp compared them byte by byte) */
{
  const gu32a *p = (const gu32a *)a, *q = (const gu32a *)b; uint32_t d = 0;
  for (int k = 0; k < n; k++) d |= p[k] ^ q[k];
  return !d;
}
static int same12(const void *a, const void *b)   /* c1opt2: 12 words equal, leaving at the first that differs (most bones differ in their
                                                     first word; the loop of same_w cost 70 instructions a matrix on the PSP) */
{
  const gu32a *p = (const gu32a *)a, *q = (const gu32a *)b;
  return p[0] == q[0] && p[1] == q[1] && p[2] == q[2] && p[3] == q[3] && p[4] == q[4] && p[5] == q[5] && p[6] == q[6] && p[7] == q[7] &&
         p[8] == q[8] && p[9] == q[9] && p[10] == q[10] && p[11] == q[11];
}
static const float ID12[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
#define C1_WORDS 200u   /* a bound for a C1 pass's own words: VIEW, WORLD 26; lighting 1 + the frame's set 18 + ambient 2 + lights 15; bones
                           8 x 13; the texture matrix and its mode 14 */
static void c1_off(int xf)   /* (xf: a transform batch, which reads VIEW) */
{
  const uint32_t n0 = L->n;
  if (L->s_light == 1) { put(C_LIGHTING, 0); L->s_light = 0; }
  if (L->s_tmap) { put(C_TMAP, 0); L->s_tmap = 0; }   /* (sb12: C1's mode or C2's) */
  if (xf && !L->s_vid) { put(C_VIEWN, 0); for (int k = 0; k < 12; k++) putf(C_VIEWD, ID12[k]); L->s_vid = 1; }
  L->c.c1_words += L->n - n0;
}
static void lit_once(uint32_t matupd)   /* the lighting state C1 and C2 share, set once a frame (sb12: C1's words as sb11's with matupd 0) */
{
  put(C_MATUPD, matupd); put(C_EMIS, 0); put(C_MAMB, 0xffffff); put(C_MALPHA, 0xff); put(C_MDIF, 0xffffff); put(C_MSPEC, 0);
  put(C_LMODE, 0); put(C_RNORM, 0);
  for (uint32_t j = 0; j < 3; j++) { put(C_LTYPE0 + j, 0); put(C_LAC0 + 3 * j, 0); put(C_LSC0 + 3 * j, 0); }
  put(C_LIGHTEN0 + 3, 0);
  L->s_c1set = 1; L->s_matupd = (int)matupd; L->s_emis = 0; L->s_have_emis = 1;
}
static int c1_draw(const GerBatch *b, int tex)
{
  const uint8_t *p0 = F->d + b->first; const GerLit *p = (const GerLit *)p0;
  const GerXform *x = (const GerXform *)(p0 + p->xf); const GerC1 *g = (const GerC1 *)(p0 + p->vtx);
  const int pass = (int)(p->pass & 15), nw = (int)g->nw;
  const uint8_t *v = (const uint8_t *)g + g->voff[pass - 1]; const uint16_t *runs = (const uint16_t *)(p0 + p->run);
  if (room() < BATCH_WORDS + C1_WORDS + 3 * p->nst + RESERVE) return -1;
  const uint32_t n0 = L->n;
  if (L->s_xf != &IDW) { put(C_WORLDN, 0); for (int k = 0; k < 12; k++) putf(C_WORLDD, ID12[k]); L->s_xf = &IDW; }
  if (L->s_vid || (L->s_vx != x && !same12(L->s_viewm, x->world))) {
    put(C_VIEWN, 0); for (int k = 0; k < 12; k++) putf(C_VIEWD, x->world[k]); memcpy(L->s_viewm, x->world, sizeof L->s_viewm); L->s_vid = 0; }
  L->s_vx = x;
  if (L->s_light != 1) { put(C_LIGHTING, 1); L->s_light = 1; }
  if (!L->s_c1set) lit_once(0);   /* once a frame */
  if (L->s_matupd != 0) { put(C_MATUPD, 0); L->s_matupd = 0; }   /* sb12: after a C2 pass (none: no word) */
  if (L->s_emis != 0) { put(C_EMIS, 0); L->s_emis = 0; }
  if (L->s_c1g != g) {   /* (another pass of the call the list set last: its ambient, lights and bones are what the GE holds; only
                            c1_draw sets them) */
  if (!L->s_have_amb || L->s_amb != g->amb) { put(C_AMBC, g->amb & 0xffffff); put(C_AMBA, g->amb >> 24); L->s_amb = g->amb; L->s_have_amb = 1; }
  for (uint32_t j = 0; j < 3; j++) {
    const int on = g->lcol[j] != 0;
    en(C_LIGHTEN0 + j, on, &L->s_len[j]);
    if (!on) continue;
    if (!L->s_have_l[j] || L->s_lcol[j] != g->lcol[j] || !same_w(L->s_ldir[j], g->ldir[j], 3)) {
      put(C_LDC0 + 3 * j, g->lcol[j]); for (uint32_t r = 0; r < 3; r++) putr(C_LX0 + 3 * j + r, g->ldir[j][r]);
      L->s_lcol[j] = g->lcol[j]; memcpy(L->s_ldir[j], g->ldir[j], sizeof L->s_ldir[j]); L->s_have_l[j] = 1; }
  }
  for (int k = 0, at = -1; k < nw; k++) {   /* the bones that differ from what the GE holds, BOFS where a run of them starts */
    if (k < L->s_nbone && same12(L->s_bone[k], g->bone[k])) continue;
    if (at != k) put(C_BOFS, 12u * (uint32_t)k);
    for (int e = 0; e < 12; e++) putr(C_BONED, g->bone[k][e]);
    memcpy(L->s_bone[k], g->bone[k], sizeof L->s_bone[k]); at = k + 1; L->c.c1_bones++;
  }
  if (nw > L->s_nbone) L->s_nbone = nw;   /* (the bones after the run kept: the GE keeps them) */
  L->s_c1g = g;
  }
  float ts[4] = { p->su, p->sv, p->tu, p->tv };
  const int tm = pass == 3 && tex;
  if (tm) {   /* u = (row 0 . n + o0) su + tu, v = (row 1 . n + o1) sv + tv, q = 1 */
    float m[12];
    for (int j = 0; j < 3; j++) { m[3 * j] = g->tgen[3 * j] * ts[0]; m[3 * j + 1] = g->tgen[3 * j + 1] * ts[1]; m[3 * j + 2] = g->tgen[3 * j + 2]; }
    m[9] = g->tgen[9] * ts[0] + ts[2]; m[10] = g->tgen[10] * ts[1] + ts[3]; m[11] = g->tgen[11];
    if (!L->s_have_tgen || !same12(L->s_tgen, m)) {
      put(C_TGENN, 0); for (int k = 0; k < 12; k++) putr(C_TGEND, m[k]); memcpy(L->s_tgen, m, sizeof m); L->s_have_tgen = 1; }
  }
  if (L->s_tmap != tm) { put(C_TMAP, tm ? 1u | 2u << 8 : 0u); L->s_tmap = tm; }   /* (1: the texture matrix, 2: the normalized normal) */
  L->c.c1_words += L->n - n0;
  xf_rest(x, (b->flags & GERF_FOG) != 0, tex && !tm ? ts : 0);
  vtype(p->vtype);
  uint32_t at = 0xffffffffu;
  for (uint32_t k = 0; k < p->nst; k++) {
    if (runs[2 * k] != at) addr_cmd(C_VADDR, v + (uint32_t)runs[2 * k] * p->stride);
    put(C_PRIM, (uint32_t)G_TRIANGLE_STRIP << 16 | runs[2 * k + 1]); at = (uint32_t)runs[2 * k] + runs[2 * k + 1];
    L->c.prims++; L->c.lit_strips++;
  }
  L->c.lit_passes++; L->c.c1_passes++;
  return 0;
}
/* ---- sb12 C2 (stagec_notes.md step 2; replay_fmt.h GERL_C2): a P06 pass the GE skins (two bones), lights and maps (pass 3) itself, by
   mapping (b).  As c1_draw: WORLD identity and VIEW the call's world matrix; LIGHTING 1 with the frame's shared set (lit_once); then
   MATUPD 3 (the material's ambient and diffuse from the vertex colour), the emissive the call's (the PS2's ambient), the global ambient
   (0, 0, 0) with alpha 255, the lights as C1's (directional, diffuse only); skinned, bone 0 32768 I and bone 1 the call's; pass 3's texture
   coordinates from the texture matrix on the normal as the GE holds it (TEXMAPMODE 1 | 3 << 8), the normal's scale (x 8 unskinned, / 4096
   skinned) and the pass's texture scale and offset folded into the matrix.  Bones, light vectors and the texture matrix rounded to the
   nearest float24 (putr).  Each part is sent when it differs from what the list set; c1_off restores the rest for other batches, c1_draw
   MATUPD 0 and EMIS 0 for C1's ---- */
static const float BONE0[12] = { 32768.0f, 0, 0, 0, 32768.0f, 0, 0, 0, 32768.0f, 0, 0, 0 };
static int c2_draw(const GerBatch *b, int tex)
{
  const uint8_t *p0 = F->d + b->first; const GerLit *p = (const GerLit *)p0;
  const GerXform *x = (const GerXform *)(p0 + p->xf); const GerC2 *g = (const GerC2 *)(p0 + p->vtx);
  const int pass = (int)(p->pass & 15);
  const uint8_t *v = (const uint8_t *)g + g->voff[pass - 1]; const uint16_t *runs = (const uint16_t *)(p0 + p->run);
  if (room() < BATCH_WORDS + C1_WORDS + 3 * p->nst + RESERVE) return -1;
  const uint32_t n0 = L->n;
  if (L->s_xf != &IDW) { put(C_WORLDN, 0); for (int k = 0; k < 12; k++) putf(C_WORLDD, ID12[k]); L->s_xf = &IDW; }
  if (L->s_vid || (L->s_vx != x && !same12(L->s_viewm, x->world))) {
    put(C_VIEWN, 0); for (int k = 0; k < 12; k++) putf(C_VIEWD, x->world[k]); memcpy(L->s_viewm, x->world, sizeof L->s_viewm); L->s_vid = 0; }
  L->s_vx = x;
  if (L->s_light != 1) { put(C_LIGHTING, 1); L->s_light = 1; }
  if (!L->s_c1set) lit_once(3);   /* once a frame */
  if (L->s_matupd != 3) { put(C_MATUPD, 3); L->s_matupd = 3; }
  if (L->s_c1g != g) {   /* (another pass of the call the list set last: its emissive, ambient, lights and bones are what the GE holds) */
    if (!L->s_have_emis || L->s_emis != g->emis) { put(C_EMIS, g->emis); L->s_emis = g->emis; L->s_have_emis = 1; }
    if (!L->s_have_amb || L->s_amb != 0xff000000u) { put(C_AMBC, 0); put(C_AMBA, 0xff); L->s_amb = 0xff000000u; L->s_have_amb = 1; }
    for (uint32_t j = 0; j < 3; j++) {
      const int on = g->lcol[j] != 0;
      en(C_LIGHTEN0 + j, on, &L->s_len[j]);
      if (!on) continue;
      if (!L->s_have_l[j] || L->s_lcol[j] != g->lcol[j] || !same_w(L->s_ldir[j], g->ldir[j], 3)) {
        put(C_LDC0 + 3 * j, g->lcol[j]); for (uint32_t r = 0; r < 3; r++) putr(C_LX0 + 3 * j + r, g->ldir[j][r]);
        L->s_lcol[j] = g->lcol[j]; memcpy(L->s_ldir[j], g->ldir[j], sizeof L->s_ldir[j]); L->s_have_l[j] = 1; }
    }
    if (g->skin) {   /* bones 0 and 1 where they differ from what the GE holds, BOFS where a run of them starts */
      const float *bm[2] = { BONE0, g->bone1 };
      for (int k = 0, at = -1; k < 2; k++) {
        if (k < L->s_nbone && same12(L->s_bone[k], bm[k])) continue;
        if (at != k) put(C_BOFS, 12u * (uint32_t)k);
        for (int e = 0; e < 12; e++) putr(C_BONED, bm[k][e]);
        memcpy(L->s_bone[k], bm[k], sizeof L->s_bone[k]); at = k + 1; L->c.c2_bones++;
      }
      if (2 > L->s_nbone) L->s_nbone = 2;
    }
    L->s_c1g = g;
  }
  float ts[4] = { p->su, p->sv, p->tu, p->tv };
  const int tm = pass == 3 && tex;
  if (tm) {   /* u = (row 0 . N sc + o0) su + tu, v = (row 1 . N sc + o1) sv + tv, q = 1 (N the GE's normal, sc its scale to the PS2's) */
    const float sc = g->skin ? 1.0f / 4096.0f : 8.0f;
    float m[12];
    for (int j = 0; j < 3; j++) { m[3 * j] = (g->tgen[3 * j] * sc) * ts[0]; m[3 * j + 1] = (g->tgen[3 * j + 1] * sc) * ts[1]; m[3 * j + 2] = g->tgen[3 * j + 2]; }
    m[9] = g->tgen[9] * ts[0] + ts[2]; m[10] = g->tgen[10] * ts[1] + ts[3]; m[11] = g->tgen[11];
    if (!L->s_have_tgen || !same12(L->s_tgen, m)) {
      put(C_TGENN, 0); for (int k = 0; k < 12; k++) putr(C_TGEND, m[k]); memcpy(L->s_tgen, m, sizeof m); L->s_have_tgen = 1; }
  }
  if (L->s_tmap != (tm ? 2 : 0)) { put(C_TMAP, tm ? 1u | 3u << 8 : 0u); L->s_tmap = tm ? 2 : 0; }   /* (1: the texture matrix, 3: the
                                                    normal as the GE holds it) */
  L->c.c2_words += L->n - n0;
  xf_rest(x, (b->flags & GERF_FOG) != 0, tex && !tm ? ts : 0);
  vtype(p->vtype);
  uint32_t at = 0xffffffffu;
  for (uint32_t k = 0; k < p->nst; k++) {
    if (runs[2 * k] != at) addr_cmd(C_VADDR, v + (uint32_t)runs[2 * k] * p->stride);
    put(C_PRIM, (uint32_t)G_TRIANGLE_STRIP << 16 | runs[2 * k + 1]); at = (uint32_t)runs[2 * k] + runs[2 * k + 1];
    L->c.prims++; L->c.lit_strips++;
  }
  L->c.lit_passes++; L->c.c2_passes++;
  return 0;
}
/* a sprite batch cut as the player's draw_cut (exact: the same pixels, u and v kept linear in x and y): asc puts the top-left corner
   first (colour and z stay the second vertex's, as the GE takes them), cw / ch cut each sprite into columns / rows of at most that
   many pixels */
static uint32_t pieces(float a, float c, int w)   /* the pieces of one side from a to c, step for step as cut_draw's loops */
{
  uint32_t k = 1; float st = c - a < 0 ? -(float)w : w;
  if (w) for (float x = a; fabsf(c - x) > w; x += st) k++;
  return k;
}
static void cut_corners(const GerVert *v, int asc, GerVert *a, GerVert *c)
{
  float t; *a = v[0]; *c = v[1];
  if (asc) {
    if (a->x > c->x) { t = a->x; a->x = c->x; c->x = t; t = a->u; a->u = c->u; c->u = t; }
    if (a->y > c->y) { t = a->y; a->y = c->y; c->y = t; t = a->v; a->v = c->v; c->v = t; }
    a->c = c->c = v[1].c; a->z = c->z = v[1].z;
  }
}
static float uv_at(float a, float c, float d, float off) { return d != 0 ? a + (c - a) * off / d : a; }
static int cut_draw(const GerBatch *b, int asc, int cw, int ch)
{
  const GerVert *v = F->v + b->first; uint32_t s, n = 0, j = 0; GerVert a, c;
  for (s = 0; s + 1 < b->count; s += 2) { cut_corners(v + s, asc, &a, &c); n += pieces(a.x, c.x, cw) * pieces(a.y, c.y, ch); }
  GerVert *o = room() < BATCH_WORDS + RESERVE ? 0 : (GerVert *)aux_get(2 * n * (uint32_t)sizeof(GerVert), BATCH_WORDS);
  if (!o) return -1;
  for (s = 0; s + 1 < b->count; s += 2) {
    cut_corners(v + s, asc, &a, &c);
    float dx = c.x - a.x, dy = c.y - a.y, sx = dx < 0 ? -(float)cw : cw, sy = dy < 0 ? -(float)ch : ch;
    for (float y = a.y;;) {
      float y1 = ch && fabsf(c.y - y) > ch ? y + sy : c.y;
      for (float x = a.x;;) {
        float x1 = cw && fabsf(c.x - x) > cw ? x + sx : c.x;
        o[j] = a; o[j].x = x; o[j].y = y; o[j].u = uv_at(a.u, c.u, dx, x - a.x); o[j].v = uv_at(a.v, c.v, dy, y - a.y);
        o[j + 1] = c; o[j + 1].x = x1; o[j + 1].y = y1; o[j + 1].u = uv_at(a.u, c.u, dx, x1 - a.x); o[j + 1].v = uv_at(a.v, c.v, dy, y1 - a.y);
        j += 2; x = x1;
        if (x1 == c.x) break;
      }
      y = y1;
      if (y1 == c.y) break;
    }
  }
  if (j < 2 * n) memset(o + j, 0, (2 * n - j) * sizeof *o);   /* (a step that lands on the corner: pieces counted one more) */
  vtype(VT_THROUGH); addr_cmd(C_VADDR, o); put(C_PRIM, (uint32_t)G_SPRITES << 16 | j); L->c.prims++; L->c.cuts++;
  return 0;
}

/* one batch; pass 1 is the second pass of an AFAIL FB_ONLY draw: the pixels that fail the alpha test, colour only */
static int gx_batch(uint32_t i, int pass)
{
  const GerBatch *b = &F->b[i];
  if (b->kind == GERK_THROUGH && (b->entry == DF0_ENTRY || b->entry == FX_ENTRY)) return 0;
  if (room() < BATCH_WORDS + RESERVE) return -1;
  const uint32_t lpass = b->kind == GERK_LIT ? ((const GerLit *)(F->d + b->first))->pass : 0;
  const int c1 = (lpass & (GERL_C1 | GERL_C2)) != 0;   /* sb11 C1, sb12 C2 */
  if (!c1 && (L->s_light == 1 || L->s_tmap || (!L->s_vid && b->kind != GERK_THROUGH))) {
    if (room() < BATCH_WORDS + 16 + RESERVE) return -1;
    c1_off(b->kind != GERK_THROUGH); }
  int tex = (b->flags & GERF_TEX) != 0;
  if (tex && (b->tex >= F->nt || (F->t[b->tex].kind == 1 && (!F->tdata[b->tex] || (F->t[b->tex].psm != 16 && !F->tpal[b->tex]))))) { tex = 0; L->c.tex_none++; }
  int cut = 0;   /* 1: the depth blur (sprites sampling the frame being drawn), 2: the motion trail (the other frame) */
  if (b->kind == GERK_THROUGH && tex && b->prim == G_SPRITES && F->t[b->tex].kind == 2) cut = F->t[b->tex].psm ? 2 : 1;
  int sc0 = b->sy0 << 10 | b->sx0, sc1 = b->sy1 << 10 | b->sx1;
  if (L->s_sc0 != sc0 || L->s_sc1 != sc1) { put(C_SCISSOR1, (uint32_t)sc0); put(C_SCISSOR2, (uint32_t)sc1); L->s_sc0 = sc0; L->s_sc1 = sc1; }
  int sh = b->flags & GERF_GOURAUD ? 1 : 0;
  if (L->s_shade != sh) { put(C_SHADE, (uint32_t)sh); L->s_shade = sh; }
  en(C_TEXTURE, tex, &L->s_ten);
  if (tex) {
    texture(b->tex);
    /* TFUNC: modulate with colour doubling (the PS2's vertex colour 0x80 = 1.0), decal; TCC from TEX0 */
    int tf = (b->tfx == 1 ? 1 : 0) | ((b->flags & GERF_TCC) ? 1 : 0) << 8 | (b->tfx == 1 ? 0 : 1 << 16);
    if (L->s_tfunc != tf) { put(C_TFUNC, (uint32_t)tf); L->s_tfunc = tf; }
  }
  int bl = (b->flags & GERF_BLEND) != 0;
  en(C_BLEND, bl, &L->s_blend);
  if (bl) blend(b);
  /* AFAIL FB_ONLY without Z write: a pixel that fails still writes its colour, so the test changes nothing and the GE must not run it */
  int at = (b->flags & GERF_ATEST) && b->atst != 1 && !(b->afail == 1 && !(b->flags & GERF_ZWRITE));
  en(C_ATEST, at, &L->s_at);
  if (at) {
    int ref = b->aref >= 0x80 ? 0xff : b->aref * 2, fn = pass ? ATINV[b->atst & 7] : ATMAP[b->atst & 7], sig = fn << 8 | ref;
    if (L->s_afunc != sig) { put(C_AFUNC, (uint32_t)(fn | ref << 8 | 0xff << 16)); L->s_afunc = sig; }
  }
  int zt = (b->flags & GERF_ZTEST) != 0;
  en(C_ZTEST, zt, &L->s_zt);
  if (zt && L->s_zfunc != ZTMAP[b->ztst & 3]) { put(C_ZFUNC, ZTMAP[b->ztst & 3]); L->s_zfunc = ZTMAP[b->ztst & 3]; }
  int zw = (b->flags & GERF_ZWRITE) && !pass;
  if (L->s_zmask != zw) { put(C_ZMASK, zw ? 0u : 1u); L->s_zmask = zw; }
  if (b->kind == GERK_XFORMF || L->s_clip == 1) en(C_CLIP, b->kind == GERK_XFORMF, &L->s_clip);   /* the GE's depth clamp for the effects */
  L->c.batches++;
  if (b->kind == GERK_XFORMF) return fx_draw(b, tex);
  if (b->kind == GERK_XFORM) return xf_draw(b, tex);
  if (b->kind == GERK_LIT) return c1 ? ((lpass & GERL_C2) ? c2_draw(b, tex) : c1_draw(b, tex)) : lit_draw(b, tex);
  en(C_FOG, 0, &L->s_fog);
  if (cut == 1) return cut_draw(b, 1, 0, 16);   /* top-left first, 16-row tiles (v3's fastest exact cut) */
  if (cut == 2) return cut_draw(b, 0, 16, 0);   /* 16-pixel columns */
  vtype(VT_THROUGH); addr_cmd(C_VADDR, F->v + b->first); put(C_PRIM, (uint32_t)b->prim << 16 | b->count); L->c.prims++;
  return 0;
}

void gx_init(GxList *l, uint32_t *words, uint32_t cap, uint8_t *aux, uint32_t cap_aux)
{
  memset(l, 0, sizeof *l); l->w = words; l->cap = cap; l->aux = aux; l->cap_aux = cap_aux & ~15u;
  l->shared = aux == (uint8_t *)words && cap_aux == 4 * cap;
}
void gx_depth_map(float lo, float hi, float zk, float *zs, float *zo)
{
  if (lo > hi) { lo = 0; hi = 16777215.0f; }
  float m = (hi - lo) / 16, a = lo - m, b = hi + m;
  if (a < 0) a = 0;
  if (b <= a) b = a + 1;
  float k = 65535.0f / (b - a);
  *zs = k / zk; *zo = -a * k;   /* GE depth = (q / zk - a) x k */
}
int gx_frame(GxList *l, const GxHost *h, GxFrame *f, int draw)
{
  L = l; H = h; F = f;
  memset(&l->c, 0, sizeof l->c);
  l->n = 0; l->naux = 0; l->alow = l->cap_aux; l->dig = 0xcbf29ce484222325ull;
  l->s_tex = l->s_ten = l->s_blend = l->s_bsig = l->s_at = l->s_afunc = l->s_zt = l->s_zfunc = l->s_zmask = l->s_shade = -1;
  l->s_tfunc = l->s_sc0 = l->s_sc1 = l->s_fog = l->s_clip = l->s_vtype = -1; l->s_base = 0xffffffffu;
  l->s_have_proj = l->s_have_vp = l->s_have_ts = l->s_have_fogp = 0; l->s_draw = draw; l->s_xf = 0;
  l->s_light = l->s_tmap = 0; l->s_vid = 1; l->s_c1set = l->s_have_amb = l->s_have_tgen = l->s_nbone = 0;   /* sb11 C1 (LIGHTING 0 and VIEW
                                                    identity below; the texture map mode sceGuInit's 0) */
  for (int j = 0; j < 3; j++) { l->s_len[j] = -1; l->s_have_l[j] = 0; }
  l->s_c1g = l->s_vx = 0;
  l->s_matupd = -1; l->s_emis = 0; l->s_have_emis = 0;   /* sb12 C2 */
  if (l->cap < 128 || l->cap_aux < 64) return -1;
  /* the frame's depth: the through vertices in place */
  if (f->zs != 1 || f->zo != 0)
    for (uint32_t i = 0; i < f->nv; i++) { float z = f->v[i].z * f->zs + f->zo; f->v[i].z = z < 0 ? 0 : z > 65535 ? 65535 : z; }
  /* the set-up (the player's init and frame start): 8888 buffers 512 a row, the 480 x 272 screen at 2048, 2048, depth range 65535..0
     (sceGuDepthRange(65535, 0)), the scissor, no lighting, culling, depth clamp or fog, linear filtering, the view matrix identity */
  uint32_t fbr = h->fb[draw];
  put(C_PSM, G_PSM_8888); put_sym(C_FBP, fbr, R_VRAM, fbr); put_sym(C_FBW, 512, R_VRAM, 512);
  put_sym(C_ZBP, h->zb, R_VRAM, h->zb); put_sym(C_ZBW, 512, R_VRAM, 512);
  put(C_OFFX, (2048 - 240) << 4); put(C_OFFY, (2048 - 136) << 4);
  putf(C_VPXS, 240); putf(C_VPXS + 1, -136); putf(C_VPXS + 2, -32768); putf(C_VPXS + 3, 2048); putf(C_VPXS + 4, 2048); putf(C_VPXS + 5, 32767);
  put(C_MINZ, 0); put(C_MAXZ, 65535);
  put(C_REGION1, 0); put(C_REGION2, 271 << 10 | 479);
  put(C_LIGHTING, 0); put(C_CULL, 0); put(C_CLIP, 0); l->s_clip = 0; put(C_FOG, 0); l->s_fog = 0;
  put(C_TFILTER, 1 | 1 << 8);
  put(C_VIEWN, 0); for (int k = 0; k < 12; k++) putf(C_VIEWD, k == 0 || k == 4 || k == 8 ? 1.0f : 0.0f);
  put(C_SCISSOR1, 0); put(C_SCISSOR2, 271 << 10 | 479); l->s_sc0 = 0; l->s_sc1 = 271 << 10 | 479;
  put(C_ZMASK, 0); l->s_zmask = 1;
  /* the clear (colour 0, depth 0): sceGuClear's sprite in clear mode, two vertices { colour, x y z, pad } */
  {
    uint8_t *q = aux_get(24, 8); uint16_t xy[2] = { 480, 272 };
    if (!q) return -1;
    memset(q, 0, 24); memcpy(q + 16, xy, 4);
    put(C_CLEAR, (1 | 4) << 8 | 1); vtype(VT_CLEAR); addr_cmd(C_VADDR, q); put(C_PRIM, (uint32_t)G_SPRITES << 16 | 2); put(C_CLEAR, 0);
  }
  int r = 0;
  for (uint32_t i = 0; i < f->nb; i++) {
    if (gx_batch(i, 0)) { l->c.dropped++; r = -1; continue; }
    if (f->b[i].twopass && (f->b[i].flags & GERF_ATEST)) { if (gx_batch(i, 1)) { l->c.dropped++; r = -1; } else l->c.passes2++; }
  }
  if (l->s_fog == 1) put(C_FOG, 0);
  if (l->s_clip == 1) put(C_CLIP, 0);
  if (l->s_tmap) put(C_TMAP, 0);   /* sb11 C1: sceGuInit's mode again, and no lighting (sb12: C1's mode or C2's) */
  if (l->s_light == 1) put(C_LIGHTING, 0);
  if (l->ret) put(C_RET, 0); else { put(C_FINISH, 0); put(C_END, 0); }
  l->c.words = l->n; l->c.aux = l->naux;
  return r;
}
