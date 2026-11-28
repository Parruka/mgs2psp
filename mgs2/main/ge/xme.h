/* xme.h: the ME's loop and the queue the main CPU feeds it (task #180, the ME picture's build order step 4c: me_step4_notes.md 4c;
   me_handoff_design.md 4.6).  Built with the rest of the picture code into the ME's unit (-DMGS2_ME_SAFE): no printing, no system
   call; the host's waits are calls through pointers (ME_CALLP).  In 4c the loop runs as a thread on the main CPU in the emulator
   (PPSSPP has no ME), or the main CPU runs its entries itself (a turn of today's path, or after a take-back).

   The main CPU (psp/xl_psp.c MGS2_ME_PIC=3) keeps the runtime's walk, each transfer's record and its copy's plan (ge/xlme.c xm_plan),
   the filter and the guards' log, the thinning's scan (it reads the game's RAM), the GE and the display.  It posts, in order:
   - XE_REC: a record as its transfer ends, with its copy's extents.  The ME copies them at once from RAM (xm_fill without the log):
     now 1, a copy that must hold the bytes as the transfer ended (before vb 60, or with scratchpad bytes: the main CPU waits for it);
     now 0, an early copy, whose bytes the guards' log makes right at the FILL; nx -1: no copy, the record walked from RAM (the main
     CPU waits for that walk).
   - XE_THIN: the thinning's ranges for the frame begun last, from the slot below (gl_thin).
   - XE_FILL: the deadline (the hook that ends the step), or the log or the queue full or no room in the arena early: the log's first
     nlog entries laid over every early copy since the last FILL (xm_lay); the main CPU waits for it, then empties the log and the filter.
   - XE_WALK: the records since the last WALK walked in order through their copies (the restart, the switches, the late walk, the sums);
     the main CPU plans no new copy (and from the next hook on waits) until it is done: the arena is in use, and the picture must be
     made by the end of the step after its transfers' (the x86 check's latest).
   - XE_HOOK: a vblank hook while the picture is on: the frame's end and its GE list (into a result slot), a frame's start (only once
     the GE has finished every list before: the frame's arena, the one list buffer and the texture cache's last textures are the GE's
     until then), the texture cache's frame (tk_frame); stop 1: the GE stopped, the frame dropped.
   - XE_STOP: the run's end.
   - XE_SYNC (step 4d, mode 4): the ME writes back and drops its whole data cache, then its tail moves: the main CPU, which then makes
     the picture itself, finds in RAM everything the ME wrote (a turn's change, after a hook's FILL, WALK and HOOK: no copy waits).
   Shared words (XmeSh: head, tail, heart, listed, ge_done, the heap's request, the place word) are the host's and the ME's; both cores
   reach them only through XS, the uncached alias on the PSP (step 4d-4), and the results through XR; the main CPU writes back its cache
   (the entry, and what it has the ME read) before raising head.  Step 4d: the ME's cache steps go through the host's hook cache
   (0: none): REC the record's extents and their bytes invalidated before the copy, FILL the log's entries, WALK and HOOK the whole
   cache written back (and invalidated) first and before the list is posted, SYNC, STOP and the hang test too. */
#ifndef MGS2_XME_H
#define MGS2_XME_H
#include <stdint.h>
#include "xlme.h"
#include "nge.h"    /* step 2: NgeRef (the copy's references) */
#include "xconv.h"   /* XcThin */
#include "fxthin.h"  /* gl38: FxBd */

#define XME_N 256u          /* the queue's entries (a power of two): a step posts at most 64 records and 4 others, and the main CPU waits
                               at each hook for the walk of two steps before */
#define XME_NREC 64         /* records between two walks (psp/xl_psp.c MP_NREC) */
#define XME_NRES 4          /* lists made and not yet taken by the main CPU (one at a time: a frame starts only once the GE has
                               finished every list before) */
