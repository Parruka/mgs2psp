/* nge.h: native step 1 (MGS2_NGE; attic/ge_native/step1_results.md): what the stub (boot/nge_stub.h, the main CPU), the walk (ge/xlme.c,
   boot/nat_x86.c), the converter (ge/xconv.c), the list writer (ge/gelist.c) and the checks (ge/gelive.c, boot/xlive_chk.c) share.
   The stub cooks a recorded P04+df0 group's mesh into the arena (nge/nge_fmt.h) and gives the group a reference (NgeRef) in its packet
   buffer's array, parallel to the NatGrp array, with the call's WORLD computed from its constant block (NgeXf).  The converter makes a
   native batch for a referenced group: its GerXform (pad NGE_MAGIC, nv and nst the cooked group's, no indices) followed by an NgeTail;
   the list writer draws it with WORLD (when it changes) and BASE / CALL into the cooked list.  With MGS2_NGE off nothing is referenced
   and every path is today's.  ME-safe: data and inline functions only (the arrays and regions are set by the host before the ME starts). */
#ifndef GE_NGE_H
#define GE_NGE_H
#include <stdint.h>
#include "../nge/nge_fmt.h"
#include "../nge/nge_list.h"

#define NGE_MAGIC 0x3145474eu   /* GerXform.pad of a native batch ("NGE1") */
#define NGE_MAXCALL 512         /* stub calls with native groups a packet buffer's transform array holds in a picture (more: today's path) */
typedef struct { uint32_t g; uint16_t xf, pad; } NgeRef;   /* g: NGE_RO of the cooked NgeGroup (0: today's path); xf: the call's NgeXf */
#define NGE_RO(r, o) ((uint32_t)(r) << 28 | (uint32_t)(o))   /* a region (0, 1) and an offset in it (never 0: the regions' first 64 B unused) */
typedef struct { uint32_t g, list_ge, vb, runs; } NgeTail;   /* after a native batch's GerXform: the group, its list's GE address, its
                                                               vertices and strips (NGE_RO each) */
/* ge/xconv.c's (the ME's .me_bss on the PSP; set by the host before the ME starts) */
extern uint8_t *xc_nge_mem[2];          /* the regions as the CPUs address them */
extern uint32_t xc_nge_ge[2];           /* their GE addresses */
extern NgeRef *xc_nge_ref[2];           /* each packet buffer's references (NAT_MAXGRP), 0: MGS2_NGE off */
extern NgeXf *xc_nge_xfa[2];            /* each packet buffer's transforms (NGE_MAXCALL) */
extern const NgeRef *xc_nge_cur;        /* the walk's: the group being converted has this reference (0: none) */
extern int xc_nge_cb;                   /* its packet buffer */
/* the converter's counts (xc_nge_n): native batches made, cut, the referenced groups it declined (today's path), and its check mode's
   differences (x86, MGS2_NGE_CHK=1: each native group against today's input, vc_df0_rec) */
enum { NGN_TRY, NGN_MADE, NGN_CUT, NGN_EMPTY, NGN_NO_ROW, NGN_NO_COUNT, NGN_NO_ALPHA, NGN_NO_CHK, NGN_CHK, NGN_D_COUNT, NGN_D_WORLD, NGN_D_VERT,
       NGN_D_STRIP, NGN_D_LIST, NGN_D_BOX, NGN_D_I1, NGN_D_XFORM, NGN_D_FRESH, NGN_W_ALL, NGN_N };
#define NGN_NAMES { "referenced groups converted", "native batches", "cut", "nothing drawn", "declined: a stream with ROW or another kind", \
  "declined: count", "declined: alpha", "declined: another check mode on", "check: compared", "check: count or input differs", \
  "check: WORLD or projection differs", "check: vertices differ", "check: strips differ", "check: list words differ", "check: box differs", \
  "check: first drawn vertex differs", "check: the batch's transform words differ", "check: the converter's transform differs from a fresh one", \
  "native batches whose stub-time WORLD or pa/pb differs from the converter's (every platform)" }
extern long xc_nge_n[NGN_N];
extern int xc_nge_chk;                  /* x86: MGS2_NGE_CHK */
static inline const void *nge_p(uint32_t ro) { return xc_nge_mem[ro >> 28 & 1] + (ro & 0x0fffffffu); }
static inline void nge_walk(int b, int i) { const NgeRef *r = xc_nge_ref[b]; xc_nge_cur = r && r[i].g ? &r[i] : 0; xc_nge_cb = b; }
static inline void nge_walk_end(void) { xc_nge_cur = 0; }
/* ---- native step 2 (attic/ge_native/step2_results.md): the people, C2 cooked per model.  The stub (boot/np6_stub.h) cooks a people
   mesh's groups once (a miss) into the arena with ge/vu1conv.c's own vertex loop (vc_p06_c2v, at the material-free values: alpha 255,
   no ADC) and gives each recorded group (ge/natp06.h, the buffer's array index) a reference to its cooked group (xc_nge_ref6).  The
   converter (ge/xconv.c xc_lit_c2), for a call whose group has one, takes the vertices, strips, UV2 form and box from the cook and does
   per call only what depends on the frame: the d of the blended vertices (skinned, weight not 1.0: the frame's bones; vc_p06_c2bl), the
   alpha when it is not 255, the depth range's w (vc_p06_c2z, when needed) and ADC (nothing drawn).  N-ref: no blended vertex and alpha
   255: the batch's GerC2 is followed by an NgeC2Tail and the GE reads the cooked vertices (GerLit.pass | GERL_NGE); else N-copy: today's
   batch, its vertices copied from the cook and those parts redone.  A group without a reference, or whose cook does not match the call
   (count, header y, skin), takes today's path. */
