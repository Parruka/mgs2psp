/* natsplit.h (gl34): the ME's time inside the native ship path and the walk, for the turn lines "nat split" and "me chains"
   (psp/xl_psp.c; MGS2_NAT_SPLIT=1).  Logs only: nothing here changes what is drawn.
   The native path's stages are timed on one group in NS_EVERY (ge/xlme.c lw_nat picks it and sets g_ns.clk for that group only): each
   NS_MARK(k) adds the ticks since the mark before to stage k, so a stage's figure is the code between two marks plus one clock read.
   Every other group pays one load and one branch a mark.  The clock is the walk's (the ME's Count through ME_CALLP; 0 in a turn the
   main CPU walks: not timed).  cal_t / cal_n: pairs of back-to-back reads (one pair each merge run), the cost of a read.
   The chains: the game sends each chain of the frame's list (re/mesh_packets.md 1: {kind, start} at [0x1b9348] + 12 + 16 i) as a
   VIF1 transfer of its own, the first while the list is still the frame's [C twin, vb 29,850-29,852: 12 transfers, each tadr a
   start of the list read at the first].  The main CPU reads the list at a transfer whose tadr is the list's first start, keeps
   it, and sends each record its transfer's place in it (XmeRec.chi; -1: not in the list).  The walk reads the clock at a
   transfer's start, before its xl_flush (the held calls' conversion: slot NS_CH_FLUSH) and at its end: three reads a transfer.
   NS_CH_OTHER: the transfers not in the list (and every transfer of another channel). */
#ifndef MGS2_NATSPLIT_H
#define MGS2_NATSPLIT_H
#include <stdint.h>
enum { NS_PTR,     /* the walk's part of a group: its record, the five pointers (ge/natp04.h), up to the translator's entry */
       NS_REG,     /* the translator's front end: the group's UNPACKs registered (block, material, header copied; the streams' descriptors) */
       NS_AD,      /* the call's A+D packet: the GIF, the GS registers, the CLUT tracking */
       NS_PRE,     /* the rest of the call before the converter: PRIM from the tag template, the call's record */
       NS_GS,      /* the host's call (gelive.c gl_call) up to the converter and its GS state (xconv.c xc_gs) */
       NS_XF,      /* the group's transform from the constant block (vu1conv.c vc_xform) */
       NS_POS,     /* per vertex: the positions and their box (vc_df0_pos) */
       NS_CUT,     /* the cut's test (xc_df0_off) */
       NS_ATT,     /* per vertex: UVs, colours, flags (vc_df0_att) */
       NS_TEX,     /* the texture's lookup (texcache) */
       NS_STRIP,   /* per vertex: the strips and their indices */
       NS_GX,      /* per group: the GE transform, the batch's data laid, its state */
       NS_ZR,      /* the depth range */
       NS_LIST,    /* the batch into the frame (gebatch.c gb_xform_in) */
       NS_BACK,    /* the way back to the translator (a cut call: with its depth-range note) */
       NS_DREW,    /* the drawn areas' tracking (gsfront.c gf_drew) */
       NS_END,     /* back in the walk: the group's end */
       NS_N };
/* gl38, the people split: a P06 group of lane 2 (Snake, the crew, the guards: chain 2) timed by stage as a ship group is, one group in
   about NS_P_EVERY (ge/xlme.c picks it: g_ns.clk set and g_ns.tbo = NS_PN for that group, so its marks add to the people's stages,
   t[NS_PN + k]).  At the walk: NS_PTR the tags and pointers (lw_p06) up to the translator's front end, NS_REG the group's UNPACKs
   registered (xl_lane_p06), NS_AD the A+D packet, NS_PRE the call's rest before the host (PRIM, T + 0 .. 0x14 decoded), NS_P_TEX the
   host's call up to the hold: the test, the GS state and the texture's look-up (xc_hold), NS_P_HOLD the call's image kept (the hold's
   copy and slot), NS_BACK the way back, NS_P_PASS the passes' look-ups (2 and 3), NS_DREW the drawn areas, NS_END the group's end.  A
   call that is not held runs the converter at once (NS_GS .. NS_LIST as a ship group's, in the people's stages).  At the flush (the
   transfer's end: gelive.c gl_flush) the held call picked last is timed again: NS_P_C2 the header, bones and lights (vc_p06_c2),
   NS_P_VTX the vertices (vc_p06_c2v), NS_P_BLOB the strips, the GE transform and the passes' records, NS_P_FILL the batch into the
   frame, NS_P_PFILL its passes.  pn / phn: groups timed at the walk / at the flush */
enum { NS_P_TEX = NS_N, NS_P_HOLD, NS_P_PASS, NS_P_C2, NS_P_VTX, NS_P_BLOB, NS_P_FILL, NS_P_PFILL, NS_PN };
#define NS_P_EVERY 16
#define NS_EVERY 32          /* about: the pick's step is 24 .. 39 groups (lw_nat), so that it does not keep time with the calls' groups */
#define NS_CH 24             /* chains of a frame's list kept (12 seen in the rain) */
enum { NS_CH_FLUSH = NS_CH, NS_CH_OTHER, NS_CH_N };
typedef struct {
  uint32_t (*clk)(void);     /* the timed group's clock; 0: no group is being timed */
  uint32_t last;
  uint64_t t[2 * NS_PN];     /* ticks by stage (the timed groups only); gl38: [NS_PN + k] the people's stages */
  int tbo;                   /* gl38: 0, or NS_PN while a people group is timed (the marks' bank) */
  int pcd, ph_i;             /* gl38: people groups to the next pick; the held call (GlFrame.held's index) to time at the flush, -1: none */
  long pn, phn, pseen;       /* gl38: people groups timed at the walk, at the flush; lane 2's groups in the timed walks */
  uint32_t (*pclk)(void);    /* gl38: the clock of the walk that picked ph_i */
  long n, nv, ncut;          /* timed groups, their vertices (the header's counts), those the cut left out */
  long seen;                 /* groups of the timed walks' merge runs */
  int cd;                    /* groups to the next pick */
  uint64_t cal_t; long cal_n;
  /* the chains */
  int ch_on, ch_cur; uint32_t ch_last; long ch_c0, ch_g0;
  uint64_t ch_t[NS_CH_N], ch_nat[NS_CH_N]; long ch_calls[NS_CH_N], ch_grp[NS_CH_N], ch_n[NS_CH_N];
} NatSplit;
extern NatSplit g_ns;
#include "me_rt.h"            /* ME_CALLP */
#define NS_CLK() ME_CALLP(g_ns.clk)()
#define NS_MARK(k) do { if (g_ns.clk) { uint32_t ns_t_ = NS_CLK(); g_ns.t[(k) + g_ns.tbo] += (uint32_t)(ns_t_ - g_ns.last); g_ns.last = ns_t_; } } while (0)
#define NS_PMARK(k) do { if (g_ns.tbo) NS_MARK(k); } while (0)   /* gl38: a mark of the people's path only */
#endif
