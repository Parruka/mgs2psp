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
/* the GS state of a call's first primitive (or of a pass), as gsref's setup() reads it from the registers (xconv.c xc_gs) */
typedef struct {
  int iip, tme, fge, abe, ctxt, ofx, ofy, scx0, scx1, scy0, scy1, zmsk, zte, ztst, ate, atst, aref, afail;
  int ba, bb, bc, bd, fix, tw, th, tcc, tfx;
} XcGs;
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
/* task #114: an effect the host thins (ge/fxthin.h): the guest address range its calls' vertex data comes from (XlCall.src), and the share
   of their strips drawn: strips 0, keep, 2 keep ... (a strip starts where two ADC vertices follow one without).  line (task #154): 1: the
   range is for the streaks (P04+2b60, xc_line: a strip starts at an ADC vertex), 0: for the 25f0 / 2870 strips (xc_fpos) */
typedef struct { uint32_t lo, hi; int keep, line, kcall; } XcThin;   /* (task #158 kcall: a streak range's calls 1 in kcall, by block) */
void xc_zr_begin(XcZr *z);
void xc_zr_end(XcZr *z);   /* the last projection's range into lo, hi */
typedef struct {
  int (*tex)(void *ctx, const uint64_t *gr, int ctxt, XcTex *t);   /* 0: no GE texture (the batch is drawn untextured) */
  void *ctx;
  double zk;               /* GE depth = Z24 x zk (the replay file's zscale) */
  float zkf;               /* task #149: (float)zk (hosts built with -DMGS2_ME_SAFE set it: gl_init) */
  float zlo, zhi;          /* the depth window in Z24 for the effects' class 3 (renderer_notes 11.9); zlo < 0: none */
  XcTrace *trace;          /* 0: none */
  int noidx;               /* 1: df0 batches without the joined index list (nidx 0: the live renderer draws each strip, renderer_notes
                              11.9 / 11.13) */
  XcZr *zr;                /* 0: none; else the df0 calls' depth range goes in (renderer_notes 11.13) */
  int cut;                 /* 1: a df0 call whose vertices' object-space box lies wholly off one side of its scissor (by 2 GE pixels) is
                              left out before its texture is asked for: nothing it holds can light a pixel (xconv.c xc_off; renderer_notes
                              11.15).  Its vertices still go into zr as xc_df0 would put them (xc_df0_zr), so the frame's depth map is
                              the same either way.  0 (older hosts): every call is converted */
  int clipfast;            /* sb3: 1: the effects' clip positions without the VU float clamps when no input needs them (ge/vu1conv.h
                              vc_vu_clip_nc); 0 (older hosts): every vertex through vc_vu_clip.  The same bits either way */
  const XcThin *thin; int nthin;   /* task #114: the effects thinned this frame (0: none): a 25f0 / 2870 call whose XlCall.src lies in
                              one of these ranges (line 0) draws 1 strip in keep; the others' triangles are not drawn (class 0) and their
                              vertices get no clip position.  The call's minimum alpha (two passes or one) stays its whole one.  Task
                              #154: a streak call (2b60, step 4's C4) in a range with line 1 the same way: its other strips' segments
                              are not drawn (class 0) */
  int c1;                  /* sb11 C1 (stagec_notes.md 1.7-1.9): 1: a lane 4 P07 call is drawn by the GE's skinning, lighting and
                              texture matrix (xconv.c xc_lit_c1; ge/replay_fmt.h GERL_C1) where the GE can draw it, else as today;
                              0 (older hosts): as today */
  int c2;                  /* sb12 C2 (stagec_notes.md step 2): 1: a lane 2 P06 call is drawn by the GE's skinning, lighting and texture
                              matrix by mapping (b) (xconv.c xc_lit_c2; ge/replay_fmt.h GERL_C2) where the GE can draw it, else as today;
                              0 (older hosts): as today */
  int c3v;                 /* sb14 3v (stagec_notes.md step 3): 1: a lane 1 df0 call's vertices in two passes (ge/vu1conv.h vc_df0_pos,
                              vc_df0_att), the UVs, colours and flags only for a call the cut does not leave out; 0: vc_df0_rec.  The
                              same lists either way */
  int c3z;                 /* sb14 3z: 1: a df0 call whose positions' box (vc_df0_pos, or xc_df0_off's) lies inside the range the depth
                              range keeps for its projection, by a bound (xconv.c xc_df0_zin), counts its vertices without computing
                              each one's w (xc_zr_df0); 0: each w.  The same depth range either way */
  int c3w;                 /* sb14 3w (with 3v): 1: a two-pass call's vertices written straight into the frame's room, where xc_df0
                              puts them after the GerXform (when the room holds 1,024 vertices); 0: into a buffer, then copied there.
                              The same bytes either way */
  int c4;                  /* sb15, Stage C step 4 (stagec_notes.md step 4): 1: P04+3740's sprites, +3a38's quads and +2b60's streaks
                              converted (xconv.c xc_c4: GERK_XFORMF with GerBatch.prim GU_SPRITES, GU_TRIANGLE_STRIP or
                              GU_LINE_STRIP); 0 (older hosts): not drawn */
  int c4x;                 /* sb15 4x (g_opt2 OPT2_C4X; stagec_notes.md 4.7): 1: P04+25f0 / +2870's strips and polygons from their clip positions
                              (the GerXform's world the identity), an untextured call's UV q not checked; 0 (older hosts): world positions, q checked */
  int cull;                /* cull study v3 (attic/cull/cull_v3.md; boot/nat_stub.h, psp/xl_psp.c MGS2_NAT_CULL, _CYCLE, MGS2_NAT_CULLCHK):
                              bit 1 (XC_CULL_FX): a 25f0 / 2870 call is first tested (xconv.c xc_fpos_off): when a bound proves that one
                              clip plane's flag bit is set on every vertex as the conversion computes it (so every triangle it classes is
                              trivially rejected and it draws nothing), the conversion is not run and its counters are added as it would
                              add them.  XC_CULL_CHK: the check mode (the conversion runs anyway; its result, counters and every vertex's
                              flags are compared with the claim).  0 (older hosts): every call converted */
} XcHost;
#define XC_CULL_FX 1
#define XC_CULL_CHK 0x80
/* counters since the start, for a report */
typedef struct {
  long calls[5], bad[5], batches[5], empty[5];   /* by entry: df0, 25f0, 2870, P06 (its four entries), P07; bad input; batches (P06,
                                                    P07: pass 1's); calls with nothing drawn */
  long cut[4], cut_v[4];   /* calls left out by XcHost.cut (df0 so far; counted in empty too) and their vertices */
  long tris[6];            /* effect triangles by class 0..4, and 2870's culled ones (counted in class 2 too) */
  long clip[2];            /* sb3: the effects' vertices; those through vc_vu_clip's clamps while XcHost.clipfast was on */
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
  long thin_calls, thin_strips, thin_drop, thin_tris;   /* task #114: effect calls thinned, their strips, the strips left out, their
                                                triangles (not in tris[]) */
  long thin_lcalls, thin_lstrips, thin_ldrop, thin_lseg;   /* task #154: streak calls thinned, their strips, the strips left out, their
                                                segments (not in c4_n[2]) */
  long c1_calls, c1_v, c1_nw[9], c1_fall[4];   /* sb11 C1 (XcHost.c1): P07 calls drawn by the GE's skinning and lighting, their
                                                vertices, by weights a vertex (1..8); calls left to today's path: vc_p07_c1 -8 (the fast
                                                form's own conditions), -9 (the clamp, a light or the ambient), the vertices (vc_p07_c1s or
                                                vc_p07_c1b -8: an offset, a slot's weights, a UV), a bone matrix (p7_mok's test) */
  long long c1_bytes;                         /* their data bytes (64-bit: above 2 GB on the codec route, the PSP's long is 32-bit) */
  long c1_zin, c1_zin_v;                      /* c1opt2: C1 calls (and vertices) whose w the depth range took by the bound alone (xc_c1_zin) */
  long c2_calls, c2_v, c2_skin, c2_fall[4];   /* sb12 C2 (XcHost.c2): P06 calls drawn by the GE's skinning and lighting, their vertices,
                                                the skinned ones among the calls; calls left to today's path: vc_p06_c2 -8 (the fast form's
                                                own conditions), -9 (the clamp, a light or the ambient), vc_p06_c2v -8 (a UV, a weight, a
                                                factor), no room */
  long long c2_bytes;                         /* their data bytes */
  long c2_zin, c2_zin_v;                      /* skinned C2 calls (and vertices) whose w the depth range took by the bound alone (xc_c2_zin) */
  long c3w_calls;                             /* sb14 3w: two-pass calls whose vertices went straight into the frame's room */
  long c3v_calls, c3v_cut, c3v_cut_v;         /* sb14 3v: lane 1 df0 calls in two passes; those the cut left out before their second pass,
                                                and their vertices */
  long c3z_in, c3z_in_v;                      /* sb14 3z: df0 calls (and vertices) whose w the depth range took by the box's bound alone */
  long c3_chk, c3_chk_bad;                    /* sb14 check mode (xc_c3_chk): calls compared with the one-pass forms; differences */
  long c4_calls[3], c4_bad[3], c4_batches[3], c4_empty[3];   /* sb15 step 4 (XcHost.c4), by kind: sprites, quads, streaks: calls; bad
                                                input; batches; calls with nothing drawn */
  long c4_n[3], c4_drop[3], c4_clip[3];       /* sprites, quads, streak segments (vertices after the first of a strip); dropped (sprites,
                                                quads; streaks: class 2); sprites with a corner moved, quads and segments to the clipper */
  long c4_drawn[3], c4_poly, c4_poly_v, c4_poly_empty, c4_lines, c4_line_drop, c4_full;   /* drawn whole (sprites, quads, strip
                                                segments); the quads' clipped polygons kept, their vertices, empty; the streaks'
                                                clipped lines kept, dropped by the clipper; left out for want of room */
  long cull_n[16];          /* cull study v3 (XcHost.cull), kept last (the check mode compares the stats before them), by index: 0 25f0 /
                              2870 calls tested; 1 left out, 2 their vertices; kept: 3 vertex 0 inside every plane, 4 no plane proven by
                              the bound, 5 a bad attribute (the conversion's -4), 6 the count out of 2..1024, 7 the matrix or a position
                              out of vc_clip_in's range; check mode: 8 left-out calls converted anyway, 9 those whose result, counters or
                              a vertex's flags DIFFER from the claim (PROBLEM if not 0), 10 kept calls whose conversion drew nothing; 11
                              kept: a thinned call; 12 vertices the test read (vertex 0, then the whole call's in pass 1), 13 of them in
                              calls then kept; 14 left-out (or checked) calls' vertices whose conversion takes the clamped path
                              (XcHost.clipfast 0); 15 check mode: the left-out calls' vertices the conversion took through the clamps */
} XcStats;
const XcStats *xc_stats(void);
/* sb1, Stage B lane 1: xc_rec_chk 1 (dev) checks each P04+df0 call that came with its streams (XlCall.rec) against vc_df0 on the VU
   memory model, decoded for it (xl_vm_sync): xc_rec_n calls checked, xc_rec_diff of them not the same */
