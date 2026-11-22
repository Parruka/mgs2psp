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
   v5 (renderer_notes.md 11.9): the effect meshes of VU1 entry P04+25f0 (40% of the frame's PS2 pixels) drawn with hardware transform
   from the float world positions the game uploads (ge/vu1conv.c vc_fpos; gsref checks them and a port of the PS2's clipper against the
   PS2's own output).  Most of their triangles cross the screen edge, where the PS2 clips them into fans.  Two ways on the GE: the GE
   draws a crossing triangle whole and its scissor cuts it, when every vertex is inside the GE's 0..4096 screen range and the depth
   window (the CPU's clipped polygon otherwise); or the CPU's clipped polygon for every crossing triangle, as the PS2 does.  Effects use
   the GE's depth clamp (GU_CLIP_PLANES): they test depth without writing it.
   v5 after v4's run on the PSP (renderer_notes.md 11.8: joining a call's strips into one indexed strip cost the GE 4.65 ms more than
   one draw per strip): the effects' strip triangles are drawn one run per draw, not joined; the last frame mode draws df0 with one
   draw per strip; and a df0 part draws each strip indexed without joins, to tell the cost of the indexed fetch from that of the joins'
   zero-area triangles.
   v6 (audit H2, renderer_notes.md 11.11): v5's last mode (df0 and effects transformed, fog) timed while the CPU works, as the game's
   logic will while the GE draws the frame before (v1-v5 timed the GE with the CPU idle; v1's CPU copy cost the GE 21 ms).  A second
   thread, of lower priority, works only while the main thread waits for the GE: line reads at the v24 deck logic's rate of reads from
   RAM, the same with its rate of written lines, the next frame's list built (the port's own pattern) and v1's worst case, 64 KB copies.
   Its rate while the GE draws against its rate alone tells how much the GE slows the CPU.  Also: the effects' clipped triangles drawn
   with no index list (v5: index lists cost df0 4.6 ms), the frame without the depth blur and the motion trail (measured only), and
   every mode's picture in rounds 1 and 2.  Each mode starts from black frame buffers, so all pictures of the frame in its exact form
   (modes 1, 2 and 4-7) should be identical: a difference comes from the run (timing, a busy bus), which tells where v4 / v5's
   1,275-pixel difference on the PSP came from (renderer_notes.md 11.9).
   Rules (tt_port/PS2_to_PSP_optimisation_lessons.md): 333 MHz through scePowerSetClockFrequency (the ceiling, nothing above);
   no hardware counters; no exception handler; HOME or X exits in order, never on its own on the PSP (in PPSSPP it exits after
   the results).
   Files next to the EBOOT (game data, personal use, never published): deck_ge.bin. Written: log.txt, shot_<mode>_r<round>.bmp. */
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
#ifdef GELIST   /* #30 step 3 (renderer_notes 11.13): the reference mode's list made by ge/gelist.c (raw GE words, one PRIM per df0
                   strip, the clear in the list), to check its pictures against this player's own in PPSSPP */
#include "../../ge/gelist.h"
static GxList g_gx; static GxHost g_gh; static GxFrame g_gf; static const uint8_t **g_gtd; static const uint32_t **g_gtp;
static float *g_vz;   /* raw depth files: the through vertices' z as stored */
static uint32_t gx_addr(void *ctx, const void *p) { (void)ctx; return (uint32_t)(uintptr_t)p; }
#endif

PSP_MODULE_INFO("MGS2GE", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#define VERSION "MGS2 GE replay v6"
#define DF0_ENTRY 0x1991d8u                 /* VU1 entry P04+df0 (guest address of its first instruction, as gsref names it) */
#define FX_ENTRY 0x19a9f0u                  /* VU1 entry P04+25f0: effect meshes with float world positions */
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
enum { C_OTHER, C_MIST, C_PART, C_BLUR, C_TRAIL, C_DF0, C_FX, NCLASS };
static const char *CLASSN[NCLASS] = { "other", "mist", "small sprites", "depth blur", "motion trail", "df0 meshes", "effect meshes" };
static uint8_t *g_class; static float *g_area;   /* per batch: class, GE pixels rasterised (before depth and alpha tests) */
static uint32_t g_first[NCLASS];           /* first batch of each class */
static uint32_t g_tv, g_xn, g_xv, g_xs, g_xsv, g_xi;   /* df0: through vertices; transform batches, vertices, strips, strip vertices, joined indices */
/* effect meshes (GERK_XFORMF), per batch: the runs of triangles the PS2 draws from the strip (first vertex, vertex count: one draw each,
   not joined: v4), then per fx way (1, 2) the clipped triangles as a triangle list in the PS2 clipper's order */
typedef struct { uint16_t *runs, *tri[3]; uint32_t nruns, ntri[3]; VcVertF *flat; } FxIdx;   /* flat: way 1's clipped triangles' vertices
                                                                                               in order (fx 3: no index list) */
static FxIdx **g_fx; static uint32_t g_fxn, g_fxv, g_fxs, g_fxsv, g_fxt[3], g_fxtv;
#define NIDENT 4096
static uint16_t g_ident[NIDENT];           /* 0, 1, 2 ...: df0 xf 4 draws each strip indexed through it */
static uint32_t g_xvmax;                   /* the most vertices in one df0 transform batch */
/* Sprite cuts (exact: the same pixels, u and v kept linear in x and y): asc puts the top-left corner first (colour and z stay the
   second vertex's, as the GE takes them), w / h cut each sprite into columns / rows at most that many pixels wide / tall. */
typedef struct { uint8_t asc, w, h; const char *name; } Cut;
enum { K_ASIS, K_ASC, K_W15, K_H16, K_ASC_H16, K_W64, K_W32, K_W16, NCUT };
static const Cut CUTS[NCUT] = { { 0, 0, 0, "as the PS2 draws it" }, { 1, 0, 0, "top-left corner first" }, { 0, 15, 0, "15-pixel columns" },
                                { 0, 0, 16, "16-row tiles" }, { 1, 0, 16, "top-left first, 16-row tiles" }, { 0, 64, 0, "64-pixel columns" },
                                { 0, 32, 0, "32-pixel columns" }, { 0, 16, 0, "16-pixel columns" } };
/* depth-blur variants that change the picture (measured only, to find the slow part) */
enum { BX_NONE, BX_NOTEX, BX_NEAREST, BX_COPY, BX_NOZ };
/* v6: what a second thread does on the CPU while the GE draws the frame (see "the CPU at work" below) */
enum { LD_NONE, LD_READ, LD_RW, LD_BUILD, LD_COPY, NLD };
typedef struct {
  const char *name;
  int8_t skip, skip2;                      /* classes left out, -1 none */
  uint8_t notex, vram, fb16, bx;           /* no textures; palette textures in VRAM; 16-bit frame buffers; a BX_ blur variant */
  int8_t blur, trail;                      /* the cut for the depth blur / motion trail; -1: the fastest exact one measured on the PSP (v3) */
  uint8_t xf;                              /* df0 meshes: 0 as the PS2 draws them (screen space), 1 hardware transform, 2 + fog, 3 + fog, joined,
                                              4 + fog, each strip drawn indexed (no joins) */
  uint8_t fx;                              /* effect meshes: 0 as the PS2 draws them, 1 hardware transform (the GE cuts what it can), 2 hardware
                                              transform, every triangle across the screen edge as the CPU's clipped polygon, 3 as 1 with the
                                              clipped triangles in one plain list per call (no index list) */
  int8_t part;                             /* PARTS: the class drawn REPS times */
  uint8_t load;                            /* LD_*: the CPU's work while the GE draws */
  uint8_t shots;                           /* bit r: the picture of round r + 1 is saved (its last frame) */
} Mode;
/* the parts alone: one class drawn REPS times over the frame drawn up to the depth blur (so the depth buffer is the real one) */
#define REPS 10
static const Mode PARTS[] = {
  { "effects transformed, GE cuts", -1, -1, 0, 1, 0, BX_NONE, -1, -1, 0, 1, C_FX, LD_NONE, 0 },
  { "effects GE cuts, no index list", -1, -1, 0, 1, 0, BX_NONE, -1, -1, 0, 3, C_FX, LD_NONE, 0 },
};
#define NPART (int)(sizeof PARTS / sizeof *PARTS)
/* the full frame: v5's last mode (its fastest exact form, 38.93 ms on the PSP), one change each */
enum { M_REF, M_UNIDX, M_NOBT, M_LREAD, M_LRW, M_LBUILD, M_LCOPY, NMODE };
static const Mode MODES[NMODE] = {
  { "reference (v5's mode 4)", -1, -1, 0, 1, 0, BX_NONE, -1, -1, 2, 1, -1, LD_NONE, 3 },
  { "effects, no index list", -1, -1, 0, 1, 0, BX_NONE, -1, -1, 2, 3, -1, LD_NONE, 3 },
  { "no depth blur, no trail (*)", C_BLUR, C_TRAIL, 0, 1, 0, BX_NONE, -1, -1, 2, 1, -1, LD_NONE, 3 },
  { "CPU: line reads", -1, -1, 0, 1, 0, BX_NONE, -1, -1, 2, 1, -1, LD_READ, 3 },
  { "CPU: reads + written lines", -1, -1, 0, 1, 0, BX_NONE, -1, -1, 2, 1, -1, LD_RW, 3 },
  { "CPU: next list built", -1, -1, 0, 1, 0, BX_NONE, -1, -1, 2, 1, -1, LD_BUILD, 3 },
  { "CPU: 64 KB copies", -1, -1, 0, 1, 0, BX_NONE, -1, -1, 2, 1, -1, LD_COPY, 3 },
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
/* GERK_XFORMF batch i: its index lists (ge/replay_fmt.h triangle classes: 1 drawn from the strip, 3 crossing the screen edge and the GE
   can draw it whole, 4 crossing, needs the PS2's polygon).  The PS2 draws the strip first, then its clipper's fans in triangle order;
   the lists keep that order (the effects blend). */
static int fx_index(uint32_t i)
{
  const GerXform *x = (const GerXform *)(D + B[i].first);
  uint32_t nv = x->nv, nfv = x->nfv;
  const uint8_t *cls = (const uint8_t *)((const VcVertF *)(x + 1) + nv + nfv); const uint16_t *fan = (const uint16_t *)(cls + ((nv + 3) & ~3u));
  if (!g_fx) { g_fx = calloc(H->nbatch, sizeof *g_fx); if (!g_fx) return -1; }
  FxIdx *f = calloc(1, sizeof *f); uint16_t *s = malloc((nv + 2) * 2); if (!f || !s) return -1;
  g_fx[i] = f; g_fxn++; g_fxv += nv; g_fxtv += nfv;
  uint32_t ns = 0;
  for (uint32_t k = 2; k < nv; ) {   /* runs of class 1 triangles k..e = vertices k-2..e */
    if (cls[k] != 1) { k++; continue; }
    uint32_t e = k; while (e + 1 < nv && cls[e + 1] == 1) e++;
    s[2 * ns] = (uint16_t)(k - 2); s[2 * ns + 1] = (uint16_t)(e - k + 3); ns++; g_fxsv += e - k + 3;
    k = e + 1;
  }
  f->runs = s; f->nruns = ns; g_fxs += ns;
  for (int w = 1; w <= 2; w++) {   /* way 1: class 3 whole (the GE's scissor cuts it), way 2: class 3 as its polygon; class 4 as its polygon */
    uint16_t *t = malloc((3 * nfv + 3 * nv + 4) * 2); if (!t) return -1;
    uint32_t nt = 0, base = nv, p = 0;
    for (uint32_t k = 2; k < nv; k++) {
      uint32_t cnt = 0, b0 = base;
      if (p < x->nfan && fan[2 * p] == k) { cnt = fan[2 * p + 1]; base += cnt; p++; }   /* the polygon the PS2 drew for triangle k */
      if (w == 1 && cls[k] == 3) { t[nt++] = (uint16_t)(k - 2); t[nt++] = (uint16_t)(k - 1); t[nt++] = (uint16_t)k; }
      else if (cls[k] >= 3) for (uint32_t j = 1; j + 1 < cnt; j++) { t[nt++] = (uint16_t)b0; t[nt++] = (uint16_t)(b0 + j); t[nt++] = (uint16_t)(b0 + j + 1); }
    }
    f->tri[w] = t; f->ntri[w] = nt; g_fxt[w] += nt;
  }
  const VcVertF *v = (const VcVertF *)(x + 1);   /* fx 3: way 1's list with its vertices copied in order (v5: df0's index lists cost the GE 4.6 ms) */
  VcVertF *fl = malloc((f->ntri[1] + 1) * sizeof *fl); if (!fl) return -1;
  for (uint32_t k = 0; k < f->ntri[1]; k++) fl[k] = v[f->tri[1][k]];
  f->flat = fl;
  return 0;
}
static int classify(void)
{
  g_class = malloc(H->nbatch); g_area = malloc(H->nbatch * sizeof *g_area);
  if (!g_class || !g_area) return -1;
  double ca[NCLASS] = { 0 }; uint32_t cn[NCLASS] = { 0 };
  for (uint32_t i = 0; i < H->nbatch; i++) {
    const GerBatch *b = &B[i]; int c = C_OTHER;
    if (b->kind == GERK_XFORMF) {   /* its area is its through batches' */
      g_area[i] = 0; g_class[i] = C_FX; if (!cn[C_FX]) g_first[C_FX] = i; cn[C_FX]++;
      if (fx_index(i) < 0) return -1;
      continue;
    }
    if (b->kind == GERK_XFORM) {   /* its area is its through batches' */
      const GerXform *x = (const GerXform *)(D + b->first);
      g_area[i] = 0; g_class[i] = C_DF0; if (!cn[C_DF0]) g_first[C_DF0] = i; cn[C_DF0]++;
      const uint16_t *runs = (const uint16_t *)((const VcVert *)(x + 1) + x->nv);
      g_xn++; g_xv += x->nv; g_xs += x->nst; g_xi += x->nidx; for (uint32_t k = 0; k < x->nst; k++) g_xsv += runs[2 * k + 1];
      if (x->nv > g_xvmax) g_xvmax = x->nv;
      continue;
    }
    g_area[i] = batch_area(b);
    if (b->entry == DF0_ENTRY) { c = C_DF0; g_tv += b->count; }
    else if (b->entry == FX_ENTRY) c = C_FX;
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
  out("effects: %u calls with hardware transform, %u vertices + %u clipped-polygon vertices; %u strip runs of %u vertices; clipped triangles drawn as %u (GE cuts) / %u (CPU cuts) triangles\n",
      (unsigned)g_fxn, (unsigned)g_fxv, (unsigned)g_fxtv, (unsigned)g_fxs, (unsigned)g_fxsv, (unsigned)g_fxt[1] / 3, (unsigned)g_fxt[2] / 3);
  for (int k = 0; k < NIDENT; k++) g_ident[k] = (uint16_t)k;
  if (g_xvmax > NIDENT) out("a df0 batch has %u vertices, more than %d: its indexed part draws it unindexed\n", (unsigned)g_xvmax, NIDENT);
  sceKernelDcacheWritebackAll();
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
static struct { int tex, ten, blend, bsig, at, afunc, zt, zfunc, zmask, shade, filt, tfunc, sc0, sc1, fog, clip; float proj[16], vp[6], ts[4], fogp[2]; uint32_t fogc; } S;   /* last state sent; -1 / NaN: unknown */
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
/* a call with hardware transform (GerXform): matrices, viewport, fog, texture scale; the batch's GS state is already set */
static void xf_setup(const GerXform *x, int fog, int tex)
{
  ScePspFMatrix4 w;
  w.x.x = x->world[0]; w.x.y = x->world[1]; w.x.z = x->world[2]; w.x.w = 0;
  w.y.x = x->world[3]; w.y.y = x->world[4]; w.y.z = x->world[5]; w.y.w = 0;
  w.z.x = x->world[6]; w.z.y = x->world[7]; w.z.z = x->world[8]; w.z.w = 0;
  w.w.x = x->world[9]; w.w.y = x->world[10]; w.w.z = x->world[11]; w.w.w = 1;
  sceGuSetMatrix(GU_MODEL, &w);
  if (memcmp(S.proj, x->proj, sizeof S.proj)) { sceGuSetMatrix(GU_PROJECTION, (const ScePspFMatrix4 *)x->proj); memcpy(S.proj, x->proj, sizeof S.proj); }
  float vp[6] = { x->vs[0], x->vs[1], x->vs[2], x->vo[0], x->vo[1], x->vo[2] };
  if (memcmp(S.vp, vp, sizeof vp)) { for (int k = 0; k < 6; k++) sceGuSendCommandf(66 + k, vp[k]); memcpy(S.vp, vp, sizeof vp); }   /* VIEWPORT X1 Y1 Z1 X2 Y2 Z2 */
  en(GU_FOG, fog, &S.fog);
  if (fog && (S.fogp[0] != x->fog1 || S.fogp[1] != x->fog2 || S.fogc != x->fogcol)) {
    sceGuSendCommandf(205, x->fog1); sceGuSendCommandf(206, x->fog2); sceGuSendCommandi(207, x->fogcol & 0xffffff);   /* FOG1, FOG2, FOGCOLOR */
    S.fogp[0] = x->fog1; S.fogp[1] = x->fog2; S.fogc = x->fogcol;
  }
  if (tex) {
    float ts[4] = { x->su, x->sv, x->tu, x->tv };
    if (memcmp(S.ts, ts, sizeof ts)) { sceGuTexScale(ts[0], ts[1]); sceGuTexOffset(ts[2], ts[3]); memcpy(S.ts, ts, sizeof ts); }
  }
}
/* a df0 call with hardware transform */
static void xf_draw(const GerBatch *b, int xf, int tex)
{
  const GerXform *x = (const GerXform *)(D + b->first);
  const VcVert *v = (const VcVert *)(x + 1); const uint16_t *runs = (const uint16_t *)(v + x->nv), *idx = runs + 2 * x->nst;
  xf_setup(x, xf >= 2 && (b->flags & GERF_FOG), tex);
  if (xf == 3) sceGuDrawArray(GU_TRIANGLE_STRIP, XFMT | GU_INDEX_16BIT, x->nidx, idx, v);
  else if (xf == 4 && x->nv <= NIDENT) for (uint32_t k = 0; k < x->nst; k++) sceGuDrawArray(GU_TRIANGLE_STRIP, XFMT | GU_INDEX_16BIT, runs[2 * k + 1], g_ident + runs[2 * k], v);
  else for (uint32_t k = 0; k < x->nst; k++) sceGuDrawArray(GU_TRIANGLE_STRIP, XFMT, runs[2 * k + 1], 0, v + runs[2 * k]);
}
/* an effect-mesh call with hardware transform (GERK_XFORMF), fx way 1 or 2: its strip triangles, then its clipped triangles */
#define XFMTF (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)
static void fx_draw(uint32_t i, int fx, int tex)
{
  const GerXform *x = (const GerXform *)(D + B[i].first); const FxIdx *f = g_fx[i];
  xf_setup(x, (B[i].flags & GERF_FOG) != 0, tex);
  const VcVertF *v = (const VcVertF *)(x + 1);
  for (uint32_t r = 0; r < f->nruns; r++) sceGuDrawArray(GU_TRIANGLE_STRIP, XFMTF, f->runs[2 * r + 1], 0, v + f->runs[2 * r]);
  if (fx == 3) { if (f->ntri[1]) sceGuDrawArray(GU_TRIANGLES, XFMTF, f->ntri[1], 0, f->flat); }
  else if (f->ntri[fx]) sceGuDrawArray(GU_TRIANGLES, XFMTF | GU_INDEX_16BIT, f->ntri[fx], f->tri[fx], v);
}
static void batch(uint32_t i, unsigned drawbuf, int pass)
{
  const GerBatch *b = &B[i]; const Mode *m = g_m; int cls = g_class[i], bx = cls == C_BLUR ? m->bx : BX_NONE;
  /* df0 and the effect meshes: their transform batches or their PS2 primitives */
  if (b->kind == GERK_XFORMF ? !m->fx : b->kind == GERK_XFORM ? !m->xf : (m->xf && b->entry == DF0_ENTRY) || (m->fx && b->entry == FX_ENTRY)) return;
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
  if (b->kind == GERK_XFORMF || S.clip == 1) en(GU_CLIP_PLANES, b->kind == GERK_XFORMF, &S.clip);   /* the GE's depth clamp for the effects
                                                                                                        (they test depth, never write it) */
  if (b->kind == GERK_XFORMF) { fx_draw(i, m->fx, tex); return; }
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
  if (m->xf || m->fx) view_identity();
  for (uint32_t i = i0; i < i1; i++) if (g_class[i] != m->skip && g_class[i] != m->skip2) one(i, drawbuf);
  en(GU_FOG, 0, &S.fog); if (S.clip == 1) en(GU_CLIP_PLANES, 0, &S.clip);
}
/* one class's batches, REPS times */
static void build_part(const Mode *m, int cls, unsigned drawbuf)
{
  st_reset(); g_m = m;
  if (m->xf || m->fx) view_identity();
  for (int r = 0; r < REPS; r++) { g_copied = 0; for (uint32_t i = g_first[cls]; i < H->nbatch; i++) if (g_class[i] == cls) one(i, drawbuf); }
  en(GU_FOG, 0, &S.fog); if (S.clip == 1) en(GU_CLIP_PLANES, 0, &S.clip);
}

/* ---- v6: the CPU at work while the GE draws (audit H2) ---- */
/* The v24 deck's busy step in the emulator's cache model (PspProf, prof 24001 24101, 2026-10-23), per ms: 1,181 instruction-cache
   fills, 593 data-cache fills by loads and 267 by stores, so 2,041 line reads from RAM; 600 dirty lines written back (stores also
   dirty lines they hit, and cache 0x18 makes lines without a read).  LD_READ: each access reads a line the 16 KB data cache does not
   hold, followed by a loop of plain instructions sized at the start (GE idle) for LD_FILLS accesses per ms; LD_RW: the same with
   LD_WBS of them stores (a store miss reads the line, which is written back when it leaves the cache).  The logic's busy step waits
   for RAM 43% of its time in the model; on the PSP's measured miss times (172 ns load, 317 ns store) LD_READ waits about 35%, LD_RW
   about 44%. */
#define LD_FILLS 2041                      /* line reads per ms */
#define LD_WBS 600                         /* lines written back per ms */
#define LDR_LINES 65536u                   /* the read region, 4 MB */
#define LDW_LINES 32768u                   /* the written region, 2 MB */
#define LDR_STEP 4099u                     /* lines from one access to the next: odd (every line in turn), 3 and 5 cache sets on */
#define LDW_STEP 2053u
#define LD_CHUNK 256                       /* line accesses between two counts */
static const char *LDN[NLD] = { "", "line reads", "reads + written lines", "next list built", "64 KB copies" };
static uint8_t *g_ldr, *g_ldw; static void *g_call2;   /* the regions; LD_BUILD's list */
static SceUID g_ld_th = -1; static int g_ld_ok;
static volatile int g_ld_kind, g_ld_run, g_ld_busy;
static volatile uint64_t g_ld_t0;          /* when the load started */
typedef struct { uint32_t units; uint64_t t; } LdRec;
static volatile LdRec g_ld_rec[2]; static volatile int g_ld_ri;   /* units done and the time after the last whole chunk: g_ld_rec[g_ld_ri] */
static int g_ld_pad[NLD];                  /* LD_READ / LD_RW: loop count after each access */
static const Mode *g_ld_mode; static unsigned g_ld_buf;   /* LD_BUILD: the mode and the draw buffer of the list it builds */
static uint32_t g_ldr_at, g_ldw_at, g_ld_acc, g_ldc_at; static volatile uint32_t g_ld_sink;
static void ld_lines(int rw, int pad)
{
  uint32_t r = g_ldr_at, w = g_ldw_at, acc = g_ld_acc, s = 0;
  for (int k = 0; k < LD_CHUNK; k++) {
    if (rw && (acc += LD_WBS) >= LD_FILLS) { acc -= LD_FILLS; *(volatile uint32_t *)(g_ldw + (w << 6)) = k; w = (w + LDW_STEP) & (LDW_LINES - 1); }
    else { s += *(volatile const uint32_t *)(g_ldr + (r << 6)); r = (r + LDR_STEP) & (LDR_LINES - 1); }
    for (int j = pad; j > 0; j--) __asm__ volatile("");
  }
  g_ldr_at = r; g_ldw_at = w; g_ld_acc = acc; g_ld_sink += s;
}
static uint32_t ld_chunk(int kind)         /* one chunk of work; its units (line accesses, lists, KB) */
{
  switch (kind) {
    case LD_READ: ld_lines(0, g_ld_pad[LD_READ]); return LD_CHUNK;
    case LD_RW: ld_lines(1, g_ld_pad[LD_RW]); return LD_CHUNK;
    case LD_BUILD: sceGuStart(GU_CALL, g_call2); build(g_ld_mode, g_ld_buf, 0, H->nbatch); sceGuFinish(); return 1;   /* the main thread
                                                 makes no GU call while this runs: it waits for the GE, then for this thread to stop */
    case LD_COPY: memcpy(g_ldw + ((g_ldc_at & 31) << 16), g_ldr + ((g_ldc_at & 63) << 16), 65536); g_ldc_at++; return 64;
  }
  return 0;
}
static int ld_thread(SceSize n, void *p)
{
  (void)n; (void)p;
  for (;;) {
    sceKernelSleepThread();
    int kind = g_ld_kind; uint32_t units = 0; uint64_t t = now();
    g_ld_t0 = t;
    while (g_ld_run) {
      units += ld_chunk(kind); t = now();
      int j = g_ld_ri ^ 1; g_ld_rec[j].units = units; g_ld_rec[j].t = t; g_ld_ri = j;   /* the main thread reads whole records only */
    }
    g_ld_busy = 0;
  }
  return 0;
}
/* the load runs only while this thread waits (its priority is lower) */
static void ld_start(int kind)
{
  g_ld_rec[0].units = g_ld_rec[1].units = 0; g_ld_kind = kind; g_ld_busy = 1; g_ld_run = 1;
  sceKernelWakeupThread(g_ld_th);
}
/* the rate since ld_start in units per ms, up to the last whole chunk (called as this thread resumes, so the load has not run on),
   then waits until the load stops */
static double ld_stop(void)
{
  g_ld_run = 0;
  int j = g_ld_ri; uint32_t units = g_ld_rec[j].units; uint64_t t = g_ld_rec[j].t, t0 = g_ld_t0;
  while (g_ld_busy) sceKernelDelayThread(200);
  return units && t > t0 ? units * 1000.0 / (double)(t - t0) : 0;
}
static double ld_alone(int kind, int ms) { ld_start(kind); sceKernelDelayThread(ms * 1000); return ld_stop(); }
/* LD_READ / LD_RW: the loop count that gives LD_FILLS accesses per ms with the GE idle (time per access about a + b x count) */
static void ld_calibrate(int kind)
{
  double want = 1.0 / LD_FILLS;            /* ms per access */
  g_ld_pad[kind] = 0; double r0 = ld_alone(kind, 30);
  g_ld_pad[kind] = 200; double r1 = ld_alone(kind, 30);
  double a = r0 > 0 ? 1 / r0 : 0, b = r0 > 0 && r1 > 0 ? (1 / r1 - a) / 200 : 0, r = r0;
  int pad = b > 0 ? (int)lround((want - a) / b) : 0;
  for (int it = 0; it < 5 && b > 0; it++) {
    if (pad < 0) pad = 0;
    g_ld_pad[kind] = pad; r = ld_alone(kind, 30);
    if (r <= 0 || fabs(r - LD_FILLS) < LD_FILLS * 0.005) break;
    pad += (int)lround((want - 1 / r) / b);
  }
  if (pad < 0) pad = 0;
  g_ld_pad[kind] = pad;
  out("CPU load calibration, %s: loop count %d (%.0f accesses per ms; %.0f with none)\n", LDN[kind], pad, r, r0);
}
/* each load alone, 5 x 100 ms with the GE idle: units per ms */
static void ld_rates(double *dst, const char *when)
{
  g_ld_mode = &MODES[M_LBUILD]; g_ld_buf = FB1;
  for (int k = LD_READ; k < NLD && !g_quit; k++) { double s = 0; for (int r = 0; r < 5; r++) s += ld_alone(k, 100); dst[k] = s / 5; }
  out("CPU work alone (%s), per ms: line reads %.0f, reads + written lines %.0f, %.2f ms per list built, copies %.0f KB\n", when,
      dst[LD_READ], dst[LD_RW], dst[LD_BUILD] > 0 ? 1 / dst[LD_BUILD] : 0, dst[LD_COPY]);
}

/* the finished picture in VRAM buffer buf as a 24-bit BMP next to the EBOOT (outside the timed part) */
static void save_shot(unsigned buf, int m, int r)
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
  char p[256]; snprintf(p, sizeof p, "%sshot_%d_r%d.bmp", g_dir, m + 1, r + 1);
  SceUID fd = sceIoOpen(p, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
  int ok = fd >= 0 && sceIoWrite(fd, img, sz) == (int)sz; if (fd >= 0) sceIoClose(fd);
  if (g_log) { fprintf(g_log, "  picture of mode %d round %d %s: %s\n", m + 1, r + 1, ok ? "saved" : "NOT saved", p); fflush(g_log); }
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
#ifdef ONLY_REF   /* with SMOKE: the reference mode alone (no parts, no CPU loads, no other modes) */
  int only_ref = smoke;
#else
  int only_ref = 0;
#endif
  pspDebugScreenInit();
  char lp[256]; snprintf(lp, sizeof lp, "%slog.txt", g_dir); g_log = fopen(lp, "w");
  out("%s\n", VERSION);
  int clk = scePowerSetClockFrequency(333, 333, 166);
  out("clock 333/166: %s (cpu %d MHz, bus %d MHz)\n", clk < 0 ? "refused" : "set", scePowerGetCpuClockFrequency(), scePowerGetBusClockFrequency());
  if (load() < 0) goto wait;
#ifdef GELIST   /* a file with the live renderer's raw depth (zscale 1: Z24 and the frame's range) is drawn by the list builder alone */
  if (H->zscale == 1.0f && !only_ref) { out("raw depth (the live renderer's): drawn by the list builder alone (ONLY_REF with SMOKE)\n"); goto wait; }
#else
  if (H->zscale == 1.0f) { out("raw depth (the live renderer's): needs the GELIST build\n"); goto wait; }
#endif
  if (classify() < 0 || vram_place() < 0) { out("out of memory for the batch tables\n"); goto wait; }
  if (!g_emu && !smoke) {   /* v2b: say what the test looks like before it starts (v2 looked like a failing GPU to its tester) */
    pspDebugScreenPrintf("\nThis test draws ONE frozen deck frame (about two minutes in all).\n"
                         "Part 1: the rain and spray effects drawn alone, two ways, then a few seconds of CPU work\n"
                         "alone; the screen shows only text and does not move.\n"
                         "Part 2: the whole frame in 7 modes. In the last 4 the CPU works on purpose while the\n"
                         "frame is drawn. The picture is squeezed to the PSP screen. That is expected.\n"
                         "It saves pictures (shot_<mode>_r<round>.bmp) next to log.txt.\n"
                         "It only draws pictures and uses its own memory: it cannot harm the PSP.\n"
                         "Results show at the end.\n\n"
                         "Press X to start.\n");
    for (;;) { SceCtrlData c; sceCtrlReadBufferPositive(&c, 1); if ((c.Buttons & PSP_CTRL_CROSS) || g_quit) break; sceKernelDelayThread(50000); }
    for (;;) { SceCtrlData c; sceCtrlReadBufferPositive(&c, 1); if (!(c.Buttons & PSP_CTRL_CROSS) || g_quit) break; sceKernelDelayThread(50000); }
  }
  void *call = memalign(64, 4 << 20), *dlist = memalign(64, 64 << 10);
  if (!call || !dlist) { out("out of memory for the lists\n"); goto wait; }
  if (!g_emu && !only_ref) {   /* v6: the CPU loads (their regions, the second list, the thread: 16 priority steps below the main thread) */
    g_ldr = memalign(64, LDR_LINES * 64); g_ldw = memalign(64, LDW_LINES * 64); g_call2 = memalign(64, 4 << 20);
    if (g_ldr && g_ldw && g_call2) { memset(g_ldr, 0, LDR_LINES * 64); memset(g_ldw, 0, LDW_LINES * 64); sceKernelDcacheWritebackAll();
      g_ld_th = sceKernelCreateThread("cpu_load", ld_thread, sceKernelGetThreadCurrentPriority() + 16, 0x10000, THREAD_ATTR_USER | THREAD_ATTR_VFPU, 0); }
    if (g_ld_th >= 0 && sceKernelStartThread(g_ld_th, 0, 0) >= 0) g_ld_ok = 1;
    else out("no CPU loads: out of memory or no thread (%08x); their modes are skipped\n", (unsigned)g_ld_th);
  }

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
#define EMU_MODE M_REF
#endif
#ifndef SMOKE_FRAMES
#define SMOKE_FRAMES 4                     /* FRAMES: PPSSPP pictures with the PSP run's history (the motion trail blends the previous frames) */
#endif
#ifndef SMOKE_ROUNDS
#define SMOKE_ROUNDS 1                     /* 2: PPSSPP's pictures of rounds 1 and 2 (v6) */
#endif
  int rounds = g_emu ? 1 : smoke ? SMOKE_ROUNDS : ROUNDS, frames = g_emu ? 3 : smoke ? SMOKE_FRAMES : FRAMES;

  /* 1. the parts alone (PSP only): the frame up to the depth blur (untimed), then one class drawn REPS times over it (timed) */
  static double p_sum[NPART], p_min[NPART]; static long p_n[NPART];
  for (int p = 0; p < NPART; p++) p_min[p] = 1e9;
  if (!g_emu && !only_ref) {
    unsigned disp = draw == FB0 ? FB1 : FB0;
    memset((void *)(0x44000000 + disp), 0, 512 * 272 * 4);
    pspDebugScreenSetColorMode(PSP_DISPLAY_PIXEL_FORMAT_8888); pspDebugScreenSetBase((u32 *)(0x44000000 + disp));
    for (int r = 0; r < rounds && !g_quit; r++)
      for (int p = 0; p < NPART && !g_quit; p++) {
        const Mode *m = &PARTS[p];
        pspDebugScreenSetXY(0, 0);
        pspDebugScreenPrintf("PART TEST %2d/%d round %d/%d: %-30s\nThe screen stays still during this part.", p + 1, NPART, r + 1, rounds, m->name);
        if (m->vram && !g_vram_ok) vram_upload();
        for (int k = 0; k < 4 && !g_quit; k++) {   /* k = 0 warms up */
          sceGuStart(GU_CALL, call); build(m, draw, 0, g_first[C_BLUR]); sceGuFinish();
          sceGuStart(GU_DIRECT, dlist);
          sceGuDrawBufferList(m->fb16 ? GU_PSM_5650 : GU_PSM_8888, (void *)draw, 512);
          sceGuSendCommandi(212, 0); sceGuSendCommandi(213, 271 << 10 | 479); sceGuDepthMask(GU_FALSE);
          sceGuClearColor(0); sceGuClearDepth(0); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
          sceGuCallList(call); sceGuFinish(); sceGuSync(GU_SYNC_FINISH, GU_SYNC_WAIT);
          sceGuStart(GU_CALL, call); build_part(m, m->part, draw); { long used = sceGuCheckList(); if (used > listmax) listmax = used; } sceGuFinish();
          uint64_t t1 = now();
          sceGuStart(GU_DIRECT, dlist); sceGuCallList(call); sceGuFinish(); sceGuSync(GU_SYNC_FINISH, GU_SYNC_WAIT);
          double t = (double)(now() - t1) / REPS;
          if (k) { p_sum[p] += t; p_n[p]++; if (t < p_min[p]) p_min[p] = t; }
        }
        if (r == rounds - 1 && g_log) { fprintf(g_log, "  part %-30s %7.2f ms per draw (min %7.2f)\n", m->name, p_sum[p] / p_n[p] / 1000, p_min[p] / 1000); fflush(g_log); }
      }
  }
  out("cuts (v3's fastest exact ones on the PSP): depth blur %s, motion trail %s\n", CUTS[g_best_blur].name, CUTS[g_best_trail].name);
  static double ld_al[NLD], ld_al2[NLD];   /* each load's rate alone (units per ms), before part 2 and after it */
  if (g_ld_ok && !g_quit) {
    pspDebugScreenSetXY(0, 3); pspDebugScreenPrintf("CPU work alone, timed (a few seconds; the screen stays still).\n");
    ld_calibrate(LD_READ); ld_calibrate(LD_RW); ld_rates(ld_al, "before part 2");
  }

  /* 2. the full frame per mode: list built first (CPU time), then the clear and the list run alone on the GE (GE time); in a mode
     with a load, the load thread works while the GE draws */
  static double ge_sum[NMODE], cpu_sum[NMODE], ge_min[NMODE], ld_sum[NMODE]; static long n[NMODE], ld_n[NMODE], ld_zero[NMODE];
  for (int m = 0; m < NMODE; m++) ge_min[m] = 1e9;
  int m0 = g_emu ? EMU_MODE : 0, m1 = g_emu ? EMU_MODE + 1 : only_ref ? M_REF + 1 : NMODE;
#ifdef GELIST
  {
    uint32_t nt = H->ntex ? H->ntex : 1, cw = (1 << 20) / 4, ca = 512 << 10;
    uint32_t *w = memalign(64, cw * 4); uint8_t *a = memalign(64, ca);
    g_gtd = calloc(nt, sizeof *g_gtd); g_gtp = calloc(nt, sizeof *g_gtp);
    if (!w || !a || !g_gtd || !g_gtp) { out("out of memory for the GE list\n"); goto wait; }
    gx_init(&g_gx, w, cw, a, ca); g_gx.ret = 1; g_gx.dig_on = 1;
    g_gh.addr = gx_addr; g_gh.fb[0] = FB0; g_gh.fb[1] = FB1; g_gh.zb = ZB;
    g_gf.b = B; g_gf.nb = H->nbatch; g_gf.v = (GerVert *)V; g_gf.nv = H->nvert; g_gf.d = D; g_gf.t = T; g_gf.nt = H->ntex;
    g_gf.tdata = g_gtd; g_gf.tpal = g_gtp; g_gf.zs = 1; g_gf.zo = 0;
    out("GE list builder (ge/gelist.c) draws mode 1\n");
    if (H->zscale == 1.0f) {   /* raw depth: the frame's own map; gx_frame maps the through z in place, so each frame starts from a copy */
      gx_depth_map(H->zlo, H->zhi, 1.0f, &g_gf.zs, &g_gf.zo);
      g_vz = malloc((H->nvert ? H->nvert : 1) * sizeof *g_vz); if (!g_vz) { out("out of memory for the depth copy\n"); goto wait; }
      for (uint32_t k = 0; k < H->nvert; k++) g_vz[k] = V[k].z;
      out("raw depth: Z24 %.0f..%.0f mapped, GE depth = Z24 x %g + %g\n", (double)H->zlo, (double)H->zhi, (double)g_gf.zs, (double)g_gf.zo);
    }
  }
#endif
  for (int r = 0; r < rounds && !g_quit; r++)
    for (int m = m0; m < m1 && !g_quit; m++)
      for (int f = 0; f < frames && !g_quit && (!MODES[m].load || g_ld_ok); f++) {
        const Mode *md = &MODES[m];
        if (md->vram && !g_vram_ok) vram_upload();
        /* v6: every mode starts from black frame buffers.  The motion trail blends the last frame into the next, and the depth blur
           samples the frame it draws on: in PPSSPP the same mode after other frames differed in 53,127 pixels by 1-3 levels after 39
           frames.  With the same start, two pictures of the same mode can differ only through the run itself (the GE is idle here) */
        if (f == 0) { memset((void *)(0x44000000 + FB0), 0, 512 * 272 * 4); memset((void *)(0x44000000 + FB1), 0, 512 * 272 * 4); }
        uint64_t t0 = now();
#ifdef GELIST
        int gxm = m == M_REF;
        if (gxm) {   /* the textures as texture() takes them, then the whole list (set-up and clear included) */
          for (uint32_t k = 0; k < H->ntex; k++) {
            int vr = md->vram && g_vtex[k];
            g_gtd[k] = vr ? (const uint8_t *)g_vtex[k] : D + T[k].data; g_gtp[k] = vr ? (const uint32_t *)g_vpal[k] : (const uint32_t *)(D + T[k].pal);
          }
          if (g_vz) for (uint32_t k = 0; k < H->nvert; k++) g_gf.v[k].z = g_vz[k];
          int gr = gx_frame(&g_gx, &g_gh, &g_gf, draw == FB0 ? 0 : 1);
          sceKernelDcacheWritebackAll();
          if (f == 0 || gr) {
            const GxCount *c = &g_gx.c;
            out("  gelist frame %d: %ld words, %ld aux bytes, %ld batches, %ld PRIMs (%ld df0 strips, %ld effect runs,\n", f, c->words, c->aux,
                c->batches, c->prims, c->xf_strips, c->fx_runs);   /* out() keeps 255 characters a call */
            out("    %ld effect triangles cut by the GE, %ld sprite cuts), %ld second passes, %ld texture sets, %ld vertex addresses,\n",
                c->fx_tris, c->cuts, c->passes2, c->tex_set, c->vaddr);
            out("    blends not mapped %ld, textures missing %ld, dropped %ld; digest %016llx\n", c->blend_other, c->tex_none, c->dropped,
                (unsigned long long)g_gx.dig);
          }
        } else
#endif
        { sceGuStart(GU_CALL, call); build(md, draw, 0, H->nbatch); { long used = sceGuCheckList(); if (used > listmax) listmax = used; } sceGuFinish(); }
        uint64_t t1 = now();
        /* the clear goes in the direct list: pspgu's sceGuClear takes its size from the current context, which in a GU_CALL list is empty */
        sceGuStart(GU_DIRECT, dlist);
#ifdef GELIST
        if (gxm) sceGuCallList(g_gx.w);
        else
#endif
        {
          sceGuDrawBufferList(md->fb16 ? GU_PSM_5650 : GU_PSM_8888, (void *)draw, 512);
          sceGuSendCommandi(212, 0); sceGuSendCommandi(213, 271 << 10 | 479); sceGuDepthMask(GU_FALSE);
          sceGuClearColor(0); sceGuClearDepth(0); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
          sceGuCallList(call);
        }
        sceGuFinish();
        if (md->load) { g_ld_mode = md; g_ld_buf = draw == FB0 ? FB1 : FB0; ld_start(md->load); }   /* the next frame's list, for LD_BUILD */
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WAIT);
        uint64_t t2 = now();
        double lr = md->load ? ld_stop() : 0;
        if (f > 0) {
          cpu_sum[m] += (double)(t1 - t0); ge_sum[m] += (double)(t2 - t1); n[m]++; if ((double)(t2 - t1) < ge_min[m]) ge_min[m] = (double)(t2 - t1);
          if (lr > 0) { ld_sum[m] += lr; ld_n[m]++; } else if (md->load) ld_zero[m]++;   /* LD_BUILD: no list finished while the GE drew */
        }
        if (!g_emu && f == frames - 1 && (md->shots >> r & 1)) save_shot(draw, m, r);   /* before the name goes on it */
        if (!g_emu) {   /* the mode's name on the finished picture (after the timing, before the swap) */
          pspDebugScreenSetBase((u32 *)(0x44000000 + draw)); pspDebugScreenSetColorMode(md->fb16 ? PSP_DISPLAY_PIXEL_FORMAT_565 : PSP_DISPLAY_PIXEL_FORMAT_8888);
          pspDebugScreenSetXY(0, 0); pspDebugScreenPrintf("FRAME TEST %d/%d round %d/%d: %-24s (odd pictures are expected)", m + 1, NMODE, r + 1, rounds, md->name);
        }
        draw = (unsigned)(uintptr_t)sceGuSwapBuffers();
        if (md->fb16) sceDisplaySetFrameBuf((void *)(0x44000000 + (draw == FB0 ? FB1 : FB0)), 512, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);
        if (r == 0 && m == EMU_MODE && f == 2) emu_screenshot();
        if (f == frames - 1 && g_log && !g_emu) {   /* progress in the log after each mode, so a run stopped early still reports what it measured */
          fprintf(g_log, "  (round %d) %-28s GE %7.2f  min %7.2f   build %6.2f", r + 1, md->name, ge_sum[m] / n[m] / 1000, ge_min[m] / 1000, cpu_sum[m] / n[m] / 1000);
          if (md->load && ld_n[m]) fprintf(g_log, "   CPU %.4g per ms (%.1f%% of alone)", ld_sum[m] / ld_n[m], ld_al[md->load] > 0 ? 100 * ld_sum[m] / ld_n[m] / ld_al[md->load] : 0);
          fputc('\n', g_log); fflush(g_log);
        }
      }
  if (g_quit) out("stopped early: the HOME menu's exit was chosen during the run (results below cover the tests finished before it)\n");
  {   /* the results on a cleared 8888 screen (a 16-bit mode may have left a 5650 picture) */
    unsigned disp = draw == FB0 ? FB1 : FB0;
    memset((void *)(0x44000000 + disp), 0, 512 * 272 * 4);
    sceDisplaySetFrameBuf((void *)(0x44000000 + disp), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
    pspDebugScreenSetColorMode(PSP_DISPLAY_PIXEL_FORMAT_8888); pspDebugScreenSetBase((u32 *)(0x44000000 + disp)); pspDebugScreenSetXY(0, 0);
  }
  if (g_ld_ok && !g_quit) ld_rates(ld_al2, "after part 2");
  if (!g_emu) {
    out("Each part alone, GE time per draw (ms; mean, min), %d x %d draws:\n", 3 * REPS, rounds);
    for (int p = 0; p < NPART; p++) if (p_n[p]) out("  %-30s %7.2f  min %7.2f\n", PARTS[p].name, p_sum[p] / p_n[p] / 1000, p_min[p] / 1000);
  }
  out("Full frame, GE time (ms; mean, min) and CPU time to build its list (ms), %d frames x %d rounds per mode; (*) changes the picture,\n"
      "measured only:\n", frames - 1, rounds);
  for (int m = 0; m < NMODE; m++) if (n[m]) out("  %-28s GE %7.2f  min %7.2f   build %6.2f\n", MODES[m].name, ge_sum[m] / n[m] / 1000, ge_min[m] / 1000, cpu_sum[m] / n[m] / 1000);
  if (g_ld_ok) {
    out("CPU work, per ms: alone before / after part 2; while the GE draws, and its share of the rate alone before:\n");
    for (int m = 0; m < NMODE; m++) if (MODES[m].load && (ld_n[m] || ld_zero[m])) {
      int k = MODES[m].load; double d = ld_n[m] ? ld_sum[m] / ld_n[m] : 0, a = ld_al[k], a2 = ld_al2[k];
      if (k == LD_BUILD) out("  %-22s %.2f / %.2f ms per list; while the GE draws %.2f ms (%.1f%% of the speed)\n", LDN[k], a > 0 ? 1 / a : 0, a2 > 0 ? 1 / a2 : 0, d > 0 ? 1 / d : 0, a > 0 ? 100 * d / a : 0);
      else out("  %-22s %.0f / %.0f %s; while the GE draws %.0f (%.1f%%)\n", LDN[k], a, a2, k == LD_COPY ? "KB" : "accesses", d, a > 0 ? 100 * d / a : 0);
      if (ld_zero[m]) out("  (%s: %ld frames finished no work unit while the GE drew)\n", LDN[k], ld_zero[m]);
    }
  }
  out("largest list %ld KB (of 4096); blends with no GE mapping: %ld\n", listmax / 1024, g_unmapped_blend);
wait:
  if (g_ld_th >= 0) { sceKernelTerminateDeleteThread(g_ld_th); g_ld_th = -1; }   /* asleep: every load stops before its mode ends */
  if (g_emu || smoke) { if (g_log) fclose(g_log); sceKernelExitGame(); return 0; }
  out("Press X (or HOME) to exit.\n");
  for (;;) { SceCtrlData c; sceCtrlReadBufferPositive(&c, 1); if ((c.Buttons & PSP_CTRL_CROSS) || g_quit) break; sceKernelDelayThread(50000); }
  sceGuTerm(); if (g_log) fclose(g_log);
  sceKernelExitGame();
  return 0;
}
