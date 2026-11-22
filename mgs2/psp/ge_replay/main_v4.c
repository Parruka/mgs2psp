/* MGS2 GE replay (MGS2GE): draws one PS2 frame of the Tanker deck with the PSP's GE, as the PS2 GS drew it, to measure what the GE
   costs for that frame before the GE renderer exists. The frame comes from tools/gsref.c (GSREF_GEOUT): every GS primitive of
   one vblank as GE batches in through mode (screen-space vertices: no hardware transform, so textures are not perspective-correct
   and there is no fog), with the textures converted by ge/texconv.c's layout. Format: ge/replay_fmt.h.
   What it measures, per mode (one change per mode, the modes take turns, 3 rounds): the CPU time to build the GE list for the
   frame and the GE time to draw it (list built first, then run alone and waited for, so the two do not overlap).
   v1 (2026-10-19, renderer_notes.md 9b): textures, blending, alpha test, filtering, the 4 costliest VU1 entries, bus sharing, merged
   draws.  v2: the parts of the one costly group (mist, depth blur, motion trail, small sprites) and fixes for it.
   v3 (after v2b's hardware run: depth blur 27 ms, motion trail 7.4 ms): each costly part alone, in variants, to find why the depth blur
   is slow; then the full frame with the fastest exact variants together.
   v4 (renderer_notes.md 11.5 step 2): the pre-lit meshes of VU1 entry P04+df0 (1,439 calls, 32,118 vertices in this frame) drawn with
   the GE's hardware transform, from the vertices the game uploads (ge/vu1conv.c; gsref checks them against the PS2's own output), in
   place of the PS2's screen-space primitives: the GE cost of hardware transform, of fog, and of one draw per call (strips joined)
   against one per strip.  The depth blur and motion trail use v3's fastest exact cuts from the PSP (top-left first in 16-row tiles,
   16-pixel columns).  The file's depth is the PS2's Z24 x zscale (header): 0.2 fits the frame's depth-writing draws (Z24 <= 292k) into
   16 bits; Z24 / 256 lost the 0.4-step gap between the deck's windows and their walls (renderer_notes.md 11.8).  On the PSP each mode's
   last frame of round 1 is saved as shot_<mode>.bmp (24-bit, 480 x 272) to compare with PPSSPP and the PS2 reference.
   Rules (tt_port/PS2_to_PSP_optimisation_lessons.md): 333 MHz through scePowerSetClockFrequency (the ceiling, nothing above);
   no hardware counters; no exception handler; HOME or X exits in order, never on its own on the PSP (in PPSSPP it exits after
   the results).
   Files next to the EBOOT (game data, personal use, never published): deck_ge.bin. Written: log.txt, shot_1.bmp .. shot_4.bmp. */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <psppower.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <math.h>
#include "../../ge/replay_fmt.h"
#include "../../ge/vu1conv.h"