extern int xc_rec_chk; extern long xc_rec_n, xc_rec_diff, xc_rec_calls;
/* sb14 (dev, x86): xc_c3_chk 1 checks 3v and 3z on every df0 call they take: the two passes against vc_df0_rec (count, transform,
   vertices, flags, box), the flags read from the stream against the converted ones, and each box's bound against every vertex's w
   (XcStats c3_chk, c3_chk_bad) */
extern int xc_c3_chk;
/* sb2, Stage B lane 2: P06 calls whose vertices came from the lane's streams (xc_rec6_calls) and those that needed the VU memory
   (xc_rec6_vm: ge/vu1conv.h vc_p06_rec -8); with xc_rec_chk, xc_rec6_n of the first checked against vc_p06, xc_rec6_diff not the same */
extern long xc_rec6_calls, xc_rec6_vm, xc_rec6_n, xc_rec6_diff;
/* sb5, Stage B lane 4: P07 calls whose vertices came from the slot's streams (xc_rec7_calls) and those that needed the VU memory
   (xc_rec7_vm: ge/vu1conv.h vc_p07_rec -8); with xc_rec_chk, xc_rec7_n of the first checked against vc_p07, xc_rec7_diff not the same */
extern long xc_rec7_calls, xc_rec7_vm, xc_rec7_n, xc_rec7_diff;
/* sb6, Stage B lane 3: lane 3's P04+25f0 / +2870 calls whose vertices came from the prim's streams (xc_rec3_calls) and those that
   needed the VU memory (xc_rec3_vm: ge/vu1conv.h vc_fpos_rec_ok 0); with xc_rec_chk, xc_rec3_n of the first checked against the VU memory,
   decoded: every quadword of the vertices xc_fpos reads (position, colour, UV of the header's count) the same, and for a call whose
   vertices are not all at one place vc_fpos's every output the same as vc_fpos_rec's; xc_rec3_diff not the same */
