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
#define OPT_XL_LANE 1048576u            /* sb1, MGS2_XL builds (runtime/ee_io.h lane_p04, ge/xlate.c xl_lane_p04): Stage B lane 1, the walk hands
                                           a P1214a0 group to the translator whole and P04+df0 reads its streams from the game's data; off: word
                                           by word, as gl7.  The same picture either way */
#define OPT_XL_NEED 2097152u            /* sb1, MGS2_GL builds (ge/xlate.c xl_vm_need, ge/gelive.c gl_need): a converter call decodes only the quadwords
                                           of the VU memory model its converter reads (renderer_notes 11.22; exact but slower in the
                                           model, so psp/xl_psp.c reads this bit only in builds made with -DSB_NEED) */
#define OPT_XL_LANE2 4194304u           /* sb2, MGS2_XL builds (runtime/ee_io.h lane_p06, ge/xlate.c xl_lane_p06): Stage B lane 2, the walk hands
                                           a P12e8b8 group (P06) to the translator whole and P06 reads its vertices from the game's data; off:
                                           word by word.  The same picture either way */
/* sb3, the exact trims of renderer_notes 11.27 (task #111): each leaves the walk, the translator and the picture as they were.  (The
   fourth, io_flush's DMA start in its own function, is not switchable: it changes only which function the code is in) */
#define OPT_LANE_MATCH 8388608u         /* runtime/ee_io.h vif1_chain_fast: one look at the tag after a group's first material picks the one lane
                                           that can take it (lane_p04 needs STCYCL 3 / 1 there, lane_p06 1 / 1); off: lane_p04, then lane_p06 */
#define OPT_DIRECT_QW 16777216u         /* runtime/ee_io.h vif1_fast_words: DIRECT data goes to the GIF path a quadword at a time when one
                                           starts at a quadword's first word; off: word by word */
#define OPT_MPG_CMP 33554432u           /* runtime/ee_io.h vif0_run: an MPG's words compared with the program eight at a time; off: two */
#define OPT_CLIP_FAST 67108864u         /* MGS2_XL builds (ge/vu1conv.c vc_clip_fast): the effects' clip positions without the VU float clamps
                                           when every input is 0 or within 2^-20 .. 2^21 in size (then no product or sum can be denormal or
                                           overflow, and the clamps change nothing); off: clamped */
#define OPT_XL_STEPC 134217728u         /* sb4, MGS2_GL builds (ge/gelive.c gl_call, gl_flush; renderer_notes 11.29): step c, lane 2's P06 calls
                                           converted at the end of their transfer, one after another; off: at the call.  The same batches in
                                           the same order, their data placed elsewhere in the frame's data block */
#define OPT_XL_LANE4 268435456u         /* sb5, MGS2_XL builds (runtime/ee_io.h lane_p07, ge/xlate.c xl_lane_p07; renderer_notes 11.32): Stage B
                                           lane 4, the walk hands the codec faces' P07 slots to the translator whole and P07 reads its
                                           vertices from the game's data; off: word by word, as gl10.  The same picture either way */
#define OPT_XL_LANE3 536870912u         /* sb6, MGS2_XL builds (runtime/ee_io.h lane_fx, ge/xlate.c xl_lane_fx; renderer_notes 11.33): Stage B
                                           lane 3, the walk hands the sort pass's effect prims to the translator whole and P04+25f0 / +2870
                                           read their vertices from the game's data; off: word by word, as sb5.  The same picture either way */
#define OPT_XL_EAGER 1073741824u        /* sb8, MGS2_XL builds (ge/xlate.c ud_eager; task #121): the lanes' constant blocks, headers and
                                           materials copied into VU memory as they arrive; off: decoded when read, as gl11.  The same VU memory */
#define OPT_XL_MARK 2147483648u         /* sb8, MGS2_XL builds (ge/xlate.c ud_new; task #121): an UNPACK's WL 1 stride, or WL 2 with CL > 2,
                                           that stays below quadword 1024 marked without the wrap test, four or two entries a turn; off: as
                                           gl11.  The same table */
extern uint32_t g_opt;
/* sb9 (Stage C step 0, stagec_notes.md; task #128): g_opt's 32 bits are all taken, so a second word, all on by default (boot.c MGS2_OPT2
   for the whole run; MGS2_OPT2_CYCLE=<hex>,<hex>... a word for each of MGS2_OPT_CYCLE's configurations, taking turns with them) */
#define OPT2_TX_UMEMO 1u                /* MGS2_GL builds (ge/texcache.c tk_get_um, ge/gelive.c gl_tex): a texture found again by its upload
                                           records' identities (the pages its region reads, each one whole record) without the area identity
                                           and the cache's hash look-up; off: tk_get_cl, as gl12.  The same cache entries in the same order */
