/* nge_cook.h: the native GE renderer's cooker, step 0 (native_ge_renderer_design.md 2.1; format nge_fmt.h).
   One mesh (a DG_OBJ, obj + 0xC0 + k 0x130; layouts re/dg_objects.md 2-3) at a time, from the game's own data in guest RAM:
   positions (stream A, mesh+0xDC), colours and ADC flags (stream B: mesh+0xE0, the object's pre-lit colour buffer for pre-lit
   meshes) or normals, UVs (stream C, mesh+0xE4), the group descriptors (mesh+0xF4).  Pre-lit groups come out byte for byte as today's
   per-frame converter writes them (ge/vu1conv.c df0_pos + vc_df0_att, streams without ROW), with the strips of ge/vu1conv.c
   vc_strips; lit groups in the design's GE-lighting form (not today's picture: those are judged by shots).
   Reads guest memory only through ram (the 32 MB image, addresses masked); writes only the arena.  Host-only work: nothing it does
   changes a guest write, so a cook (at stage load or on a miss) never moves the game's state.  Game data: personal use only. */
#ifndef NGE_COOK_H
#define NGE_COOK_H
#include "nge_fmt.h"

#define NGE_RAM_MASK 0x1ffffffu
/* the arena: cap bytes at mem, whose GE address is ge (16-byte aligned; not across a 16 MB boundary) */
void nge_arena_init(NgeArena *a, void *mem, uint32_t cap, uint32_t ge);
void nge_arena_reset(NgeArena *a);   /* a stage change: every cooked mesh gone */
static inline NgeMesh *nge_mesh_at(const NgeArena *a, uint32_t off) { return (NgeMesh *)(void *)(a->mem + off); }
static inline NgeGroup *nge_groups(const NgeArena *a, const NgeMesh *m) { return (NgeGroup *)(void *)(a->mem + m->grp); }
/* cook mesh (guest address) with the vertex alpha alpha_ge (0..255, the GE's: PS2 alpha doubled, 0x80 -> 0xff); *off = the NgeMesh's
   arena offset.  NGE_OK, NGE_E_ROOM (the arena is full: nothing kept), NGE_E_BAD (counts or pointers out of range), NGE_E_OVR (a stream
   is overridden this frame, mesh+0xD0 low nibble: cook it on a frame without) */
int nge_cook_mesh(NgeArena *a, const uint8_t *ram, uint32_t mesh, uint32_t alpha_ge, uint32_t *off);
/* 1 when the mesh in RAM still has what m was cooked from (descriptor array, stream bases, the descriptors' sum, group count) */
int nge_mesh_same(const NgeMesh *m, const uint8_t *ram);
/* the sum the check uses */
uint32_t nge_desc_sum(const uint8_t *ram, uint32_t desc, int ng);
/* the arena moved to GE address new_ge (its bytes copied there by the caller): every cooked list's BASE and VADDR words rewritten */
void nge_relocate(NgeArena *a, uint32_t new_ge);
/* bytes one mesh would take (the census: no arena needed); 0 when it cannot be cooked */
uint32_t nge_mesh_bytes(const uint8_t *ram, uint32_t mesh, uint32_t *nv, uint32_t *ngr, uint32_t *nlit);
#endif
