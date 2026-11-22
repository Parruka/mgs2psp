/* c3model.h (sb13, task #144; x86 only): Stage C step 3, the rooms' meshes (P1214a0 groups, VU1 entry P04+df0), sized before any GE
   code (c3model.c).  Not the "C3" of native_draw_study.md §3's table (a native path per call): here C3 is step 3, as C1 and C2 were
   steps 1 and 2. */
#ifndef MGS2_C3MODEL_H
#define MGS2_C3MODEL_H
#include <stdint.h>
#include <stdio.h>
#include "vu1conv.h"
#include "xlate.h"
#include "xconv.h"

extern int c3m_on;                 /* MGS2_C3_MODEL=1 */
extern long c3m_vb;                /* the vblank the run is in (boot/xlive_chk.c sets it) */
extern const uint8_t *c3m_ram;     /* the EE RAM (boot/xlive_chk.c), for the streams' guest addresses */
/* one df0 call as xc_call has read it (n, v, fl, x: vc_df0_rec's or vc_df0's results), before the cut and the depth range use it; off:
   1 when XcHost.cut leaves it out.  Reads only */
void c3m_call(const XlCall *c, const XcGs *d, const XcHost *h, int n, const VcVert *v, const uint16_t *fl, const VcXform *x, int off);
void c3m_kind(int k);              /* every converter call xc_call takes: 0 df0, 1 25f0, 2 2870, 3 P06, 4 P07 */
void c3m_report(FILE *f);
void c3m_vblank(void);             /* each vblank, after c3m_vb (MGS2_C3_WATCH: the watched pages read-only again) */
#endif
