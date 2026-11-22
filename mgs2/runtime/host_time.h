/* PSP timing regions (psp/main_psp.c): the host time spent in the IOP / sound model, in the walk of the GS packets and in the game's draw
   pipeline, kept apart from the rest of the game's EE work (the GE renderer replaces the draw pipeline and the GS walk; the IOP / sound side
   has its own plan); MGS2_XL / MGS2_GL builds add the translator's and the picture's.  Exclusive: a region entered inside another one
   pauses it.  Nothing on x86. */
#ifndef MGS2_HOST_TIME_H
#define MGS2_HOST_TIME_H
#include <stdint.h>
#define HOST_IOP 0
#define HOST_GS 1
#define HOST_DG 2                       /* boot/dispatch.h: the call of 0x11da90 (DG_ActLast candidate, decomp_names_sles.md): the draw pipeline */
#define HOST_XL 3                       /* MGS2_XL builds: the live packet translator (psp/xl_psp.c; renderer Stage A) */
#define HOST_GL 4                       /* MGS2_GL builds: the live picture's own work (psp/xl_psp.c): the converters' calls, a frame's
                                           end, its GE list, the hand-over to the GE and the display (the frame builder's work on the GS
                                           primitives counts where the translator does: HOST_XL, or HOST_GS in the one walk) */
#define HOST_GW 5                       /* MGS2_GL builds: the CPU waiting for the GE to finish the frame before */
/* exact host-side speedups that can be switched off at run time (boot.c g_opt, all on by default): they leave the game state unchanged,
   so the PSP harness switches them per window of deck vblanks and times each against the others in one run */
#define OPT_GS_BLOCK 1u                 /* ee_io.h dma_run: the GS-side walk skips data in bulk */
#define OPT_POLL_DUE 2u                 /* kernel.h k_poll: the alarm / vblank scan only once an event is due */
#define OPT_VIF0_FAST 4u                /* ee_io.h VIF0: an MPG that rewrites the same words leaves the program hash valid; V4-32 / V4-16
                                           UNPACK without mask or skipping in bulk; the DMA collects its data in blocks */
#define OPT_SPR_BULK 8u                 /* ee_io.h io_dma_start: scratchpad DMA copies whole runs of quadwords at once */
#define OPT_NATIVE 16u                  /* boot/native_w00a.c: the native forms (line_sph, the deck's rain streaks); off: the translation */
#define OPT_ZT 32u                      /* boot.c rt_dg_call: the draw pipeline takes no game time (Ffm 2026-10-21; it changes the game
                                           state, so it is not an exact speedup: off = the clock before v20, for the old references only) */
#define OPT_LITE 64u                    /* boot.c rt_dg_call: with OPT_ZT, a step that is not shown runs the reduced draw pipeline
                                           (boot/dg_lite.h, gen_pre.h GEN_LITE).  On by default since v21: from boot the game reads the
                                           same bytes but for leftovers in vector lanes it never uses, and the shown frames are the same
                                           but for the deck's motion trail, which samples the frame before (speed_notes §34) */
#define OPT_GS_FAST 128u                /* ee_io.h vif1_chain_fast: the VIF1 chain walk in one loop (with OPT_GS_BLOCK); off: dma_feed */
#define OPT_NAT_KEY 256u                /* boot/native_elf.c: the sort key loop of P12dbc0 natively; off: the translation */
#define OPT_STAGE_TAB 512u              /* v26, boot/dispatch.h and psp/main_psp.c: a call through a register to a stage address skips
                                           gen_call's search (gen_call has no case there) and the overlay's name compare; off: both as v25f.
                                           (The stage's own lookup, tools/stage_tab.py's table, is the same either way.) */
#define OPT_XL 1024u                    /* MGS2_XL builds (psp/xl_psp.c): the live packet translator runs beside the exact walk, reading
                                           only, so the game state is the same either way; off: it does nothing (back on: it starts again) */
#define OPT_GL 2048u                    /* MGS2_GL builds (psp/xl_psp.c): with OPT_XL, the translator's shown frames become GE frames,
                                           drawn and displayed (reading only: the game state is the same either way); off: no frame is
                                           made (the translator still runs) and the last picture stays on the screen */
#define OPT_GL_ASYNC 4096u              /* MGS2_GL builds: the GE draws a frame while the game runs on (the CPU waits only before the
                                           next frame begins); off: the CPU waits for each frame at its hand-over (HOST_GW) */
#define OPT_XL_VMUD 8192u               /* MGS2_XL builds (ge/xlate.c xl_vm_mode): the translator's pending VU memory decoded UNPACK by
                                           UNPACK when something needs all of it (each converter call of the picture); off: all 1,024
                                           quadwords scanned, as before 2026-10-27.  The same VU memory either way */
#define OPT_GL_CUT 32768u               /* MGS2_GL builds (ge/xconv.h XcHost.cut): a df0 call whose vertices' box lies wholly off one side of
                                           its scissor is left out before its texture is made (it lights no pixel either way); off: every
                                           call is converted, as before 2026-10-27.  The same picture either way */
#define OPT_TK_BLK 16384u               /* MGS2_GL builds (ge/texcache.c tk_blk): a texture is converted a GS block at a time (ge/texconv.c
                                           tc_texture_blk, ge/gsup2.c gu_block); off: texel by texel, as before 2026-10-27.  The same
                                           bytes either way */
#define OPT_GU_MEMO 65536u              /* MGS2_XL builds (ge/gsup2.c gu_area_memo): an area identity made again walks a block's states only
                                           where they differ from the block's before it; off: every block's, as before 2026-10-30 (codec_speed.md
                                           step 1).  The same identities either way */
#define OPT_TK_FLAGS 131072u            /* MGS2_GL builds (ge/texcache.c tk_flags): a texture not converted is told from a render target by the
                                           map's flags alone (gu_area_flags); off: by its whole area identity, as before 2026-10-30.  The same
                                           answer either way */
#define OPT_GU_CLUT 262144u             /* MGS2_XL builds (ge/gsup2.c gu_clut_memo): the identity of CLUT entries 0..255 that one load put in
                                           place in order is kept with the load; off: worked out at each ask, as before 2026-10-30.  The same
                                           identities either way */
#define OPT_XL_V48 524288u              /* MGS2_XL builds (ge/xlate.c xl_ud_v48): V4-8 unsigned UNPACKs (one a P07 call) have their own decode;
                                           off: bit by bit (ud_any), as before 2026-10-30 (codec_speed.md step 2).  The same vectors either way */
extern uint32_t g_opt;
#if defined(__psp__) || defined(GEN_DG_TRACE)   /* (x86 GEN_DG_TRACE: boot.c keeps the draw pipeline's depth) */
void host_enter(int k);
void host_leave(int k);
#define HOST_ENTER(k) host_enter(k)
#define HOST_LEAVE(k) host_leave(k)
#else
#define HOST_ENTER(k) ((void)0)
#define HOST_LEAVE(k) ((void)0)
#endif
#endif
