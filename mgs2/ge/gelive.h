/* gelive.h: the live renderer's frame (renderer_notes 11.13, #30 step 3).  What the translator (ge/xlate.h) hands over while the
   transfers of one shown frame are walked -> GE batches (ge/gebatch.h GbOut, o.live): the GS front end's path 2 / 3 primitives (the
   game's DIRECT draws) and the VU1 calls of the entries with a converter (ge/xconv.h: P04+df0, +25f0, +2870, P06), in the order they come,
   with their textures from the PSP texture path (ge/texcache.h) by pointer: the cache keeps a texture to the end of the next frame, so
   the GE can draw a frame while the next one is made.  The draws of the VU1 entries without a converter are not in the frame yet
   (renderer_notes 11.12 plan item 4).  gl_write saves a frame as a replay file (ge/replay_fmt.h, its textures copied in) for
   psp/ge_replay and for the check against tools/gsref.c's export of the same vblank (GSREF_GELIVE).  Portable C: boot/xlive_chk.c runs
   it on x86 (MGS2_XLIVE_GB), the PSP harness on its shown frames.  Game data passes through it: personal use only. */
#ifndef MGS2_GELIVE_H
#define MGS2_GELIVE_H
#include <stdint.h>
#include <stdio.h>
#include "gebatch.h"
#include "gsup.h"
#include "texcache.h"
#include "xlate.h"
#include "xconv.h"
#include "gelist.h"

#define GL_XCAP 226560u   /* the most data one converter call writes (ge/xconv.c: P04+25f0 with 1,024 vertices and 8,192 polygon vertices) */
#define GL_HASH 1024      /* the frame's texture table: slots of its hash (a power of two) */

typedef struct {
  long frames;
  long prims, prims_tex, uncertain, path1;    /* front-end primitives: taken (of those textured); left out: an input written by code
                                                 the front end does not see (gf_unknown), drawn on path 1 */
  long unrep[4];                              /* taken primitives with state the GE draws otherwise (gb_state's GB_U_* bits) */
  long calls, conv, conv_empty, conv_bad, conv_full, other;   /* VU1 calls of converter entries: converted, nothing drawn, bad input,
                                                 no room; calls of other entries (not drawn) */
  long tex_asks, tex_memo, tex_new, tex_fb, tex_rt, tex_lost, tex_unsup, tex_full;   /* texture lookups: answered by the last one; new
                                                 in the frame; frame buffers (GE kind 2); drawn untextured: render targets, not known,
                                                 refused (a format or size not converted), the frame's table full */
  long tex16;                                 /* new textures in 16-bit colour (GE 5551) */
  long batches, xbatches, verts, dbytes, tbytes, ntex;   /* the frame: batches (of those transform batches), through vertices, data
                                                 bytes, texel and palette bytes its textures hold, textures */
  long dropped;                               /* what did not fit (GbOut.dropped) */
  long z16;                                   /* front-end primitives with a 16-bit depth buffer that test or write depth (zraw maps them as
                                                 Z24: none seen so far) */
  long zr_calls, zr_always;                   /* depth-tested depth-writing df0 calls in the range; depth-writing df0 calls with ZTST ALWAYS */
  long bent;                                  /* zchk: effect triangles of class 3 with a vertex outside the frame's map (the GE clamps it) */
  long out_w, out_o;                          /* zchk: df0 and P06 triangles with a vertex in front that the map puts outside 0..65535 (the
                                                 GE drops them: GE_CLIP_PLANES off), of depth-tested depth-writing calls / of the others */
  long lit_calls, lit_passes, lit_nopass;     /* P06 and P07 calls in the frame; their passes 2 and 3 in the frame, not drawn (ge/xconv.h
                                                 xc_pass) */
} GlCount;