#define XME_LINE __attribute__((aligned(64)))   /* step 4d-3: a part of the ctx one core writes starts on a 64-byte line of its own */
#if defined(__psp__)   /* step 4d-4: the stores before a shared word's reach memory first (sync), on both cores; the shared words and the
                          results through main RAM's uncached alias, the same address on both cores (0x40000000 | address: hle_me.h) */
#define XME_BARRIER() __asm__ __volatile__("sync" ::: "memory")
#define XME_UNC 0x40000000u
#else
#define XME_BARRIER() __asm__ __volatile__("" ::: "memory")   /* the compiler keeps the entry's or result's stores before the shared word's */
#define XME_UNC 0u
#endif
enum { XE_REC = 1, XE_THIN = 2, XE_FILL = 3, XE_WALK = 4, XE_HOOK = 5, XE_STOP = 6, XE_SYNC = 7,
       XE_LOAD = 8 };   /* gl39: the load probe (ge/xme.c xme_load): why = the flavour (0 none, 1 arithmetic in registers, 2 cached reads of
                           h.opt2 bytes of read-only memory at h.opt, 3 uncached writes to the loop's own 256-byte block), nlog = its time in us.
                           Its time is not in t_k's entry slots (xme_run adds it to the unused slot 0): t_load holds it */
enum { XME_T_IDLE = 0, XME_T_GEW = 8, XME_T_LIST = 9, XME_T_N = 10 };   /* gl20: XmeCtx.t_k's slots (1-7: the entry kinds) */
enum { XME_C_ALL = 0, XME_C_INV = 1 };   /* step 4d: the cache hook's ops: the whole data cache written back and invalidated; a range
                                            invalidated (its lines are never the ME's to write: dirty ones are not kept) */
enum { XME_RQ_MORE = 1, XME_RQ_LESS = 2, XME_RQ_RESIZE = 3 };   /* step 4d-2: the ME's heap requests (me_rt.h me_heap_grow's hooks) */
#define XME_C_BIG 16384u                 /* a record's or log's bytes from which one XME_C_ALL is cheaper than the ranges (the ME's data
                                            cache is 16 KB) */
enum { XME_AT_WALK = 0x100, XME_AT_LAY = 0x200, XME_AT_RESFULL = 0x300, XME_AT_GEWAIT = 0x301, XME_AT_END = 0x310, XME_AT_LIST = 0x311,
       XME_AT_BEGIN = 0x312, XME_AT_TKFRAME = 0x313, XME_AT_IDLE = 0x500, XME_AT_HANG = 0x600 };
typedef struct { int ch, lanes, restart, x0, nx, t, now; uint32_t chcr, madr, qwc, tadr, opt, opt2;
                 int nb; int16_t ng0[2], ngn[2], p0[2], pn[2]; uint32_t noff;   /* step 2: ng0 / ngn int16 (NAT_MAXGRP 2304), the people's
                                                    p0 / pn beside them in the bytes that took (the record keeps its size) */
                 int16_t chi;                    /* gl34 (ge/natsplit.h): the transfer's place in its frame's chain list (-1: none; < NS_CH) */
                 uint8_t nru[2];                 /* gl36: each buffer's reuse switch (boot/nat_stub.h NatBuf.ru, 0 or 1) as the record was made;
                                                    fix B: with chi in the 4 bytes gl35's int chi took (the queue keeps gl35's size) */
               } XmeRec;   /* native step 1 (xlme.h XmPic): the ship-mesh groups the record's walk
                                                 draws (step 2: and the people's, p0 / pn as ng0 / ngn, their copy in the arena right after
                                                 the ship's: XME_POFF) */
/* step 2 (gl41's rule; gl42: on gl41's layout): the copy at the record is gl41's ship copy (its groups, then their references: ge/nge.h
   NGE_NAT_ROOM), then on 16 bytes the people's groups and their references, so that the walk never reads the main CPU's arrays, which the
   stub writes again for a later picture while the ME's walk may still be to come */
