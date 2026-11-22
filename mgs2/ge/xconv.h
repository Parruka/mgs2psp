/* xconv.h: the translator's converters (renderer_notes 11.2, 11.10): one VU1 call as ge/xlate.c hands it over (the VU1 data memory as
   the EE's uploads left it, the GS registers after the call's A+D packet and tag template) -> one GE replay batch (ge/replay_fmt.h
   GERK_XFORM for P04+df0, GERK_XFORMF for P04+25f0 / +2870), without micro code.  What gsref's export takes from the micro program's
   results (the effects' clip positions and clip bits) is recomputed in the VU's own order (re/vu1_programs.md 3.7, ge/vu1conv.h
   vc_vu_clip).  Portable C; the GE-side tests are float.  tools/gsref.c runs it beside its own export (GSREF_XLCONV=1), the PSP will
   run it on the shown calls.  Build it with -ffp-contract=off (psp/Makefile BASEF has it): the effects' clip math must not fuse a
   multiply with an add.  Game data passes through it: what it writes is personal use only. */
#ifndef MGS2_XCONV_H
#define MGS2_XCONV_H
#include <stdint.h>
#include "replay_fmt.h"
#include "xlate.h"

/* the GE texture the host made for the call's texture (TEX0 / CLAMP of GS context ctxt): its index in the replay, GerTex kind, GE size,
   the PS2 texel its region starts at, and the lowest alpha of the palette entries it uses */
typedef struct { int index, kind, tw, th, u0, v0, minalpha; } XcTex;
/* optional, for checks: what the converter computed for each vertex of a 25f0 / 2870 call */
typedef struct {
  int n;
  float c[1024][4];        /* the clip position (the micro program stores it back at T+0x100 + 3i) */
  uint16_t post[1024];     /* the flag word it stores back (T+0x102 + 3i w): the data's, with ADC + bit 14 when the triangle that ends
                              here has a clip flag on one of its vertices and no ADC from the data */
  uint8_t cls[1024];       /* the triangle class of ge/replay_fmt.h GERK_XFORMF (2 also for 2870's culled triangles) */
} XcTrace;
/* the depth range of the depth-tested, depth-writing df0 calls (renderer_notes 11.13, rule V2): every vertex in front of the camera
   (PS2 clip w > 0), its w clamped to the clip planes', in Z24.  Z24 is monotone in w for one projection, so the calls keep a w range
   per projection (pa, pb and the viewport's z terms) and turn it into Z24 when the projection changes and at xc_zr_end. */
typedef struct {
  float pa, pb, vs2, vo2, wmin, wmax; int on;   /* the current projection's w range (on: it has one) */
  float lo, hi;                                 /* Z24 (lo > hi: none) */
  long calls, verts, always;                    /* calls taken and their vertices in front; depth-writing calls with ZTST ALWAYS (left out) */
} XcZr;
void xc_zr_begin(XcZr *z);
void xc_zr_end(XcZr *z);   /* the last projection's range into lo, hi */
typedef struct {
  int (*tex)(void *ctx, const uint64_t *gr, int ctxt, XcTex *t);   /* 0: no GE texture (the batch is drawn untextured) */
  void *ctx;
  double zk;               /* GE depth = Z24 x zk (the replay file's zscale) */
  float zlo, zhi;          /* the depth window in Z24 for the effects' class 3 (renderer_notes 11.9); zlo < 0: none */
  XcTrace *trace;          /* 0: none */
  int noidx;               /* 1: df0 batches without the joined index list (nidx 0: the live renderer draws each strip, renderer_notes
                              11.9 / 11.13) */
  XcZr *zr;                /* 0: none; else the df0 calls' depth range goes in (renderer_notes 11.13) */
  int cut;                 /* 1: a df0 call whose vertices' object-space box lies wholly off one side of its scissor (by 2 GE pixels) is
                              left out before its texture is asked for: nothing it holds can light a pixel (xconv.c xc_off; renderer_notes
                              11.15).  Its vertices still go into zr as xc_df0 would put them (xc_df0_zr), so the frame's depth map is
                              the same either way.  0 (older hosts): every call is converted */
} XcHost;
/* counters since the start, for a report */
typedef struct {
  long calls[5], bad[5], batches[5], empty[5];   /* by entry: df0, 25f0, 2870, P06 (its four entries), P07; bad input; batches (P06,
                                                    P07: pass 1's); calls with nothing drawn */
  long cut[4], cut_v[4];   /* calls left out by XcHost.cut (df0 so far; counted in empty too) and their vertices */
  long tris[6];            /* effect triangles by class 0..4, and 2870's culled ones (counted in class 2 too) */
  long polys, poly_v, poly_empty, poly_culled, poly_full;   /* clipped polygons kept, their vertices; empty; back-face culled; no room */
  long lit_kind[4], lit_v, lit_exact, lit_pass[4], lit_nopass[4], lit_cull, lit_ramp, lit_own2;   /* P06: calls by entry (+0, +358, +738,
                                                    +ab0), their vertices, calls in the exact form; passes 2 and 3 drawn ([2], [3]) and not
                                                    drawn (pass 2: a UV2 z other than 1.0; pass 3: from T+0x104); calls whose cull bit the
                                                    GE does not do; pass-3 light ramps (not seen in the captures: unchecked); calls whose pass
                                                    2 has vertices of its own (UV2 - UV1 not one value) */
  long face_v, face_pass[4], face_nopass[4], face_own2, face_unseen;   /* P07: vertices; passes 2 and 3 drawn and not drawn (pass 2: a
                                                    UV2 z other than 1.0; pass 3: flags 0x100 or 0x400); pass 2 with vertices of its own;
                                                    calls on a path the captures never took (counted in bad[4]: vu1conv.h vc_p07 -7) */
  long face_exact, face_chk, face_chkdiff;   /* P07 calls in the exact form; x86 checks (xconv.c built with -DXC_P7CHK): calls of the
                                                fast form converted again in the exact form, and those whose output differs */
} XcStats;
const XcStats *xc_stats(void);

/* 1: the call's batch in *b and its data in blob[0..*size); b->first: the batch's offset in the blob (0 but for P06 and P07), to
   which the host adds the blob's offset in its data block; 0: no batch (an entry without a converter, an MSCNT, or nothing drawn);
   < 0: bad input (vu1conv.h), -5: blob too small.  P06 and P07 (GERK_LIT): the batch is pass 1's, and the blob holds what passes 2
   and 3 draw too */
int xc_call(const XlCall *c, const XcHost *h, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size);
/* P06's or P07's pass 2 or 3 (the translator's pass callback, ge/xlate.h xl_on_pass, with the GS registers after that pass's A+D packet), right
   after xc_call gave the call's pass 1: 1, the pass's batch in *b, whose data is in the call's blob (blob: where it lies now; b->first
   its offset in the blob, as above; the pass's texture scale written there); 0: the pass is not drawn (counted in XcStats) */
int xc_pass(const XlCall *c, int pass, const XcHost *h, GerBatch *b, uint8_t *blob);
#endif