PSP_MODULE_INFO("MGS2GE", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#define VERSION "MGS2 GE replay v4"
#define DF0_ENTRY 0x1991d8u                 /* VU1 entry P04+df0 (guest address of its first instruction, as gsref names it) */
#define FB0 0x000000                       /* VRAM, relative: two 8888 buffers of 512 x 272, a 16-bit depth buffer */
#define FB1 0x088000
#define ZB  0x110000
#define FRAMES 40                          /* frames per mode per round */
#define ROUNDS 3

static char g_dir[192] = "ms0:/PSP/GAME/MGS2GE/";
static volatile int g_quit;
static FILE *g_log;
static int g_emu;
static uint64_t now(void) { return (uint64_t)sceKernelGetSystemTimeWide(); }
static void out(const char *fmt, ...)
{
  char b[256]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
  pspDebugScreenPrintf("%s", b);
  if (g_log) { fputs(b, g_log); fflush(g_log); }
}
static int exit_cb(int a, int b, void *c) { (void)a; (void)b; (void)c; g_quit = 1; return 0; }
static int cb_thread(SceSize n, void *p)
{
  (void)n; (void)p;
  int cb = sceKernelCreateCallback("exit", exit_cb, 0); sceKernelRegisterExitCallback(cb);
  sceKernelSleepThreadCB(); return 0;
}

/* ---- the replay data ---- */
static uint8_t *g_file; static const GerHeader *H; static const GerBatch *B; static const GerVert *V; static const GerTex *T; static const uint8_t *D;
static int load(void)
{
  char p[256]; snprintf(p, sizeof p, "%sdeck_ge.bin", g_dir);
  FILE *f = fopen(p, "rb"); if (!f) { out("cannot open %s\n", p); return -1; }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  g_file = memalign(64, n); if (!g_file || fread(g_file, 1, n, f) != (size_t)n) { fclose(f); out("cannot read %s (%ld bytes)\n", p, n); return -1; }
  fclose(f);
  H = (const GerHeader *)g_file;
  if (H->magic != GER_MAGIC) { out("%s: not a replay file\n", p); return -1; }
  B = (const GerBatch *)(g_file + H->off_batch); V = (const GerVert *)(g_file + H->off_vert); T = (const GerTex *)(g_file + H->off_tex); D = g_file + H->off_data;
  sceKernelDcacheWritebackAll();
  out("%s: vb %u, %u batches, %u vertices, %u textures, %u data bytes, depth = Z24 x %g\n", p, H->vb, H->nbatch, H->nvert, H->ntex, H->ndata, (double)H->zscale);
  return 0;
}

/* ---- modes ---- */
/* What each batch is, from its own data (v1 log: these four share one VU1 entry and cost 39 of the 75 ms):
   mist: sprites with a palette texture, depth test GEQUAL, more than 10,000 pixels per sprite on average;
   small sprites: the same with 10,000 or fewer (particles);
   depth blur: sprites sampling the frame buffer being drawn (32 strips 30 x 272, bottom-right corner first, Z GEQUAL, almost all fail);
   motion trail: one full-screen sprite sampling the other frame buffer.
   v2b on the PSP (full 74.4 ms): depth blur 27.0 ms for 0.26 M pixels (and the same from a copy of the frame), motion trail 7.4 ms
   (1.5 ms in 64-pixel columns), mist 3.2, small sprites 0.9; textures in VRAM -5.6 ms; 16-bit frame buffers -17 ms. */
enum { C_OTHER, C_MIST, C_PART, C_BLUR, C_TRAIL, C_DF0, NCLASS };
static const char *CLASSN[NCLASS] = { "other", "mist", "small sprites", "depth blur", "motion trail", "df0 meshes" };
static uint8_t *g_class; static float *g_area;   /* per batch: class, GE pixels rasterised (before depth and alpha tests) */
static uint32_t g_first[NCLASS];           /* first batch of each class */
static uint32_t g_tv, g_xn, g_xv, g_xs, g_xsv, g_xi;   /* df0: through vertices; transform batches, vertices, strips, strip vertices, joined indices */
/* Sprite cuts (exact: the same pixels, u and v kept linear in x and y): asc puts the top-left corner first (colour and z stay the
   second vertex's, as the GE takes them), w / h cut each sprite into columns / rows at most that many pixels wide / tall. */
typedef struct { uint8_t asc, w, h; const char *name; } Cut;
enum { K_ASIS, K_ASC, K_W15, K_H16, K_ASC_H16, K_W64, K_W32, K_W16, NCUT };
static const Cut CUTS[NCUT] = { { 0, 0, 0, "as the PS2 draws it" }, { 1, 0, 0, "top-left corner first" }, { 0, 15, 0, "15-pixel columns" },
                                { 0, 0, 16, "16-row tiles" }, { 1, 0, 16, "top-left first, 16-row tiles" }, { 0, 64, 0, "64-pixel columns" },
                                { 0, 32, 0, "32-pixel columns" }, { 0, 16, 0, "16-pixel columns" } };
/* depth-blur variants that change the picture (measured only, to find the slow part) */
enum { BX_NONE, BX_NOTEX, BX_NEAREST, BX_COPY, BX_NOZ };
typedef struct {
  const char *name;
  int8_t skip;                             /* a class left out, -1 none */
  uint8_t notex, vram, fb16, bx;           /* no textures; palette textures in VRAM; 16-bit frame buffers; a BX_ blur variant */
  int8_t blur, trail;                      /* the cut for the depth blur / motion trail; -1: the fastest exact one measured on the PSP (v3) */
  uint8_t xf;                              /* df0 meshes: 0 as the PS2 draws them (screen space), 1 hardware transform, 2 + fog, 3 + fog, joined */
} Mode;
/* the parts alone: the df0 meshes drawn REPS times over the frame drawn up to the depth blur (so the depth buffer is the real one) */
#define REPS 10
static const Mode PARTS[] = {
  { "df0 as the PS2 draws it", -1, 0, 1, 0, BX_NONE, -1, -1, 0 },
  { "df0 transformed", -1, 0, 1, 0, BX_NONE, -1, -1, 1 },
  { "df0 transformed, fog", -1, 0, 1, 0, BX_NONE, -1, -1, 2 },
  { "df0 transformed, fog, joined", -1, 0, 1, 0, BX_NONE, -1, -1, 3 },
};
#define NPART (int)(sizeof PARTS / sizeof *PARTS)
/* the full frame */
enum { M_BEST, M_XF, M_XF_FOG, M_XF_JOIN, NMODE };
static const Mode MODES[NMODE] = {
  { "best (as v3)", -1, 0, 1, 0, BX_NONE, -1, -1, 0 },
  { "best, df0 transformed", -1, 0, 1, 0, BX_NONE, -1, -1, 1 },
  { "best, df0 tr. + fog", -1, 0, 1, 0, BX_NONE, -1, -1, 2 },
  { "best, df0 tr. + fog, joined", -1, 0, 1, 0, BX_NONE, -1, -1, 3 },
};
static int g_best_blur = K_ASC_H16, g_best_trail = K_W16;   /* v3 on the PSP (prof/hw_ge_v3): 2.65 and 1.36 ms */
#define COPYB 0x154000                     /* BX_COPY: the frame's copy, 8888 512 x 272 (to 0x1dc000) */
#define VTEX0 0x154000                     /* vram: textures from here to the end of VRAM (the copy overwrites them: g_vram_ok) */
#define VTEX1 0x200000
static uint32_t *g_vtex, *g_vpal;          /* vram: VRAM address of each texture's data and palette, 0 if not placed */
static int g_vram_ok;                      /* the textures are in VRAM now */
static float batch_area(const GerBatch *b)
{
  const GerVert *v = V + b->first; float a = 0; uint32_t k, n = b->count;
  if (b->prim == 6) { for (k = 0; k + 1 < n; k += 2) a += fabsf(v[k + 1].x - v[k].x) * fabsf(v[k + 1].y - v[k].y); return a; }
  for (k = 0; k + 2 < n; k += b->prim == 3 ? 3 : 1) {
    const GerVert *p = b->prim == 5 ? &v[0] : &v[k], *q = b->prim == 5 ? &v[k + 1] : &v[k + 1], *r = b->prim == 5 ? &v[k + 2] : &v[k + 2];
    a += fabsf((q->x - p->x) * (r->y - p->y) - (r->x - p->x) * (q->y - p->y)) * 0.5f;
  }
  return a;
}
static int classify(void)
{
  g_class = malloc(H->nbatch); g_area = malloc(H->nbatch * sizeof *g_area);
  if (!g_class || !g_area) return -1;
  double ca[NCLASS] = { 0 }; uint32_t cn[NCLASS] = { 0 };
  for (uint32_t i = 0; i < H->nbatch; i++) {
    const GerBatch *b = &B[i]; int c = C_OTHER;
    if (b->kind == GERK_XFORM) {   /* its area is its through batches' */
      const GerXform *x = (const GerXform *)(D + b->first);
      g_area[i] = 0; g_class[i] = C_DF0; if (!cn[C_DF0]) g_first[C_DF0] = i; cn[C_DF0]++;
      const uint16_t *runs = (const uint16_t *)((const VcVert *)(x + 1) + x->nv);
      g_xn++; g_xv += x->nv; g_xs += x->nst; g_xi += x->nidx; for (uint32_t k = 0; k < x->nst; k++) g_xsv += runs[2 * k + 1];
      continue;
    }
    g_area[i] = batch_area(b);
    if (b->entry == DF0_ENTRY) { c = C_DF0; g_tv += b->count; }
    else if ((b->flags & GERF_TEX) && b->prim == 6) {
      const GerTex *t = &T[b->tex];
      if (t->kind == 2) c = t->psm ? C_TRAIL : C_BLUR;
      else if ((b->flags & GERF_ZTEST) && b->ztst == 2) c = g_area[i] / (b->count / 2 ? b->count / 2 : 1) > 10000 ? C_MIST : C_PART;
    }
    if (!cn[c]) g_first[c] = i;
    g_class[i] = c; ca[c] += g_area[i]; cn[c]++;
  }
  for (int c = 0; c < NCLASS; c++) out("%-14s %5u batches, %8.0f GE pixels\n", CLASSN[c], (unsigned)cn[c], ca[c]);
  out("df0: %u screen-space vertices in its PS2 primitives; %u calls with hardware transform: %u vertices, %u strips of %u vertices, %u indices joined\n",
      (unsigned)g_tv, (unsigned)g_xn, (unsigned)g_xv, (unsigned)g_xs, (unsigned)g_xsv, (unsigned)g_xi);
  return 0;
}
static int vram_place(void)
{
  g_vtex = calloc(H->ntex, 4); g_vpal = calloc(H->ntex, 4); float *ta = calloc(H->ntex, sizeof *ta); uint8_t *done = calloc(H->ntex, 1);
  if (!g_vtex || !g_vpal || !ta || !done) return -1;
  double all = 0, in = 0; uint32_t at = VTEX0, n = 0;
  for (uint32_t i = 0; i < H->nbatch; i++) if (B[i].flags & GERF_TEX) { ta[B[i].tex] += g_area[i]; if (T[B[i].tex].kind == 1) all += g_area[i]; }
  for (;;) {   /* greedy: most GE pixels per byte first */
    int best = -1; float bv = 0;
    for (uint32_t t = 0; t < H->ntex; t++) {
      uint32_t sz = ((T[t].size + 15) & ~15u) + T[t].npal * 4;
      if (done[t] || T[t].kind != 1 || ta[t] <= 0 || at + sz > VTEX1) continue;
      if (ta[t] / sz > bv) { bv = ta[t] / sz; best = t; }
    }
    if (best < 0) break;
    const GerTex *t = &T[best]; done[best] = 1;
    g_vtex[best] = 0x04000000 + at; at += (t->size + 15) & ~15u;
    g_vpal[best] = 0x04000000 + at; at += (t->npal * 4 + 15) & ~15u;
    in += ta[best]; n++;
  }
  out("textures in VRAM: %u of them, %u KB, %.1f%% of the palette-textured GE pixels\n", (unsigned)n, (unsigned)((at - VTEX0) >> 10), all ? 100 * in / all : 0);
  free(ta); free(done);
  return 0;
}
/* before a mode with vram when g_vram_ok is 0 (a blur copy uses the same VRAM) */
static void vram_upload(void)
{
  for (uint32_t t = 0; t < H->ntex; t++) if (g_vtex[t]) {
    memcpy((void *)(g_vtex[t] + 0x40000000), D + T[t].data, T[t].size); memcpy((void *)(g_vpal[t] + 0x40000000), D + T[t].pal, T[t].npal * 4);
  }
  sceKernelDcacheWritebackAll(); g_vram_ok = 1;
}

/* ---- the GE list for one frame ---- */
static struct { int tex, ten, blend, bsig, at, afunc, zt, zfunc, zmask, shade, filt, tfunc, sc0, sc1, fog; float proj[16], vp[6], ts[4], fogp[2]; uint32_t fogc; } S;   /* last state sent; -1 / NaN: unknown */
static void st_reset(void) { memset(&S, 0xff, sizeof S); }
static void en(int cap, int on, int *cache) { if (*cache != on) { if (on) sceGuEnable(cap); else sceGuDisable(cap); *cache = on; } }
static const uint8_t ATMAP[8] = { GU_NEVER, GU_ALWAYS, GU_LESS, GU_LEQUAL, GU_EQUAL, GU_GEQUAL, GU_GREATER, GU_NOTEQUAL };   /* PS2 ATST -> GE */
static const uint8_t ATINV[8] = { GU_ALWAYS, GU_NEVER, GU_GEQUAL, GU_GREATER, GU_NOTEQUAL, GU_LESS, GU_LEQUAL, GU_EQUAL };   /* its negation */
static const uint8_t ZTMAP[4] = { GU_NEVER, GU_ALWAYS, GU_GEQUAL, GU_GREATER };                                             /* PS2 ZTST -> GE */
static long g_unmapped_blend;
static void blend(const GerBatch *b)
{
  int sig = b->ba | b->bb << 2 | b->bc << 4 | b->bd << 6 | b->fix << 8;
  if (S.bsig == sig) return;
  S.bsig = sig;
  unsigned f = b->fix >= 0x80 ? 0xff : b->fix * 2, W = 0xffffff;
  switch (sig & 0xff) {   /* D << 6 | C << 4 | B << 2 | A; A, B, D: 0 Cs 1 Cd 2 zero; C: 0 As 1 Ad 2 FIX */
    case 0x44: sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); break;        /* (Cs - Cd) As + Cd */
    case 0x48: sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, W); break;                        /* (Cs - 0) As + Cd */
    case 0x42: sceGuBlendFunc(GU_REVERSE_SUBTRACT, GU_SRC_ALPHA, GU_FIX, 0, W); break;           /* (0 - Cs) As + Cd */
    case 0x58: sceGuBlendFunc(GU_ADD, GU_DST_ALPHA, GU_FIX, 0, W); break;                        /* (Cs - 0) Ad + Cd */
    case 0x54: sceGuBlendFunc(GU_ADD, GU_DST_ALPHA, GU_ONE_MINUS_DST_ALPHA, 0, 0); break;        /* (Cs - Cd) Ad + Cd */
    case 0x64: sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, f | f << 8 | f << 16, (255 - f) * 0x010101u); break;   /* (Cs - Cd) FIX + Cd */
    default: g_unmapped_blend++; sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); break;
  }
}
static const Mode *g_m;                    /* the mode of the list being built */
static void texture(int ti, unsigned drawbuf, int cls)
{
  if (S.tex == ti) return;
  S.tex = ti;
  const GerTex *t = &T[ti];
  if (t->kind == 2) {   /* a frame buffer: the one being drawn (or its copy), or the other (the previous frame) */
    unsigned rel = t->psm ? (drawbuf == FB0 ? FB1 : FB0) : cls == C_BLUR && g_m->bx == BX_COPY ? COPYB : drawbuf;
    sceGuTexMode(g_m->fb16 ? GU_PSM_5650 : GU_PSM_8888, 0, 0, 0); sceGuTexImage(0, 512, 512, 512, (void *)(0x04000000 + rel));
  } else {
    int vr = g_m->vram && g_vtex[ti];
    sceGuTexMode(t->psm == 8 ? GU_PSM_T8 : GU_PSM_T4, 0, 0, 1); sceGuTexImage(0, t->tw, t->th, t->bw, vr ? (const void *)g_vtex[ti] : D + t->data);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0); sceGuClutLoad(t->npal / 8, vr ? (const void *)g_vpal[ti] : D + t->pal);
  }
  sceGuTexWrap(t->repeat & 1 ? GU_REPEAT : GU_CLAMP, t->repeat & 2 ? GU_REPEAT : GU_CLAMP);
}
#define VFMT (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)
static uint32_t pieces(float a, float c, int w) { return w ? (uint32_t)(fabsf(c - a) / w) + 1 : 1; }
static float uv_at(float a, float c, float d, float off) { return d != 0 ? a + (c - a) * off / d : a; }
/* a sprite batch drawn with a Cut (see CUTS) */
static void draw_cut(const GerBatch *b, const Cut *k)
{
  const GerVert *v = V + b->first; uint32_t s, n = 0, j = 0;
  for (s = 0; s + 1 < b->count; s += 2) n += pieces(v[s].x, v[s + 1].x, k->w) * pieces(v[s].y, v[s + 1].y, k->h);
  GerVert *o = sceGuGetMemory(n * 2 * sizeof *o);
  for (s = 0; s + 1 < b->count; s += 2) {
    GerVert a = v[s], c = v[s + 1]; float t;
    if (k->asc) {
      if (a.x > c.x) { t = a.x; a.x = c.x; c.x = t; t = a.u; a.u = c.u; c.u = t; }
      if (a.y > c.y) { t = a.y; a.y = c.y; c.y = t; t = a.v; a.v = c.v; c.v = t; }
      a.c = c.c = v[s + 1].c; a.z = c.z = v[s + 1].z;
    }
    float dx = c.x - a.x, dy = c.y - a.y, sx = dx < 0 ? -(float)k->w : k->w, sy = dy < 0 ? -(float)k->h : k->h;
    for (float y = a.y;;) {
      float y1 = k->h && fabsf(c.y - y) > k->h ? y + sy : c.y;
      for (float x = a.x;;) {
        float x1 = k->w && fabsf(c.x - x) > k->w ? x + sx : c.x;
        o[j] = a; o[j].x = x; o[j].y = y; o[j].u = uv_at(a.u, c.u, dx, x - a.x); o[j].v = uv_at(a.v, c.v, dy, y - a.y);
        o[j + 1] = c; o[j + 1].x = x1; o[j + 1].y = y1; o[j + 1].u = uv_at(a.u, c.u, dx, x1 - a.x); o[j + 1].v = uv_at(a.v, c.v, dy, y1 - a.y);
        j += 2; x = x1;
        if (x1 == c.x) break;
      }
      y = y1;
      if (y1 == c.y) break;
    }
  }
  sceGuDrawArray(GU_SPRITES, VFMT, j, 0, o);
}
/* one batch; pass 1 is the second pass of an AFAIL FB_ONLY draw: the pixels that fail the alpha test, colour only */
static int g_copied;                       /* BX_COPY: the frame has been copied for the depth blur */
#define XFMT (GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_3D)
/* a df0 call with hardware transform (GerXform); the batch's GS state is already set */
static void xf_draw(const GerBatch *b, int xf, int tex)
{
  const GerXform *x = (const GerXform *)(D + b->first);
  const VcVert *v = (const VcVert *)(x + 1); const uint16_t *runs = (const uint16_t *)(v + x->nv), *idx = runs + 2 * x->nst;
  ScePspFMatrix4 w;
  w.x.x = x->world[0]; w.x.y = x->world[1]; w.x.z = x->world[2]; w.x.w = 0;
  w.y.x = x->world[3]; w.y.y = x->world[4]; w.y.z = x->world[5]; w.y.w = 0;
  w.z.x = x->world[6]; w.z.y = x->world[7]; w.z.z = x->world[8]; w.z.w = 0;
  w.w.x = x->world[9]; w.w.y = x->world[10]; w.w.z = x->world[11]; w.w.w = 1;
  sceGuSetMatrix(GU_MODEL, &w);
  if (memcmp(S.proj, x->proj, sizeof S.proj)) { sceGuSetMatrix(GU_PROJECTION, (const ScePspFMatrix4 *)x->proj); memcpy(S.proj, x->proj, sizeof S.proj); }
  float vp[6] = { x->vs[0], x->vs[1], x->vs[2], x->vo[0], x->vo[1], x->vo[2] };
  if (memcmp(S.vp, vp, sizeof vp)) { for (int k = 0; k < 6; k++) sceGuSendCommandf(66 + k, vp[k]); memcpy(S.vp, vp, sizeof vp); }   /* VIEWPORT X1 Y1 Z1 X2 Y2 Z2 */
  int fog = xf >= 2 && (b->flags & GERF_FOG);
  en(GU_FOG, fog, &S.fog);
  if (fog && (S.fogp[0] != x->fog1 || S.fogp[1] != x->fog2 || S.fogc != x->fogcol)) {
    sceGuSendCommandf(205, x->fog1); sceGuSendCommandf(206, x->fog2); sceGuSendCommandi(207, x->fogcol & 0xffffff);   /* FOG1, FOG2, FOGCOLOR */
    S.fogp[0] = x->fog1; S.fogp[1] = x->fog2; S.fogc = x->fogcol;
  }
  if (tex) {
    float ts[4] = { x->su, x->sv, x->tu, x->tv };
    if (memcmp(S.ts, ts, sizeof ts)) { sceGuTexScale(ts[0], ts[1]); sceGuTexOffset(ts[2], ts[3]); memcpy(S.ts, ts, sizeof ts); }
  }
  if (xf == 3) sceGuDrawArray(GU_TRIANGLE_STRIP, XFMT | GU_INDEX_16BIT, x->nidx, idx, v);
  else for (uint32_t k = 0; k < x->nst; k++) sceGuDrawArray(GU_TRIANGLE_STRIP, XFMT, runs[2 * k + 1], 0, v + runs[2 * k]);
}
static void batch(uint32_t i, unsigned drawbuf, int pass)
{
  const GerBatch *b = &B[i]; const Mode *m = g_m; int cls = g_class[i], bx = cls == C_BLUR ? m->bx : BX_NONE;
  if (b->kind == GERK_XFORM ? !m->xf : m->xf && b->entry == DF0_ENTRY) return;   /* df0: its transform batches or its PS2 primitives */
  if (bx == BX_COPY && !g_copied) {
    sceGuCopyImage(GU_PSM_8888, 0, 0, 480, 272, 512, (void *)(0x04000000 + drawbuf), 0, 0, 512, (void *)(0x04000000 + COPYB));
    sceGuTexSync(); sceGuTexFlush(); g_copied = 1; S.tex = -1; g_vram_ok = 0;
  }
  int sc0 = b->sy0 << 10 | b->sx0, sc1 = b->sy1 << 10 | b->sx1;
  if (S.sc0 != sc0 || S.sc1 != sc1) { sceGuSendCommandi(212, sc0); sceGuSendCommandi(213, sc1); S.sc0 = sc0; S.sc1 = sc1; }   /* SCISSOR1 / 2 */
  int sh = b->flags & GERF_GOURAUD ? GU_SMOOTH : GU_FLAT;
  if (S.shade != sh) { sceGuShadeModel(sh); S.shade = sh; }
  int tex = (b->flags & GERF_TEX) && !m->notex && bx != BX_NOTEX;
  en(GU_TEXTURE_2D, tex, &S.ten);
  if (tex) {
    texture(b->tex, drawbuf, cls);
    int filt = bx == BX_NEAREST ? GU_NEAREST : GU_LINEAR;
    if (S.filt != filt) { sceGuTexFilter(filt, filt); S.filt = filt; }
    /* TFUNC: modulate with colour doubling (the PS2's vertex colour 0x80 = 1.0), decal; TCC from TEX0 */
    int tf = (b->tfx == 1 ? GU_TFX_DECAL : GU_TFX_MODULATE) | ((b->flags & GERF_TCC) ? GU_TCC_RGBA : GU_TCC_RGB) << 8 | (b->tfx == 1 ? 0 : 1 << 16);
    if (S.tfunc != tf) { sceGuSendCommandi(201, tf); S.tfunc = tf; }
  }
  int bl = (b->flags & GERF_BLEND) != 0;
  en(GU_BLEND, bl, &S.blend);
  if (bl) blend(b);
  /* AFAIL FB_ONLY without Z write: a pixel that fails still writes its colour, so the test changes nothing and the GE must not run it
     (v1 did, and dropped the mist and the particles: 1,415 batches, 3.8 M PS2 pixels) */
  int at = (b->flags & GERF_ATEST) && b->atst != 1 && !(b->afail == 1 && !(b->flags & GERF_ZWRITE));
  en(GU_ALPHA_TEST, at, &S.at);
  if (at) {
    int ref = b->aref >= 0x80 ? 0xff : b->aref * 2, fn = pass ? ATINV[b->atst] : ATMAP[b->atst], sig = fn << 8 | ref;
    if (S.afunc != sig) { sceGuAlphaFunc(fn, ref, 0xff); S.afunc = sig; }
  }
  int zt = (b->flags & GERF_ZTEST) && bx != BX_NOZ;
  en(GU_DEPTH_TEST, zt, &S.zt);
  if (zt && S.zfunc != ZTMAP[b->ztst]) { sceGuDepthFunc(ZTMAP[b->ztst]); S.zfunc = ZTMAP[b->ztst]; }
  int zw = (b->flags & GERF_ZWRITE) && !pass;
  if (S.zmask != zw) { sceGuDepthMask(zw ? GU_FALSE : GU_TRUE); S.zmask = zw; }
  if (b->kind == GERK_XFORM) { xf_draw(b, m->xf, tex); return; }
  en(GU_FOG, 0, &S.fog);
  int cut = cls == C_BLUR ? m->blur : cls == C_TRAIL ? m->trail : K_ASIS;
  if (cut < 0) cut = cls == C_BLUR ? g_best_blur : g_best_trail;
  if (cut != K_ASIS && b->prim == GU_SPRITES) draw_cut(b, &CUTS[cut]);
  else sceGuDrawArray(b->prim, VFMT, b->count, 0, V + b->first);
}
static void one(uint32_t i, unsigned drawbuf)
{
  batch(i, drawbuf, 0);
  if (B[i].twopass && (B[i].flags & GERF_ATEST)) batch(i, drawbuf, 1);
}
/* the frame's batches [i0, i1) in mode m */
static void view_identity(void)
{
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1; sceGuSetMatrix(GU_VIEW, &I);
}
static void build(const Mode *m, unsigned drawbuf, uint32_t i0, uint32_t i1)
{
  st_reset(); g_m = m; g_copied = 0;
  if (m->xf) view_identity();
  for (uint32_t i = i0; i < i1; i++) if (m->skip < 0 || g_class[i] != m->skip) one(i, drawbuf);
  en(GU_FOG, 0, &S.fog);
}
/* one class's batches, REPS times */
static void build_part(const Mode *m, int cls, unsigned drawbuf)
{
  st_reset(); g_m = m;
  if (m->xf) view_identity();
  for (int r = 0; r < REPS; r++) { g_copied = 0; for (uint32_t i = g_first[cls]; i < H->nbatch; i++) if (g_class[i] == cls) one(i, drawbuf); }
  en(GU_FOG, 0, &S.fog);
}