extern long xc_rec3_calls, xc_rec3_vm, xc_rec3_n, xc_rec3_diff;

/* 1: the call's batch in *b and its data in blob[0..*size); b->first: the batch's offset in the blob (0 but for P06 and P07), to
   which the host adds the blob's offset in its data block; 0: no batch (an entry without a converter, an MSCNT, or nothing drawn);
   < 0: bad input (vu1conv.h), -5: blob too small.  P06 and P07 (GERK_LIT): the batch is pass 1's, and the blob holds what passes 2
   and 3 draw too */
int xc_call(const XlCall *c, const XcHost *h, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size);
/* P06's or P07's pass 2 or 3 (the translator's pass callback, ge/xlate.h xl_on_pass, with the GS registers after that pass's A+D packet), right
   after xc_call gave the call's pass 1: 1, the pass's batch in *b, whose data is in the call's blob (blob: where it lies now; b->first
   its offset in the blob, as above; the pass's texture scale written there); 0: the pass is not drawn (counted in XcStats) */
int xc_pass(const XlCall *c, int pass, const XcHost *h, GerBatch *b, uint8_t *blob);
/* sb11 C1: the larger of bound (xc_hold_test's, for a lane 4 call it takes) and the most data the call takes in its blob drawn by C1
   (xconv.c xc_lit_c1): the room a host reserves for a held call while XcHost.c1 is on (ge/gelive.c gl_call) */
