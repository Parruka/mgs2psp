/* nge_cook.h: the native GE renderer's cooker (native_ge_renderer_design.md 2.1; format nge_fmt.h).  Step 0's cooker with step 1's
   changes: the cooked mesh goes into one block the caller gives (a dry run first gives its size), the strips are kept after each list,
   and each group keeps its first drawn vertex (i1) and its vertex alpha.
   One mesh (a DG_OBJ, obj + 0xC0 + k 0x130; layouts re/dg_objects.md 2-3) at a time, from the game's own data in guest RAM:
   positions (stream A, mesh+0xDC), colours and ADC flags (stream B: mesh+0xE0), UVs (stream C, mesh+0xE4), the group descriptors
   (mesh+0xF4).  Pre-lit groups come out byte for byte as today's per-frame converter writes them (ge/vu1conv.c df0_pos + vc_df0_att,
   streams without ROW), with the strips of ge/vu1conv.c vc_strips; lit groups in the design's GE-lighting form (not drawn in step 1).
   Reads guest memory only through ram (the 32 MB image, addresses masked); writes only the block.  Host-only work: nothing it does
   changes a guest write, so a cook never moves the game's state.  Game data: personal use only. */
#ifndef NGE_COOK_H
#define NGE_COOK_H
#include "nge_fmt.h"

#define NGE_RAM_MASK 0x1ffffffu
/* a block of cap bytes at mem (region offset base, GE address of the region's offset 0: ge); mem 0 for a dry run */
static inline void nge_arena_at(NgeArena *a, void *mem, uint32_t cap, uint32_t base, uint32_t ge)
{ a->mem = (uint8_t *)mem; a->cap = cap; a->base = base; a->ge = ge; a->used = 0; }
/* cook mesh (guest address) with the vertex alpha alpha_ge (0..255, the GE's) into a's block, group array first (its region offset is
   a->base); m: the host's record.  NGE_OK, NGE_E_ROOM (the block is too small), NGE_E_BAD (counts or pointers out of range), NGE_E_OVR
   (a stream is overridden this frame, mesh+0xD0 low nibble).  A dry run (a->mem 0, a->cap ~0u) leaves the bytes it needs in a->used */
int nge_cook_mesh(NgeArena *a, const uint8_t *ram, uint32_t mesh, uint32_t alpha_ge, NgeMesh *m, uint8_t reg);
/* 1 when the mesh in RAM still has what m was cooked from (descriptor array, stream bases, the descriptors' sum, group count, pre-lit) */
int nge_mesh_same(const NgeMesh *m, const uint8_t *ram);
/* the sum the check uses */
uint32_t nge_desc_sum(const uint8_t *ram, uint32_t desc, int ng);
#endif
