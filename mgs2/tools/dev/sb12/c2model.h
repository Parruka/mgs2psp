/* c2model.h (sb12, task #140; x86 only): C2 for the people (P06) sized before any GE code (c2model.c) */
#ifndef C2MODEL_H
#define C2MODEL_H
#include <stdint.h>
#include <stdio.h>
#include "vu1conv.h"
extern int c2m_on;     /* MGS2_C2_MODEL=1 */
extern long c2m_vb;    /* the vblank the run is in (boot/xlive_chk.c sets it), for the model windows' volumes */
/* one converted P06 call: vm and top where the converter read its constants (and its vertices when sp is 0), entry its entry, sp the
   lane's streams (A, B, C, D, E, F) or 0 for the VU memory's vertices, n its vertices, L its record, v1 pass 1's vertices, v3 pass 3's
   (0 when not drawn) */
void c2m_call(const uint8_t *vm, uint32_t top, uint32_t entry, const void *const *sp, int n, const VcLit *L, const void *v1, const void *v3);
/* a P06 pass's GS state (xconv.c: pass 1 in xc_lit, passes 2 and 3 in xc_pass_do): TEX0's TFX and TCC, ABE and ALPHA (A | B << 2 |
   C << 4 | D << 6 | FIX << 8), FGE; tex: the pass has a GE texture */
void c2m_pass(int pass, int tme, int tfx, int tcc, int abe, uint32_t bl, int fge, int tex);
void c2m_report(FILE *f);
#endif
