/* gelist.h: the GE list of one live frame (renderer_notes 11.13, #30 step 3).  A frame of GE batches (ge/gebatch.h's arrays, or a
   replay file's) -> the GE's command words, by the rules of psp/ge_replay/main.c's reference mode (v6 MODES[0], v5's mode 4: the
   game's GS draws in through mode, df0 and the effect meshes with hardware transform and fog, each df0 strip one PRIM, the effects'
   triangles across the screen edge cut by the GE (an index list), the depth blur in 16-row tiles with the top-left corner first, the
   motion trail in 16-pixel columns, a second pass for AFAIL FB_ONLY with Z write), written as raw words (the command << 24 | its
   24-bit argument; a float as its top 24 bits, as libgu sends it) with the last state kept, so only changes are sent.  What sceGu
   costs on the PSP (prof/hw_ge_v6: 23.17 ms of CPU for the deck frame's list) is the reason for this file.
   The list is whole: the frame's set-up (buffers, offset, viewport, scissor, depth range, clear), its batches, then FINISH and END for
   sceGeListEnQueue (or RET, ret = 1, for sceGuCallList); it relies on sceGuInit's defaults for the rest (texture map mode, levels, colour and stencil masks, dither).  The
   vertices of the clear and of the sprite cuts and the effects' index lists go into the list's own memory (aux), so the list, aux, the
   frame's arrays and its textures must stay as they are until the GE has drawn it.  Aux is taken from its top down; given the list's own
   block (aux = the words, cap_aux = 4 cap), the two share it, the words from the bottom up and aux from the top down, so a frame needs
   room for its words and aux together, not for the largest of each (2026-10-27: 302 KB at most on both routes, against 271 KB of words
   and 229 KB of aux at most, in different frames).
   Depth: GE depth = clamp(q x zs + zo, 0, 65535) for each depth value q of the frame (its through vertices' z, rewritten in place, and
   its transform batches' viewport z terms); zs = 1, zo = 0 draws the frame as stored (a replay file's).  gx_depth_map gives zs and zo
   for a frame whose values are Z24 x zk and whose depth-tested writers span [lo, hi] in Z24 (renderer_notes 11.13's rule: tools/gsref.c
   GSREF_ZMAP's).  All arithmetic is in float, so the x86 check build (with the PSP's rounding, MGS2_FPU_RZ) gives the same words.
   Portable C: the words name memory through GxHost.addr (the PSP: the pointer itself; x86: anything), and the digest (dig_on) hashes
   each address word as what it names (which array, which offset) and leaves BASE out, so the x86 check build and the PSP give the same
   digest for the same frame.  Game data passes through it: personal use only. */
#ifndef MGS2_GELIST_H
#define MGS2_GELIST_H
#include <stdint.h>
#include "replay_fmt.h"

typedef struct {
  const GerBatch *b; uint32_t nb;
  GerVert *v; uint32_t nv;                    /* the through batches' vertices (z rewritten by gx_frame) */
  const uint8_t *d;                           /* the data block: transform batches (GerXform and what follows it) */
  const GerTex *t; uint32_t nt;
  const uint8_t *const *tdata; const uint32_t *const *tpal;   /* kind 1: each texture's texels and palette (GerTex data / pal unused) */
  float zs, zo;                               /* the depth map (above) */
} GxFrame;

typedef struct {
  uint32_t (*addr)(void *ctx, const void *p); /* the GE address of a pointer into the frame, its textures or the list's aux */
  void *ctx;
  uint32_t fb[2], zb;                         /* VRAM offsets (from the start of VRAM) of the two 8888 frame buffers (512 pixels a row) and
                                                 the 16-bit depth buffer (512 a row) */
} GxHost;

typedef struct {                              /* one frame's */
  long words, aux, batches, prims, passes2, tex_set, vaddr, cuts, fx_runs, fx_tris, xf_strips;
  long blend_other;                           /* blend equations not in the table (drawn as (Cs - Cd) As + Cd, as the player) */
  long tex_none;                              /* textured batches whose texture is missing (drawn untextured) */
  long dropped;                               /* batches left out: no room in the list or aux */
  long lit_passes, lit_strips;                /* P06 passes drawn (GERK_LIT batches) and their strips */
} GxCount;

typedef struct {
  uint32_t *w; uint32_t n, cap;               /* the list (cap words) */
  uint8_t *aux; uint32_t naux, cap_aux;       /* the frame's own vertices and indices: the naux bytes from aux + alow to aux + cap_aux */
  uint32_t alow; int shared;                  /* the lowest aux byte taken (from aux); 1: aux is the list's block (gx_init) */
  int dig_on; uint64_t dig;                   /* 1: gx_frame computes the frame's digest (FNV-1a 64 of the words as above) */
  int ret;                                    /* 1: the list ends with RET (a list sceGuCallList calls), 0: FINISH and END */
  GxCount c;
  /* the state the list has set (private) */
  int s_tex, s_ten, s_blend, s_bsig, s_at, s_afunc, s_zt, s_zfunc, s_zmask, s_shade, s_tfunc, s_sc0, s_sc1, s_fog, s_clip, s_vtype;
  uint32_t s_base, s_proj[16], s_vp[6], s_ts[4], s_fogp[3];
  const void *s_xf;                           /* the GerXform whose world matrix the list set last */
  int s_have_proj, s_have_vp, s_have_ts, s_have_fogp, s_draw;
} GxList;

/* aux == (uint8_t *)words and cap_aux == 4 cap: one block for both (above) */
void gx_init(GxList *L, uint32_t *words, uint32_t cap, uint8_t *aux, uint32_t cap_aux);
/* the frame's list, drawn into frame buffer draw (0 or 1; a kind 2 texture with psm 1 samples the other one): 0, or -1 when something
   did not fit (c.dropped; the list is still whole) */
int gx_frame(GxList *L, const GxHost *h, GxFrame *f, int draw);
/* the depth map for values q = Z24 x zk whose depth-tested writers span [lo, hi] in Z24 (lo > hi: none, then the whole of Z24):
   the 16 bits cover [max(lo - m, 0), hi + m], m = (hi - lo) / 16 */
void gx_depth_map(float lo, float hi, float zk, float *zs, float *zo);
#endif