uint32_t xc_c1_hold_bound(const XlCall *c, uint32_t bound);
/* sb12 C2: the same for a lane 2 P06 call while XcHost.c2 is on (xconv.c xc_lit_c2) */
uint32_t xc_c2_hold_bound(const XlCall *c, uint32_t bound);

/* sb4, step c (renderer_notes 11.29): a P06 call of Stage B lane 2 (its vertices in the game's data, XlCall.rec) converted at the end of
   its transfer instead of at the call, the held calls one after another.  At the call the host asks xc_hold_test (no side effects); when
   it says 1 and the frame has room, xc_hold does what xc_call does before the converter (St.calls, the GS state, the texture look-up) and
   keeps what the converter reads besides the streams; xc_hold_pass at each pass the same for the pass (1: drawn).  At the transfer's end
   (ge/xlate.h xl_on_flush, before the game can change the streams' RAM) xc_held_run converts the call as xc_call would have (its batch
   and blob), and xc_held_pass gives each drawn pass's batch.  The batches and blobs are those of xc_call and xc_pass, byte for byte */
#define XC_HI 27   /* the record's image of the VU quadwords a held call reads besides the streams (xconv.c hi_io): abs 2 .. 5 in img[0 .. 3],
                      T + k in img[4 + k] for k 0 .. 6, 8 .. 0xb, 0x14, the skinned entries' 0xc .. 0xf, pass 3's 0x10 .. 0x12 (when drawn), and
                      pass 3's T + 0xfe, 0xff in img[25], img[26]: img[0 .. 24] is VU addresses 2 .. 0x1a, the memory of a call at T 6 */
#define XC_7S 14   /* sb10 T2: the slots of VU addresses 0 .. 63 a held P07 call reads (ge/xlate.h xl_vgen): 0, 1, 2 (abs 2 .. 11), 4 (16 ..
                      19), 6, 7 (24 .. 31), 8 .. 15 (the bones, 32 .. 63) */