#define OPT2_GU_LDID 2u                 /* MGS2_XL builds (ge/gsup2.c gu_ld_ident): a CLUT load whose blocks hold records with the identities
                                           of a load kept by identity is that load again; off: a new load, as gl12.  The same identities */
#define OPT2_TX_FRONT 4u                /* sb10, Stage C step 1 T1 (stagec_notes.md 1.3), MGS2_GL builds (ge/texcache.c tk_ufront): a look-up
                                           the texture memo answered before, with no block of the map changed since, goes straight to its
                                           entries; off: the memo's own path, as gl13.  The same entries, counters and stamps */
#define OPT2_C7 8u                      /* sb10, Stage C step 1 T2 (stagec_notes.md, step 1), MGS2_GL builds (ge/gelive.c gl_call, ge/xconv.c
                                           hold7): lane 4's P07 calls held to the end of their transfer (step c for the codec faces); off:
                                           converted at the call, as gl13.  The same batches */
#define OPT2_C1 16u                     /* sb11, Stage C step 1 C1 (stagec_notes.md 1.9), MGS2_GL builds (ge/xconv.c xc_lit_c1, ge/gelist.c
                                           c1_draw): the codec faces' P07 calls skinned, lit and (pass 3) mapped by the GE itself where it
                                           can; off: today's exact vertices.  A different picture (the GE's rounding), so off by default
                                           (OPT2_DEFAULT) until Ffm's OK */
#define OPT2_C2 32u                     /* sb12, Stage C step 2 C2 (stagec_notes.md step 2), MGS2_GL builds (ge/xconv.c xc_lit_c2, ge/gelist.c
                                           c2_draw): the people's P06 calls skinned, lit and (pass 3) mapped by the GE itself where it can;
                                           off: today's exact vertices.  A different picture (the GE's rounding), so off by default until
                                           Ffm's OK */
#define OPT2_C3V 64u                    /* sb14, Stage C step 3 3v (stagec_notes.md step 3), MGS2_GL builds (ge/xconv.c xc_call, ge/vu1conv.c
                                           vc_df0_pos / vc_df0_att): a lane's df0 call converted in two passes, the positions and their box
                                           first, the colours, UVs and flags only for a call the cut keeps.  Exact: the same lists */
#define OPT2_C3Z 128u                   /* sb14, Stage C step 3 3z: a df0 call's depth range by its box when the box's whole range lies inside
                                           the range already kept (ge/xconv.c xc_df0_zin, as C1 and C2).  Exact: the same ranges */
#define OPT2_C3W 256u                   /* sb14, Stage C step 3 3w (with 3v): a two-pass df0 call's vertices written straight into the frame's
                                           room (ge/xconv.c xc_call, xc_df0), not into a buffer and then copied.  Exact: the same lists */
#define OPT2_C4 512u                    /* sb15, Stage C step 4 (stagec_notes.md step 4), MGS2_GL builds (ge/xconv.c xc_c4, ge/gelist.c
                                           fx_draw): the effects the picture left out drawn, P04+3740's sprites, +3a38's quads and +2b60's
                                           streaks; off: not drawn, as gl16.  A different picture (more of the game's), so off by default
                                           until Ffm's OK.  NOTE: a cycle word written for gl16 (ffffffdf ...) has this bit set */
#define OPT2_C4X 1024u                  /* sb15 4x (stagec_notes.md 4.7), MGS2_GL builds (ge/xconv.c xc_fpos): P04+25f0 / +2870's effect strips
                                           drawn from the clip positions the micro program computes, the GE's world the identity (as step 4's
                                           sprites), not their world positions through the call's matrix, which the GE holds in 24-bit floats
                                           (up to 4.6 px off in the opening's rain); and an untextured call's stale UV q not refused (the
                                           deck's flat triangles at vb 24,467-24,947).  A different picture (closer to the PS2's), so off by
                                           default until Ffm's OK */
#define OPT2_G0 2048u                   /* sbg0, G0 (task #165, rain_study.md section 6), boot/native_w00a.c (the private nat/ copy): line_sph's
                                           prims done natively without placing their vertices (the seed jumped, the polls, registers and
                                           the scratchpad as the translation leaves them); off: placed in place as since v18.  The game's
                                           state differs only in memory the renderer alone reads, so off by default until it is proven.
                                           NOTE: gl16 / gl17 cycle words (fffff9df, ffffffdf) have this bit set */
#define OPT2_G0H 4096u                  /* sbg0, with OPT2_G0: those prims hidden from the draw pipeline (+164 |= 0x3000, as the game hides
                                           them itself), so it builds no packets from their unplaced vertices */