#define XME_POFF(r) ((r)->noff + XME_R16(NGE_NAT_ROOM((r)->nb, (r)->ngn[(r)->nb])))   /* gl42: after gl41's ship copy (ge/nge.h) */
/* (step 2:) the people's references (ge/nge.h xc_nge_ref6), pn words right after their groups: copied with them at the record, so the walk
   never reads the main CPU's array, which the stub writes again for a later picture while the ME's walk may still be to come */
#define XME_P6ROFF(r) (XME_POFF(r) + XME_R16((uint32_t)(r)->pn[(r)->nb] * (uint32_t)sizeof(NatP6)))
/* (XME_R16, XME_NATBYTES: ge/xlme.h, which boot/xlive_chk.c's mirror reads too) */
/* (native step 1's note:) the ship-mesh groups the record's walk
                                                 draws.  A record with a copy (nx >= 0): nb the packet buffer its plan bound (-1: none),
                                                 ngn[nb] groups from index ng0[nb] of that buffer's array, which the ME copies into the
                                                 arena at noff as it copies the extents (REC).  A record walked from RAM (nx -1): nb -1,
                                                 ng0 / ngn each buffer's groups not yet taken; the walk reads the array in place (the main
                                                 CPU waits for it) and answers in XmeSh nat_b / nat_used */
typedef struct { int32_t vb; int end, d, dig, begin, stop; uint32_t opt, opt2; int dz, dzn; } XmeHook;   /* gl29: dz, dzn the ending frame's
                                                 MGS2_GL_DZ arm and steps (gelist.h GxList.dz, dzn; 0, 0: none).  cull study v3: begin
                                                 is 1 | the frame's XcHost.cull << 8 (in the word gl35 had: XmeE keeps its size) */
typedef char xme_rec_size_check[sizeof(XmeRec) == 80 ? 1 : -1];   /* gl36 fix B: gl35's 80 B */
typedef struct { int kind, why, nlog; XmeRec r; XmeHook h; } XmeE;
typedef char xme_hook_size_check[sizeof(XmeHook) == 40 ? 1 : -1];   /* cull study v3: gl35's 40 B */
typedef char xme_e_size_check[sizeof(XmeE) == 132 ? 1 : -1];       /* cull study v3: gl35's 132 B (the queue's entry) */
typedef struct { int32_t fvb; int d, r, nt, dig; uint32_t *list; uint64_t dig_list, dig_frame; GxCount c;
                 int dz, dzn; GxDzCount dc; uint32_t t_list; int t_me, me_turn; int32_t a_nb, a_nv, a_nd;
#ifdef __psp__
                 uint64_t dig_c;   /* gl41: the content digest (xm_list), printed with the list line */
#endif
               } XmeRes;   /* gl29: the list's arm, steps and counts; xm_list's
                                                 own time in the loop's clock's ticks (t_me 1: the ME's Count, else microseconds); me_turn 1:
                                                 made by the loop on the ME or its thread (an ME turn), 0: by the main CPU in its own turn */
