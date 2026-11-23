/* nge_list.h: the transform of a cooked ship mesh at stub time (native_ge_renderer_design.md 1.3; step 0's nge_list.h).
   WORLD from the mesh's constant block (SPR 0xc54 points to it; q0..q3 the local -> clip matrix that the VU reads at T + 0..3), computed
   exactly as today's converter (ge/vu1conv.c vc_xform: the VU's float reading, x 32768 for 16-bit positions) so its 12 floats are the
   bits today's batch holds; the projection's two terms (pa, pb) likewise.  Plain C99; x86 twin and psp-gcc. */
#ifndef NGE_LIST_H
#define NGE_LIST_H
#include "nge_cook.h"

typedef struct { float world[12]; float pa, pb; } NgeXf;
/* the transform from the constant block at guest address blk (q0..q3); 0 when blk is not in RAM */
int nge_xf_block(const uint8_t *ram, uint32_t blk, NgeXf *x);
#endif
