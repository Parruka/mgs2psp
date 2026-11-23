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
#endif
