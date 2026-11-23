/* nge_list.h: the native GE renderer's list words for cooked ship meshes, step 0 (native_ge_renderer_design.md 1.3).
   Per mesh: WORLD from the mesh's 384-byte constant block (mesh+0xFC as SPR 0xc54 points to it; q0..q3 the local -> clip matrix that
   the VU reads at T + 0..3), computed exactly as today's converter (ge/vu1conv.c vc_xform: the VU's float reading, x 32768 for 16-bit
   positions) so its 12 words are the bits today's list sends; the projection's two terms (pa, pb) likewise.  Per group: BASE and a
   CALL into the group's cooked list (nge_cook.h), then BASE back to the caller's value.  Plain C99; x86 twin and psp-gcc. */
#ifndef NGE_LIST_H
#define NGE_LIST_H
#include "nge_cook.h"

typedef struct { float world[12]; float pa, pb; } NgeXf;
/* the transform from the constant block at guest address blk (q0..q3); 0 when blk is not in RAM */
int nge_xf_block(const uint8_t *ram, uint32_t blk, NgeXf *x);
/* the 13 words WORLDN 0, WORLDD x 12 (each float's top 24 bits, as ge/gelist.c putf sends them) into w; returns 13 */
uint32_t nge_put_world(uint32_t *w, const NgeXf *x);
/* the words that draw cooked group g of arena a: BASE (the list's high bits), CALL (its low 24 bits), BASE base_back (the caller's,
   BASE's argument field); returns 3, or 0 when the group has no list (NGE_GF_*: it stays on today's path or draws nothing) */
uint32_t nge_put_call(uint32_t *w, const NgeArena *a, const NgeGroup *g, uint32_t base_back);
/* the GE address of arena offset off */
static inline uint32_t nge_ge(const NgeArena *a, uint32_t off) { return a->ge + off; }
#endif