#define OPT2_H3 8192u                   /* sbg0, H3 (task #165, rain_study.md section 6 "Next"): body_sph's, charaspread's and rain_gas_pers's
                                           prims hidden from the draw pipeline in their acts' show paths (the private wpl/gen_s_w00a.h, H3_WR);
                                           their acts' work unchanged.  NOTE: gl16 / gl17 cycle words have this bit set too */
#define OPT2_NBS 16384u                 /* sbg0, task #165 (rain_study.md section 8): body_sph's parts done natively (boot/native_w00a.c, the
                                           private nat/ copy, nat_bs_part; the hook in the private wpl/gen_s_w00a.h): the same VU0 operations,
                                           draws, memory, registers and time as the translation.  NOTE: gl16 / gl17 cycle words have this bit set */
#define OPT2_G0C 32768u                /* sbg0, task #165 (rain_study.md section 9): charaspread's particle sets skipped natively (boot/native_w00a.c,
                                           the private nat/ copy, nat_cs_sets; the hook in the private wpl/gen_s_w00a.h), only with OPT2_H3 and both
                                           prims hidden: the lives, births, draws, game random calls, registers, frames and time as the translation
                                           leaves them; the positions, velocities and packets (which only the hidden prims' drawing reads) left as
                                           they are.  Once on, keep H3 on for the rest of the run.  NOTE: gl16 / gl17 cycle words have this bit set */
#define OPT2_H3R 65536u               /* sbg0, task #167 (rain_study.md section 10): rain.c's streak prims hidden from the draw pipeline (the private
                                           wpl/gen_s_w00a.h H3R_WR, h3r_second): the first set's show path keeps the hide bits, the second set gets bit 12
                                           each act call and back with the bit off.  Only with the streaks not drawn (MGS2_GL_STREAKS=0) is the
                                           picture the same.  NOTE: gl16 / gl17 cycle words have this bit set */
#define OPT2_G0R 131072u              /* sbg0, task #167 (rain_study.md section 10): with OPT2_H3R, rain.c's streak loop does its entries but the
                                           last in place in RAM (boot/native_w00a.c g0r_call / g0r_entry; the hooks in the private wpl/gen_s_w00a.h): no
                                           copies through the scratchpad and no copy of the hidden second set's vertices; the vertex maths (VU0's random
                                           numbers) and the first set's buffers as the translation leaves them.  Once on, keep H3R on for the rest of
                                           the run.  NOTE: gl16 / gl17 cycle words have this bit set */
#define OPT2_M5 262144u               /* gl39 M5 (runtime/kernel.h k_take_interrupts): the pending interrupts taken by a walk of the set bits
                                           (count trailing zeros), lowest channel first, INTC before DMAC, each bit read again after the
                                           handler before: the same handlers in the same order as the loops over all 26 channels.  Exact */
#define OPT2_P1 524288u               /* gl41 P1 (attic/gl41s; runtime/vu0.h vu_run_shared): the VU0 micro-program call through a 64-entry
                                           (image hash, entry) cache straight to the program's VFPU / C form; a miss or a changed image takes
                                           gl39's path, which fills the entry.  The same functions in the same FPU-mode test.  Exact */
#define OPT2_P3 1048576u              /* gl41 P3 (attic/gl41s; gen_vu.h made by tools gl41_genvu.py): the box cull's VFPU form
                                           (vupv_96936c0f_0) without its 8 per-corner NaN / Inf checks: each checked register is only
                                           multiplied (lanes z, w kept) before its next check, so a non-finite lane is still caught there
                                           and the call falls back as before.  Exact */
#define OPT2_B43 2097152u             /* gl43 B (Ffm's rain option "B", 2026-11-22 13:03/13:05; attic/gl43; boot/native_b43.h): the aft deck's
                                           floor and falling splashes keep only the seed's draws, the time and their slots' lives, and their
                                           prims and ripple.c's stay hidden.  A visible change (DEVIATIONS.md B entry): off by default */
#define OPT2_DEFAULT (~(OPT2_C2 | OPT2_C4 | OPT2_C4X | OPT2_G0 | OPT2_G0H | OPT2_H3 | OPT2_NBS | OPT2_G0C | OPT2_H3R | OPT2_G0R | OPT2_B43))   /* gl43: and B43; g_opt2 at the start: every switch on but C2, C4, C4X, G0, G0H, H3, NBS, G0C, H3R and G0R (sbg0: G0, H3, NBS, G0C, H3R and G0R until proven; sb15: C4 and C4X until Ffm's OK; sb12 for gl15:
                                           C1 on by default, my call after gl14's run, which Ffm left to me on 2026-11-02 17:12; sb11 / gl14:
                                           ~OPT2_C1) */
extern uint32_t g_opt2;
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