typedef struct XmeSh {      /* step 4d-4: the words both cores write, in XmeCtx.sh; reached only through XS(x): on the PSP the uncached alias
                               (a cached copy on either core would be stale: the ME's and the main CPU's caches do not see each other's
                               writes), on x86 the words themselves */
  uint32_t head, tail;        /* entries posted by the main CPU, done by the ME */
  uint32_t heart;             /* the ME's heartbeat: the loop's entries, records and waits, and every 64th of the picture code's beats
                                 (me_rt.h ME_BEAT) */
  uint32_t listed;            /* lists the ME made (results posted) */
  uint32_t ge_done;           /* lists the GE has finished or the main CPU let go (the main CPU's) */
  uint32_t idle, stopped, hung;
  uint32_t rq_seq, rq_done;   /* step 4d-2: the ME's heap requests posted (the ME's), answered (the main CPU's) */
  int rq_op; void *rq_p; uint32_t rq_n, rq_got; void *rq_r;   /* the request (XME_RQ_*, the region, the bytes) and its answer */
  uint32_t at;                /* where the loop is (a take-back reads it): 0 between entries (set after the tail moves, so a 0 means every
                                   entry before the tail is whole: the main CPU may go on from there, a warm take-back), the entry's kind
                                   in one, XME_AT_* in its parts: a walk's record (+ its index), a fill's laying (+ the record's index), a
                                   hook's parts, the waits, the idle wait and the hang test (both between entries) */
  uint32_t ge2;               /* gl23: 1: a frame may begin while the GE draws the one before (two frames on the GE side; the main CPU's,
                                 set at an ME turn's start, 0 in its own turns) */
  uint32_t me_ctl;            /* step 4d-4: the real ME's control block (psp/xme_me.h XmeMeCtl, its uncached address: its waits move
                                 hle_me_start's heartbeat too and see an exit request); 0: the loop is the emulator's thread */
  uint32_t t_idle_q, t_idle_lo, t_idle_hi;   /* gl24: the loop's idle waits' time in its clock's ticks, written only by the loop (q odd while
                                 it writes them).  gl20-23 kept it in t_k[XME_T_IDLE], a cached write in the main CPU's turns (its line
                                 holds tk_fb and fm_k).  gl23's stale turn lines (hooks 0, lists < 0) came from n_sync++ and t_k[XE_SYNC]
                                 written after the hand-over's write-back: gl24b writes the cache back after them (ge/xme.c xme_run) */
  int32_t np6_b; uint32_t np6_used;   /* native step 2: the people's, as nat_b / nat_used */
  int32_t nat_b; uint32_t nat_used;   /* native step 1: the last record walked from RAM with groups: the buffer its walk bound (-1: none)
                                 and the groups it took (the loop's; the main CPU reads them after its wait for that walk) */
} XmeSh;
#define XS(x) ((volatile XmeSh *)((uintptr_t)&(x)->sh | XME_UNC))   /* step 4d-4: the shared words */
#define XR(x) ((const XmeRes *)((uintptr_t)(x)->res | XME_UNC))       /* step 4d-4: the results as the main CPU reads them (the ME
                                                                         writes them back before listed moves) */
struct XmeCtx;
typedef void (*XmeWait)(struct XmeCtx *x, int why);   /* the host's wait: why 0 the queue empty (the loop's idle), 1 the GE (a frame's
                                                          start), 2 the hang test (never returns), 3 the heap's answer (step 4d-2) */
