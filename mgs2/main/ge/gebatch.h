/* gebatch.h: the GE renderer's batch writer (renderer_notes 11.10, "DIRECT draws" step 4): the GS primitives of the front end
   (ge/gsfront.h) and the converters' transform batches (ge/xconv.h) -> one frame of GE replay batches (ge/replay_fmt.h) in memory:
   batches, vertices, textures and the data block, which a player can draw from as they are (psp/ge_replay's B, V, T and D) or gb_write
   saves as a replay file.  The rules are tools/gsref.c's export (GSREF_GEOUT), which uses this file (2026-10-23: its 8 deck exports
   byte-identical with and without GSREF_XLDIRECT).  The host adds the textures (gb_put + gb_tex; gsref converts them from its GS
   memory) and anything only it knows (gsref adds the pixels its software GS wrote to each batch's px).  Portable C.
   Game data passes through it: what it holds and writes is personal use only. */
#ifndef MGS2_GEBATCH_H
#define MGS2_GEBATCH_H
#include <stdint.h>
#include <stdio.h>
#include "replay_fmt.h"
#include "gsfront.h"

typedef struct {
  GerBatch *b; int nb, cap_b, cur;     /* batches; cur: the through batch the next primitive may continue (-1: none) */
  GerVert *v; int nv, cap_v;           /* the through batches' vertices */
  GerTex *t; int nt, cap_t;            /* textures (cap_t set by gb_init) */
  int *u0, *v0;                        /* the PS2 texel each texture's region starts at (GE u = PS2 u - u0) */
  uint8_t *d; uint32_t nd, cap_d;      /* the data block: texels, palettes and transform batch data, each at a 64-byte boundary */
  GerVert last[2];                     /* the current batch's last two vertices (strip and fan continuation) */
  double zk;                           /* GE depth = Z24 x zk (the file's zscale) */
  float zkf;                           /* task #149: (float)zk, made by gb_init (no doubles on the ME) */
  int zraw;                            /* 1: a through vertex's z is the PS2's Z as it is (Z24 at most 0xffffff; Z16 buffers' Z16), for a
                                          depth map applied later (ge/gelist.h); 0: Z24 x zk, at most 65535 */
  long dropped;                        /* what did not fit: no memory, or the texture table full */
  int live;                            /* 1: the live renderer's frame (ge/gelive.h): a transform batch ends the current through batch,
                                          since the call's own PS2 primitives are not in the frame */
  int fixed;                           /* 1: b, v and d lie in the host's arena (gb_arena): what does not fit is dropped, never realloc'd */
  uint32_t room, room_cap;             /* gb_room: the offset handed out and its size */
} GbOut;
#define GB_NONE 0xffffffffu

int gb_init(GbOut *o, double zk, int max_tex);        /* 0, or -1: no memory */
/* the batches, vertices and data in one arena of the host's (64-byte aligned, size bytes): nb batches, nv vertices, the data the rest;
   after gb_init.  0, or -1: the arena is too small for nb and nv */
int gb_arena(GbOut *o, void *mem, uint32_t size, uint32_t nb, uint32_t nv);
void gb_reset(GbOut *o);                                /* a new frame: empty, the memory kept */
uint32_t gb_put(GbOut *o, const void *p, uint32_t n);   /* n bytes into the data block: their offset, or GB_NONE (no memory) */
int gb_tex(GbOut *o, const GerTex *t, int u0, int v0);  /* a texture whose texels and palette the host put: its index, or -1 (full) */

/* the batch state of a primitive drawn with state d and texture ti (-1: none), from VU1 entry entry (0: none); returns what of that
   state the replay does not draw as the PS2 does (the GE draws the primitive without it): */
enum {
  GB_U_ANEVER = 1,   /* alpha test NEVER with an AFAIL other than FB_ONLY (FB_ONLY is drawn without Z write) */
  GB_U_FBMSK = 2,    /* a frame-buffer write mask */
  GB_U_DFP = 4,      /* destination alpha test, FBA or PABE */
  GB_U_AFAIL = 8     /* AFAIL ZB_ONLY or RGB_ONLY */
};
int gb_state(const GfDraw *d, int ti, uint32_t entry, GerBatch *b);
/* one primitive (d->prim 0..6; n = 1, 2 or 3 vertices in the GS's order) with the state gb_state made: appended to the current batch
   when the state is the same and, for strips and fans, it continues that batch's vertex order; else a new batch.  The batch is drawn
   in two passes (twopass) when it has AFAIL FB_ONLY with Z write and a vertex alpha x the texture's lowest alpha can be below AREF.
   0, or -1: no memory (the primitive is dropped) */
int gb_prim(GbOut *o, const GerBatch *b, const GfDraw *d, const GfVtx *const *v, int n);
GerVert gb_vert(const GbOut *o, const GfDraw *d, const GfVtx *v, int ti);   /* a vertex as a GERK_THROUGH batch draws it */
/* a transform batch (GERK_XFORM / GERK_XFORMF / GERK_LIT, ge/xconv.h xc_call) whose data is blob[0..size): its first becomes the
   data's offset plus b->first (the batch's offset in the blob: 0 but for P06's GerLit); the current through batch stays current.
   0, or -1: no memory */
int gb_xform(GbOut *o, const GerBatch *b, const void *blob, uint32_t size);
/* the same without a copy: gb_room gives cap bytes at the next 64-byte boundary of the data block (0: no room), the host writes the
   batch's data there, and gb_xform_in adds the batch with its first size bytes (size <= cap).  0, or -1: no memory */
uint8_t *gb_room(GbOut *o, uint32_t cap);
uint32_t gb_left(const GbOut *o);   /* the most gb_room can give now: the arena's free bytes (fixed), else 0xffffffff */
int gb_xform_in(GbOut *o, const GerBatch *b, uint32_t size);
/* a batch whose data an earlier one put at offset base of the data block (P06's passes 2 and 3: their call's blob, ge/xconv.h
   xc_pass): first = base + b->first, nothing copied.  0, or -1: no memory */
int gb_xform_at(GbOut *o, const GerBatch *b, uint32_t base);
/* sb4, step c (ge/gelive.c, renderer_notes 11.29): a batch whose data comes later keeps its place in the order.  gb_slot takes the next
   batch slot now, as gb_xform_in / gb_xform_at would take it (the current through batch ends): its index, or -1 (no memory); until filled
   it draws nothing.  gb_fill_in fills slot i as gb_xform_in fills the next one (the data from gb_room, size bytes); gb_fill_at as
   gb_xform_at.  0, or -1 */
int gb_slot(GbOut *o);
int gb_fill_in(GbOut *o, int i, const GerBatch *b, uint32_t size);
int gb_fill_at(GbOut *o, int i, const GerBatch *b, uint32_t base);
int gb_write(const GbOut *o, FILE *f, uint32_t vb, uint32_t w, uint32_t h);   /* the replay file (vblank vb, w x h); 0, or -1 */
#endif