typedef struct GlFrame {
  GbOut o;                          /* the frame; its textures' o.t[i].data and pal stay 0: tdata and tpal hold them */
  const uint8_t **tdata; const uint32_t **tpal; uint64_t *tkey; uint32_t *tsize;
  int max_tex;
  int16_t hash[GL_HASH];            /* tkey -> texture index in this frame, -1: none */
  TkCache *tk; GuMap *map;
  XcHost h;
  int memo_on, memo_check;          /* 1: a lookup with the last one's TEX0, CLAMP and TEXA and no texture source changed since takes its
                                       answer; memo_check: the texture path asked as well and the answers compared (memo_bad) */
  struct { uint64_t t0, cl, texa; long gen; uint32_t fbp; int ti, valid; XcTex xt; } memo;
  long memo_bad;
  int zraw;                         /* 1 (gl_init with zk 0): the PSP's depth (renderer_notes 11.13): through z and the converters' depth
                                       in Z24, mapped by the GE list with the frame's range (map_lo, map_hi; ge/gelist.h gx_depth_map),
                                       the effects' class 3 window the last frame's range.  0: GE depth = Z24 x zk, at most 65535, the
                                       window [0, 65535 / zk] (tools/gsref.c's export) */
  int zchk;                         /* 1 (x86 checks): gl_end counts what the frame's map leaves outside the GE's range (n.bent, out_*) */
  XcZr zr;                          /* the depth-tested depth-writing df0 calls' range (Z24, xconv.h) */
  float tlo, thi; long tz0, tztop;  /* the depth-tested depth-writing GS draws' range (Z24 clamped to the depth format, as the GS
                                       writes it), without the vertices at 0 and at the format's top (counted: clears and the like) */
  float zlo, zhi;                   /* the frame's range: zr's and the GS draws' (zlo > zhi: none) */
  float map_lo, map_hi;             /* the range the frame's depth map covers: its own, or the last frame's with one */
  GlCount n, all, max;              /* this frame; summed over frames; the largest of each count in one frame */
  int64_t all_dbytes, all_tbytes;   /* all.dbytes and all.tbytes summed in 64 bits, for the totals' lines (a long is 32 bits on the PSP:
                                       gl6's run printed its data total as -901,521 KB) */
  int lit_ok; uint32_t lit_base;    /* the last call was a P06 call whose pass 1 is in the frame, its blob at lit_base of the data block */
} GlFrame;

/* zk: GE depth = Z24 x zk, or 0 for zraw.  0, or -1: no memory; then gb_arena on f->o for an arena */
int gl_init(GlFrame *f, TkCache *tk, GuMap *map, double zk, int max_tex);
void gl_begin(GlFrame *f);                    /* a shown frame starts: the frame is emptied */
void gl_end(GlFrame *f);                      /* it ended: its counts go into all and max */
/* the translator's callbacks (xl_on_prim, xl_on_call, xl_on_pass) while the frame's transfers are walked */
void gl_prim(GlFrame *f, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n);
void gl_call(GlFrame *f, const XlCall *c);
void gl_pass(GlFrame *f, const XlCall *c, int pass);   /* P06's or P07's pass 2 or 3, after gl_call of the same call */
/* the entries with a converter: P04+df0, +25f0, +2870, P06's four and P07 (ge/vu1conv.h) */
static inline int gl_conv_entry(uint32_t e)
{
  return e == XL_DF0 || e == XL_FX || e == XL_FX2 || e == 0x19fe08u || e == 0x1a0160u || e == 0x1a0540u || e == 0x1a08c0u || e == 0x1a31e0u;
}
int gl_write(const GlFrame *f, FILE *fp, uint32_t vb);   /* the frame as a replay file, 480 x 272; 0, or -1 */

/* the PSP harness's frame memory (psp/xl_psp.c, MGS2_GL; boot/xlive_chk.c MGS2_XLIVE_GX makes the same frames with the same sizes) */
#define GL_PSP_ARENA (1696u << 10)   /* the frame's batches, vertices and data (gb_arena): data 1,432 KB */
#define GL_PSP_NB 3072u
#define GL_PSP_NV 6144u
#define GL_PSP_LIST (352u << 10)     /* the GE list and its aux in one block (gelist.h).  2026-10-27, both routes from boot (x86): at most
                                        2,043 batches, 2,608 vertices, 1,240 KB of data and 302 KB of list and aux in one frame */
#define GL_PSP_MAXTEX 1024
#define GL_PSP_TEXKB 2560            /* the texture pool's size by default (KB; MGS2_GL_TEXKB); the cache's budget is 7/8 of it */
/* the frame as the GE draws it, for the check of the PSP against x86: FNV-1a 64 by 32-bit words over the list's own digest (l->dig:
   gx_frame with dig_on), the counts, the batches, the through vertices (after gx_frame's depth map), the data block, the list's aux and
   each texture (its GerTex, texels and palette).  Not the list's length, which holds BASE words that depend on where the memory lies. */
uint64_t gl_digest(const GlFrame *f, const GxList *l);
#endif