/* the finished picture in VRAM buffer buf as a 24-bit BMP next to the EBOOT (outside the timed part) */
static void save_shot(unsigned buf, int m)
{
  static uint8_t img[54 + 480 * 272 * 3];
  uint32_t sz = sizeof img; uint8_t *h = img; memset(h, 0, 54);
  h[0] = 'B'; h[1] = 'M'; memcpy(h + 2, &sz, 4); h[10] = 54; h[14] = 40;
  int32_t w = 480, ht = 272; memcpy(h + 18, &w, 4); memcpy(h + 22, &ht, 4); h[26] = 1; h[28] = 24;
  uint32_t isz = 480 * 272 * 3; memcpy(h + 34, &isz, 4);
  const volatile uint32_t *src = (const volatile uint32_t *)(0x44000000 + buf);
  for (int y = 0; y < 272; y++) {   /* bottom-up rows, BGR; the GE's 8888 is R in the low byte */
    uint8_t *o = img + 54 + (271 - y) * 480 * 3;
    for (int x = 0; x < 480; x++) { uint32_t c = src[y * 512 + x]; o[3 * x] = c >> 16; o[3 * x + 1] = c >> 8; o[3 * x + 2] = c; }
  }
  char p[256]; snprintf(p, sizeof p, "%sshot_%d.bmp", g_dir, m + 1);
  SceUID fd = sceIoOpen(p, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
  int ok = fd >= 0 && sceIoWrite(fd, img, sz) == (int)sz; if (fd >= 0) sceIoClose(fd);
  if (g_log) { fprintf(g_log, "  picture of mode %d %s: %s\n", m + 1, ok ? "saved" : "NOT saved", p); fflush(g_log); }
}
static void emu_screenshot(void) { if (g_emu) { sceDisplayWaitVblankStart(); sceDisplayWaitVblankStart(); sceIoDevctl("emulator:", 0x20, 0, 0, 0, 0); } }   /* after the swap has taken effect */

int main(int argc, char **argv)
{
  if (argc > 0 && argv[0]) { const char *s = strrchr(argv[0], '/'); if (s && (size_t)(s - argv[0] + 1) < sizeof g_dir) { memcpy(g_dir, argv[0], s - argv[0] + 1); g_dir[s - argv[0] + 1] = 0; } }
  int th = sceKernelCreateThread("cb", cb_thread, 0x11, 0x1000, 0, 0); if (th >= 0) sceKernelStartThread(th, 0, 0);
  { int r = 0; if (sceIoDevctl("emulator:", 3, 0, 0, &r, 4) >= 0 && r) g_emu = 1; }   /* PPSSPP: EMULATOR_DEVCTL__IS_EMULATOR */
#ifdef SMOKE   /* PPSSPP check of the PSP path: no start notice, one round, exits at the end */
  int smoke = g_emu; g_emu = 0;
#else
  int smoke = 0;
#endif
  pspDebugScreenInit();
  char lp[256]; snprintf(lp, sizeof lp, "%slog.txt", g_dir); g_log = fopen(lp, "w");
  out("%s\n", VERSION);
  int clk = scePowerSetClockFrequency(333, 333, 166);
  out("clock 333/166: %s (cpu %d MHz, bus %d MHz)\n", clk < 0 ? "refused" : "set", scePowerGetCpuClockFrequency(), scePowerGetBusClockFrequency());
  if (load() < 0) goto wait;
  if (classify() < 0 || vram_place() < 0) { out("out of memory for the batch tables\n"); goto wait; }
  if (!g_emu && !smoke) {   /* v2b: say what the test looks like before it starts (v2 looked like a failing GPU to its tester) */
    pspDebugScreenPrintf("\nThis test draws ONE frozen deck frame in two parts (about two minutes in all).\n"
                         "Part 1: the ship's meshes drawn alone; the screen shows only a line of text and does not move.\n"
                         "Part 2: the whole frame in 4 modes. The picture is squeezed to the PSP screen and has no\n"
                         "fog except on the ship's meshes in two modes. That is expected.\n"
                         "It saves one picture per mode (shot_1.bmp .. shot_4.bmp) next to log.txt.\n"
                         "It only draws pictures: it cannot harm the PSP. Results show at the end.\n\n"
                         "Press X to start.\n");
    for (;;) { SceCtrlData c; sceCtrlReadBufferPositive(&c, 1); if ((c.Buttons & PSP_CTRL_CROSS) || g_quit) break; sceKernelDelayThread(50000); }
    for (;;) { SceCtrlData c; sceCtrlReadBufferPositive(&c, 1); if (!(c.Buttons & PSP_CTRL_CROSS) || g_quit) break; sceKernelDelayThread(50000); }
  }
  void *call = memalign(64, 4 << 20), *dlist = memalign(64, 64 << 10);
  if (!call || !dlist) { out("out of memory for the lists\n"); goto wait; }

  sceGuInit();
  sceGuStart(GU_DIRECT, dlist);
  sceGuDrawBuffer(GU_PSM_8888, (void *)FB0, 512); sceGuDispBuffer(480, 272, (void *)FB1, 512); sceGuDepthBuffer((void *)ZB, 512);
  sceGuOffset(2048 - 240, 2048 - 136); sceGuViewport(2048, 2048, 480, 272); sceGuDepthRange(65535, 0);
  sceGuScissor(0, 0, 480, 272); sceGuEnable(GU_SCISSOR_TEST);   /* this pspgu's sceGuScissor also sets the drawing region (REGION1 / 2); without it the GE draws one pixel */
  sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_CLIP_PLANES);
  sceGuFinish(); sceGuSync(GU_SYNC_FINISH, GU_SYNC_WAIT);
  sceDisplayWaitVblankStart(); sceGuDisplay(GU_TRUE);

  unsigned draw = FB0; long listmax = 0;
#ifndef EMU_MODE
#define EMU_MODE M_BEST
#endif
#ifndef SMOKE_FRAMES
#define SMOKE_FRAMES 4                     /* FRAMES: PPSSPP pictures with the PSP run's history (the motion trail blends the previous frames) */
#endif
  int rounds = g_emu || smoke ? 1 : ROUNDS, frames = g_emu ? 3 : smoke ? SMOKE_FRAMES : FRAMES;

  /* 1. the parts alone (PSP only): the frame up to the depth blur (untimed), then one class drawn REPS times over it (timed) */
  static double p_sum[NPART], p_min[NPART]; static long p_n[NPART];
  for (int p = 0; p < NPART; p++) p_min[p] = 1e9;
  if (!g_emu) {
    unsigned disp = draw == FB0 ? FB1 : FB0;
    memset((void *)(0x44000000 + disp), 0, 512 * 272 * 4);
    pspDebugScreenSetColorMode(PSP_DISPLAY_PIXEL_FORMAT_8888); pspDebugScreenSetBase((u32 *)(0x44000000 + disp));
    for (int r = 0; r < rounds && !g_quit; r++)
      for (int p = 0; p < NPART && !g_quit; p++) {
        const Mode *m = &PARTS[p];
        pspDebugScreenSetXY(0, 0);
        pspDebugScreenPrintf("PART TEST %2d/%d round %d/%d: %-30s\nThe screen stays still during this part (about a minute).", p + 1, NPART, r + 1, rounds, m->name);
        if (m->vram && !g_vram_ok) vram_upload();
        for (int k = 0; k < 4 && !g_quit; k++) {   /* k = 0 warms up */
          sceGuStart(GU_CALL, call); build(m, draw, 0, g_first[C_BLUR]); sceGuFinish();
          sceGuStart(GU_DIRECT, dlist);
          sceGuDrawBufferList(m->fb16 ? GU_PSM_5650 : GU_PSM_8888, (void *)draw, 512);
          sceGuSendCommandi(212, 0); sceGuSendCommandi(213, 271 << 10 | 479); sceGuDepthMask(GU_FALSE);
          sceGuClearColor(0); sceGuClearDepth(0); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
          sceGuCallList(call); sceGuFinish(); sceGuSync(GU_SYNC_FINISH, GU_SYNC_WAIT);
          sceGuStart(GU_CALL, call); build_part(m, C_DF0, draw); { long used = sceGuCheckList(); if (used > listmax) listmax = used; } sceGuFinish();
          uint64_t t1 = now();
          sceGuStart(GU_DIRECT, dlist); sceGuCallList(call); sceGuFinish(); sceGuSync(GU_SYNC_FINISH, GU_SYNC_WAIT);
          double t = (double)(now() - t1) / REPS;
          if (k) { p_sum[p] += t; p_n[p]++; if (t < p_min[p]) p_min[p] = t; }
        }
        if (r == rounds - 1 && g_log) { fprintf(g_log, "  part %-30s %7.2f ms per draw (min %7.2f)\n", m->name, p_sum[p] / p_n[p] / 1000, p_min[p] / 1000); fflush(g_log); }
      }
  }
  out("cuts (v3's fastest exact ones on the PSP): depth blur %s, motion trail %s\n", CUTS[g_best_blur].name, CUTS[g_best_trail].name);

  /* 2. the full frame per mode: list built first (CPU time), then the clear and the list run alone on the GE (GE time) */
  static double ge_sum[NMODE], cpu_sum[NMODE], ge_min[NMODE]; static long n[NMODE];
  for (int m = 0; m < NMODE; m++) ge_min[m] = 1e9;
  int m0 = g_emu ? EMU_MODE : 0, m1 = g_emu ? EMU_MODE + 1 : NMODE;
  for (int r = 0; r < rounds && !g_quit; r++)
    for (int m = m0; m < m1 && !g_quit; m++)
      for (int f = 0; f < frames && !g_quit; f++) {
        const Mode *md = &MODES[m];
        if (md->vram && !g_vram_ok) vram_upload();
        uint64_t t0 = now();
        sceGuStart(GU_CALL, call); build(md, draw, 0, H->nbatch); { long used = sceGuCheckList(); if (used > listmax) listmax = used; } sceGuFinish();
        uint64_t t1 = now();
        /* the clear goes in the direct list: pspgu's sceGuClear takes its size from the current context, which in a GU_CALL list is empty */
        sceGuStart(GU_DIRECT, dlist);
        sceGuDrawBufferList(md->fb16 ? GU_PSM_5650 : GU_PSM_8888, (void *)draw, 512);
        sceGuSendCommandi(212, 0); sceGuSendCommandi(213, 271 << 10 | 479); sceGuDepthMask(GU_FALSE);
        sceGuClearColor(0); sceGuClearDepth(0); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
        sceGuCallList(call); sceGuFinish();
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WAIT);
        uint64_t t2 = now();
        if (f > 0) { cpu_sum[m] += (double)(t1 - t0); ge_sum[m] += (double)(t2 - t1); n[m]++; if ((double)(t2 - t1) < ge_min[m]) ge_min[m] = (double)(t2 - t1); }
        if (!g_emu && r == 0 && f == frames - 1) save_shot(draw, m);   /* before the name goes on it */
        if (!g_emu) {   /* the mode's name on the finished picture (after the timing, before the swap) */
          pspDebugScreenSetBase((u32 *)(0x44000000 + draw)); pspDebugScreenSetColorMode(md->fb16 ? PSP_DISPLAY_PIXEL_FORMAT_565 : PSP_DISPLAY_PIXEL_FORMAT_8888);
          pspDebugScreenSetXY(0, 0); pspDebugScreenPrintf("FRAME TEST %d/%d round %d/%d: %-24s (odd pictures are expected)", m + 1, NMODE, r + 1, rounds, md->name);
        }
        draw = (unsigned)(uintptr_t)sceGuSwapBuffers();
        if (md->fb16) sceDisplaySetFrameBuf((void *)(0x44000000 + (draw == FB0 ? FB1 : FB0)), 512, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);
        if (r == 0 && m == EMU_MODE && f == 2) emu_screenshot();
        if (f == frames - 1 && g_log && !g_emu) {   /* progress in the log after each mode, so a run stopped early still reports what it measured */
          fprintf(g_log, "  (round %d) %-26s GE %7.2f  min %7.2f   build %6.2f\n", r + 1, md->name, ge_sum[m] / n[m] / 1000, ge_min[m] / 1000, cpu_sum[m] / n[m] / 1000);
          fflush(g_log);
        }
      }
  if (g_quit) out("stopped early: the HOME menu's exit was chosen during the run (results below cover the tests finished before it)\n");
  {   /* the results on a cleared 8888 screen (a 16-bit mode may have left a 5650 picture) */
    unsigned disp = draw == FB0 ? FB1 : FB0;
    memset((void *)(0x44000000 + disp), 0, 512 * 272 * 4);
    sceDisplaySetFrameBuf((void *)(0x44000000 + disp), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
    pspDebugScreenSetColorMode(PSP_DISPLAY_PIXEL_FORMAT_8888); pspDebugScreenSetBase((u32 *)(0x44000000 + disp)); pspDebugScreenSetXY(0, 0);
  }
  if (!g_emu) {
    out("Each part alone, GE time per draw (ms; mean, min), %d x %d draws; (*) changes the picture, measured only:\n", 3 * REPS, rounds);
    for (int p = 0; p < NPART; p++) if (p_n[p]) out("  %-30s %7.2f  min %7.2f\n", PARTS[p].name, p_sum[p] / p_n[p] / 1000, p_min[p] / 1000);
  }
  out("Full frame, GE time (ms; mean, min) and CPU time to build its list (ms), %d frames x %d rounds per mode:\n", frames - 1, rounds);
  for (int m = 0; m < NMODE; m++) if (n[m]) out("  %-26s GE %7.2f  min %7.2f   build %6.2f\n", MODES[m].name, ge_sum[m] / n[m] / 1000, ge_min[m] / 1000, cpu_sum[m] / n[m] / 1000);
  out("largest list %ld KB (of 4096); blends with no GE mapping: %ld\n", listmax / 1024, g_unmapped_blend);
wait:
  if (g_emu || smoke) { if (g_log) fclose(g_log); sceKernelExitGame(); return 0; }
  out("Press X (or HOME) to exit.\n");
  for (;;) { SceCtrlData c; sceCtrlReadBufferPositive(&c, 1); if ((c.Buttons & PSP_CTRL_CROSS) || g_quit) break; sceKernelDelayThread(50000); }
  sceGuTerm(); if (g_log) fclose(g_log);
  sceKernelExitGame();
  return 0;
}
