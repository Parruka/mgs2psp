/* gsfront.h: the GS front end of the GE renderer (renderer_notes 11.2, 11.10): register writes as the GIF hands them over (A+D and
   REGLIST writes, PACKED quadwords) -> the GS primitives they draw, each with the GS state it is drawn with.  The register rules, the
   vertex queue and the primitive rules are tools/gsref.c's (the reference renderer, whose replay of the deck capture draws the expected
   frames); gsref keeps its own copy of them and checks this one against it (GSREF_XLDIRECT).  gf_setup and the two types are
   shared: gsref takes its primitives' state from gf_setup (2026-10-23: its 8 deck exports and 24 images unchanged).  Fed by the
   translator (ge/xlate.c xl_on_prim), it gives every PATH2 primitive of the 8 deck frames as gsref draws it: 752 of 752, state and
   vertices, none uncertain, and the export made from its data is byte-identical (renderer_notes 11.10, "DIRECT draws").  Portable C.
   Game data passes through it: what it writes is personal use only. */
#ifndef MGS2_GSFRONT_H
#define MGS2_GSFRONT_H
#include <stdint.h>

/* a vertex as the GS takes it: x, y and u, v in 12.4 fixed point (x, y before XYOFFSET), z, fog, colour, S, T and Q */
typedef struct { int x, y; uint32_t z; int f, r, g, b, a; float s, t, q; int u, v; } GfVtx;
/* the GS state a primitive is drawn with: PRIM (or PRMODE) attributes, then the registers of its context and the shared ones */
typedef struct {
  int prim, iip, tme, fge, abe, aa1, fst, ctxt;
  int ofx, ofy, scx0, scx1, scy0, scy1;
  uint32_t fbp, fbw, fbmsk; int fpsm; uint32_t zbp; int zpsm, zmsk;
  int ate, atst, aref, afail, date, datm, zte, ztst;
  int ba, bb, bc, bd, fix, pabe, fba, colclamp;
  int fcr, fcg, fcb;
  int tpsm, tw, th, tcc, tfx, cpsm, csa; uint32_t tbp[7], tbw[7];
  int wms, wmt, minu, maxu, minv, maxv;
  int lcm, mxl, mmag, mmin; float k; int l;
} GfDraw;
void gf_setup(const uint64_t *gr, GfDraw *d);      /* gr: the GS registers by A+D address */

/* what a primitive's inputs came from, when the front end did not see all of it (gf_unknown: a VU1 call the translator does not run
   kicked its own tags and vertices): the PRIM register, the vertex queue, the vertex fields */
enum { GF_U_PRIM = 1, GF_U_QUEUE = 2, GF_U_RGBAQ = 4, GF_U_ST = 8, GF_U_Q = 16, GF_U_UV = 32, GF_U_FOG = 64, GF_U_PQ = 128, GF_U_ALL = 255 };
/* (GF_U_Q: the vertex's Q; GF_U_PQ: the Q a PACKED ST left for the next PACKED RGBAQ) */
typedef struct GfGs GfGs;
/* one primitive: PRIM 0..6 (point, line, line strip, triangle, triangle strip, triangle fan, sprite) as d->prim, n = 1, 2 or 3 vertices
   in the GS's order; the GS's zero-area triangles (strip restarts) are not sent */
typedef void (*GfPrimFn)(void *ctx, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n);
struct GfGs {
  uint64_t gr[256];        /* the GS registers by A+D address; TEX2 merges into TEX0 as on the GS; the vertex registers are not kept */
  GfVtx cur;               /* the vertex being assembled */
  float q;                 /* the Q of a PACKED ST, which the next PACKED RGBAQ takes */
  GfVtx vq[3]; int vqn;    /* the vertex queue */
  int vqu[3], curu;        /* GF_U_* vertex fields not known in each queued vertex and in the current one */
  GfDraw d; int dirty;     /* the state from gr, rebuilt at the first primitive after a register write */
  int path;                /* the GIF path being fed (1-3), for the callback */
  int unk, unk_used;       /* GF_U_* not known now; those a primitive used (valid in the callback) */
  GfPrimFn fn; void *ctx;
  long prims[8], zero_area, prim7, uncertain;   /* primitives sent by type, zero-area triangles not sent, PRIM 7 kicks, sent with unk_used */
  struct GuMap *up;        /* optional (set after gf_reset): the texture sources (ge/gsup.h), told of transfers, IMAGE data and CLUT loads */
};
void gf_reset(GfGs *g, GfPrimFn fn, void *ctx);
void gf_reg(GfGs *g, int r, uint64_t v);           /* a register write (A+D or REGLIST), r = the A+D address */
void gf_packed(GfGs *g, int desc, const uint32_t *q);   /* one PACKED quadword for register descriptor desc (0..15) */
void gf_unknown(GfGs *g, int what);                /* GF_U_*: written by code the front end does not see; known again when written */
void gf_image(GfGs *g, uint32_t ee, uint32_t n);   /* n bytes of IMAGE data from EE address ee (for g->up) */
void gf_image_run(GfGs *g, uint32_t ee, uint32_t n);   /* the same for n / 16 quadwords in a row, as n / 16 calls of gf_image(g, ee + 16 k, 16) */
/* for g->up: draws the front end does not see (a VU1 call's own primitives) may write in contexts ctxts (bit 0: 1, bit 1: 2); the
   front end's own primitives tell the map themselves (gu_drawn: the frame and depth buffers within the scissor) */
void gf_drew(GfGs *g, int ctxts);
#endif
