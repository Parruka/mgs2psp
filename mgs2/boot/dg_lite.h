/* The reduced draw pipeline (speed_notes §33): on a step that will not be shown, boot.c rt_dg_call sets g_dg_lite and these translated
   functions return at once (gen_pre.h FN_ENTER with GEN_LITE).  Each one is checked on x86 first: the shown steps' packets and the game
   state (without renderer-only memory) stay the same.  The test folds to 0 in every other function. */
#ifndef DG_LITE_H
#define DG_LITE_H
#if defined(DG_LITE_DYN) && !defined(__psp__)   /* dev (x86): the set comes from MGS2_LITE_FN=<hex>[,<hex>...] (boot.c rt_lite_fn; unset: 0x1214a0) */
int rt_lite_fn(uint32_t a);
#define GEN_LITE_FN(a) (g_dg_lite && rt_lite_fn(a))
#else
#define GEN_LITE_FN(a) ((a) == 0x1214a0u)   /* the per-group mesh packet builder (opaque and sorted meshes, re/mesh_packets.md §3) */
#endif
#endif