typedef struct {
  uint32_t entry, top, hy, fogcol;   /* the call; header y (its pass flags; P07: T + 0 w); FOGCOL (xc_xform) */
  int np, slot, pslot[2], pk[2];     /* drawn passes so far, the batch slots of the call and of those passes, their numbers */
  uint32_t ng6;                      /* native step 2 (ge/nge.h): the call's people reference at the call (xc_nge_cur6), 0 none */
  union {
    uint32_t img[XC_HI][4] __attribute__((aligned(64)));   /* P06 (addresses 0, 1 of that memory are the record's 32 bytes before img,
                                                            pk and padding: P06 never reads them) */
    struct { const uint8_t *sp[XC_7S]; uint32_t t7[6][4]; } p7;   /* sb10 T2, P07: where each slot's 64 bytes are as the call saw them
                                                            (the game's data, or a copy in xconv.c's log; both kept to the transfer's
                                                            end), and T + 0 .. 3, T + 0xfe, 0xff */
  };
  XcGs d; XcTex t;                   /* the call's GS state and texture */
  XlRec rec;                         /* the call's streams (the game's data: valid to the transfer's end) */
  XcGs pd[2]; XcTex pt[2];           /* the drawn passes' (the first np) */
#ifndef __psp__
  uint32_t chk7[60][4];              /* sb10 T2, x86 check (xc_c7_chk): the quadwords a P07 call reads as they were at the call */
#endif
} XcHeld;
/* 1: c can be held (lane 2's P06 call; the header's count 1..64; the streams vc_p06_rec reads; ROW z 1.0; a strip to draw; sb10 T2:
   lane 4's P07 call as xconv.c hold7_test says), with *bound the most data its blob takes (xc_lit's -5 test) and *npd its passes 2 and
   3 that will be drawn; 0: it runs at the call */
int xc_hold_test(const XlCall *c, uint32_t *bound, int *npd);
/* sb10 T2: 1 when xc_hold has room for a P07 call's copies (else the host runs the held calls first); xc_hold7_reset: no call is held
   (gl_begin), so the copies go */
int xc_hold7_room(void);
void xc_hold7_reset(void);
/* sb10 T2 x86 check: xc_c7_chk 1: each held P07 call's record also keeps the quadwords it reads (chk7), and at the transfer's end the
   memory it is converted from is compared with them: xc_c7_n calls compared, xc_c7_diff not the same; xc_c7_held the P07 calls held,
   xc_c7_snap the slots copied into the log, xc_c7_put the slots placed in the scratch memory, xc_c7_rb the calls rebuilt (vc_p07_rec -8) */
extern int xc_c7_chk; extern long xc_c7_n, xc_c7_diff, xc_c7_held, xc_c7_snap, xc_c7_put, xc_c7_rb;
void xc_hold(const XlCall *c, const XcHost *h, XcHeld *r);   /* the call's work before the converter, and its record */
int xc_hold_pass(const XlCall *c, int pass, const XcHost *h, XcHeld *r);   /* a pass of the held call r: 1 drawn (its record kept), 0 not */
/* at the transfer's end, the held calls in their order: xc_held_run as xc_call (1: the batch in *b, its data blob[0..*size), which the
   host places as gl_call does; anything else is a fault: the host counts it), then xc_held_pass(r, k, ...) for each drawn pass k < r->np
   with the blob where it lies; xc_held_begin before the first and xc_held_end after the last (P06 / P07's pass state of xc_call kept) */
void xc_held_begin(void);
int xc_held_run(const XcHeld *r, const XcHost *h, GerBatch *b, uint8_t *blob, uint32_t cap, uint32_t *size);
void xc_held_pass(const XcHeld *r, int k, GerBatch *b, uint8_t *blob);
void xc_held_end(void);
/* checks (x86): xc_held_force 1: every held call through the rebuilt VU memory (vc_p06), as one whose vc_p06_rec says -8; xc_held_poison
   1: the record's image filled with a poison before the call's quadwords go in, and the scratch VU memory before each use */
extern int xc_held_force, xc_held_poison;
#endif
