/* xl_psp.c: the live packet translator in the PSP harness, measured (renderer_notes 11.12 "The plan for the PSP", step 1 on the PSP;
   task #45).  boot.c and runtime/ee_io.h are built with -DMGS2_XLIVE: io_dma_start hands each VIF1 / GIF transfer to xlive_chk_dma before
   its exact walk.  While g_opt has OPT_XL, the second walk (ge/xlive.c, spans) feeds the translator (ge/xlate.c: VU memory with lazy
   UNPACKs, GS registers, VU1 calls), its GS front end (ge/gsfront.c) and the texture source map with content keys (ge/gsup.c), all timed
   as the region HOST_XL.  Nothing is drawn yet and no texture is converted.  It reads guest RAM and the scratchpad and writes only its
   own memory, so every hash stays as it is.  OPT_XL off: nothing; back on: the translator starts again knowing nothing (xl_reset), as
   the x86 check does at a window's start (the map keeps what it knew).  xl_psp_report: totals for log.txt.  main_psp.c built with
   -DMGS2_XL times the region and writes it to steps.csv (xl_us).
   The one walk (xl3, renderer_notes 11.12, task #47; MGS2_XL_ONE=1 in mgs2.cfg, the default): a VIF1 transfer that runtime/ee_io.h's
   vif1_chain_fast walks is not walked a second time: that walk hands the translator the same runs of words and calls xlive_one_end at the
   transfer's end, so the translator's time is inside the region HOST_GS; GIF transfers keep the second walk (HOST_XL).  MGS2_XL_ONE=0:
   the second walk for every transfer, as xl2.
   Shown frames only (xl5, renderer_notes 11.12 "Shown frames only", task #47; MGS2_XL_SKIP=1 in mgs2.cfg, the default): the transfers
   after a vblank whose draw-pipeline calls were all reduced (boot.c g_dg_calls, which main_psp.c's host_vb_end hands to xl_psp_vblank
   before it clears them) are not handed to the translator, as the x86 check MGS2_XLIVE_SKIP does; it keeps its state across them.
   MGS2_XL_SKIP=0: every transfer.
   The live picture (gl1, renderer_notes 11.13 "3e", task #56; built with -DMGS2_GL, main_psp.c too): while g_opt has OPT_XL and OPT_GL,
   the transfers of each shown step (one after a step with full draw-pipeline calls) become a frame of GE batches (ge/gelive.c: the front
   end's primitives and the converters' calls; textures from ge/texcache.c in a pool of their own, so the cache never takes the game's
   heap); at the next vblank hook the frame becomes a GE list (ge/gelist.c; raw depth, mapped by the frame's range) that the GE draws into
   one of two 8888 frame buffers while the game runs on, and a frame the GE has finished is shown (sceDisplaySetFrameBuf, at the next
   display vblank).  One set of frame memory (the volatile block's last 2 MB): before the next frame begins the CPU waits for the GE
   (HOST_GW), and the texture cache keeps what this and the last frame used.  Times: HOST_GL (the converters' calls, the frame's end, the
   list, the hand-over, the display), HOST_GW (waits); the rest of the frame builder (the GS primitives) inside the translator's region.
   It reads guest RAM and writes only its own memory and VRAM, so every hash stays as it is.  The harness's text is on a third buffer
   (main_psp.c), shown again at the end (xl_psp_gl_end).  Every wait for the GE gives up after 2 s (gl_sync, 2026-10-27, for the first
   runs on the PSP): a "picture: STOPPED" line, the text on the screen again, no more frames, and the run goes on.  mgs2.cfg: MGS2_GL_DIG=<from>:<to>: each frame whose last vblank lies there
   gets its list digest and its frame digest (ge/gelive.h gl_digest) in log.txt, for the x86 check (boot/xlive_chk.c MGS2_XLIVE_GX: the
   same frames with the same sizes); MGS2_GL_SHOT=<vb>[,<vb>...]: at those vblank hooks the last finished frame is saved as
   shot_<its last vblank>.bmp, outside the step's time; MGS2_GL_TEXKB: the texture pool (KB, default GL_PSP_TEXKB). */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "host_time.h"