#define GERL_NGE 0x40u          /* GerLit.pass: N-ref (the pass's vertices in the arena: the NgeC2Tail after its GerC2) */
typedef struct {                /* one cooked people group (48 B), in the arena */
  uint32_t vb;                  /* region offset of VB1 (n st bytes, then zeros to 16) and, uv2 2, VB2 (pass 2's own: UV2) after it */
  uint32_t runs;                /* region offset of its strips: nst (uint16 first vertex, uint16 count), of the flags without ADC */
  uint32_t bl;                  /* region offset of the blended vertices' indices (nb bytes), 0: none */
  int32_t du, dv;               /* uv2 1: pass 2's UV offset (L.du, L.dv) */
  uint16_t n, nst;              /* vertices; strips */
  uint16_t hy, nb;              /* the header's y it was cooked for; blended vertices */
  uint8_t st, skin, uv2, cull;  /* the stride (20, 24); 1 skinned; L.uv2; L.cull */
  uint8_t ok, reg, pad[2];      /* 1: drawn natively; the region */
  int16_t blo[3], bhi[3];       /* vc_p06_c2box: the positions' box (skinned: the depth range's bound) */
} NgeP6G;
typedef struct { uint32_t vb[2], og, orun; } NgeC2Tail;   /* N-ref, after the GerC2: VB1 and pass 2's VB (NGE_RO each); the blob offsets
                                                             of the GerC2 and of the strips today's batch would have (the digest's) */
#define NGE_P6REF 1024          /* the people references a packet buffer holds (ge/natp06.h NP6_MAXGRP) */
extern uint32_t *xc_nge_ref6[2];        /* each packet buffer's people references (NP6_MAXGRP; NGE_RO of the NgeP6G, 0: today's path) */
extern uint32_t xc_nge_cur6;            /* the walk's: the people group being converted has this reference (0: none) */
extern uint32_t xc_nge_cap;             /* each region's bytes (the list's digest: a pointer into a region) */
enum { NG6_TRY, NG6_REF, NG6_COPY, NG6_EMPTY, NG6_NO_MATCH, NG6_NO_CHK, NG6_BLEND_V, NG6_ALPHA, NG6_ZW, NG6_CHK, NG6_D_VERT, NG6_D_RUN,
       NG6_D_L, NG6_D_Z, NG6_N };
#define NG6_NAMES { "people groups with a cook converted", "N-ref batches", "N-copy batches", "nothing drawn (ADC or no strip)", \
  "declined: the cook does not match the call", "declined: another check mode on", "blended vertices redone", \
  "batches with alpha not 255", "depth range w computed from the cook", "check: compared", "check: vertices differ", \
  "check: strips differ", "check: uv2, du, dv, amin or cull differ", "check: depth range differs" }
extern long xc_nge6_n[NG6_N];
static inline void nge_walk6(int b, uint32_t i) { const uint32_t *r = xc_nge_ref6[b]; xc_nge_cur6 = r && i < NGE_P6REF ? r[i] : 0; }
static inline void nge_walk6_end(void) { xc_nge_cur6 = 0; }
/* the walk's (ge/xlme.c lw_nat6): the reference of group i from the references the record carries (XmPic.np6_r: the copy made with the
   groups at the record, or the main CPU's array for a walk from RAM, which the main CPU waits for), never the main CPU's array later */
static inline void nge_walk6r(const uint32_t *r, int i) { xc_nge_cur6 = r ? r[i] : 0; }
/* a C2 pass's vertices: in the blob after its GerC2 (today's), or N-ref the arena's */
static inline const uint8_t *nge_c2v(uint32_t pass, const void *g, uint32_t gsz, int32_t voff)
{
  if (pass & GERL_NGE) return (const uint8_t *)nge_p(((const NgeC2Tail *)(const void *)((const uint8_t *)g + gsz))->vb[(pass & 15) == 2]);
  return (const uint8_t *)g + voff;
}
/* gl41: a 16-bit digest of a transform's WORLD and projection terms: the stub keeps its call's in each reference (NgeRef.pad), so the
   converter compares it with its own (NGN_W_ALL) from the reference alone, which the record copies (the transform array it is not) */
static inline uint16_t nge_xf_h16(const float *w12, float pa, float pb)
{
  union { float f; uint32_t u; } c; uint32_t h = 2166136261u;
  for (int k = 0; k < 12; k++) { c.f = w12[k]; h = (h ^ c.u) * 16777619u; }
  c.f = pa; h = (h ^ c.u) * 16777619u; c.f = pb; h = (h ^ c.u) * 16777619u;
  return (uint16_t)(h ^ h >> 16);
}
/* gl41: the arena room for a record's copy of n groups of buffer b: the NatGrps and, with native step 1 set up, their references */
#define NGE_NAT_ROOM(b, n) ((uint32_t)(n) * ((uint32_t)sizeof(NatGrp) + (xc_nge_ref[b] ? (uint32_t)sizeof(NgeRef) : 0u)))
/* gl41: the reference of group i of a walk whose groups start at index i0 of buffer b: from rf (the record's copy of its groups'
   references, taken with the groups when the loop runs apart from the main CPU: the main CPU refills its packet buffers for a later
   picture before such a walk) or, rf 0, from the buffer in place (a walk the main CPU waits for, or the x86 twin's) */
static inline void nge_walk_r(int b, const void *rf, int i0, int i)
{
  const NgeRef *r = rf ? (const NgeRef *)rf + i : xc_nge_ref[b] ? xc_nge_ref[b] + i0 + i : 0; xc_nge_cur = r && r->g ? r : 0; xc_nge_cb = b;
}
#endif