typedef void (*XmeCache)(struct XmeCtx *x, int op, const void *p, uint32_t n);   /* step 4d: XME_C_ALL, or XME_C_INV of [p, p + n) */
typedef struct XmeCtx {
  /* the host's, before the loop starts */
  XmPic *p; XlvMem *m; XmView *v;
  XmCopy fc;                  /* the copies as the ME fills them: mem, spr, mem_size, arena, arena_n and ext (the main CPU's plans write
                                 ext before they are posted); fills, laid, laid_lines, laid_b: the ME's counts */
  const XmLogE *log;          /* the main CPU's log: a FILL lays its first nlog entries */
  int32_t hang_vb;            /* the hang test (MGS2_ME_PIC_HANG; the thread's loop only): at the first HOOK with vb >= hang_vb the loop
                                 stops with its heartbeat stopped; -1: none */
  int hang_in;                /* step 4d: the hang test inside a walk instead (MGS2_ME_PIC_HANG_IN=1): at the first WALK after hang_vb the
                                 loop stops in its first record (a cold take-back) */
  XmeCache cache;             /* step 4d: the ME's cache steps (the host sets it only while the ME makes the picture); 0: none */
  uint32_t (*clk)(void);      /* gl20: the main CPU's clock for the loop's own times (t_k) when the main CPU runs the loop or an entry
                                 (its thread or itself): the system's microseconds; set once, never switched (gl21) */
  uint32_t (*clk_me)(void);   /* gl21: the ME's clock, its Count (psp/xme_me.c xme_me_clk); set once before the ME starts.  gl20 had one
                                 pointer that the host switched after the start: the ME's first idle wait called the main CPU's clock,
                                 a system call (a syscall exception on the ME), and the ME never ran (gl20's PSP log) */
  XmeWait w_me;               /* gl21: the ME's wait (xme_me_wait): XME_CLK reads clk_me when the loop's wait is this one */
  XmeWait w_thr;              /* gl26: the emulator's loop thread's wait (psp/xl_psp.c mq_wait_thr): xme_walk times the walk's split with
                                 clk there (with clk_me on the ME, not at all with any other wait: the main CPU's own turns) */
  const NatGrp *nat_g[2];     /* native step 1: the two packet buffers' group arrays (boot/nat_stub.h g_nat[].g; 0: none), the main
                                 CPU's to write; each starts on a 64-byte line of its own and ends on one */
  int nat_t_on;               /* 1: the merge runs timed on the ME and the emulator's thread (MGS2_NAT_T; never in the main CPU's turns) */
  int nh_chk;                 /* gl34: > 0: the hinted pointer look-ups checked against the plain ones (ge/xlme.c lw_nptr_h; MGS2_NAT_HCHK) */
  int ns_on;                  /* gl34: 1: the native path's split and the walk's time by chain (ge/natsplit.h; MGS2_NAT_SPLIT; needs nat_t_on) */
  void *fm[2]; GxList *fl[2]; /* gl23: the two frame memories (each: the arena, then its list's block) and their lists; fm[1] 0: one only */
  uint32_t fm_arena, fm_nb, fm_nv;   /* gl23: the arena's bytes, batches and vertices (gb_arena) */
  /* shared words, on lines of their own (step 4d-3: no line holds bytes one core writes through its cache and bytes the other writes;
     XME_LINE, and the ctx is 64-aligned); step 4d-4: reached only through XS (the uncached alias on the PSP, on both cores) */
  XmeSh sh XME_LINE;
  XmeE e[XME_N] XME_LINE;     /* the main CPU's (the ME drops an entry's lines from its cache before it reads it) */
  XmeRes res[XME_NRES] XME_LINE;   /* the ME's (written back at the hook before listed moves) */
  XcThin thin_r[16] XME_LINE; int thin_n, thin_keep3, thin_nd; struct { uint32_t lo, hi; } thin_d[512];   /* the thinning's slot (gl_thin's;
                                                                                                          the main CPU's) */
  FxBd bd_r[FX_RANGES]; int bd_n;   /* gl38 (ge/bound.h): the frame's rain owners' blocks, in the thinning's slot (gl_bd_ranges'; the main CPU's) */
  /* the ME's own (the main CPU's only in its own turns and after a take-back, each after a hand-over of the caches) */
  XmeRec rec[XME_NREC] XME_LINE; int nrec;
  int hang_arm;               /* step 4d: hang_in's test armed (the hook at hang_vb run): the loop stops at the next WALK with records,
                                 its place word at the walk's first record (the main CPU's own runs of entries never stop) */
  uint64_t t_qw, t_words, t_calls, t_ad; uint32_t l_qw, l_words, l_calls, l_ad;   /* psp/xl_psp.c sum()'s, for the ME's walks */
  long n_rec, n_now, n_early, n_ram, n_lay, n_fill, n_walk, n_walked, n_hook, n_thin, n_list, n_begin, n_gewait, n_restart, n_over, n_resfull;
  long n_sync, n_rq;          /* step 4d: hand-overs; heap requests */
  long n_natrec, n_natg; uint64_t n_natb;   /* native step 1: records with groups copied, their groups and bytes */
  const NatP6 *np6_g[2];      /* native step 2: the people's arrays (boot/np6_stub.h g_np6[].g; 0: none), as nat_g */
  long n_np6g; uint64_t n_np6b;   /* native step 2: the people's groups copied and their bytes */
  int fm_k; long tk_fb, n_ge2;   /* gl23: the frame memory in use; the texture cache's frame at the last frame's start; frames begun
                                    while the GE still had the one before */
  uint64_t t_k[XME_T_N];      /* gl20: the loop's own time in clk's ticks: XME_T_IDLE unused (gl24: XmeSh t_idle), 1-7 each entry kind's whole time
                                 (XE_*), XME_T_GEW the frame starts' and full results' waits for the GE, XME_T_LIST the frame ends and
                                 lists (both inside XE_HOOK's) */
  uint64_t t_ws[XM_TS_N]; long n_ws[XM_TS_N];   /* gl26: the walk's split (xlme.h xm_tsplit) in clk's or clk_me's ticks, and counts */
  long n_call, n_cinv; uint64_t n_cinv_b;   /* step 4d-4: the loop's cache steps through the host's hook (the whole cache; ranges, their
                                               bytes) */
  int ws_on;                  /* gl27: the walk's split timed (MGS2_ME_SPLIT; set once before the ME starts): gl26's timers cost the ME
                                 about 6 us a timed callback on the PSP, so they are off unless asked for */
  /* gl38, the hooks' and records' split, in the clock's ticks (as t_k): t_h (XME_H_*) the parts of XE_HOOK besides the GE waits; t_gx /
     n_gx the lists' parts and batches by kind (ge/gelist.h GX_T_*); t_r (XME_R_*) the parts of XE_REC */
  long n_load, n_load_it[4]; uint64_t t_load[4]; uint32_t ld_ri, ld_sum;   /* gl39: XE_LOAD entries, their blocks and time by flavour, the read's place, the sum */
  uint64_t t_h[6], t_gx[10], t_r[3]; long n_gx[4], n_gxl, n_gxc;   /* (n_gxl: the lists timed by part, one in XME_GX_EVERY; n_gxc: their changes of kind) */
  uint32_t fm_arena2;         /* step 2: the arena's bytes of a frame begun with XmeHook.begin's bit 24 (the carve after the opening; 0: none) */
} XmeCtx;
#define XME_GX_EVERY 4       /* a list in 4 is timed by part (a clock read at each change of the batches' kind: about 0.5 us each on the ME) */
enum { XME_H_END = 0,     /* the frame's end (gl_end: the last held calls' conversion, the counts) */
       XME_H_RES,         /* the result's words and the cache written back (the GE reads the list and the frame from RAM) */
       XME_H_BEGIN,       /* a frame's start after the GE's wait: the texture guard, the frame memory, the frame emptied */
       XME_H_TKF,         /* the texture cache's frame (tk_frame) */
       XME_H_N };
enum { XME_R_INV = 0,     /* the record's extents and bytes out of the ME's cache */
       XME_R_COPY,        /* the extents copied from RAM */
       XME_R_NAT,         /* the record's native groups copied */
       XME_R_N };

int xme_entry(XmeCtx *x, const XmeE *e, XmeWait w);   /* one entry (w: the wait for the GE): 1 at XE_STOP */
void xme_run(XmeCtx *x, XmeWait w);                   /* the loop: the entries in order until XE_STOP; heartbeat; idle and GE waits through w */
void xme_bind(XmeCtx *x, XmeWait w);                  /* step 4d-4: the loop's ctx and wait for the heap's requests, set by the host before the
                                                         loop starts (the thread's or the ME's wait, as w in xme_run) */
void xme_heap_ask(int resize);   /* step 4d-2 (the host calls it when the ME makes the picture next): the picture heap's hooks ask the
                                    main CPU through the loop's ctx (resize: the region resize hook too); the host's own hooks back with
                                    me_heap_grow and me_heap_resize when the main CPU makes the picture itself */
#endif