#include "../ge/xlive.h"
#include "../ge/xlate.h"
#include "../ge/gsfront.h"
#include "../ge/gsup.h"
#ifdef MGS2_GL
#include <string.h>
#include <malloc.h>
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include "../ge/texcache.h"
#include "../ge/gelive.h"
#include "../ge/gelist.h"
#endif

#define XL_GSZ (32u << 20)                /* guest RAM (runtime GUEST_SIZE) */
static XlvMem M; static GuMap *map;
static int state;                        /* 0: not started, 1: on, 2: off since it was on, -1: no memory for the map */
static long ncall, nprim, nrestart, ntransfer, none;
extern int g_xl_one;                     /* runtime/ee_io.h: the exact walk feeds the translator */
char *mgs2_getenv(const char *k);        /* main_psp.c: mgs2.cfg's settings (newlib's getenv does not see them) */
static int one_mode = 1, skip_mode = 1, cfg_read;   /* MGS2_XL_ONE, MGS2_XL_SKIP */
static int skip_now; static long sk_vb, sk_tr;      /* shown frames only: this step's transfers are skipped; vblanks, transfers skipped */
static void cfg(void)
{
  const char *e;
  if (cfg_read) return;
  cfg_read = 1;
  if ((e = mgs2_getenv("MGS2_XL_ONE"))) one_mode = atoi(e) != 0;     /* mgs2.cfg, through main_psp.c */
  if ((e = mgs2_getenv("MGS2_XL_SKIP"))) skip_mode = atoi(e) != 0;
}
#ifdef MGS2_GL
/* ---- the live picture ---- */
#define GL_FB0 0x000000u                 /* VRAM offsets: two 8888 frame buffers and the 16-bit depth buffer, 512 pixels a row (as
                                            psp/ge_replay); the harness's text at GL_TXT (main_psp.c) */
#define GL_FB1 0x088000u
#define GL_ZB 0x110000u
#define GL_TXT 0x154000u
void *host_vol_tail(size_t n);           /* main_psp.c: n bytes from the end of the volatile block, or 0 */
const char *host_dir(void);              /* main_psp.c: the EBOOT's folder */
static TkCache *tk; static GlFrame glf; static GxList gxl; static GxHost gxh; static GxFrame gxf;
static uint32_t gl_dl[256] __attribute__((aligned(64)));   /* the direct list (sceGuStart writes it through the uncached view) */
static int gl_ok;                        /* 1: on; -1: no memory or no set-up, so no picture; 2: ended (xl_psp_gl_end); 3: stopped, the
                                            GE did not finish (gl_sync) */
#define GL_GE_LIMIT 2000000u             /* us: the GE draws a frame in 10-45 ms (MGS2GE v4-v6 on the PSP) */
static int gl_now, gl_full;              /* a frame is being made; the step that ended had full draw-pipeline calls */
static int gl_ge, gl_ge_buf; static long gl_ge_vb;   /* the GE has a frame not shown yet: its buffer and its last vblank */
static int gl_shown_buf = -1; static long gl_shown_vb = -1, gl_first_vb = -1;
static long gl_built, gl_shown, gl_listfail, gl_shots;   /* frames made (the one drawn into buffer gl_built & 1), shown, saved */
static long gl_stop_vb = -2; static int gl_stop_r;        /* gl_sync's stop: the last list's frame (-1: the set-up list), the GE's state */
static long dig_from = -1, dig_to = -1, dig_n; static uint64_t dig_all = 0xcbf29ce484222325ull;
static long shot_vb[16]; static int shot_n;
static uint32_t gl_texkb = GL_PSP_TEXKB, gl_vol_n; static uint8_t *gl_vol; static void *gl_pool;
static long gx_words_max, gx_aux_max, gx_both_max, gx_drop; static uint64_t gx_words_sum;
static uint32_t gl_addr(void *ctx, const void *p) { (void)ctx; return (uint32_t)(uintptr_t)p; }
static void gl_line(FILE *f, const GlCount *n, int64_t db, int64_t tb, const char *what)   /* as boot/xlive_chk.c's gb_line; db, tb: n's data
                                                                                     and texture bytes (64-bit sums for the totals) */
{
  fprintf(f, "%s %ld batches (%ld transform), %ld vertices, %lld KB data, %ld textures (%lld KB, %ld new 16-bit); primitives %ld (%ld "
          "textured; left out: %ld uncertain, %ld path 1; state drawn otherwise %ld/%ld/%ld/%ld); calls %ld: converted %ld, nothing drawn %ld, "
          "bad %ld, no room %ld; other calls %ld; texture lookups %ld (memo %ld): frame buffers %ld, untextured: render targets %ld, not "
          "known %ld, refused %ld, table full %ld; dropped %ld", what, n->batches, n->xbatches, n->verts, (long long)(db >> 10), n->ntex,
          (long long)(tb >> 10), n->tex16, n->prims, n->prims_tex, n->uncertain, n->path1, n->unrep[0], n->unrep[1], n->unrep[2], n->unrep[3],
          n->calls, n->conv, n->conv_empty, n->conv_bad, n->conv_full, n->other, n->tex_asks, n->tex_memo, n->tex_fb, n->tex_rt,
          n->tex_lost, n->tex_unsup, n->tex_full, n->dropped);
  fprintf(f, "; P06 and P07 calls %ld, passes 2 and 3 drawn %ld, not drawn %ld", n->lit_calls, n->lit_passes, n->lit_nopass);
}

/* the CPU waits for the GE to finish its lists: 0 once it has.  A GE that has not finished in GL_GE_LIMIT stops the picture (gl_ok 3: no
   more lists, the harness's text on the screen again; the run goes on): -1.  (sceGeDrawSync(0) would wait for ever.) */
static int gl_sync(void)
{
  uint64_t t0; int r;
  if (!sceGeDrawSync(1)) return 0;
  t0 = (uint64_t)sceKernelGetSystemTimeWide();
  while ((r = sceGeDrawSync(1)))
    if ((uint64_t)sceKernelGetSystemTimeWide() - t0 < GL_GE_LIMIT) sceKernelDelayThread(100);
    else {
      gl_stop_vb = gl_ge ? gl_ge_vb : -1; gl_stop_r = r; gl_ok = 3; gl_now = gl_ge = 0;
      sceDisplaySetFrameBuf((void *)(0x44000000u + GL_TXT), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
      fprintf(stderr, "picture: STOPPED: the GE has not finished %s%ld in %u ms (sceGeDrawSync(1) %08x): no more frames, the run goes on\n",
              gl_stop_vb < 0 ? "its set-up list" : "the list of the frame of vb ", gl_stop_vb < 0 ? 0 : gl_stop_vb, GL_GE_LIMIT / 1000, (unsigned)r);
      return -1;
    }
  return 0;
}
/* with the translator's start (its first transfer): the texture pool, the frame memory, the GE */
static void gl_setup(void)
{
  const char *e; struct mallinfo mi;
  gl_ok = -1;
  if ((e = mgs2_getenv("MGS2_GL_TEXKB")) && atoi(e) >= 256) gl_texkb = (uint32_t)atoi(e);
  if ((e = mgs2_getenv("MGS2_GL_DIG")) && sscanf(e, "%ld:%ld", &dig_from, &dig_to) != 2) dig_from = dig_to = -1;
  for (e = mgs2_getenv("MGS2_GL_SHOT"); e && *e && shot_n < 16; ) { shot_vb[shot_n++] = strtol(e, (char **)&e, 10); if (*e == ',') e++; else break; }
  gl_vol_n = GL_PSP_ARENA + GL_PSP_LIST;
  gl_vol = host_vol_tail(gl_vol_n); gl_pool = memalign(64, gl_texkb << 10);
  if (!gl_vol || !gl_pool) { fprintf(stderr, "picture: no memory (frame memory %s, texture pool %s): no picture\n", gl_vol ? "ok" : "none",
                                     gl_pool ? "ok" : "none"); free(gl_pool); gl_pool = 0; return; }
  memset(gl_vol, 0, gl_vol_n); memset(gl_pool, 0, gl_texkb << 10);
  gu_zero_empty(map, 1);                 /* the map runs from the boot's first transfer (as the x86 check from vb 0) */
  if (!(tk = tk_new(map, (gl_texkb << 10) / 8 * 7)) || tk_pool(tk, gl_pool, gl_texkb << 10) || gl_init(&glf, tk, map, 0, GL_PSP_MAXTEX)
      || gb_arena(&glf.o, gl_vol, GL_PSP_ARENA, GL_PSP_NB, GL_PSP_NV)) { fprintf(stderr, "picture: set-up failed: no picture\n"); return; }
  gx_init(&gxl, (uint32_t *)(gl_vol + GL_PSP_ARENA), GL_PSP_LIST / 4, gl_vol + GL_PSP_ARENA, GL_PSP_LIST); gxl.ret = 1;   /* one block */
  gxh.addr = gl_addr; gxh.ctx = 0; gxh.fb[0] = GL_FB0; gxh.fb[1] = GL_FB1; gxh.zb = GL_ZB;
  memset((void *)(0x44000000u + GL_FB0), 0, GL_ZB - GL_FB0);   /* both frame buffers black, through the uncached view */
  sceGuInit();                           /* as psp/ge_replay, without sceGuDisplay: the harness shows the frames itself */
  sceGuStart(GU_DIRECT, gl_dl);
  sceGuDrawBuffer(GU_PSM_8888, (void *)GL_FB0, 512); sceGuDispBuffer(480, 272, (void *)GL_FB1, 512); sceGuDepthBuffer((void *)GL_ZB, 512);
  sceGuOffset(2048 - 240, 2048 - 136); sceGuViewport(2048, 2048, 480, 272); sceGuDepthRange(65535, 0);
  sceGuScissor(0, 0, 480, 272); sceGuEnable(GU_SCISSOR_TEST); sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_CLIP_PLANES);
  sceGuFinish();
  if (gl_sync()) return;
  gl_ok = 1; mi = mallinfo();
  fprintf(stderr, "picture: on: frame memory %u KB at %08x (arena %u KB: %u batches, %u vertices; list and aux %u KB), texture pool "
          "%u KB at %08x (the cache's budget %u KB); VRAM frame buffers %06x / %06x, depth %06x, text %06x; malloc in use %u KB of %u KB\n",
          (unsigned)gl_vol_n >> 10, (unsigned)(uintptr_t)gl_vol, GL_PSP_ARENA >> 10, GL_PSP_NB, GL_PSP_NV, GL_PSP_LIST >> 10,
          (unsigned)gl_texkb, (unsigned)(uintptr_t)gl_pool, (unsigned)gl_texkb / 8 * 7, GL_FB0, GL_FB1, GL_ZB, GL_TXT, (unsigned)mi.uordblks >> 10,
          (unsigned)mi.arena >> 10);
  if (dig_from >= 0) fprintf(stderr, "picture: digests of the frames whose last vblank is in %ld..%ld\n", dig_from, dig_to);
}
/* the GE has drawn the frame: it is on the screen from the next display vblank */
static void gl_show(void)
{
  sceDisplaySetFrameBuf((void *)(0x44000000u + (gl_ge_buf ? GL_FB1 : GL_FB0)), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
  gl_ge = 0; gl_shown++; gl_shown_buf = gl_ge_buf; gl_shown_vb = gl_ge_vb;
}
static void gl_wait(void) { int r; HOST_ENTER(HOST_GW); r = gl_sync(); HOST_LEAVE(HOST_GW); if (!r) gl_show(); }
/* the frame that ended (its last vblank fvb) -> its GE list, drawn into the buffer that is not on the screen */
static void gl_list(long fvb)
{
  int d, dig = fvb >= dig_from && fvb <= dig_to;
  if (gl_ge) { gl_wait(); if (gl_ok != 1) return; }   /* (a frame begins only after the GE has drawn the one before, so not expected) */
  d = (int)(gl_built++ & 1);
  gxf.b = glf.o.b; gxf.nb = (uint32_t)glf.o.nb; gxf.v = glf.o.v; gxf.nv = (uint32_t)glf.o.nv; gxf.d = glf.o.d;
  gxf.t = glf.o.t; gxf.nt = (uint32_t)glf.o.nt; gxf.tdata = glf.tdata; gxf.tpal = glf.tpal;
  gx_depth_map(glf.map_lo, glf.map_hi, 1.0f, &gxf.zs, &gxf.zo);
  gxl.dig_on = dig;
  if (gx_frame(&gxl, &gxh, &gxf, d) && ++gl_listfail <= 50)
    fprintf(stderr, "picture: vb %ld: the list did not fit: %ld batches left out (%ld words, %ld aux bytes)\n", fvb, gxl.c.dropped, gxl.c.words, gxl.c.aux);
  gx_words_sum += gxl.c.words; gx_drop += gxl.c.dropped;
  if (gxl.c.words > gx_words_max) gx_words_max = gxl.c.words;
  if (gxl.c.aux > gx_aux_max) gx_aux_max = gxl.c.aux;
  if (4 * gxl.c.words + gxl.c.aux > gx_both_max) gx_both_max = 4 * gxl.c.words + gxl.c.aux;
  if (dig) {
    uint64_t g = gl_digest(&glf, &gxl); dig_all = (dig_all ^ g) * 0x100000001b3ull; dig_n++;
    fprintf(stderr, "picture: vb %ld buffer %d list %016llx frame %016llx: %ld words, %ld aux bytes, %ld batches, %ld PRIMs, %d textures, "
            "%ld dropped\n", fvb, d, (unsigned long long)gxl.dig, (unsigned long long)g, gxl.c.words, gxl.c.aux, gxl.c.batches, gxl.c.prims,
            glf.o.nt, gxl.c.dropped);
  }
  sceKernelDcacheWritebackAll();         /* the GE reads the list, the frame and the textures from RAM */
  sceGuStart(GU_DIRECT, gl_dl); sceGuCallList(gxl.w); sceGuFinish();
  gl_ge = 1; gl_ge_buf = d; gl_ge_vb = fvb;
  if (!(g_opt & OPT_GL_ASYNC)) gl_wait();
}
/* the last frame shown, as a 24-bit BMP next to the EBOOT */
static void gl_shot(void)
{
  static uint8_t rows[16 * 1440]; uint8_t h[54]; uint32_t sz = 54 + 480 * 272 * 3, isz = 480 * 272 * 3; int32_t w = 480, ht = 272;
  char p[256]; SceUID fd; int ok;
  memset(h, 0, sizeof h); h[0] = 'B'; h[1] = 'M'; memcpy(h + 2, &sz, 4); h[10] = 54; h[14] = 40; memcpy(h + 18, &w, 4); memcpy(h + 22, &ht, 4);
  h[26] = 1; h[28] = 24; memcpy(h + 34, &isz, 4);
  snprintf(p, sizeof p, "%sshot_%ld.bmp", host_dir(), gl_shown_vb);
  if ((fd = sceIoOpen(p, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777)) < 0) { fprintf(stderr, "picture: cannot write %s\n", p); return; }
  ok = sceIoWrite(fd, h, 54) == 54;
  const volatile uint32_t *src = (const volatile uint32_t *)(0x44000000u + (gl_shown_buf ? GL_FB1 : GL_FB0));
  for (int y0 = 271; y0 >= 0 && ok; y0 -= 16) {   /* bottom-up rows of B, G, R; the GE's 8888 has R in the low byte */
    int n = 0;
    for (int y = y0; y > y0 - 16 && y >= 0; y--, n++)
      for (int x = 0; x < 480; x++) { uint32_t c = src[y * 512 + x]; uint8_t *o = rows + n * 1440 + 3 * x; o[0] = (uint8_t)(c >> 16); o[1] = (uint8_t)(c >> 8); o[2] = (uint8_t)c; }
    ok = sceIoWrite(fd, rows, n * 1440) == n * 1440;
  }
  sceIoClose(fd); gl_shots += ok;
  fprintf(stderr, "picture: the frame of vb %ld %s %s\n", gl_shown_vb, ok ? "saved as" : "NOT saved:", p);
}
/* psp/main_psp.c host_vb_end, after the next step's timer started: the picture's work at the hook that starts step vb counts in step vb */
void xl_psp_frame(long vb)
{
  if (gl_ok != 1) return;
  HOST_ENTER(HOST_GL);
  if (gl_now) { gl_now = 0; gl_end(&glf); gl_list(vb - 1); }
  if (gl_ge && sceGeDrawSync(1) == 0) gl_show();
  if (gl_ok == 1 && gl_full && state == 1 && (g_opt & (OPT_XL | OPT_GL)) == (OPT_XL | OPT_GL)) {
    if (gl_ge) gl_wait();                /* the frame memory and the cache's last textures are the GE's until it has drawn */
    if (gl_ok == 1) {                    /* (gl_sync's stop: no more frames) */
      glf.h.cut = (g_opt & OPT_GL_CUT) != 0;
      gl_begin(&glf); gl_now = 1;
      if (gl_first_vb < 0) { gl_first_vb = vb; fprintf(stderr, "picture: the first frame begins at vb %ld\n", vb); }
    }
  }
  tk_frame(tk);
  HOST_LEAVE(HOST_GL);
}
/* psp/main_psp.c finish(), before the summary: no more frames, the GE idle, the harness's text on the screen again */
void xl_psp_gl_end(void)
{
  if (gl_ok != 1 || gl_sync()) return;   /* (gl_sync's stop puts the text on the screen itself) */
  gl_ok = 2; gl_now = gl_ge = 0;
  sceDisplaySetFrameBuf((void *)(0x44000000u + GL_TXT), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
}
#endif
/* psp/main_psp.c host_vb_end, before it clears them: the draw-pipeline calls of the step that ended, full and reduced; its transfers come
   in the step that starts now */
void xl_psp_vblank(uint32_t full, uint32_t reduced)
{
  cfg();
  skip_now = skip_mode && !full && reduced;
  if (skip_now && state == 1) sk_vb++;
#ifdef MGS2_GL
  gl_full = full != 0;
#endif
}
#ifdef MGS2_GL
/* psp/main_psp.c host_vb_end, outside the step's time: MGS2_GL_SHOT's saves at the hook that starts step vb */
void xl_psp_gl_vblank(long vb)
{
  if (gl_ok != 1) return;
  for (int k = 0; k < shot_n; k++) if (shot_vb[k] == vb) {
    if (gl_ge) { if (gl_sync()) return; gl_show(); }
    if (gl_shown_buf >= 0) gl_shot(); else fprintf(stderr, "picture: no frame shown yet at vb %ld\n", vb);
  }
}
/* psp/v26me4_exc.c built with -DMGS2_GL_EXC (psp/gl1.sh), in the exception: the frame buffer on the display, through the uncached view
   (the crash record goes over the game's picture there), or 0 while the harness's text is on it */
uint32_t xl_psp_gl_screen(void) { return gl_ok == 1 && gl_shown_buf >= 0 ? 0x44000000u + (gl_shown_buf ? GL_FB1 : GL_FB0) : 0; }
#endif
/* the translator's quadword, word, call and A+D counters are 32-bit longs on the PSP (a route passes 2^31 words) and xl_reset clears
   its own: summed here a transfer at a time, from their low 32 bits */
static uint64_t t_qw, t_words, t_calls, t_ad; static uint32_t l_qw, l_words, l_calls, l_ad;
static void sum(void)
{
  const XlStats *S = xl_stats(); uint32_t q = (uint32_t)M.qw, w = (uint32_t)S->words, c = (uint32_t)S->calls, a = (uint32_t)S->ad_kicks;
  t_qw += q - l_qw; l_qw = q; t_words += w - l_words; l_words = w; t_calls += c - l_calls; l_calls = c; t_ad += a - l_ad; l_ad = a;
}
static const uint8_t *ram(void *ctx, uint32_t ee, uint32_t n) { (void)ctx; ee &= XL_GSZ - 1; return ee + n <= XL_GSZ ? M.mem + ee : 0; }
#ifndef MGS2_GL
static void on_call(void *ctx, const XlCall *c) { (void)ctx; (void)c; ncall++; }
static void on_prim(void *ctx, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n) { (void)ctx; (void)g; (void)d; (void)v; (void)n; nprim++; }
#else
static void on_call(void *ctx, const XlCall *c)   /* a converter's call is the picture's work (HOST_GL); the rest only counts */
{
  (void)ctx; ncall++;
  if (!gl_now) return;
  if (gl_conv_entry(c->entry) && !c->cont) { HOST_ENTER(HOST_GL); gl_call(&glf, c); HOST_LEAVE(HOST_GL); }
  else gl_call(&glf, c);
}
static void on_pass(void *ctx, const XlCall *c, int pass)   /* P06's passes 2 and 3 (HOST_GL) */
{
  (void)ctx;
  if (gl_now) { HOST_ENTER(HOST_GL); gl_pass(&glf, c, pass); HOST_LEAVE(HOST_GL); }
}
static void on_prim(void *ctx, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n)
{
  (void)ctx; nprim++;
  if (gl_now) gl_prim(&glf, g, d, v, n);
}
#endif

void xlive_chk_dma(const uint8_t *mem, const uint8_t *spr, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int one)
{
  if (!(g_opt & OPT_XL)) { if (state == 1) state = 2; return; }
  { static int ud = 1; int m = (g_opt & OPT_XL_VMUD) != 0; if (m != ud) { xl_vm_mode(m); ud = m; } }   /* (xlate.c's default: 1) */
  { static int v4 = 1; int m = (g_opt & OPT_XL_V48) != 0; if (m != v4) { xl_ud_v48(m); v4 = m; } }   /* (xlate.c's default: 1) */
  if (map) { static int am = 1, cm = 1; int a = (g_opt & OPT_GU_MEMO) != 0, c = (g_opt & OPT_GU_CLUT) != 0;   /* (gsup2.c's defaults: 1) */
    if (a != am) { gu_area_memo(map, a); am = a; }
    if (c != cm) { gu_clut_memo(map, c); cm = c; } }
#ifdef MGS2_GL
  if (tk) { static int tb = 1; int m = (g_opt & OPT_TK_BLK) != 0; if (m != tb) { tk_blk(tk, m); tb = m; } }   /* (texcache.c's default: 1) */
  if (tk) { static int tf = 1; int m = (g_opt & OPT_TK_FLAGS) != 0; if (m != tf) { tk_flags(tk, m); tf = m; } }   /* (texcache.c's default: 1) */
#endif
  if (state != 1) {
    if (state < 0) return;
    if (!state) {
      M.mem = mem; M.mem_size = XL_GSZ; M.spr = spr; M.seen = 0; M.fast = 1;
      if (!(map = gu_new(ram, 0))) { state = -1; fprintf(stderr, "translator: no memory for the texture map, off\n"); return; }
      gu_keys(map, 1);
      xl_on_call(on_call, 0); xl_on_prim(on_prim, 0); xl_uploads(map);
      cfg();
#ifdef MGS2_GL
      xl_on_pass(on_pass, 0);
      gl_setup();
#endif
    } else { nrestart++; sum();
#ifdef MGS2_GL
      gu_zero_empty(map, 0);             /* the map did not see the transfers while OPT_XL was off: what it never saw written is not known */
#endif
    }
    xl_reset(); l_words = l_calls = l_ad = 0; state = 1;
  }
  if (skip_now) { sk_tr++; return; }   /* after reduced calls only: the shown frame does not need it */
  ntransfer++;
  if (one_mode && one) { none++; g_xl_one = 1; return; }   /* the exact walk feeds it, then xlive_one_end */
  HOST_ENTER(HOST_XL); xlv_dma(&M, ch, chcr, madr, qwc, tadr); HOST_LEAVE(HOST_XL);
  sum();
}
void xlive_one_end(void) { xl_flush(); g_xl_one = 0; sum(); }   /* the game may change this RAM before the next transfer */

void xl_psp_report(FILE *f)
{
  if (!f) return;
  if (!state) { fprintf(f, "translator: never ran (OPT_XL off)\n"); return; }
  if (state < 0) { fprintf(f, "translator: no memory for the texture map, it did not run\n"); return; }
  const XlStats *S = xl_stats(); const GfGs *F = xl_front(); const GuStats *U = gu_stats(map);
  fprintf(f, "translator: one walk %s, shown frames only %s: %ld vblanks after reduced calls only, %ld transfers not handed over\n",
          one_mode ? "on" : "off", skip_mode ? "on" : "off", sk_vb, sk_tr);
  fprintf(f, "translator: %ld transfers (%ld fed by the exact walk: their tags and quadwords not counted), %ld tags, %llu quadwords; %llu VIF1 words, %llu VU1 calls (%ld callbacks), %llu A+D packets; "
          "front end %ld primitives; texture map %ld transfers (%ld not modelled, %ld in pieces), %ld CLUT loads, %ld IMAGE quadwords "
          "without a transfer, %ld block entries dropped; started again %ld times; since the last start: %ld calls without an entry, %ld "
          "A+D packets without an end, GIF tags path 1/2/3 %ld/%ld/%ld, %ld unknown VIF commands, %ld zero-area primitives\n", ntransfer,
          none, M.tags, (unsigned long long)t_qw, (unsigned long long)t_words, (unsigned long long)t_calls, ncall, (unsigned long long)t_ad, nprim,
          U->xfers, U->xfer_unmod, U->xfer_split, U->loads, U->image_orphan, U->dropped, nrestart, S->calls_noentry, S->ad_bad,
          S->gif_tags[1], S->gif_tags[2], S->gif_tags[3], S->unk, F->zero_area);
#ifdef MGS2_GL
  if (gl_ok < 0) { fprintf(f, "picture: none (no memory or no set-up)\n"); return; }
  if (!gl_ok) { fprintf(f, "picture: never set up\n"); return; }
  if (gl_ok == 3)
    fprintf(f, "picture: STOPPED: the GE did not finish %s%ld in %u ms (sceGeDrawSync(1) %08x); the totals are up to there\n",
            gl_stop_vb < 0 ? "its set-up list" : "the list of the frame of vb ", gl_stop_vb < 0 ? 0 : gl_stop_vb, GL_GE_LIMIT / 1000, (unsigned)gl_stop_r);
  fprintf(f, "picture: %ld frames made (the first began at vb %ld), %ld shown, %ld saved; lists: %ld that did not fit, %ld batches left out, "
          "%llu words (the largest %ld), the largest aux %ld bytes, the largest list and aux together %ld bytes (of %u); digests %ld "
          "(vb %ld..%ld), all %016llx\n", gl_built, gl_first_vb, gl_shown, gl_shots, gl_listfail, gx_drop, (unsigned long long)gx_words_sum,
          gx_words_max, gx_aux_max, gx_both_max, GL_PSP_LIST, dig_n, dig_from, dig_to,
          (unsigned long long)dig_all);
  gl_line(f, &glf.all, glf.all_dbytes, glf.all_tbytes, "picture: the frames:"); fprintf(f, "; memo answers that differed %ld\n", glf.memo_bad);
  gl_line(f, &glf.max, glf.max.dbytes, glf.max.tbytes, "picture: the largest of each in one frame:"); fputc('\n', f);
  { const TkStats *k = tk_stats(tk);
    fprintf(f, "picture: textures: %ld uses, textures %ld hits / %ld conversions, palettes %ld / %ld; render targets %ld, not known %ld, refused "
            "%ld (no room in the pool %ld), texels or entries the map could not give %ld; converted %lld KB, kept %ld (%lld KB, peak %lld KB), "
            "evicted %ld, the heaviest frame %lld KB; pool %u KB, budget %u KB\n", k->uses, k->hits, k->misses, k->pal_hits, k->pal_misses, k->rt,
            k->lost, k->unsup, k->nomem, k->unknown, (long long)(k->conv_bytes >> 10), k->live, (long long)(k->live_bytes >> 10),
            (long long)(k->peak_bytes >> 10), k->evicted, (long long)(k->peak_frame_bytes >> 10), (unsigned)gl_texkb, (unsigned)gl_texkb / 8 * 7);
    fprintf(f, "picture: texture conversions: %ld a block at a time, %ld handed back to the texel path; GS blocks read %ld, %ld of them whole "
            "from one upload\n", k->blk, k->blk_back, gu_stats(map)->blocks, gu_stats(map)->blocks_one); }
  { const XcStats *x = xc_stats();
    fprintf(f, "picture: converters: df0 calls %ld, batches %ld, left out off the scissor %ld (%ld vertices); effect calls %ld / %ld, batches "
            "%ld / %ld; P06 calls %ld, batches %ld, passes 2 and 3 drawn %ld / %ld; P07 calls %ld, batches %ld, bad %ld (paths not seen %ld), "
            "in the exact form %ld, passes 2 and 3 drawn %ld / %ld\n", x->calls[0], x->batches[0], x->cut[0], x->cut_v[0], x->calls[1],
            x->calls[2], x->batches[1], x->batches[2], x->calls[3], x->batches[3], x->lit_pass[2], x->lit_pass[3], x->calls[4], x->batches[4],
            x->bad[4], x->face_unseen, x->face_exact, x->face_pass[2], x->face_pass[3]); }
#endif
}
