/* xl_psp.c: the live packet translator in the PSP harness, measured (renderer_notes 11.12 "The plan for the PSP", step 1 on the PSP;
   task #45).  boot.c and runtime/ee_io.h are built with -DMGS2_XLIVE: io_dma_start hands each VIF1 / GIF transfer to xlive_chk_dma before
   its exact walk.  While g_opt has OPT_XL, the second walk (ge/xlive.c, spans) feeds the translator (ge/xlate.c: VU memory with lazy
   UNPACKs, GS registers, VU1 calls), its GS front end (ge/gsfront.c) and the texture source map with content keys (ge/gsup.c), all timed
   as the region HOST_XL.  Nothing is drawn yet and no texture is converted.  It reads guest RAM and the scratchpad and writes only its
   own memory, so every hash stays as it is.  OPT_XL off: nothing; back on: the translator starts again knowing nothing (xl_reset), as
   the x86 check does at a window's start (the map keeps what it knew).  xl_psp_report: totals for log.txt.  main_psp.c built with
   -DMGS2_XL times the region and writes it to steps.csv (xl_us).
   The one walk (xl3, renderer_notes 11.12, task #47; MGS2_XL_ONE=1 in mgs2.cfg, the default): a VIF1 transfer that runtime/ee_io.h's
   vif1_chain_fast walks is not walked a second time: that walk hands the translator the same runs of words and calls xlive_one_end at the
   transfer's end, so the translator's time is inside the region HOST_GS; GIF transfers keep the second walk (HOST_XL).  MGS2_XL_ONE=0:
   the second walk for every transfer, as xl2.
   Shown frames only (xl5, renderer_notes 11.12 "Shown frames only", task #47; MGS2_XL_SKIP=1 in mgs2.cfg, the default): the transfers
   after a vblank whose draw-pipeline calls were all reduced (boot.c g_dg_calls, which main_psp.c's host_vb_end hands to xl_psp_vblank
   before it clears them) are not handed to the translator, as the x86 check MGS2_XLIVE_SKIP does; it keeps its state across them.
   MGS2_XL_SKIP=0: every transfer.
   The live picture (gl1, renderer_notes 11.13 "3e", task #56; built with -DMGS2_GL, main_psp.c too): while g_opt has OPT_XL and OPT_GL,
   the transfers of each shown step (one after a step with full draw-pipeline calls) become a frame of GE batches (ge/gelive.c: the front
   end's primitives and the converters' calls; textures from ge/texcache.c in a pool of their own, so the cache never takes the game's
   heap); at the next vblank hook the frame becomes a GE list (ge/gelist.c; raw depth, mapped by the frame's range) that the GE draws into
   one of two 8888 frame buffers while the game runs on, and a frame the GE has finished is shown (sceDisplaySetFrameBuf, at the next
   display vblank).  One set of frame memory (the volatile block's last 2 MB): before the next frame begins the CPU waits for the GE
   (HOST_GW), and the texture cache keeps what this and the last frame used.  Times: HOST_GL (the converters' calls, the frame's end, the
   list, the hand-over, the display), HOST_GW (waits); the rest of the frame builder (the GS primitives) inside the translator's region.
   It reads guest RAM and writes only its own memory and VRAM, so every hash stays as it is.  The harness's text is on a third buffer
   (main_psp.c), shown again at the end (xl_psp_gl_end).  Every wait for the GE gives up after 2 s (gl_sync, 2026-10-27, for the first
   runs on the PSP): a "picture: STOPPED" line, the text on the screen again, no more frames, and the run goes on.  mgs2.cfg: MGS2_GL_DIG=<from>:<to>: each frame whose last vblank lies there
   gets its list digest and its frame digest (ge/gelive.h gl_digest) in log.txt, for the x86 check (boot/xlive_chk.c MGS2_XLIVE_GX: the
   same frames with the same sizes); MGS2_GL_SHOT=<vb>[,<vb>...]: at those vblank hooks the last finished frame is saved as
   shot_<its last vblank>.bmp, outside the step's time; MGS2_GL_TEXKB: the texture pool (KB, default GL_PSP_TEXKB). */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>       /* offsetof (task #180 step 4d-3's line check) */
#include "host_time.h"
#include "bbox.h"         /* gl44 H2: the black box's phase words (psp/bbox.c; main-CPU code only) */
#include "../ge/xlive.h"
#include "../ge/xlate.h"
#include "../ge/gsfront.h"
#include "../ge/gsup.h"
#include "../ge/me_rt.h"   /* task #149: the picture files built ME-safe: their heap (me_heap_init), its counts */
#include <unistd.h>       /* sbrk(0), for the heap's events */
#include <malloc.h>       /* newlib's mallinfo and malloc_usable_size, for the resize hook */
#ifndef ME_HEAP_KB
#define ME_HEAP_KB 768              /* the picture heap's first region (KB) without MGS2_ME_HEAP_KB in mgs2.cfg: what the
                                       picture's set-up takes (734 KB in 13 blocks before the first frame: me14p2 on the
                                       emulator) and a margin; the hooks below give the rest and take it back */
#endif
#ifndef ME_HEAP_GROW_KB
#define ME_HEAP_GROW_KB 0          /* the least region the hook gives when nothing fits (KB); a larger request gets its own
                                       size, rounded up to 64 bytes (0: every region; 32 left 20 KB unused below newlib's break
                                       at its highest in me14p8, vb 23,674) */
#endif
static uint8_t *me_hb; static size_t me_hn;   /* its first region */
static int me_rs_on;                          /* task #180 step 4d-2: the resize hook is on (MGS2_ME_HEAP_RESIZE) */
/* the picture heap's hook (the main CPU only): a region from newlib's heap when no free block fits a request */
static size_t me_grow = (size_t)ME_HEAP_GROW_KB << 10;   /* MGS2_ME_HEAP_GROW_KB in mgs2.cfg (0: each region the request's own size) */
static long me_vb;   /* the step the picture is in (xl_psp_frame), for the hooks' events */
/* the hooks' events for the log (op: + a region given, ! refused, - taken back, = resized in place, > resized and moved, ? resize
   refused), with newlib's break after each */
static struct { long vb; uint32_t at, from, n, w, brk, room, top, fr; char op; } me_ev[48]; static int me_nev, me_nev_all;
/* gl41 heap fix (b): newlib's room after each event (psp/main_psp.c host_heap_room: above the break, the free top chunk, all free bytes);
   the lowest room above the break plus the free top over every event (a block newlib takes at its top can have no more) */
unsigned host_heap_room(unsigned *top, unsigned *fr);
static uint32_t me_room_min = ~0u; static long me_room_vb = -1; static char me_room_op;
static void me_ev_add(char op, void *at, void *from, size_t n, size_t w)
{
  unsigned top, fr, room = host_heap_room(&top, &fr);
  if (room + top < me_room_min) { me_room_min = room + top; me_room_vb = me_vb; me_room_op = op; }
  me_nev_all++; if (me_nev == 48) return;
  me_ev[me_nev].room = room; me_ev[me_nev].top = top; me_ev[me_nev].fr = fr;
  me_ev[me_nev].vb = me_vb; me_ev[me_nev].op = op; me_ev[me_nev].at = (uint32_t)(uintptr_t)at; me_ev[me_nev].from = (uint32_t)(uintptr_t)from;
  me_ev[me_nev].n = (uint32_t)n; me_ev[me_nev].w = (uint32_t)w; me_ev[me_nev].brk = (uint32_t)(uintptr_t)sbrk(0); me_nev++;
}
/* step 4d-3: every region the hooks give is on whole 64-byte lines (memalign(64), its size rounded up to 64): newlib's chunk headers
   lie on the lines before and after it, so no line holds a byte the ME writes in the region and a byte newlib writes for another block */
static long me_misal, me_realn;   /* regions newlib's realloc moved off a line start: left there (no memory for a copy), copied to one */
static void *me_more(size_t want, size_t *got)
{
  size_t n = want < me_grow ? (me_grow + 63) & ~(size_t)63 : (want + 63) & ~(size_t)63;
  void *p = memalign(64, n); if (p) *got = n; me_ev_add(p ? '+' : '!', p, 0, p ? n : want, want); return p;
}
/* the other hook: a region the hook gave goes back to newlib's heap once no block in it is in use */
static void me_less(void *r) { free(r); me_ev_add('-', r, 0, 0, 0); }
/* the third: a region whose one block a realloc grows, resized by newlib's realloc (in place at the top of its heap, as gl16's tables grew).
   newlib's realloc grows a block that ends at its top chunk in place only when the top already holds the growth; otherwise it takes the
   whole new size from a top grown by sbrk and joins it to the block, so the break rises by the new size, not the growth, and the rest
   stays free at the top (me14p6: 72 and 132 KB for 32 and 64 KB of growth of the texture cache's table at vb 10,340 and 10,848; a block
   made and freed first, to grow the top by the shortfall alone, came from a free chunk elsewhere instead: me14p7).  So after a resize of
   a region that ended at the top chunk (its block ends malloc_usable_size - 4 bytes on: newlib's next chunk; the top runs to the break)
   while the top was short, the free top is given back at once (malloc_trim): the break then stands where a growth in place would have
   left it.  me_trim counts the times memory went back. */
static long me_trim;
static void *me_resize(void *r, size_t want)   /* (step 4d-3: the size rounded up to whole lines; a region newlib's realloc moved off a
                                                  line start is copied to a line-aligned one) */
{
  struct mallinfo mi; size_t have = malloc_usable_size(r); int top; void *p;
  want = (want + 63) & ~(size_t)63; mi = mallinfo();
  top = want > have && (uint8_t *)r + have - 4 + mi.keepcost == (uint8_t *)sbrk(0) && mi.keepcost < want - have + 64;
  p = realloc(r, want);
  if (top && p) me_trim += malloc_trim(0);
  if (p && ((uintptr_t)p & 63)) { void *q = memalign(64, want); if (q) { memcpy(q, p, want); free(p); p = q; me_realn++; } else me_misal++; }
  me_ev_add(!p ? '?' : p == r ? '=' : '>', p ? p : r, r, want, want); return p;
}
#ifdef MGS2_GL
#include <string.h>
#include <malloc.h>
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>         /* gl28d: MGS2_EYE peeks the R trigger (sceCtrlPeekBufferPositive: reads only) */
#include <pspge.h>
#include <pspgu.h>
#include "../ge/texcache.h"
#include "../ge/gelive.h"
#include "../ge/gelist.h"
#include "../ge/fxthin.h"   /* task #114 */
#include "../ge/xlme.h"     /* task #180: the split (step 4b) */
#include <pspfpu.h>         /* task #180 step 4c: the loop's thread takes the main thread's FPU rule */
#include "../ge/xme.h"      /* task #180 step 4c: the ME's loop and its queue */
#include "../ge/nge.h"      /* gl41: NGE_NAT_ROOM (a record's group copy with its native references) */
/* native step 1 (native_renderer_design.md 6.1): boot/nat_stub.h's (boot.c): the stub's arrays, counts and schedule */
extern NatBuf g_nat[2]; extern uint32_t g_nat_bseq[2]; extern int g_nat_cfg; extern long g_nat_c[NC_N];
extern void (*g_nat_sync)(int b); int rt_nat_arm_at(long vb, long *turn);
int rt_nat_inpl_at(long vb); extern long g_nat_stc; static long nat_stc0;   /* gl33: static streams in place (boot/nat_stub.h) */
int rt_nat_fast_at(long vb); extern long g_nat_fst;   /* gl34: the fast native front end (boot/nat_stub.h) */
int rt_nat_keep_at(long vb); extern long g_nat_kpt;   /* gl35: the keeps (boot/nat_stub.h) */
static long nk_kpt0, nk_n0[9];
/* cull study (ge/xconv.h XcHost.cull; attic/cull/cull_v3.md): XcHost.cull at vblank vb, as the twin's boot/nat_stub.h
   rt_nat_cull_at: MGS2_NAT_CULL=<bits> for the whole run, MGS2_NAT_CULL_CYCLE=<bits>,... by MGS2_OPT_CYCLE's turns (turn k from vb
   from + 1 + 100 k with MGS2_OPT_CYCLE=from:100:...), MGS2_NAT_CULLCHK=1 the check mode (0x80) */
char *mgs2_getenv(const char *k);   /* (main_psp.c; declared again below) */
static int cull_set, cull_all, cull_ncyc, cull_chk; static long cull_from, cull_turn = 1; static uint8_t cull_cyc[32];
static int rt_nat_cull_at(long vb)
{
  if (!cull_set) {
    const char *e, *c = mgs2_getenv("MGS2_OPT_CYCLE"); long f = -1, n = 0; cull_set = 1;
    cull_all = (e = mgs2_getenv("MGS2_NAT_CULL")) ? atoi(e) & 1 : 0;
    if ((e = mgs2_getenv("MGS2_NAT_CULL_CYCLE")) && c && sscanf(c, "%ld:%ld", &f, &n) == 2 && n > 0 && f >= 0) {
      char *p = (char *)e;
      while (cull_ncyc < 32 && *p) { cull_cyc[cull_ncyc++] = (uint8_t)(strtoul(p, &p, 10) & 1); if (*p != ',') break; p++; }
      if (cull_ncyc) { cull_from = f; cull_turn = n; } }
    cull_chk = (e = mgs2_getenv("MGS2_NAT_CULLCHK")) ? atoi(e) & 1 : 0;
    if (cull_all || cull_ncyc || cull_chk) fprintf(stderr, "nat: cull study v3: MGS2_NAT_CULL %d, MGS2_NAT_CULL_CYCLE %s (bit 1: the effect strips that "
                         "would draw nothing left out before their conversion; by the same turns), MGS2_NAT_CULLCHK %d\n", cull_all,
                         cull_ncyc ? mgs2_getenv("MGS2_NAT_CULL_CYCLE") : "none", cull_chk);
  }
  int k = cull_ncyc && vb > cull_from ? cull_cyc[((vb - cull_from - 1) / cull_turn) % cull_ncyc] : cull_all;
  return k ? k | (cull_chk ? 0x80 : 0) : 0;
}
int rt_nat_reuse_at(long vb); extern int g_nat_rcfg, g_nat_rchk; static long nr_n0[20];   /* gl36: the reuse (boot/nat_stub.h, ge/xlate.h) */
/* gl38 (ge/bound.h; boot/nat_stub.h rt_bound_at): the bound turns.  bd_cur: the mask of the frame being made (set where the frame
   begins: mq_frame, xl_psp_frame); the transfers it leaves out never reach the queue (xlive_chk_dma); the turns from the cycle's first
   vblank are the ME's, one a MGS2_ME_PIC_TURN vblanks, but a turn of kind 7, which is the main CPU's with the ME resting (mq_turn_hook) */
int rt_bound_at(long vb, long *turn); int rt_nat_f1_at(long vb); extern int g_bd_cfg, g_nat_f1cfg, g_nat_f1chk, g_bd_rain;
static int bd_cur, bd_chi = -1; static long bd_tix = -1, bd_tr[NS_CH + 1], bd_tr0[NS_CH + 1], bd_n0[GL_BD_N], bd_fr, bd_fr0, bd_grp, bd_grp0, bd_ov0, bd_bad0;
static long nr_r0[8];   /* gl37: the reuse's rule (ge/xlate.h xl_nr_r) */
/* gl39: the digest-off turns (MGS2_DIG_CYCLE), the ME's load in turns of kind 8 (MGS2_LOAD_CYCLE, MGS2_LOAD_US) and the coupling probes:
   counts by turn of what the main CPU does for the queue and the GE, by the timed part it was in (index host_top() + 1: 0 = none, the
   step's remainder, "logic"); with MGS2_PROBE9=1 one mq_service call in 8 and every whole-cache write-back of a post are timed (two clock
   reads each).  A line a turn after the bound line (bd_tline).  Nothing here changes what is drawn or the game's state */
int host_top(void); int rt_dig_off_at(long fvb); int rt_load_at(long vb); extern int g_dig_cfg, g_load_cfg; extern char _ftext[];
enum { P9_N = 8 };
static int p9_on = -1, p9_k; static uint32_t p9_load_us = 50000;
static long p9_sv[P9_N], p9_svt[P9_N], p9_wbn[P9_N], p9_dma[P9_N], p9_poll, p9_done, p9_subm, p9_post, p9_ldp, p9_ldskip;
static uint64_t p9_svus[P9_N], p9_wbus[P9_N];
static struct { long sv[P9_N], svt[P9_N], wbn[P9_N], dma[P9_N], poll, done, subm, post, ldp, ldskip, ln, lit[4]; uint64_t svus[P9_N], wbus[P9_N], lt[4]; } p9_0;
static int p9_top(void) { int t = host_top() + 1; return t < 0 || t >= P9_N ? P9_N - 1 : t; }
static void p9_setup(void)
{
  const char *e; p9_on = (e = mgs2_getenv("MGS2_PROBE9")) && atoi(e) > 0;
  if ((e = mgs2_getenv("MGS2_LOAD_US")) && atoi(e) > 0 && atoi(e) <= 200000) p9_load_us = (uint32_t)atoi(e);
  fprintf(stderr, "picture: gl39 probes: service and write-back times %s (MGS2_PROBE9); the ME's load in turns of kind 8: %u us an entry "
          "(MGS2_LOAD_US), flavours by MGS2_LOAD_CYCLE %s; digest-off turns (MGS2_DIG_CYCLE) %s\n", p9_on ? "on, one service call in 8" : "off",
          (unsigned)p9_load_us, g_load_cfg ? "set" : "not set (0: no load)", g_dig_cfg ? "set" : "not set");
}
/* gl40 (attic/gl40/gl40_plan.md 1d, 6): the copy plan's lever XP a turn (boot/nat_stub.h rt_xp_at: MGS2_XP_CYCLE, MGS2_XP_CHECK; ge/xlme.h
   c->xp) and the plan probe (MGS2_PROBE10=1: the plan's time by part through xmc.clk, mq_chk_end's whole time, mp_mark's time and the
   waits inside mq_chk_end by reason, all on the system's microseconds; the counts in every run): a 'gl40 plan' line a turn after the
   'gl39 probe' line, with M5's walks (rt/kernel.h k_take_m5, g_k_m5).  Nothing here changes the game's state */
int rt_xp_at(long vb); extern int g_xp_cfg; extern long g_k_m5;
static int p10_on = -1, p10_in; static long p10_ce, p10_mk, p10_wn[16]; static uint64_t p10_ceus, p10_mkus, p10_wus[16];
static struct { long plans, adds, moves, cpy, ra, rb, mk, chk, dif, room, ce, mkn, m5, ext, wn[16]; uint64_t tw, ts, tr, ceus, mkus, wus[16]; } p10_0;
static uint32_t p10_clk(void) { return sceKernelGetSystemTimeLow(); }
static void p10_setup(void)
{
  const char *e; p10_on = (e = mgs2_getenv("MGS2_PROBE10")) && atoi(e) > 0;
  fprintf(stderr, "picture: gl40 copy plan: lever XP by MGS2_XP_CYCLE / MGS2_XP_CHECK %s; the plan probe (MGS2_PROBE10) %s; M5 by MGS2_OPT2 bit 18 "
          "(its walks counted a turn)\n", g_xp_cfg ? "set" : "not set (gl39's plan)", p10_on ? "on: the plan's time by part, mq_chk_end's time and waits" : "off (counts only)");
}
#include "../ge/vu1conv.h"   /* gl34: vc_km_n */
#ifdef MGS2_ME_REAL         /* task #180 step 4d-4: the loop on the PSP's Media Engine (psp/xme_me.c), started by hle_me (psp/me/) */
#include <pspiofilemgr.h>
#include "hle_me.h"
#include "hle_me_mgs2.h"
#include "ps2rc_exc.h"
#include "xme_me.h"
volatile Ps2rcMeBlock ps2rc_me_block;   /* hle_me_mgs2.c's record of the ME's last exception (its handler writes it; the main CPU reads it
                                           through hle_me_exception) */
#endif
#endif

#define XL_GSZ (32u << 20)                /* guest RAM (runtime GUEST_SIZE) */
/* task #180 step 4d-3 (me_step4_notes.md 4d): the objects of this file the ME writes while it makes the picture (the translator's memory
   M, the frame glf, the list gxl and its host gxh, the walk's XmPic xmp and its view mp_view) go in .bss.me, renamed .me_bss after the
   compile (mkpsp18e.sh) and placed by psp/me_sec.ld with the ME-safe objects' .bss: 64-byte lines that hold no byte only the main CPU
   writes (the two cores' data caches do not see each other's writes) */
#define ME_LN __attribute__((section(".bss.me"), aligned(64)))
static XlvMem M ME_LN; static GuMap *map;
static int state;                        /* 0: not started, 1: on, 2: off since it was on, -1: no memory for the map, -2: stopped with the
                                            picture (task #180 step 4d: mode 4's cold take-back) */
static long ncall, nprim, nrestart, ntransfer, none;
extern int g_xl_one;                     /* runtime/ee_io.h: the exact walk feeds the translator */
extern int g_xl_lane;                    /* sb1, sb2 runtime/ee_io.h: Stage B lanes 1 (OPT_XL_LANE) and 2 (OPT_XL_LANE2); sb5 lane 4 (OPT_XL_LANE4); sb6 lane 3 (OPT_XL_LANE3) */
char *mgs2_getenv(const char *k);        /* main_psp.c: mgs2.cfg's settings (newlib's getenv does not see them) */
static int one_mode = 1, skip_mode = 1, cfg_read;   /* MGS2_XL_ONE, MGS2_XL_SKIP */
#ifdef MGS2_GL
static int mp_want;                                 /* task #180: MGS2_ME_PIC */
#ifdef MGS2_ME_REAL
static int mq_me_try, mq_edram;                     /* task #180 step 4d-4: the loop on the ME asked for; the arena in its eDRAM */
#define MQ_ED() mq_edram
#define MQ_EDRAM 0x80000000u                        /* the ME's eDRAM (kseg0; probe 2: 4 MB, reliable) */
#define MQ_EDRAM_KB 4096u
static int mq_emu(void) { int out = 0; return sceIoDevctl("kemulator:", 0x00000003, NULL, 0, &out, sizeof out) >= 0; }   /* hle_me's test */
#else
#define MQ_ED() 0
#endif
static void mp_hook(long vb), mp_end(void);
static int mq_on;                                   /* task #180 step 4c: MGS2_ME_PIC=3, the ME's loop (below); 4 (step 4d) its real ME's way */
static void mq_frame(long vb), mq_drain(void), mq_service(void), mq_ge_finish(void), mq_late(int why, int wait), mq_chk_end(void), mq_end(void);
static void ge_fin_cb(int id);   /* gl22: the GE's finish callback (below, with mq_subm) */
static int mq_fill(int why);
static void nat_tline(const char *who, long vb0, long vb, int me), nat_t0set(void), nat_psp_setup(void);   /* native step 1 */
static int32_t nat_fa[3];   /* native step 1: the frame arena's most in the turn: batches, vertices, data bytes */
#endif
static int skip_now; static long sk_vb, sk_tr;      /* shown frames only: this step's transfers are skipped; vblanks, transfers skipped */
static void cfg(void)
{
  const char *e;
  if (cfg_read) return;
  cfg_read = 1;
  if ((e = mgs2_getenv("MGS2_XL_ONE"))) one_mode = atoi(e) != 0;     /* mgs2.cfg, through main_psp.c */
  if ((e = mgs2_getenv("MGS2_XL_SKIP"))) skip_mode = atoi(e) != 0;
#ifdef MGS2_GL
  if ((e = mgs2_getenv("MGS2_ME_PIC"))) { mp_want = atoi(e); if (mp_want < 0 || mp_want > 4) mp_want = 1; }   /* task #180: 1 4b-1, 2 4b-2, 3 4c,
                                                                                                                 4 4d (the real ME's way) */
#endif
}
#ifdef MGS2_GL
/* ---- the live picture ---- */
#define GL_FB0 0x000000u                 /* VRAM offsets: two 8888 frame buffers and the 16-bit depth buffer, 512 pixels a row (as
                                            psp/ge_replay); the harness's text at GL_TXT (main_psp.c) */
#define GL_FB1 0x088000u
#define GL_ZB 0x110000u
#define GL_TXT 0x154000u
void *host_vol_tail(size_t n);           /* main_psp.c: n bytes from the end of the volatile block, or 0 */
const char *host_dir(void);              /* main_psp.c: the EBOOT's folder */
static TkCache *tk; static GlFrame glf ME_LN; static GxList gxl ME_LN, gxl2 ME_LN; static GxHost gxh ME_LN; static GxFrame gxf;   /* (ME_LN: step 4d-3) */
static FxThin gl_ft;   /* task #114: the rain's drop sheets and the foot splash drawn thinner (cfg MGS2_GL_RAIN, MGS2_GL_SPLASH) */
static uint32_t gl_dl[256] __attribute__((aligned(64)));   /* the direct list (sceGuStart writes it through the uncached view) */
static int gl_ok;                        /* 1: on; -1: no memory or no set-up, so no picture; 2: ended (xl_psp_gl_end); 3: stopped, the
                                            GE did not finish (gl_sync) */
#define GL_GE_LIMIT 2000000u             /* us: the GE draws a frame in 10-45 ms (MGS2GE v4-v6 on the PSP) */
static int gl_now, gl_full;              /* a frame is being made; the step that ended had full draw-pipeline calls */
static int thin_due;                     /* task #149: a frame began and the thinning's scan has not run for it yet */
static int gl_ge, gl_ge_buf; static long gl_ge_vb;   /* the GE has a frame not shown yet: its buffer and its last vblank */
static int gl_shown_buf = -1; static long gl_shown_vb = -1, gl_first_vb = -1;
static long gl_built, gl_shown, gl_listfail, gl_shots;   /* frames made (the one drawn into buffer gl_built & 1), shown, saved */
static long gl_stop_vb = -2; static int gl_stop_r;        /* gl_sync's stop: the last list's frame (-1: the set-up list), the GE's state */
static long dig_from = -1, dig_to = -1, dig_n; static uint64_t dig_all = 0xcbf29ce484222325ull;
static long shot_vb[48]; static int shot_n;   /* gl29: 48 (SHOT_MAX) */
/* gl28d (2026-11-20, the black marks on the deck that only the PSP shows): five diagnostics, each its own mgs2.cfg switch, each off
   when its key is absent (then nothing here runs and nothing is printed: gl28 as it was).  They read the pad, the display and VRAM and
   write files next to the EBOOT as the shots do; nothing writes game RAM, nothing changes what is drawn (MGS2_ME_PIC_GE2_AB changes
   only the overlap of the ME's next frame with the GE's drawing, as GE2=2 did in gl23).
   MGS2_EYE=1: the owner's marker: the R trigger peeked once a vblank hook (xl_psp_gl_vblank, outside the step's time); an 'eye: R
     down|up' line at each press and release (the game never reads the real pad: it plays pad.txt; the pad's readers, the end screen
     and the crash catcher, use sceCtrlReadBufferPositive, which a peek does not consume).
   MGS2_GL_LIVE=from:to:every[:max_r]: what the screen shows, copied as it is (no drain, no GE wait: the buffer on the display is
     complete, the GE draws only into the other one) to live_<vb>.bmp, at vb = from + k*every + every/2 up to to, and (with MGS2_EYE=1)
     at an R press inside from..to (gl28d2: a press outside the window writes only its 'eye' line), at most max_r (default 8) presses
     30 vblanks apart or more.
   MGS2_GL_FBCHK=vb: from vb, at each list given to the GE, the display's own answer (sceDisplayGetFrameBuf, IMMEDIATE and NEXTFRAME)
     compared with the list's target buffer, and the display vblanks since the frame before was shown, by turn; answers that name
     neither buffer counted apart (gl28d2).
   MGS2_ME_PIC_GE2_AB=vb: from vb, GE2 only in every other ME turn (on when (vb / (2 * TURN)) is odd); mode 4's turns only.
   MGS2_GL_SHOT_FLIPWAIT=1: from MGS2_GL_FLIPWAIT_FROM on, the shots' drained path keeps the flip wait and gives the GE every list
     made (the screen not torn by a list drawn into the buffer still on it; the frame saved is the last one made, gl27's own when at
     most one list was waiting after the drain). */
static long gl_buf_vb[2] = { -1, -1 }, gl_buf_n[2];   /* the frame each buffer holds since gl_show: its last vblank, its number shown */
static int eye_on, eye_r; static long eye_vb0 = -1, eye_n[2]; static uint64_t eye_t0;   /* MGS2_EYE: on; R now; the first hook; presses */
static long live_from = -1, live_to = -1, live_every, live_rmax = 8, live_rlast = -1000000, live_np, live_nr, live_bad;   /* MGS2_GL_LIVE */
static long fbc_from = -1, fbc_n[2], fbc_imm[2], fbc_next[2], fbc_same[2], fbc_err, fbc_lines;   /* MGS2_GL_FBCHK, by turn (0 ME, 1 main) */
static long fbc_oth;   /* gl28d2: answers (rc >= 0) that name neither buffer 0 nor buffer 1 (not in the VRAM view fb_off knows, or the text) */
static uint32_t fbc_dvc[2], fbc_dvc_t[2]; static long fbc_n0[2], fbc_i0[2], fbc_x0[2], fbc_s0[2];   /* the largest vblanks since the show:
                                                                                                      the run's, the turn's; at its start */
static long ge2_ab = -1; static int shot_fw;   /* MGS2_ME_PIC_GE2_AB, MGS2_GL_SHOT_FLIPWAIT */
#define GL28D_COLD __attribute__((noinline, cold))   /* the diagnostics kept out of the hot paths they hang off (mq_service, the hook) */
static GL28D_COLD void eye_poll(long vb);
static GL28D_COLD void gl_live(long vb, int r);
static GL28D_COLD int shot_wait(long vb);
/* gl29 (2026-11-20, gl29_plan.md G1-G4: the black marks on the deck, a test of the shadow decals' depth): three mgs2.cfg switches, each
   off when its key is absent (then nothing here runs and nothing is printed: gl28's lists word for word and gl28's lines).
   MGS2_GL_DZ=<vb0>:<vb1>:<arm>[,...] (ge/gelist.h): the frames whose last vblank lies in vb0..vb1 get that depth arm for the 0x42 shadow
     decals (O, X, Y, W, B<n>); the digest lines gain the arm and its counts; a 'gl29 dz window' line as each window (each entry, and each
     stretch of O between two entries, split at MGS2_GL_LIVE's start, and the O after the last) has left the screen: frames by turn, the
     counts, the lists' build time, the vblank hooks with R down (MGS2_EYE); the eye and live lines gain the arm of the frame on the
     screen; at the end R held by arm and turn, and by window.
   MGS2_GL_ZDUMP=<from>:<views>[:<mirror_vb>]: at each MGS2_GL_SHOT shot from vb from on, after the BMP, the depth buffer (512 x 272
     16-bit words, read through the uncached view, as the shots read colour) to zbuf_<frame vb>.raw; views 2: also through the |0x600000
     mirror (zbm_<frame vb>.raw) at every one, views 1: at the shot of vb mirror_vb only; the shot line gains the arm, 'last list = shot
     frame' (the GE idle, every list made given to it, the last one the frame saved), the files and the VRAM read times.
   MGS2_GL_SHOT_RANGE=<from>:<to>:<step>: a shot (drained as MGS2_GL_SHOT's, no depth) at every step-th vblank hook from..to (the
     emulator's dense reference; not set on the PSP).
   Reads only: nothing here writes a register, game RAM or VRAM.
   gl30 (gl30_plan.md): the dz text gains 'sub:' (X's decals paired with a subset base, XS; ge/gelist.h GxDzCount.nsub) after 'pair:', and
   the window line 'subset' after 'paired'; nothing else here changes. */
#define SHOT_MAX 48                      /* MGS2_GL_SHOT's entries (gl28: 16) */
static GxDzSched dz_t; static int dz_on;   /* MGS2_GL_DZ: the schedule; 1: on */
static long zd_from = -1, zd_mir = -1; static int zd_views;   /* MGS2_GL_ZDUMP */
static long sr_from = -1, sr_to = -1, sr_step;   /* MGS2_GL_SHOT_RANGE */
typedef struct { int16_t arm, n; } DzLab;
typedef struct { int32_t vb0, vb1, lab, fr[2], n42, noth, npair, fb[GX_DZF_N], nbias, nw, r[2], r10; uint32_t aux, us[2], usmax[2];
                 int32_t nsub; } DzWin;   /* gl30: nsub, X's subset pairs (XS) */
#define DZ_NWIN (2 * GX_DZ_MAX + 4)
static DzLab dz_lab[GX_DZ_MAX + 1]; static int dz_nlab, dz_elab[GX_DZ_MAX];   /* the arms named (0: O) and each entry's */
static DzWin dz_w[DZ_NWIN]; static int dz_nw, dz_wcur;   /* the windows in vb order; the first whose line is not printed yet */
static long dz_held[GX_DZ_MAX + 1][2];   /* vblank hooks with R down, by the arm of the frame on the screen and the turn (0 ME, 1 main) */
static GL28D_COLD void gl29_cfg(void);
static GL28D_COLD void dz_acc(long fvb, const GxDzCount *c, uint32_t us, int me);
static GL28D_COLD const char *dz_txt(char *s, int n, int dz, int dzn, const GxDzCount *c);
static GL28D_COLD const char *dz_fnm(long fvb, char *s);
static GL28D_COLD void dz_r(long vb);
static GL28D_COLD void dz_tick(FILE *f, long upto);
static GL28D_COLD int gl29_same(void), gl29_me(void);
static uint32_t gl_texkb = GL_PSP_TEXKB, gl_vol_n; static uint8_t *gl_vol, *gl_vol2; static void *gl_pool; static int gl_hold;   /* gl23: gl_vol2 */
static long gx_words_max, gx_aux_max, gx_both_max, gx_drop; static uint64_t gx_words_sum;
static long gx_c1p, gx_c1b, gx_c1w;   /* sb11 C1: the lists' GxCount c1_passes, c1_bones, c1_words */
static long gx_c4s, gx_c4r, gx_c4l;   /* sb15 step 4: the lists' GxCount fx_spr, fx_lruns, fx_lines */
static long gx_c2p, gx_c2b, gx_c2w;   /* sb12 C2: c2_passes, c2_bones, c2_words */
static uint32_t gl_addr(void *ctx, const void *p) { (void)ctx; return (uint32_t)(uintptr_t)p; }
/* native step 1 (MGS2_NGE; boot/nge_stub.h, ge/nge.h): the cooked meshes' arena, the tail of each frame memory's arena (nge_k bytes each,
   0: off and every size as before); the GE's finishes (ge_fin_n, below) hold an eviction until the lists that may use a mesh are done */
uint32_t nge_cfg_bytes(void); void nge_turn_line(long vb0, long vb1); void nge_end_line(void);
int nge_host_setup(uint8_t *m0, uint32_t ge0, uint8_t *m1, uint32_t ge1, uint32_t cap, uint32_t cap0, uint32_t *const p6r[2],
                   volatile uint32_t *fin, void *(*al)(uint32_t n, const char *name));
uint32_t nge_cfg_bytes2(void); uint32_t nge_np6_bytes(void); long nge_grow_vb(void); void nge_grow(uint8_t *const p6[2], long vb);
static uint32_t nge_k; static int nge_done; static volatile uint32_t ge_fin_n;
/* step 2, the carve after the opening: each frame memory's arena tail nge_kg from the first frame begun at or after nge_grow_vb (the hook's
   begin bit 24: ge/xme.c gives that frame the smaller arena, mq->fm_arena2); nge_gst 1 from there, 2 once the GE finished 3 lists after it
   (as the evictions' guard) AND the GE finished a list of a frame begun with the smaller arena (nge_gok, gl_show: lists go to the GE one at
   a time in order, so every list of a frame begun with the larger arena is done, even one that waited in the ME's results): then the
   regions whole and the people's arrays there */
static uint32_t nge_kg, nge_gfin; static int nge_gst, nge_gok; static long nge_gvb0; static uint8_t *nge_p6b[2];
static void gl_line(FILE *f, const GlCount *n, int64_t db, int64_t tb, const char *what)   /* as boot/xlive_chk.c's gb_line; db, tb: n's data
                                                                                     and texture bytes (64-bit sums for the totals) */
{
  fprintf(f, "%s %ld batches (%ld transform), %ld vertices, %lld KB data, %ld textures (%lld KB, %ld new 16-bit); primitives %ld (%ld "
          "textured; left out: %ld uncertain, %ld path 1; state drawn otherwise %ld/%ld/%ld/%ld); calls %ld: converted %ld, nothing drawn %ld, "
          "bad %ld, no room %ld; other calls %ld; texture lookups %ld (memo %ld): frame buffers %ld, untextured: render targets %ld, not "
          "known %ld, refused %ld, table full %ld; dropped %ld", what, n->batches, n->xbatches, n->verts, (long long)(db >> 10), n->ntex,
          (long long)(tb >> 10), n->tex16, n->prims, n->prims_tex, n->uncertain, n->path1, n->unrep[0], n->unrep[1], n->unrep[2], n->unrep[3],
          n->calls, n->conv, n->conv_empty, n->conv_bad, n->conv_full, n->other, n->tex_asks, n->tex_memo, n->tex_fb, n->tex_rt,
          n->tex_lost, n->tex_unsup, n->tex_full, n->dropped);
  fprintf(f, "; P06 and P07 calls %ld, passes 2 and 3 drawn %ld, not drawn %ld", n->lit_calls, n->lit_passes, n->lit_nopass);
}

/* the CPU waits for the GE to finish its lists: 0 once it has.  A GE that has not finished in GL_GE_LIMIT stops the picture (gl_ok 3: no
   more lists, the harness's text on the screen again; the run goes on): -1.  (sceGeDrawSync(0) would wait for ever.) */
static int gl_sync(void)
{
  uint64_t t0; int r;
  if (!sceGeDrawSync(1)) return 0;
  t0 = (uint64_t)sceKernelGetSystemTimeWide();
  while ((r = sceGeDrawSync(1)))
    if ((uint64_t)sceKernelGetSystemTimeWide() - t0 < GL_GE_LIMIT) sceKernelDelayThread(100);
    else {
      gl_stop_vb = gl_ge ? gl_ge_vb : -1; gl_stop_r = r; gl_ok = 3; gl_now = gl_ge = 0;
      sceDisplaySetFrameBuf((void *)(0x44000000u + GL_TXT), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
      fprintf(stderr, "picture: STOPPED: the GE has not finished %s%ld in %u ms (sceGeDrawSync(1) %08x): no more frames, the run goes on\n",
              gl_stop_vb < 0 ? "its set-up list" : "the list of the frame of vb ", gl_stop_vb < 0 ? 0 : gl_stop_vb, GL_GE_LIMIT / 1000, (unsigned)r);
      return -1;
    }
  return 0;
}
/* gl28d: MGS2_EYE and MGS2_GL_LIVE read at the picture's set-up (out of line: gl_setup is inlined into the hot xlive_chk_dma) */
static GL28D_COLD void gl28d_cfg(void)
{
  const char *e;
  if ((e = mgs2_getenv("MGS2_EYE"))) {   /* gl28d: the owner's marker (only 1 turns it on) */
    eye_on = atoi(e) == 1;
    fprintf(stderr, "gl28 eye (MGS2_EYE %.16s): %s\n", e, eye_on ? "on: the R trigger peeked once a vblank hook, outside the step's time "
            "(sceCtrlPeekBufferPositive: the game plays pad.txt and never reads the pad; a peek takes nothing from the end screen's or the "
            "crash catcher's reads); an 'eye: R down|up' line at each press and release, t in ms from the first hook" : "not 1, so off"); }
  if ((e = mgs2_getenv("MGS2_GL_LIVE"))) {   /* gl28d: live captures of the buffer on the screen */
    long a = -1, b = -1, ev = 0, mr = 8; int n = sscanf(e, "%ld:%ld:%ld:%ld", &a, &b, &ev, &mr);
    if (n >= 3 && a >= 0 && b >= a && ev > 0) { live_from = a; live_to = b; live_every = ev; live_rmax = n == 4 && mr >= 0 ? mr : 8; }
    if (live_from >= 0) fprintf(stderr, "gl28 live (MGS2_GL_LIVE %.40s): the buffer on the screen copied as it is (no drain, no wait for the "
                                "GE) to live_<vb>.bmp next to the EBOOT, outside the step's time: at vb %ld + k*%ld + %ld up to %ld; at R presses "
                                "(MGS2_EYE): %s %ld inside vb %ld-%ld (a press outside: its 'eye' line only), 30 vblanks apart or more; a "
                                "'picture: gl28 live' line each\n", e, live_from, live_every, live_every / 2, live_to, eye_on ? "the first" :
                                "none (MGS2_EYE off), else the first", live_rmax, live_from, live_to);
    else fprintf(stderr, "gl28 live (MGS2_GL_LIVE %.40s): not from:to:every[:max_r] with every > 0, so off\n", e); }
}
/* with the translator's start (its first transfer): the texture pool, the frame memory, the GE */
static XmeCtx *mq; static int mq_m4; static int np6_on; static void nat_psp_sync(int b);   /* step 2: nge_psp_setup's (defined below) */
/* native step 1: a block of the system's memory outside newlib's heap for the stub's tables (newlib's heap has about 64 KB to spare) */
static void *nge_sys_alloc(uint32_t n, const char *name)
{
  SceUID id = sceKernelAllocPartitionMemory(2, name, PSP_SMEM_Low, n, 0);
  return id >= 0 ? sceKernelGetBlockHeadAddr(id) : 0;
}
/* native step 1: the stub's regions (each frame memory's arena tail, carved in gl_setup) and tables; mq_setup calls it after the native
   arrays took their blocks (so they find the memory they found before) and before the ME starts; with no ME, after mp_setup */
static void nge_psp_setup(void)
{
  if (!nge_k || nge_done) return;
  uint32_t f0 = (uint32_t)sceKernelMaxFreeMemSize(), f1 = (uint32_t)sceKernelTotalFreeMemSize();
  uint32_t k2 = nge_cfg_bytes2(), p6 = nge_np6_bytes(); uint32_t *p6r[2] = { 0, 0 };
  if (k2 > nge_k && !(mq && mq_m4 && gl_vol2)) {   /* (the smaller arena is given at the ME loop's frame start, two frame memories) */
    fprintf(stderr, "picture: step 2: the carve after the opening needs mode 4's loop and two frame memories: none (each region %u KB)%s\n",
            (unsigned)(nge_k >> 10), p6 ? "; the people stay the builder's" : ""); k2 = nge_k; p6 = 0; }
  uint8_t *m0 = gl_vol + GL_PSP_ARENA - k2, *m1 = gl_vol2 ? gl_vol2 + GL_PSP_ARENA - k2 : 0;
  if (k2 > nge_k) { nge_kg = k2 + p6; mq->fm_arena2 = GL_PSP_ARENA - nge_kg;
    if (p6) { nge_p6b[0] = m0 - p6; nge_p6b[1] = m1 - p6; mq->np6_g[0] = (const NatP6 *)(void *)nge_p6b[0]; mq->np6_g[1] = (const NatP6 *)(void *)nge_p6b[1];
      for (int b = 0; b < 2; b++) p6r[b] = (uint32_t *)(void *)(nge_p6b[b] + (((uint32_t)(NP6_MAXGRP * sizeof(NatP6)) + 63u) & ~63u));
      np6_on = 1; g_nat_sync = nat_psp_sync; } }
  int r = nge_host_setup(m0, (uint32_t)(uintptr_t)m0, m1, (uint32_t)(uintptr_t)m1, k2, nge_k, p6r, &ge_fin_n, nge_sys_alloc);
  nge_done = 1;
  if (r) { if (nge_kg) mq->fm_arena2 = 0; nge_kg = 0; if (nge_p6b[0]) { np6_on = 0; mq->np6_g[0] = mq->np6_g[1] = 0; nge_p6b[0] = nge_p6b[1] = 0; } }
  else if (nge_kg) fprintf(stderr, "picture: step 2: the carve after the opening from vb %ld: each frame memory's arena %u KB then (the cooked "
                           "region %u KB, the people's block %u KB at %08x and %08x)\n", nge_grow_vb(), (unsigned)((GL_PSP_ARENA - nge_kg) >> 10),
                           (unsigned)(k2 >> 10), (unsigned)(p6 >> 10), (unsigned)(uintptr_t)nge_p6b[0], (unsigned)(uintptr_t)nge_p6b[1]);
  fprintf(stderr, "picture: native step 1 %s: each frame memory's arena %u KB (%u KB less the cooked meshes' %u KB); the system's memory outside "
          "newlib's heap before: largest block %u KB, total %u KB; after: %u KB, %u KB\n", r ? "NOT ON (today's path)" : "on",
          (unsigned)((GL_PSP_ARENA - nge_k) >> 10), (unsigned)(GL_PSP_ARENA >> 10), (unsigned)(nge_k >> 10), (unsigned)(f0 >> 10), (unsigned)(f1 >> 10),
          (unsigned)((uint32_t)sceKernelMaxFreeMemSize() >> 10), (unsigned)((uint32_t)sceKernelTotalFreeMemSize() >> 10));
  if (r) nge_k = 0;
  sceKernelDcacheWritebackAll();
}
static void gl_setup(void)
{
  const char *e; struct mallinfo mi;
  gl_ok = -1;
  if ((e = mgs2_getenv("MGS2_GL_TEXKB")) && atoi(e) >= 256) gl_texkb = (uint32_t)atoi(e);
  if ((e = mgs2_getenv("MGS2_GL_DIG")) && sscanf(e, "%ld:%ld", &dig_from, &dig_to) != 2) dig_from = dig_to = -1;
  if ((e = mgs2_getenv("MGS2_GL_HOLD"))) gl_hold = atoi(e);   /* sb4: step c's records in use at most (1..GL_HOLD) */
  { int rn = (e = mgs2_getenv("MGS2_GL_RAIN")) ? atoi(e) : 1, sp = (e = mgs2_getenv("MGS2_GL_SPLASH")) ? atoi(e) : 1;   /* task #114: 1 in N */
    int sk = (e = mgs2_getenv("MGS2_GL_STREAKS")) ? atoi(e) : 1;   /* task #154: the rain's streaks (drawn with C4 only) */
    fx_thin_init(&gl_ft, rn, sp, sk);
    if ((e = mgs2_getenv("MGS2_GL_STREAK_CALLS"))) fx_thin_calls(&gl_ft, atoi(e));   /* task #158: their calls 1 in N, by block */
    if ((e = mgs2_getenv("MGS2_GL_RAIN_SPRITES"))) fx_thin_rainspr(&gl_ft, atoi(e)); }   /* task #161: 0: the rain's sprites left out */
  for (e = mgs2_getenv("MGS2_GL_SHOT"); e && *e && shot_n < SHOT_MAX; ) { shot_vb[shot_n++] = strtol(e, (char **)&e, 10); if (*e == ',') e++; else break; }
  gl28d_cfg();   /* gl28d: MGS2_EYE, MGS2_GL_LIVE */
  gl29_cfg();    /* gl29: MGS2_GL_DZ, MGS2_GL_ZDUMP, MGS2_GL_SHOT_RANGE (after gl28d_cfg: the windows split at MGS2_GL_LIVE's start) */
  gl_vol_n = GL_PSP_ARENA + GL_PSP_LIST; nge_k = nge_cfg_bytes();   /* native step 1 */
  gl_vol = host_vol_tail(gl_vol_n); gl_pool = memalign(64, gl_texkb << 10);
  if (!gl_vol || !gl_pool) { fprintf(stderr, "picture: no memory (frame memory %s, texture pool %s): no picture\n", gl_vol ? "ok" : "none",
                                     gl_pool ? "ok" : "none"); free(gl_pool); gl_pool = 0; return; }
  memset(gl_vol, 0, gl_vol_n); memset(gl_pool, 0, gl_texkb << 10);
  gu_zero_empty(map, 1);                 /* the map runs from the boot's first transfer (as the x86 check from vb 0) */
  if (!(tk = tk_new(map, (gl_texkb << 10) / 8 * 7)) || tk_pool(tk, gl_pool, gl_texkb << 10) || gl_init(&glf, tk, map, 0, GL_PSP_MAXTEX)
      || gb_arena(&glf.o, gl_vol, GL_PSP_ARENA - nge_k, GL_PSP_NB, GL_PSP_NV)) { fprintf(stderr, "picture: set-up failed: no picture\n"); return; }
  gx_init(&gxl, (uint32_t *)(gl_vol + GL_PSP_ARENA), GL_PSP_LIST / 4, gl_vol + GL_PSP_ARENA, GL_PSP_LIST); gxl.ret = 1;   /* one block */
  if ((e = mgs2_getenv("MGS2_ME_PIC_GE2")) && atoi(e) > 0) {   /* gl23: the second frame memory (the heap: the volatile block is full) */
    if ((gl_vol2 = memalign(64, gl_vol_n))) { memset(gl_vol2, 0, gl_vol_n); sceKernelDcacheWritebackInvalidateRange(gl_vol2, gl_vol_n);
      gx_init(&gxl2, (uint32_t *)(gl_vol2 + GL_PSP_ARENA), GL_PSP_LIST / 4, gl_vol2 + GL_PSP_ARENA, GL_PSP_LIST); gxl2.ret = 1; }
    fprintf(stderr, "picture (gl23): the second frame memory %s (%u KB at %08x)\n", gl_vol2 ? "made" : "NOT made: no memory (one frame on the GE side)",
            (unsigned)gl_vol_n >> 10, (unsigned)(uintptr_t)gl_vol2); }
  gxh.addr = gl_addr; gxh.ctx = 0; gxh.fb[0] = GL_FB0; gxh.fb[1] = GL_FB1; gxh.zb = GL_ZB;
  memset((void *)(0x44000000u + GL_FB0), 0, GL_ZB - GL_FB0);   /* both frame buffers black, through the uncached view */
  sceGuInit();                           /* as psp/ge_replay, without sceGuDisplay: the harness shows the frames itself */
  sceGuSetCallback(GU_CALLBACK_FINISH, ge_fin_cb);   /* gl22: the GE's finish, timed (and with MGS2_ME_PIC_GEFAST told to the ME) */
  sceGuStart(GU_DIRECT, gl_dl);
  sceGuDrawBuffer(GU_PSM_8888, (void *)GL_FB0, 512); sceGuDispBuffer(480, 272, (void *)GL_FB1, 512); sceGuDepthBuffer((void *)GL_ZB, 512);
  sceGuOffset(2048 - 240, 2048 - 136); sceGuViewport(2048, 2048, 480, 272); sceGuDepthRange(65535, 0);
  sceGuScissor(0, 0, 480, 272); sceGuEnable(GU_SCISSOR_TEST); sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_CLIP_PLANES);
  sceGuFinish();
  if (gl_sync()) return;
  gl_ok = 1; mi = mallinfo();
  fprintf(stderr, "picture: on: frame memory %u KB at %08x (arena %u KB: %u batches, %u vertices; list and aux %u KB), texture pool "
          "%u KB at %08x (the cache's budget %u KB); VRAM frame buffers %06x / %06x, depth %06x, text %06x; malloc in use %u KB of %u KB\n",
          (unsigned)gl_vol_n >> 10, (unsigned)(uintptr_t)gl_vol, GL_PSP_ARENA >> 10, GL_PSP_NB, GL_PSP_NV, GL_PSP_LIST >> 10,
          (unsigned)gl_texkb, (unsigned)(uintptr_t)gl_pool, (unsigned)gl_texkb / 8 * 7, GL_FB0, GL_FB1, GL_ZB, GL_TXT, (unsigned)mi.uordblks >> 10,
          (unsigned)mi.arena >> 10);
  if (dig_from >= 0) fprintf(stderr, "picture: digests of the frames whose last vblank is in %ld..%ld\n", dig_from, dig_to);
  if (gl_hold >= 1 && gl_hold <= GL_HOLD) glf.hold_max = gl_hold;
  fprintf(stderr, "picture: step c holds up to %d P06 calls (%u bytes a call)\n", glf.hold_max, (unsigned)sizeof(XcHeld));
  if (fx_thin_on(&gl_ft)) { glf.ft = &gl_ft; glf.ram = M.mem; glf.ft_on = 1; }   /* task #114 (after gl_init, which clears glf) */
  if (!fx_thin_on(&gl_ft)) fprintf(stderr, "picture: the rain's drop sheets, the foot splash and the rain's streaks drawn as the PS2 "
                                    "(MGS2_GL_RAIN, MGS2_GL_SPLASH, MGS2_GL_STREAKS)\n");
  else fprintf(stderr, "picture: the rain's drop sheets drawn 1 strip in %d, the foot splash 1 in %d, the rain's streaks 1 in %d (with C4; 0: none) "
               "(MGS2_GL_RAIN, MGS2_GL_SPLASH, MGS2_GL_STREAKS; 1: as the PS2)\n", gl_ft.keep[0], gl_ft.keep[1], gl_ft.keep[2]);
  if (gl_ft.keep_calls > 1) fprintf(stderr, "picture: the rain's streak calls drawn 1 in %d, whole calls by their block (with C4) "
                                    "(MGS2_GL_STREAK_CALLS)\n", gl_ft.keep_calls);
  if (!gl_ft.keep[3]) fprintf(stderr, "picture: the rain's sprites (body_sph.c, charaspread.c, line_sph.c, rain_gas_pers.c) left out whole, "
                              "with C4 (MGS2_GL_RAIN_SPRITES=0)\n");   /* task #161 */
}
/* gl27: the screen flip.  gl_show hands a drawn frame to the display for its next vblank (PSP_DISPLAY_SETBUF_NEXTFRAME); until then
   the display still scans the buffer of the frame before, and that buffer is the one the next list draws into (from a clear to black).
   When the ME's next result was ready at once (TWO, GE2), mq_service gave the GE that list in the same vblank, and the GE cleared and drew
   the buffer on the screen: the black marks seen on the PSP in gl26 [I].  MGS2_GL_FLIPWAIT: 0 off (gl26; the cases counted), 1 the next
   list waits for a vblank after gl_show, 2 in every other ME turn and the main CPU turn after it (as GE2=2), and always from the vblank
   MGS2_GL_FLIPWAIT_FROM on.  flip_force: the shots' path (no wait, as gl26).  Counts by turn: 0 the ME's, 1 the main CPU's */
static uint32_t gl_show_vc;   /* the display's vblank count when gl_show ran */
static int flip_cfg, flip_on, flip_w, flip_force; static long flip_from = -1; static uint32_t flip_t0;   /* flip_w: 1 + the bucket of the
                                                                                                           hold now on (gl27b), 0: none */
static int flip_sel(long vb);
static long flip_hold[2], flip_race[2], flip_race_dir; static uint64_t flip_us[2];
static long flip_h0[2], flip_r0[2], flip_mvb0 = -1; static uint64_t flip_u0[2];   /* at a turn's start */
/* the GE has drawn the frame: it is on the screen from the next display vblank */
static void gl_show(void)
{
  sceDisplaySetFrameBuf((void *)(0x44000000u + (gl_ge_buf ? GL_FB1 : GL_FB0)), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
  gl_show_vc = sceDisplayGetVcount();   /* gl27 */
  if (nge_gst == 1 && gl_ge_vb >= nge_gvb0) nge_gok = 1;   /* step 2: a frame begun with the smaller arena drawn (the growth's guard) */
  gl_ge = 0; gl_shown++; gl_shown_buf = gl_ge_buf; gl_shown_vb = gl_ge_vb;
  gl_buf_vb[gl_ge_buf & 1] = gl_ge_vb; gl_buf_n[gl_ge_buf & 1] = gl_shown;   /* gl28d: what each buffer holds (MGS2_GL_LIVE, MGS2_EYE) */
}
static void gl_wait(void) { int r; HOST_ENTER(HOST_GW); r = gl_sync(); HOST_LEAVE(HOST_GW); if (!r) gl_show(); }
/* the frame that ended (its last vblank fvb) -> its GE list, drawn into the buffer that is not on the screen */
static void gl_list(long fvb)
{
  int d, dig = fvb >= dig_from && fvb <= dig_to && !rt_dig_off_at(fvb);   /* gl39: MGS2_DIG_CYCLE */
  if (gl_ge) { gl_wait(); if (gl_ok != 1) return; }   /* (a frame begins only after the GE has drawn the one before, so not expected) */
  d = (int)(gl_built++ & 1);
  gxf.b = glf.o.b; gxf.nb = (uint32_t)glf.o.nb; gxf.v = glf.o.v; gxf.nv = (uint32_t)glf.o.nv; gxf.d = glf.o.d;
  gxf.t = glf.o.t; gxf.nt = (uint32_t)glf.o.nt; gxf.tdata = glf.tdata; gxf.tpal = glf.tpal;
  gx_depth_map(glf.map_lo, glf.map_hi, 1.0f, &gxf.zs, &gxf.zo);
  gxl.dig_on = dig;
  uint32_t dzt = 0;
  if (dz_on) { int n = 0; gxl.dz = gx_dz_arm(&dz_t, fvb, &n, 0); gxl.dzn = n; dzt = sceKernelGetSystemTimeLow(); }   /* gl29: MGS2_GL_DZ */
  if (gx_frame(&gxl, &gxh, &gxf, d) && ++gl_listfail <= 50)
    fprintf(stderr, "picture: vb %ld: the list did not fit: %ld batches left out (%ld words, %ld aux bytes)\n", fvb, gxl.c.dropped, gxl.c.words, gxl.c.aux);
  if (dz_on) dz_acc(fvb, &gxl.dc, sceKernelGetSystemTimeLow() - dzt, 0);
  gx_words_sum += gxl.c.words; gx_drop += gxl.c.dropped;
  gx_c1p += gxl.c.c1_passes; gx_c1b += gxl.c.c1_bones; gx_c1w += gxl.c.c1_words;
  gx_c4s += gxl.c.fx_spr; gx_c4r += gxl.c.fx_lruns; gx_c4l += gxl.c.fx_lines;
  gx_c2p += gxl.c.c2_passes; gx_c2b += gxl.c.c2_bones; gx_c2w += gxl.c.c2_words;
  if (gxl.c.words > gx_words_max) gx_words_max = gxl.c.words;
  if (gxl.c.aux > gx_aux_max) gx_aux_max = gxl.c.aux;
  if (4 * gxl.c.words + gxl.c.aux > gx_both_max) gx_both_max = 4 * gxl.c.words + gxl.c.aux;
  if (dig) {
    uint64_t g = gl_digest(&glf, &gxl); dig_all = (dig_all ^ g) * 0x100000001b3ull; dig_n++; char zt[224];
    fprintf(stderr, "picture: vb %ld buffer %d list %016llx frame %016llx: %ld words, %ld aux bytes, %ld batches, %ld PRIMs, %d textures, "
            "%ld dropped%s\n", fvb, d, (unsigned long long)gxl.dig, (unsigned long long)g, gxl.c.words, gxl.c.aux, gxl.c.batches, gxl.c.prims,
            glf.o.nt, gxl.c.dropped, dz_on ? dz_txt(zt, sizeof zt, gxl.dz, gxl.dzn, &gxl.dc) : "");
    /* gl41: the content digest (ge/gelive.c gl_digest_c: a native batch hashed as today's), as the twin prints it: with MGS2_NGE on, the
       list and frame digests differ by design (the CALLs) and this one equals the switch-off run's */
    fprintf(stderr, "picture: vb %ld content %016llx\n", fvb, (unsigned long long)gl_digest_c(&glf));
  }
  sceKernelDcacheWritebackAll();         /* the GE reads the list, the frame and the textures from RAM */
  if (gl_shown && sceDisplayGetVcount() == gl_show_vc) flip_race_dir++;   /* gl27: counted only */
  sceGuStart(GU_DIRECT, gl_dl); sceGuCallList(gxl.w); sceGuFinish();
  gl_ge = 1; gl_ge_buf = d; gl_ge_vb = fvb;
  if (!(g_opt & OPT_GL_ASYNC)) gl_wait();
}
/* a frame buffer as a 24-bit BMP next to the EBOOT, through the uncached view, 16 rows at a time (gl28d: the shots' writer, shared with
   MGS2_GL_LIVE): 1 saved, 0 a write failed, -1 not opened; *rd: the microseconds spent reading VRAM */
static uint8_t gl_rows[16 * 1440];
static __attribute__((noinline)) int gl_bmp(const char *p, int buf, uint32_t *rd)
{
  uint8_t h[54]; uint32_t sz = 54 + 480 * 272 * 3, isz = 480 * 272 * 3, t0 = 0; int32_t w = 480, ht = 272;
  SceUID fd; int ok;
  memset(h, 0, sizeof h); h[0] = 'B'; h[1] = 'M'; memcpy(h + 2, &sz, 4); h[10] = 54; h[14] = 40; memcpy(h + 18, &w, 4); memcpy(h + 22, &ht, 4);
  h[26] = 1; h[28] = 24; memcpy(h + 34, &isz, 4);
  if (rd) *rd = 0;
  if ((fd = sceIoOpen(p, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777)) < 0) return -1;
  ok = sceIoWrite(fd, h, 54) == 54;
  const volatile uint32_t *src = (const volatile uint32_t *)(0x44000000u + (buf ? GL_FB1 : GL_FB0));
  for (int y0 = 271; y0 >= 0 && ok; y0 -= 16) {   /* bottom-up rows of B, G, R; the GE's 8888 has R in the low byte */
    int n = 0;
    if (rd) t0 = sceKernelGetSystemTimeLow();   /* (the shots: no timer) */
    for (int y = y0; y > y0 - 16 && y >= 0; y--, n++)
      for (int x = 0; x < 480; x++) { uint32_t c = src[y * 512 + x]; uint8_t *o = gl_rows + n * 1440 + 3 * x; o[0] = (uint8_t)(c >> 16); o[1] = (uint8_t)(c >> 8); o[2] = (uint8_t)c; }
    if (rd) *rd += sceKernelGetSystemTimeLow() - t0;
    ok = sceIoWrite(fd, gl_rows, n * 1440) == n * 1440;
  }
  sceIoClose(fd);
  return ok;
}
/* gl29: MGS2_GL_ZDUMP: the depth buffer (512 x 272 16-bit words from base, the uncached view or its |0x600000 mirror) as it is, row 0
   first, to p, 16 rows at a time through gl_rows (no heap): 1 saved, 0 a write failed, -1 not opened; *rd: the microseconds reading VRAM */
static GL28D_COLD int gl_zdump(const char *p, uint32_t base, uint32_t *rd)
{
  SceUID fd; int ok = 1;
  *rd = 0;
  if ((fd = sceIoOpen(p, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777)) < 0) return -1;
  const volatile uint32_t *src = (const volatile uint32_t *)base;
  for (int y0 = 0; y0 < 272 && ok; y0 += 16) {   /* 16 rows: 4096 words, 16 KB */
    uint32_t t0 = sceKernelGetSystemTimeLow();
    for (int i = 0; i < 16 * 256; i++) { uint32_t w = src[y0 * 256 + i]; uint8_t *o = gl_rows + 4 * i;
      o[0] = (uint8_t)w; o[1] = (uint8_t)(w >> 8); o[2] = (uint8_t)(w >> 16); o[3] = (uint8_t)(w >> 24); }   /* the words as in VRAM */
    *rd += sceKernelGetSystemTimeLow() - t0;
    ok = sceIoWrite(fd, gl_rows, 16 * 1024) == 16 * 1024;
  }
  sceIoClose(fd);
  return ok;
}
/* the last frame shown, as a 24-bit BMP next to the EBOOT (gl28d: kept out of line, as before).  gl29: at the hook of vb; z 1: an
   MGS2_GL_SHOT shot (MGS2_GL_ZDUMP's depth from zd_from on), 0: an MGS2_GL_SHOT_RANGE one; with MGS2_GL_DZ or MGS2_GL_ZDUMP on, the
   line gains the arm, 'last list = shot frame', the depth files and the VRAM read times */
static __attribute__((noinline)) void gl_shot_do(long vb, int z);
static __attribute__((noinline)) void gl_shot(long vb, int z) { uint32_t o = bb_set(BB_SH); gl_shot_do(vb, z); bb_set(o); }   /* gl44: SH */
static __attribute__((noinline)) void gl_shot_do(long vb, int z)
{
  char p[256]; int ok, g29 = dz_on || zd_from >= 0; uint32_t rc = 0;
  snprintf(p, sizeof p, "%sshot_%ld.bmp", host_dir(), gl_shown_vb);
  if ((ok = gl_bmp(p, gl_shown_buf, g29 ? &rc : 0)) < 0) { fprintf(stderr, "picture: cannot write %s\n", p); return; }
  gl_shots += ok;
  if (!g29) { fprintf(stderr, "picture: the frame of vb %ld %s %s\n", gl_shown_vb, ok ? "saved as" : "NOT saved:", p); return; }
  char a[8], q[256], m[256], zs[640]; uint32_t rz = 0, rm = 0; int oz = -2, om = -2, same = gl29_same();
  if (z && zd_from >= 0 && vb >= zd_from) {
    snprintf(q, sizeof q, "%szbuf_%ld.raw", host_dir(), gl_shown_vb); oz = gl_zdump(q, 0x44000000u + GL_ZB, &rz);
    if (zd_views == 2 || vb == zd_mir) { snprintf(m, sizeof m, "%szbm_%ld.raw", host_dir(), gl_shown_vb); om = gl_zdump(m, 0x44600000u + GL_ZB, &rm); } }
#define ZST(o) ((o) == 1 ? "saved as" : (o) ? "NOT saved (not opened):" : "NOT saved (a write failed):")
  if (oz == -2) snprintf(zs, sizeof zs, "no depth");
  else { int k = snprintf(zs, sizeof zs, "depth %s %s (VRAM read %.1f ms)", ZST(oz), q, rz / 1000.0);
    if (om != -2 && k > 0 && k < (int)sizeof zs) snprintf(zs + k, sizeof zs - k, "; the |0x600000 mirror %s %s (VRAM read %.1f ms)", ZST(om), m, rm / 1000.0); }
#undef ZST
  fprintf(stderr, "picture: the frame of vb %ld %s %s; gl29 shot vb %ld (%s, %s turn): dz %s; last list = shot frame: %s; colour VRAM read "
          "%.1f ms; %s\n", gl_shown_vb, ok ? "saved as" : "NOT saved:", p, vb, z ? "MGS2_GL_SHOT" : "MGS2_GL_SHOT_RANGE",
          gl29_me() ? "me" : "main", dz_on ? dz_fnm(gl_shown_vb, a) : "off", same ? "yes" : "NO", rc / 1000.0, zs);
}
/* psp/main_psp.c host_vb_end, after the next step's timer started: the picture's work at the hook that starts step vb counts in step vb */
void xl_psp_frame(long vb)
{
  me_vb = vb;   /* task #149: the heap hooks' events */
  if (mq_on) { mq_frame(vb); return; }   /* task #180 step 4c: the hook's work through the ME's loop */
  mp_hook(vb);  /* task #180: the step's records walked, before its frame ends */
  if (gl_ok != 1) return;
  HOST_ENTER(HOST_GL);
  if (gl_now) { gl_now = 0; gl_end(&glf); gl_list(vb - 1); }
  if (gl_ge && sceGeDrawSync(1) == 0) gl_show();
  if (gl_ok == 1 && gl_full && state == 1 && (g_opt & (OPT_XL | OPT_GL)) == (OPT_XL | OPT_GL)) {
    if (gl_ge) gl_wait();                /* the frame memory and the cache's last textures are the GE's until it has drawn */
    if (gl_ok == 1) {                    /* (gl_sync's stop: no more frames) */
      glf.h.cut = (g_opt & OPT_GL_CUT) != 0; glf.h.clipfast = (g_opt & OPT_CLIP_FAST) != 0;
      glf.stepc = (g_opt & OPT_XL_STEPC) != 0;   /* sb4: step c (renderer_notes 11.29) */
      glf.stepc7 = (g_opt2 & OPT2_C7) != 0;      /* sb10 T2: and lane 4's P07 calls */
      glf.h.c1 = (g_opt2 & OPT2_C1) != 0;        /* sb11 C1: the faces skinned and lit by the GE */
      glf.h.c2 = (g_opt2 & OPT2_C2) != 0;        /* sb12 C2: the people skinned and lit by the GE */
      glf.h.c3v = (g_opt2 & OPT2_C3V) != 0;      /* sb14 3v: df0 in two passes */
      glf.h.c3z = (g_opt2 & OPT2_C3Z) != 0;      /* sb14 3z: df0's depth range by the box */
      glf.h.c3w = (g_opt2 & OPT2_C3W) != 0;      /* sb14 3w: two-pass df0 vertices straight into the frame */
      glf.h.c4 = (g_opt2 & OPT2_C4) != 0;        /* sb15 step 4: the sprites, quads and streaks drawn */
      glf.h.c4x = (g_opt2 & OPT2_C4X) != 0;      /* sb15 4x: the effect strips from their clip positions */
      glf.h.cull = rt_nat_cull_at(vb);           /* cull study */
      glf.bd = bd_cur = rt_bound_at(vb, 0); gl_ft.all = (bd_cur & (BD_SORT | BD_RAIN)) != 0; bd_fr += bd_cur != 0;   /* gl38 */
      gl_begin(&glf); gl_now = 1; thin_due = 1;   /* task #149 */
      if (gl_first_vb < 0) { gl_first_vb = vb; fprintf(stderr, "picture: the first frame begins at vb %ld\n", vb); }
    }
  }
  tk_frame(tk);
  HOST_LEAVE(HOST_GL);
}
/* psp/main_psp.c finish(), before the summary: no more frames, the GE idle, the harness's text on the screen again */
void xl_psp_gl_end(void)
{
  mp_end();   /* task #180: the last step's records */
  if (gl_ok != 1 || gl_sync()) return;   /* (gl_sync's stop puts the text on the screen itself) */
  gl_ok = 2; gl_now = gl_ge = 0;
  sceDisplaySetFrameBuf((void *)(0x44000000u + GL_TXT), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
}
#endif
/* psp/main_psp.c host_vb_end, before it clears them: the draw-pipeline calls of the step that ended, full and reduced; its transfers come
   in the step that starts now */
void xl_psp_vblank(uint32_t full, uint32_t reduced)
{
  cfg();
  skip_now = skip_mode && !full && reduced;
  if (skip_now && state == 1) sk_vb++;
#ifdef MGS2_GL
  gl_full = full != 0;
#endif
}
#ifdef MGS2_GL
/* psp/main_psp.c host_vb_end, outside the step's time: MGS2_GL_SHOT's saves at the hook that starts step vb */
void xl_psp_gl_vblank(long vb)
{
  int r = 0;
  if (eye_on) { long n = eye_n[1]; eye_poll(vb); r = eye_n[1] != n; }   /* gl28d: MGS2_EYE (r: R went down at this hook) */
  if (gl_ok != 1) return;
  int sh = 0;   /* gl29: a shot of MGS2_GL_SHOT's at this hook (then none of MGS2_GL_SHOT_RANGE's) */
  for (int k = 0; k < shot_n; k++) if (shot_vb[k] == vb) {
    sh = 1;
    if (mq_on && shot_fw && flip_on && flip_from >= 0 && vb >= flip_from) { if (shot_wait(vb)) return; }   /* gl28d: MGS2_GL_SHOT_FLIPWAIT */
    else if (mq_on) { flip_force = 1; mq_drain(); mq_service(); mq_ge_finish(); flip_force = 0; if (gl_ok != 1) return; }   /* gl27: no flip
                                                                                                                        wait here */   /* task #180 step 4c: the loop's entries done, its
                                                                                       last list drawn and shown */
    else if (gl_ge) { if (gl_sync()) return; gl_show(); }
    if (gl_shown_buf >= 0) gl_shot(vb, 1); else fprintf(stderr, "picture: no frame shown yet at vb %ld\n", vb);
  }
  if (!sh && sr_from >= 0 && vb >= sr_from && vb <= sr_to && (vb - sr_from) % sr_step == 0) {   /* gl29: MGS2_GL_SHOT_RANGE, drained as above */
    if (mq_on && shot_fw && flip_on && flip_from >= 0 && vb >= flip_from) { if (shot_wait(vb)) return; }
    else if (mq_on) { flip_force = 1; mq_drain(); mq_service(); mq_ge_finish(); flip_force = 0; if (gl_ok != 1) return; }
    else if (gl_ge) { if (gl_sync()) return; gl_show(); }
    if (gl_shown_buf >= 0) gl_shot(vb, 0); else fprintf(stderr, "picture: no frame shown yet at vb %ld\n", vb);
  }
  if (dz_on) dz_tick(stderr, gl_shown_vb);   /* gl29: the lines of the windows the screen has left */
  if (live_from >= 0) gl_live(vb, r);   /* gl28d: MGS2_GL_LIVE */
}
/* psp/v26me4_exc.c built with -DMGS2_GL_EXC (psp/gl1.sh), in the exception: the frame buffer on the display, through the uncached view
   (the crash record goes over the game's picture there), or 0 while the harness's text is on it */
uint32_t xl_psp_gl_screen(void) { return gl_ok == 1 && gl_shown_buf >= 0 ? 0x44000000u + (gl_shown_buf ? GL_FB1 : GL_FB0) : 0; }
#endif
/* the translator's quadword, word, call and A+D counters are 32-bit longs on the PSP (a route passes 2^31 words) and xl_reset clears
   its own: summed here a transfer at a time, from their low 32 bits */
static uint64_t t_qw, t_words, t_calls, t_ad; static uint32_t l_qw, l_words, l_calls, l_ad;
static void sum(void)
{
  const XlStats *S = xl_stats(); uint32_t q = (uint32_t)M.qw, w = (uint32_t)S->words, c = (uint32_t)S->calls, a = (uint32_t)S->ad_kicks;
  t_qw += q - l_qw; l_qw = q; t_words += w - l_words; l_words = w; t_calls += c - l_calls; l_calls = c; t_ad += a - l_ad; l_ad = a;
}
static const uint8_t *ram(void *ctx, uint32_t ee, uint32_t n)
{
  (void)ctx; ee &= XL_GSZ - 1;
  if (ee + n > XL_GSZ) return 0;
#ifdef MGS2_GL
  if (M.view) return xv_ram(M.view, ee, n);   /* task #180: in a record's late walk, through its copy */
#endif
  return M.mem + ee;
}
#ifndef MGS2_GL
static void on_call(void *ctx, const XlCall *c) { (void)ctx; (void)c; ncall++; }
static void on_prim(void *ctx, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n) { (void)ctx; (void)g; (void)d; (void)v; (void)n; nprim++; }
#else
static void on_call(void *ctx, const XlCall *c)   /* a converter's call is the picture's work (HOST_GL); the rest only counts */
{
  (void)ctx; ncall++;
  if (!gl_now) return;
  if ((gl_conv_entry(c->entry) || (glf.h.c4 && gl_c4_entry(c->entry))) && !c->cont) { HOST_ENTER(HOST_GL); gl_call(&glf, c); HOST_LEAVE(HOST_GL); }
  else gl_call(&glf, c);
}
static void on_pass(void *ctx, const XlCall *c, int pass)   /* P06's passes 2 and 3 (HOST_GL) */
{
  (void)ctx;
  if (gl_now) { HOST_ENTER(HOST_GL); gl_pass(&glf, c, pass); HOST_LEAVE(HOST_GL); }
}
static void on_flush(void *ctx)   /* sb4, step c: a transfer's end: the P06 calls held back are converted (HOST_GL) */
{
  (void)ctx;
  if (glf.nheld) { HOST_ENTER(HOST_GL); glf.hflush[0]++; gl_flush(&glf); HOST_LEAVE(HOST_GL); }
}
static void on_prim(void *ctx, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n)
{
  (void)ctx; nprim++;
  if (gl_now) gl_prim(&glf, g, d, v, n);
}
#endif

#ifdef MGS2_GL
/* ---- task #180, the ME picture's build order step 4b-1 (me_step4_notes.md): the split on the main CPU.  MGS2_ME_PIC=1 in mgs2.cfg:
   each transfer the translator would take (not one of a step "shown frames only" leaves out) is queued as a record with its switches
   (g_opt, g_opt2), whether the runtime's walk would have fed it (the one walk) and its lanes; the runtime's walk marks the image data it
   skips (runtime/ee_io.h XL_IMG); as the transfer ends, ge/xlme.c xm_copy copies what its late walk will read into the arena (the image
   data outside the two ranges the game rewrites stays in RAM); at the next vblank hook, before the frame ends, the step's records are
   walked in order (ge/xlme.c xm_switch, xm_walk: the lanes in the late walk) through each one's view (ge/xlview.h: its copy where it has
   the bytes, RAM elsewhere), and the GS map reads through that view too.  No room in the arena: the records before are walked then and
   the copy made again; still none (or a chain that does not end, or too many ranges): the record walked at once from RAM, which still
   holds the bytes as sent.  MGS2_ME_PIC_KB: the arena (KB, default 3072); MGS2_ME_PIC_RNG: entries in each of its work arrays (16384). */
static int mp_on;
static XmPic xmp ME_LN; static XmCopy xmc; static XmView mp_view ME_LN;   /* (ME_LN: step 4d-3) */
#define MP_NREC 64                       /* records a step (19 at most on the x86 routes: me_step1_notes.md 3) */
typedef struct { int ch, lanes, restart, x0, nx, t, filled, h0, nh; uint32_t chcr, madr, qwc, tadr, opt, opt2;
                 int nb; int16_t ng0[2], ngn[2], p0[2], pn[2]; uint32_t noff;   /* native step 1: as ge/xme.h XmeRec (step 2: p0 / pn the people's) */
                 uint8_t nru[2];                          /* gl36: as ge/xme.h XmeRec (0 or 1) */
                 int16_t chi;                             /* gl38: the transfer's chain (ns_chain_at, as it started) */ } MpRec;   /* (4b-2: t, filled,
                                            and the check's line hashes h0 .. h0 + nh - 1) */
/* ---- task #180 step 4b-2 (me_step4_notes.md): MGS2_ME_PIC=2, the copy at its deadline.  From the hook of vb MGS2_ME_PIC_DL on (default
   60: the boot's writers are not guarded, me_handoff_design.md 4.10) a record's copy is planned as its transfer ends (ge/xlme.c xm_plan:
   the extents and their places in the arena) and its lines go into the filter (a bit per 64-byte line of RAM); its bytes are copied at
   the deadline, the hook that ends the step, from RAM as it is then with the guards' log laid over them (xm_fill).  The guards (the 12
   G0 functions' stores through boot/gen_pre.h, the runtime's writes into game RAM through runtime/ee_io.h ME_GUARD_RT: the scratchpad DMA
   to RAM, the SIF's IOP-to-EE copies, the disc reads, three kernel calls) log, before they write, the 64 old bytes of each line in the
   filter once per line and count of records started (t).  A record whose copy holds scratchpad bytes (unguarded: the game writes the
   scratchpad from everywhere) is copied as its transfer ends, as in 4b-1.  The log full, the queue full or no room in the arena: every
   waiting copy filled then (with the walks, for the last two).  MGS2_ME_PIC_LOG: the log's entries (default 1024, 72 bytes each);
   MGS2_ME_PIC_CHK=1: the check, each deadline copy compared line by line with the bytes as its transfer ended (hashes, MGS2_ME_PIC_CHKN
   of them at most a step, default 65536): a line that differs is a write no guard logged. */
int g_g0_on;                             /* boot/gen_pre.h rt_g0_pre, runtime/ee_io.h DGT_PRE: a copy is due (or a record in its walk) */
extern int g_xl_img, g_xl_end;           /* runtime/ee_io.h (boot.c): set from a queued record's start to its end */
static int mp_mode;                      /* 1: the copy as the transfer ends (4b-1); 2: at its deadline (4b-2) */
static long mp_dl = 60; static int mp_chk, mp_seg, mp_unfilled;   /* the hand-over vb; the check; records started (their t); waiting */
static XmLogE *mp_log; static int mp_nlog, mp_logcap, mp_nlog_max;
typedef struct { uint32_t l; int32_t t; uint32_t gen; } MpH;        /* the log's lines: line -> the t it was last logged with */
static MpH *mp_h; static uint32_t mp_hn, mp_hgen = 1; static int mp_hbits;
static uint8_t *mp_bm;                   /* the filter: the lines of the waiting copies' RAM extents */
static uint32_t *mp_ch; static int mp_chn, mp_chcap;   /* the check's line hashes (as each transfer ended) */
static long mg_st[13], mg_skip[13], mg_dup[13], mg_log[13], mg_walk[13]; static uint64_t mg_rtb;   /* guards: [0] the runtime, [1..12] G0 */
static long mp_fill_dl, mp_fill_early[3], mp_fill_end[3], mp_full_log;   /* fills at the deadline, early (log, queue, room), at the end
                                                                             (boot, scratchpad, 4b-1) */
static long mp_chk_lines, mp_chk_diff, mp_chk_rec, mp_chk_skip, mp_chk_prn;
static XmCopy xmr; static uint8_t *mp_chr;   /* the check's second walk (rd_only: the bytes the transfer itself reads) and its flag
                                               per line piece (b4): bits 0-3 the piece's quadwords the transfer reads (by place in
                                               their line), bits 4-7 a 4-bit hash of its other quadwords (the margin) when it has both */
static long mp_chk_drd, mp_chk_dmg, mp_chk_prnm, mp_chk_rdfail, mp_chk_lrd, mp_chk_dboth;
static int mp_nolay, mp_scr; static long mp_scr_q, mp_scr_rec;   /* me18c b5 (diagnostics, one a run): MGS2_ME_PIC_NOLAY=1 the guards' log
                                                                  not laid over the copies (the check's control); MGS2_ME_PIC_SCRAMBLE=1
                                                                  (with the check) each copy's margin quadwords turned over before its walk */
static long mpc_w = -1, mpc_rd, mpc_mg, mpc_rec;   /* me18c b3: the check's differing lines by window of 100 vblanks (printed as each ends) */
static void mpc_flush(void)
{
  if (mpc_w >= 0 && (mpc_rd | mpc_mg))
    fprintf(stderr, "me picture check window: vb %ld-%ld: %ld lines differ (%ld in bytes the transfers read, %ld only in a margin) in %ld copies\n",
            mpc_w * 100, mpc_w * 100 + 99, mpc_rd + mpc_mg, mpc_rd, mpc_mg, mpc_rec);
  mpc_rd = mpc_mg = mpc_rec = 0;
}
#ifdef ME_WATCH
/* ---- task #180 step 4b-2, diagnostic b3 (built with -DME_WATCH; MGS2_ME_PIC_WATCH=1 with MGS2_ME_PIC_CHK=1): the read lines the check
   finds changed (the first MPW_N) are watched from then on.  When a later copy's plan holds one, its 64 bytes as the transfer ends are
   kept, and every checkpoint (runtime/ee_io.h ME_WCK: a transfer's start and end, a guarded write's logged line, a kernel call, a thread
   switch, an interrupt handler or alarm before and after, the vblank hook's parts, the native qword copy before and after, the deadline)
   compares the line with them: a change is printed with the checkpoints around it and the words that changed.  A line a guard logs is
   covered by the log from there on and is let go; at the deadline every line is let go.  A line the check finds changed although RAM
   still holds its bytes as the transfer ended is printed with the log's entries for it. ---- */
#define MPW_N 32
typedef struct { uint32_t l; int armed, letgo, t, ch, seen, prn; unsigned rmask; uint32_t tadr; long vb; uint8_t b[64]; } MpW;   /* b4: prn, rmask */
static MpW mpw[MPW_N]; static int mpw_n, mpw_on; static uint8_t *mpw_bm; int g_mpw_armed;
static int mpw_lid; static uint32_t mpw_la, mpw_lb; static long mpw_lvb;
static long mpw_prn, mpw_chg, mpw_logd, mpw_d_chg, mpw_d_same, mpw_d_ram, mpw_d_new, mpw_d_logd, mpw_d_other, mpw_iv[20][20], mpw_ivm[20][20], mpw_chg_rd;
static const char *const mpw_name[20] = { "the plan", "a transfer's start (ch, madr)", "a transfer's end (ch, record)",
  "a guarded write's logged line (who, line)", "a kernel call (number, pc)", "a thread switch (from, to)", "a handler's start (fn, cause)",
  "a handler's end (fn, cause)", "an alarm's start (fn, alarm)", "an alarm's end (fn, alarm)", "the hook: before io_vblank (start, vb)",
  "the hook: after io_vblank (start, vb)", "the hook: after opt_window (-, vb)", "the hook: after sif_vblank (-, vb)",
  "the hook: before host_vb_end (-, vb)", "the deadline: before the fill (why, records)", "the native qword copy: before (dst, qwords)",
  "the native qword copy: after (dst, qwords)", "?", "?" };
static void mpw_change(MpW *w, const uint8_t *p, int id, uint32_t a, uint32_t b)
{
  const uint32_t *o = (const uint32_t *)w->b, *n = (const uint32_t *)p; int nd = 0, k0 = -1;
  int inr = 0;
  for (int k = 0; k < 16; k++) if (o[k] != n[k]) { if (k0 < 0) k0 = k; nd++; if ((w->rmask >> (k >> 2)) & 1) inr = 1; }
  mpw_chg++; w->seen++; if (inr) { mpw_iv[mpw_lid][id]++; mpw_chg_rd++; } else mpw_ivm[mpw_lid][id]++;
  if (w->prn < 8 && mpw_prn < 1000) {   /* b4: at most 8 a line */
    mpw_prn++; w->prn++;
    fprintf(stderr, "me picture watch: vb %ld: line %07x (record %d, ch %d, tadr %08x, planned at vb %ld) changed between %s %x %x (vb %ld) and %s "
            "%x %x: %d words, from +%02x:", me_vb, (unsigned)(w->l << 6), w->t, w->ch, (unsigned)w->tadr, w->vb, mpw_name[mpw_lid], (unsigned)mpw_la,
            (unsigned)mpw_lb, mpw_lvb, mpw_name[id], (unsigned)a, (unsigned)b, nd, 4 * k0);
    for (int k = 0, m = 0; k < 16 && m < 6; k++) if (o[k] != n[k]) { fprintf(stderr, " +%02x%c %08x>%08x", 4 * k, (w->rmask >> (k >> 2)) & 1 ? 'r' : 'm',
                                                                                    (unsigned)o[k], (unsigned)n[k]); m++; }
    fprintf(stderr, "\n"); }
  memcpy(w->b, p, 64);
}
void rt_me_watch(int id, uint32_t a, uint32_t b)   /* runtime/ee_io.h ME_WCK: a checkpoint (called only while a line is armed) */
{
  for (int i = 0; i < mpw_n; i++) {
    MpW *w = &mpw[i]; const uint8_t *p;
    if (!w->armed) continue;
    p = M.mem + ((size_t)w->l << 6);
    if (memcmp(p, w->b, 64)) mpw_change(w, p, id, a, b);
  }
  mpw_lid = id; mpw_la = a; mpw_lb = b; mpw_lvb = me_vb;
}
static MpW *mpw_find(uint32_t l) { for (int i = 0; i < mpw_n; i++) if (mpw[i].l == l) return &mpw[i]; return 0; }
static int mpw_bit(uint32_t l) { return (mpw_bm[l >> 3] >> (l & 7)) & 1; }
static void mpw_arm(const MpRec *e, uint32_t l, unsigned qm)   /* mp_chk_plan: a watched line in a copy planned now (as its transfer ends);
                                                                 qm: the piece's quadwords the transfer reads */
{
  MpW *w = mpw_find(l);
  if (!w) return;
  if (w->armed) { if (w->t == e->t) w->rmask |= qm; return; }
  w->armed = 1; w->letgo = 0; g_mpw_armed++; w->t = e->t; w->ch = e->ch; w->tadr = e->tadr; w->vb = me_vb; w->seen = 0; w->rmask = qm;
  memcpy(w->b, M.mem + ((size_t)l << 6), 64);
  mpw_lid = 2; mpw_la = (uint32_t)e->ch; mpw_lb = (uint32_t)e->t; mpw_lvb = me_vb;   /* (xlive_chk_end checked the lines armed before) */
}
static void mpw_letgo(void) { for (int i = 0; i < mpw_n; i++) mpw[i].armed = 0; g_mpw_armed = 0; }
static void mpw_logged(uint32_t l, int who)   /* mg_lines: a guard logs line l (in the filter) before it writes */
{
  MpW *w;
  if (!mpw_bit(l) || !(w = mpw_find(l)) || !w->armed) return;
  rt_me_watch(3, (uint32_t)who, l << 6);
  w->armed = 0; w->letgo = 1; g_mpw_armed--; mpw_logd++;
}
static void mpw_diff(const MpRec *e, uint32_t a, uint32_t f, const uint8_t *cp)   /* mp_chk_fill: a read line of e's copy differs */
{
  uint32_t l = a >> 6; MpW *w = mpw_bit(l) ? mpw_find(l) : 0;
  if (!w) {
    if (mpw_n < MPW_N) { w = &mpw[mpw_n++]; memset(w, 0, sizeof *w); w->l = l; mpw_bm[l >> 3] |= (uint8_t)(1u << (l & 7)); }
    mpw_d_new++; return; }
  if (w->t != e->t || (!w->armed && !w->letgo)) { mpw_d_other++; return; }
  if (w->letgo) { mpw_d_logd++; return; }
  if (w->seen) { mpw_d_chg++; return; }
  { const uint8_t *r = M.mem + a; const uint32_t *cw = (const uint32_t *)cp, *rw = (const uint32_t *)r; int same = !memcmp(M.mem + ((size_t)l << 6), w->b, 64), k0 = -1;
    if (same) mpw_d_same++; else mpw_d_ram++;
    for (uint32_t k = 0; k < (f - a) / 4; k++) if (cw[k] != rw[k]) { k0 = (int)k; break; }
    if (mpw_prn < 400) {
      mpw_prn++;
      fprintf(stderr, "me picture watch: vb %ld: line %07x+%u (record %d, ch %d, tadr %08x) differs in the deadline copy with no change seen at a "
              "checkpoint; RAM now %s the bytes as the transfer ended; the copy against RAM: %s", me_vb, (unsigned)a, (unsigned)(f - a), e->t, e->ch,
              (unsigned)e->tadr, same ? "holds" : "differs from", k0 < 0 ? "equal" : "differs");
      if (k0 >= 0) fprintf(stderr, " from +%02x (%08x, RAM %08x)", (unsigned)(4 * k0 + (a & 63)), (unsigned)cw[k0], (unsigned)rw[k0]);
      fprintf(stderr, "; the log's entries for the line:");
      for (int i = mp_nlog - 1, m = 0; i >= 0 && m < 6; i--) if (mp_log[i].l == l) {
        m++; fprintf(stderr, " %d (t %d, its bytes %s)", i, (int)mp_log[i].t, memcmp(mp_log[i].old, w->b, 64) ? "differ from the plan's" : "= the plan's"); }
      fprintf(stderr, "\n"); } }
}
#endif
static MpRec mp_rec[MP_NREC]; static int mp_nrec, mp_restart_due;
static long mp_queued, mp_late_runs, mp_drain_full, mp_drain_room, mp_direct, mp_hooks, mp_hist[9]; static uint32_t mp_step_max; static long mp_step_max_vb;
static long mp_img_runs, mp_img_spr, mp_img_small, mp_img_full;
static void sum(void);
static SceUID mp_sys = -1; static uint32_t mp_sys_n, mp_sys_free[4];   /* step 4d-3: the system's block for the plan's rb, tmp and extents */
static void mp_setup(void)
{
  const char *e; uint32_t kb, vk; int cap, caps, xcap, sys;
  if (!mp_want) return;
  kb = (e = mgs2_getenv("MGS2_ME_PIC_KB")) && atoi(e) >= 256 ? (uint32_t)atoi(e) : 3072;
  cap = (e = mgs2_getenv("MGS2_ME_PIC_RNG")) && atoi(e) >= 1024 ? atoi(e) : 16384;
  /* step 4d-3, mode 4 (your PSP's): ra and mk sized to their use (at most 541 and 16 entries in 4c's runs: 2,048 each, MGS2_ME_PIC_RNGS),
     the extents to a walk's (at most 2,353 in 4d-1's runs: 4,096, MGS2_ME_PIC_EXT), rb and tmp as before (at most 10,513 used: 16,384,
     MGS2_ME_PIC_RNG); rb, tmp and the extents in a block of the system's memory outside newlib's heap (MGS2_ME_PIC_SYS=0: newlib's heap).
     An array that fills is exact: xm_plan's fail, the record walked from RAM (an image mark left out: its bytes copied) */
  caps = mp_want == 4 ? 2048 : 0; xcap = mp_want == 4 ? 4096 : cap; sys = mp_want == 4;
  if ((e = mgs2_getenv("MGS2_ME_PIC_RNGS"))) caps = atoi(e) >= 64 ? atoi(e) : 0;
  if (caps >= cap) caps = 0;
  if ((e = mgs2_getenv("MGS2_ME_PIC_EXT")) && atoi(e) >= 256) xcap = atoi(e);
  if ((e = mgs2_getenv("MGS2_ME_PIC_SYS"))) sys = atoi(e) != 0;
  memset(&xmc, 0, sizeof xmc);
  xmc.mem = M.mem; xmc.spr = M.spr; xmc.mem_size = XL_GSZ; xmc.cap = cap; xmc.cap_s = caps; xmc.ext_cap = xcap; xmc.arena_n = kb << 10;
  if (sys) {
    uint32_t nb = ((uint32_t)cap * 2u * (uint32_t)sizeof(XcR) + (uint32_t)xcap * (uint32_t)sizeof(XvExt) + 63u) & ~63u;
    mp_sys_free[0] = (uint32_t)sceKernelMaxFreeMemSize(); mp_sys_free[1] = (uint32_t)sceKernelTotalFreeMemSize();
    mp_sys = sceKernelAllocPartitionMemory(2, "me_pic_plan", PSP_SMEM_Low, nb, 0);
    mp_sys_free[2] = (uint32_t)sceKernelMaxFreeMemSize(); mp_sys_free[3] = (uint32_t)sceKernelTotalFreeMemSize();
    if (mp_sys >= 0) {
      uint8_t *bl = (uint8_t *)sceKernelGetBlockHeadAddr(mp_sys); mp_sys_n = nb;
      xmc.rb = (XcR *)bl; xmc.tmp = (XcR *)(bl + (size_t)cap * sizeof(XcR)); xmc.ext = (XvExt *)(bl + (size_t)cap * 2 * sizeof(XcR)); }
    else fprintf(stderr, "me picture split (step 4d-3): no block of %u KB of the system's memory for the plan's arrays (%08x): in newlib's "
                 "heap\n", (unsigned)(nb >> 10), (unsigned)mp_sys); }
  xmc.ra = malloc((size_t)XC_CAPS(&xmc) * sizeof(XcR)); xmc.mk = malloc((size_t)XC_CAPS(&xmc) * sizeof(XcR));
  if (mp_sys < 0) { xmc.rb = malloc((size_t)cap * sizeof(XcR)); xmc.tmp = malloc((size_t)cap * sizeof(XcR)); xmc.ext = malloc((size_t)xcap * sizeof(XvExt)); }
  vk = mp_want == 4 ? ((e = mgs2_getenv("MGS2_ME_PIC_VIEW_KB")) ? (uint32_t)atoi(e) : 128) : 0;   /* step 4d-2: the view's buffers (below) */
#ifdef MGS2_ME_REAL
  if (mp_want == 4) {   /* step 4d-4: the loop on the ME (MGS2_ME_PIC_ME, default 1; never on the emulator), its arena and the view's
                           buffers in its eDRAM when they fit (MGS2_ME_PIC_EDRAM, default 1) */
    mq_me_try = !((e = mgs2_getenv("MGS2_ME_PIC_ME")) && !atoi(e)) && !mq_emu();
    mq_edram = mq_me_try && !((e = mgs2_getenv("MGS2_ME_PIC_EDRAM")) && !atoi(e)) && kb + 4u * vk <= MQ_EDRAM_KB; }
  xmc.arena = mq_edram ? (uint8_t *)(uintptr_t)MQ_EDRAM : memalign(64, xmc.arena_n);
#else
  xmc.arena = memalign(64, xmc.arena_n);
#endif
  if (!xmc.ra || !xmc.rb || !xmc.mk || !xmc.tmp || !xmc.ext || !xmc.arena) {
    fprintf(stderr, "me picture split: no memory for the arena (%u KB) and its arrays (%d entries): off, today's walk\n", (unsigned)kb, cap);
    free(xmc.ra); free(xmc.mk); if (!MQ_ED()) free(xmc.arena);
    if (mp_sys >= 0) { sceKernelFreePartitionMemory(mp_sys); mp_sys = -1; } else { free(xmc.rb); free(xmc.tmp); free(xmc.ext); }
#ifdef MGS2_ME_REAL
    mq_me_try = mq_edram = 0;
#endif
    mp_want = 0; return; }
  memset(&xmp, 0, sizeof xmp);
  xmp.m = &M; xmp.map = map; xmp.tk = tk; xmp.glf = &glf; xmp.gxl = &gxl; xmp.gxh = &gxh; xmp.follow = XM_F_ALL; xmp.lanes = 15; xmp.lw_on = 1;
  xm_init(&xmp);
  memset(&mp_view, 0, sizeof mp_view); mp_view.arena = xmc.arena; mp_view.mem = M.mem; mp_view.spr = M.spr; mp_view.mem_size = XL_GSZ;
  if (mp_want == 4) {   /* task #180 step 4d-2: the view's four fixed buffers for the map's reads partly in a copy (the eDRAM after the
                           arena with the loop on the ME, step 4d-4; else main RAM), MGS2_ME_PIC_VIEW_KB each (default 128: the longest
                           such read in 4c's runs, 131,072 bytes; 0: none) */
    int i = 0;
    if (vk && MQ_ED()) {
      for (i = 0; i < 4; i++) mp_view.fix[i] = (uint8_t *)(uintptr_t)(0x80000000u + ((kb + (uint32_t)i * vk) << 10));
      mp_view.fix_n = vk << 10; }
    else if (vk) {
      while (i < 4 && (mp_view.fix[i] = memalign(64, (size_t)vk << 10))) i++;
      if (i < 4) { while (i--) { free(mp_view.fix[i]); mp_view.fix[i] = 0; }
        fprintf(stderr, "me picture split: no memory for the view's fixed buffers (4 x %u KB): the picture heap's\n", (unsigned)vk); }
      else mp_view.fix_n = vk << 10; } }
  mp_on = 1; mp_mode = 1;
  if (mp_want >= 2) {   /* step 4b-2 (and 4c: the check and its diagnostics only in 4b-2) */
    if ((e = mgs2_getenv("MGS2_ME_PIC_DL"))) mp_dl = atol(e);
    mp_chk = mp_want == 2 && (e = mgs2_getenv("MGS2_ME_PIC_CHK")) && atoi(e);
    mp_nolay = mp_want == 2 && (e = mgs2_getenv("MGS2_ME_PIC_NOLAY")) && atoi(e);   /* b5 */
    mp_scr = mp_chk && (e = mgs2_getenv("MGS2_ME_PIC_SCRAMBLE")) && atoi(e);
    if (mp_nolay || mp_scr) fprintf(stderr, "me picture diagnostic b5: %s\n", mp_nolay ? "the guards log as before, but their log is not "
                                    "laid over the copies (MGS2_ME_PIC_NOLAY=1): the check must find the read bytes they keep" : "after the "
                                    "check, each copy's quadwords the transfer does not read (the margins after tags) turned over (xor "
                                    "5a5a5a5a) before its walk (MGS2_ME_PIC_SCRAMBLE=1)");
    mp_logcap = (e = mgs2_getenv("MGS2_ME_PIC_LOG")) && atoi(e) >= 64 ? atoi(e) : 1024;
    for (mp_hbits = 6; (1u << mp_hbits) < 2u * (uint32_t)mp_logcap; mp_hbits++) ;
    mp_hn = 1u << mp_hbits;
    mp_chcap = mp_chk ? ((e = mgs2_getenv("MGS2_ME_PIC_CHKN")) && atoi(e) >= 1024 ? atoi(e) : 65536) : 0;
    mp_log = malloc((size_t)mp_logcap * sizeof *mp_log); mp_h = calloc(mp_hn, sizeof *mp_h); mp_bm = calloc(XL_GSZ / 64 / 8, 1);
    mp_ch = mp_chcap ? malloc((size_t)mp_chcap * sizeof *mp_ch) : 0;
    if (mp_chcap) {   /* the check's second walk: its own arrays, no arena */
      memset(&xmr, 0, sizeof xmr); xmr.mem = M.mem; xmr.spr = M.spr; xmr.mem_size = XL_GSZ; xmr.cap = cap; xmr.ext_cap = cap;
      xmr.arena_n = 0xffffffffu; xmr.rd_only = 1;
      xmr.ra = xmc.ra; xmr.rb = xmc.rb; xmr.mk = xmc.mk; xmr.tmp = xmc.tmp;   /* b3: xmc's work arrays, free once its own plan is made (nmk
                                                                                  0 here: mk is not read), 512 KB less than b2 */
      xmr.ext = malloc((size_t)cap * sizeof(XvExt)); mp_chr = malloc((size_t)mp_chcap);
      if (!xmr.ext || !mp_chr) {
        fprintf(stderr, "me picture check: no memory for its second walk: every line counted as read\n");
        free(xmr.ext); free(mp_chr); mp_chr = 0; } }
#ifdef ME_WATCH
    mpw_on = mp_chk && (e = mgs2_getenv("MGS2_ME_PIC_WATCH")) && atoi(e) && (mpw_bm = calloc(XL_GSZ / 64 / 8, 1));
    if (mpw_on) fprintf(stderr, "me picture watch (diagnostic b3): on: the first %d read lines the check finds changed are watched at the "
                        "checkpoints from then on (MGS2_ME_PIC_WATCH)\n", MPW_N);
#endif
    if (!mp_log || !mp_h || !mp_bm || (mp_chcap && !mp_ch)) {
      fprintf(stderr, "me picture deadline (step 4b-2): no memory for the log (%d entries), its table or the filter: the copy as each "
              "transfer ends (4b-1)\n", mp_logcap);
      free(mp_log); free(mp_h); free(mp_bm); free(mp_ch); mp_log = 0; mp_h = 0; mp_bm = 0; mp_ch = 0; mp_chcap = 0; }
    else {
      mp_mode = 2;
      fprintf(stderr, "me picture deadline (task #180, step 4b-2): on from vb %ld: each copy planned as its transfer ends and filled at "
              "the hook that ends its step, from RAM with the guards' log laid over it (log %d entries at %08x, filter %u KB); records with "
              "scratchpad bytes copied as they end; check %s (MGS2_ME_PIC=2, MGS2_ME_PIC_DL, MGS2_ME_PIC_LOG, MGS2_ME_PIC_CHK)\n", mp_dl,
              mp_logcap, (unsigned)(uintptr_t)mp_log, (unsigned)(XL_GSZ / 64 / 8 / 1024), mp_chk ? "on: each deadline copy compared with "
              "the bytes as its transfer ended, line by line" : "off"); } }
  fprintf(stderr, "me picture split (task #180, step 4b-1): on: each transfer queued with its copy (arena %u KB at %08x, %d entries a work "
          "array), walked at the next vblank hook through its view, the lanes in that walk (MGS2_ME_PIC, MGS2_ME_PIC_KB, MGS2_ME_PIC_RNG)\n",
          (unsigned)kb, (unsigned)(uintptr_t)xmc.arena, cap);
  if (caps || xcap != cap || mp_sys >= 0)
    fprintf(stderr, "me picture split (step 4d-3): the plan's arrays: ra and mk %d entries each, rb and tmp %d each, the extents %d "
            "(MGS2_ME_PIC_RNGS, MGS2_ME_PIC_RNG, MGS2_ME_PIC_EXT): %u KB in all; rb, tmp and the extents %s%08x (%u KB; MGS2_ME_PIC_SYS); the "
            "system's memory outside newlib's heap before it: largest block %u KB, total %u KB; after: %u KB, %u KB\n", XC_CAPS(&xmc), cap, xcap,
            (unsigned)((((size_t)XC_CAPS(&xmc) + (size_t)cap) * 2 * sizeof(XcR) + (size_t)xcap * sizeof(XvExt)) >> 10),
            mp_sys >= 0 ? "in a block of the system's memory at " : "in newlib's heap at ", (unsigned)(uintptr_t)xmc.rb,
            (unsigned)((mp_sys >= 0 ? mp_sys_n : (uint32_t)(((size_t)cap * 2 * sizeof(XcR) + (size_t)xcap * sizeof(XvExt)))) >> 10),
            (unsigned)(mp_sys_free[0] >> 10), (unsigned)(mp_sys_free[1] >> 10), (unsigned)(mp_sys_free[2] >> 10), (unsigned)(mp_sys_free[3] >> 10));
#ifdef MGS2_ME_REAL
  if (mp_want == 4)
    fprintf(stderr, "me picture split (step 4d-4): the loop on the ME %s; the arena (%u KB) and the view's 4 buffers (%u KB each) %s\n",
            mq_me_try ? "asked for (MGS2_ME_PIC_ME)" : mq_emu() ? "not asked for: the emulator has no ME" : "not asked for (MGS2_ME_PIC_ME=0)",
            (unsigned)kb, (unsigned)vk, mq_edram ? "in its eDRAM at 80000000 (MGS2_ME_PIC_EDRAM; the main CPU cannot reach them)" : "in main RAM");
#endif
  if (mp_want >= 3) { void mq_setup(void); mq_setup(); }   /* step 4c (4d: mode 4) */
}
/* step 4b-2: the filter's bits of lines l0 .. l1 set (v) or cleared */
static void mp_bm_range(uint32_t l0, uint32_t l1, int v)
{
  if (l1 >= XL_GSZ / 64) l1 = XL_GSZ / 64 - 1;
  for (; l0 <= l1 && (l0 & 7); l0++) { if (v) mp_bm[l0 >> 3] |= (uint8_t)(1u << (l0 & 7)); else mp_bm[l0 >> 3] &= (uint8_t)~(1u << (l0 & 7)); }
  if (l0 <= l1 && l1 - l0 + 1 >= 8) { uint32_t nb = (l1 - l0 + 1) >> 3; memset(mp_bm + (l0 >> 3), v ? 0xff : 0, nb); l0 += nb << 3; }
  for (; l0 <= l1; l0++) { if (v) mp_bm[l0 >> 3] |= (uint8_t)(1u << (l0 & 7)); else mp_bm[l0 >> 3] &= (uint8_t)~(1u << (l0 & 7)); }
}
static void mp_mark(const MpRec *e, int v)   /* a record's RAM extents into the filter or out of it */
{
  const XvExt *x = xmc.ext + e->x0;
  for (int i = 0; i < e->nx; i++) if (x[i].a0 < XV_SPR) mp_bm_range(x[i].a0 >> 6, (x[i].a1 - 1) >> 6, v);
}
static int mp_spr(const MpRec *e)   /* its copy holds scratchpad bytes (keyed from XV_SPR), or bytes of the RAM the scratchpad is (GUEST_SPR0) */
{
  const XvExt *x = xmc.ext + e->x0;
  for (int i = 0; i < e->nx; i++) if (x[i].a0 >= XV_SPR || x[i].a0 < 0x4000u) return 1;
  return 0;
}
static uint32_t mp_hash(const uint8_t *p, uint32_t n)   /* the check's: FNV-1a over the words of n bytes (16-byte aligned pieces) */
{
  const uint32_t *w = (const uint32_t *)p; uint32_t h = 2166136261u;
  for (uint32_t i = 0; i < n / 4; i++) h = (h ^ w[i]) * 16777619u;
  return h ^ n;
}
static uint32_t mp_hashq(const uint8_t *p, uint32_t a, uint32_t f, unsigned qm, int rd)   /* b4: FNV-1a over the quadwords of [a, f) (p its
                                                                                           bytes) whose bit in qm is rd */
{
  uint32_t h = 2166136261u, n = 0;
  for (uint32_t q = a; q < f; q += 16) if ((int)((qm >> ((q >> 4) & 3)) & 1) == rd) {
    const uint32_t *w = (const uint32_t *)(p + (q - a));
    h = (h ^ w[0]) * 16777619u; h = (h ^ w[1]) * 16777619u; h = (h ^ w[2]) * 16777619u; h = (h ^ w[3]) * 16777619u; n++; }
  return h ^ n;
}
static void mp_chk_plan(MpRec *e)   /* the check: the record's lines (64-byte lines cut at its extents' ends) hashed from RAM as it ends,
                                        each flagged by whether it holds bytes the transfer itself reads (the second walk, no margins) */
{
  const XvExt *x = xmc.ext + e->x0, *r = 0; int n = 0, rx0 = 0, rnx = -1, j = 0;
  if (mp_chr) {
    xmr.next = 0; xmr.used = 0; xmr.nmk = 0; xmr.mk_full = 0;
    if (xm_plan(&xmr, e->chcr, e->madr, e->qwc, e->tadr, &rx0, &rnx)) { rnx = -1; mp_chk_rdfail++; } else r = xmr.ext + rx0; }
  e->h0 = mp_chn; e->nh = -1;
  for (int i = 0; i < e->nx; i++)
    for (uint32_t a = x[i].a0; a < x[i].a1; ) {
      uint32_t f = ((a >> 6) + 1) << 6; if (f > x[i].a1) f = x[i].a1;
      if (mp_chn + n == mp_chcap) { mp_chk_skip++; return; }
      unsigned qm = 15;   /* b4: the piece's quadwords the transfer reads (all of them without the second walk) */
      if (mp_chr) {   /* (both lists sorted by address) */
        if (r) { qm = 0; while (j < rnx && r[j].a1 <= a) j++;
          for (int jj = j; jj < rnx && r[jj].a0 < f; jj++) { uint32_t s = r[jj].a0 > a ? r[jj].a0 : a, u = r[jj].a1 < f ? r[jj].a1 : f;
            for (uint32_t q = s & ~15u; q < u; q += 16) qm |= 1u << ((q >> 4) & 3); } }
        mp_chr[mp_chn + n] = (uint8_t)(qm | (qm ? (mp_hashq(M.mem + a, a, f, qm, 0) & 15) << 4 : 0)); }
#ifdef ME_WATCH
      if (mpw_on && mpw_bit(a >> 6)) mpw_arm(e, a >> 6, qm);
#endif
      mp_ch[mp_chn + n++] = qm ? mp_hashq(M.mem + a, a, f, qm, 1) : mp_hash(M.mem + a, f - a); a = f; }
  e->nh = n; mp_chn += n;
}
static void mp_chk_fill(const MpRec *e)   /* the check: the deadline copy's lines against those hashes */
{
  const XvExt *x = xmc.ext + e->x0; int n = 0, bad = 0, sc = 0;
  if (e->nh < 0) return;
  for (int i = 0; i < e->nx; i++)
    for (uint32_t a = x[i].a0; a < x[i].a1; ) {
      uint32_t f = ((a >> 6) + 1) << 6; if (f > x[i].a1) f = x[i].a1;
      unsigned c = mp_chr ? mp_chr[e->h0 + n] : 15u, qm = c & 15u; const uint8_t *cp = xmc.arena + x[i].off + (a - x[i].a0);
      int rd = 0, dm = 0;   /* b4: rd the read quadwords differ, dm the margin's (in a piece with both: by its 4-bit hash) */
      if (qm) { rd = mp_hashq(cp, a, f, qm, 1) != mp_ch[e->h0 + n]; dm = mp_chr && (mp_hashq(cp, a, f, qm, 0) & 15) != (c >> 4); }
      else dm = mp_hash(cp, f - a) != mp_ch[e->h0 + n];
      mp_chk_lrd += qm != 0;
      if (rd && dm) mp_chk_dboth++;
      if (rd || dm) {
        mp_chk_diff++; bad = 1; if (rd) mp_chk_drd++; else mp_chk_dmg++;
        { long w = me_vb / 100; if (w != mpc_w) { mpc_flush(); mpc_w = w; } if (rd) mpc_rd++; else mpc_mg++; }   /* b3 */
#ifdef ME_WATCH
        if (mpw_on && rd) mpw_diff(e, a, f, xmc.arena + x[i].off + (a - x[i].a0));
#endif
        if (rd ? mp_chk_prn < 60 : mp_chk_prnm < 12) { if (rd) mp_chk_prn++; else mp_chk_prnm++;
          fprintf(stderr, "me picture check: vb %ld: record %d (ch %d, chcr %08x, tadr %08x): line %07x+%u of extent %07x..%07x (read quadwords "
                  "%u%u%u%u: %s) differs from the bytes as its transfer ended\n", me_vb, e->t, e->ch, (unsigned)e->chcr, (unsigned)e->tadr,
                  (unsigned)a, (unsigned)(f - a), (unsigned)x[i].a0, (unsigned)x[i].a1, qm & 1, (qm >> 1) & 1, (qm >> 2) & 1, (qm >> 3) & 1,
                  rd ? (dm ? "in bytes the transfer reads and in the margin" : "in bytes the transfer reads") : "only in the margin"); } }
      if (mp_scr && mp_chr && qm != 15u) {   /* b5: the piece's margin quadwords turned over (the walk reads the copy after this) */
        uint8_t *wp = xmc.arena + x[i].off + (a - x[i].a0); int k0 = 0;
        for (uint32_t q = a; q + 16 <= f; q += 16) if (!((qm >> ((q >> 4) & 3)) & 1)) {
          uint32_t *w = (uint32_t *)(wp + (q - a)); w[0] ^= 0x5a5a5a5au; w[1] ^= 0x5a5a5a5au; w[2] ^= 0x5a5a5a5au; w[3] ^= 0x5a5a5a5au;
          mp_scr_q++; k0 = 1; }
        if (k0 && !sc) { sc = 1; mp_scr_rec++; } }
      n++; mp_chk_lines++; a = f; }
  mp_chk_rec += bad; mpc_rec += bad;
}
/* every waiting copy filled now, from RAM as it is with the log laid over it (why: 0 the deadline, 1 the log full, 2 the queue full,
   3 no room); the log, its table and the filter emptied */
static void mp_fill_all(int why)
{
  int n = 0;
  if (mq_on) { mq_fill(why); return; }   /* step 4c: the ME lays the log */
#ifdef ME_WATCH
  if (g_mpw_armed) rt_me_watch(15, (uint32_t)why, (uint32_t)mp_nrec);
#endif
  for (int i = 0; i < mp_nrec && mp_unfilled; i++) {
    MpRec *e = &mp_rec[i];
    if (e->nx < 0 || e->filled) continue;
    xm_fill(&xmc, e->x0, e->nx, e->t, mp_nolay ? 0 : mp_log, mp_nolay ? 0 : mp_nlog); e->filled = 1; n++;   /* (b5: NOLAY) */
    if (mp_chk) mp_chk_fill(e);
    mp_mark(e, 0); }
  if (why) mp_fill_early[why - 1] += n; else mp_fill_dl += n;
#ifdef ME_WATCH
  if (mpw_on) mpw_letgo();
#endif
  if (mp_nlog > mp_nlog_max) mp_nlog_max = mp_nlog;
  mp_nlog = 0; mp_unfilled = 0;
  if (!++mp_hgen) { memset(mp_h, 0, (size_t)mp_hn * sizeof *mp_h); mp_hgen = 1; }
  g_g0_on = mp_mode == 2 && g_xl_end;   /* (a record in its walk: a write there is counted) */
}
/* a guarded write of n bytes at RAM offset o, before it writes (who: 0 the runtime, 1..12 the G0 functions): each line in the filter that
   is not in the log since the last record started goes in with its 64 bytes */
static void mg_lines(uint32_t o, uint32_t n, int who)
{
  uint32_t l = o >> 6, l1 = (o + (n ? n : 1) - 1) >> 6;
  if (g_xl_end) mg_walk[who]++;
  if (l1 >= XL_GSZ / 64) l1 = XL_GSZ / 64 - 1;
  for (; l <= l1; l++) {
    uint32_t h;
    if (!((mp_bm[l >> 3] >> (l & 7)) & 1)) { mg_skip[who]++; continue; }
    h = (l * 0x9e3779b1u) >> (32 - mp_hbits);
    while (mp_h[h].gen == mp_hgen && mp_h[h].l != l) h = (h + 1) & (mp_hn - 1);
    if (mp_h[h].gen == mp_hgen && mp_h[h].t == mp_seg) { mg_dup[who]++; continue; }
    if (mp_nlog == mp_logcap) { mp_full_log++; mp_fill_all(1); return; }   /* (nothing of this write is written yet) */
#ifdef ME_WATCH
    if (mpw_on && g_mpw_armed) mpw_logged(l, who);
#endif
    { XmLogE *e = &mp_log[mp_nlog++]; e->l = l; e->t = mp_seg; memcpy(e->old, M.mem + ((size_t)l << 6), 64); }
    mp_h[h].gen = mp_hgen; mp_h[h].l = l; mp_h[h].t = mp_seg; mg_log[who]++;
  }
}
void rt_g0_pre(uint32_t a, uint32_t n, int fn)   /* boot/gen_pre.h: G0 function fn's store of n bytes at guest address a, before it writes */
{
  if (!g_g0_on) return;
  if (fn < 1 || fn > 12) fn = 0;
  mg_st[fn]++;
  if ((a >> 28) == 1) return;   /* the register space (gen_pre.h GEN_FAST): no RAM */
  mg_lines(a & (XL_GSZ - 1), n, fn);
}
void rt_me_guard(uint32_t a, uint32_t n)   /* runtime/ee_io.h DGT_PRE (only while g_g0_on): the runtime writes n bytes of game RAM at a */
{
  mg_st[0]++; mg_rtb += n;
  mg_lines(a & (XL_GSZ - 1), n, 0);
}
/* the queued records walked in order, each through its copy (or from RAM, a record with none), and the arena emptied (4b-2: the
   waiting copies filled first; why as mp_fill_all's) */
static void mp_late(int why)
{
  if (mq_on) { mq_late(why, why == 3); return; }   /* step 4c: the walks are the ME's */
  if (mp_unfilled) mp_fill_all(why);
  for (int i = 0; i < mp_nrec; i++) {
    MpRec *e = &mp_rec[i];
    if (e->restart) { sum(); gu_zero_empty(map, 0); xl_reset(); l_words = l_calls = l_ad = 0; }   /* a start again (xlive_chk_dma) */
    xm_switch(&xmp, e->opt, e->opt2);
    if (e->nx >= 0) { mp_view.x = xmc.ext + e->x0; mp_view.n = e->nx; mp_view.last = 0; M.view = &mp_view; }
    xm_tsplit(0, 0, 0);   /* gl26: the main CPU's own walk: no clock (never the ME's) */
    HOST_ENTER(HOST_XL); xm_walk(&xmp, e->ch, e->chcr, e->madr, e->qwc, e->tadr, e->lanes, e->opt); HOST_LEAVE(HOST_XL);
    M.view = 0; sum();
  }
  mp_nrec = 0; xmc.next = 0; xmc.used = 0; mp_late_runs++; mp_chn = 0;
}
/* runtime/ee_io.h XL_IMG, while g_xl_img: nb bytes of image data at host address p, the queued record's (whole 16-byte pieces kept) */
extern int g_xl_img, g_xl_end;   /* defined there (boot.c) */
void xlive_img(const void *p, uint32_t nb)
{
  const uint8_t *b = (const uint8_t *)p; uint32_t a0, a1;
  mp_img_runs++;
  if (b < M.mem || b >= M.mem + XL_GSZ) { mp_img_spr++; return; }   /* the scratchpad's: copied whole */
  a0 = (uint32_t)(b - M.mem); a1 = a0 + nb; if (a1 > XL_GSZ) a1 = XL_GSZ;
  a0 = (a0 + 15) & ~15u; a1 &= ~15u;
  if (a1 <= a0) { mp_img_small++; return; }
  if (xmc.nmk && xmc.mk[xmc.nmk - 1].k1 == a0) { xmc.mk[xmc.nmk - 1].k1 = a1; return; }
  if (xmc.nmk == XC_CAPS(&xmc)) { if (!xmc.mk_full) mp_img_full++; xmc.mk_full = 1; return; }   /* (step 4d-3: mk's room) */
  xmc.mk[xmc.nmk].k0 = a0; xmc.mk[xmc.nmk].k1 = a1; xmc.nmk++;
}
/* runtime/ee_io.h io_dma_start, after the exact walk of the queued record's transfer: its copy */
void xlive_chk_end(int ch)
{
  MpRec *e = &mp_rec[mp_nrec - 1]; int r, x0 = 0, nx = 0;
  (void)ch; g_xl_img = g_xl_end = 0;
#ifdef ME_WATCH
  if (g_mpw_armed) rt_me_watch(2, (uint32_t)ch, (uint32_t)e->t);
#endif
  if (mq_on) { mq_chk_end(); return; }   /* step 4c */
  xmc.xp = rt_xp_at(me_vb);   /* gl40 */
  r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx);
  if ((r == 2 || r == 3) && mp_nrec > 1) {   /* no room: the records before (filled and) walked now, the copy planned again */
    MpRec keep = *e; mp_drain_room++; mp_nrec--; mp_late(3); mp_rec[0] = keep; mp_nrec = 1; e = &mp_rec[0];
    r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx);
  }
  if (r) { mp_direct++; mp_late(3); return; }   /* none: walked at once (its nx -1: from RAM) */
  e->x0 = x0; e->nx = nx;
  if (mp_mode == 2 && me_vb >= mp_dl && !mp_spr(e)) {   /* 4b-2: its bytes at the deadline */
    e->filled = 0; mp_mark(e, 1); if (mp_chk) mp_chk_plan(e); mp_unfilled++; }
  else { xm_fill(&xmc, x0, nx, 0, 0, 0); e->filled = 1; mp_fill_end[mp_mode < 2 ? 2 : me_vb < mp_dl ? 0 : 1]++; }
  g_g0_on = mp_mode == 2 && mp_unfilled > 0;
}
static uint64_t mp_bytes_hook;
static void mp_hook(long vb)   /* xl_psp_frame, first: the step's records */
{
  uint32_t kb;
  if (!mp_on) return;
  mp_hooks++; kb = (uint32_t)((xmc.bytes - mp_bytes_hook) >> 10); mp_bytes_hook = xmc.bytes;
  if (kb > mp_step_max) { mp_step_max = kb; mp_step_max_vb = vb; }
  if (kb) mp_hist[kb < 512 ? 0 : kb < 1024 ? 1 : kb < 1536 ? 2 : kb < 2048 ? 3 : kb < 2560 ? 4 : kb < 3072 ? 5 : kb < 3584 ? 6 : kb < 4096 ? 7 : 8]++;
  if (mp_nrec) mp_late(0);
}
static void mp_end(void) { if (mq_on) { mq_end(); return; } if (mp_on && mp_nrec) mp_late(0); }
/* ---- task #180 step 4c (me_step4_notes.md 4c): MGS2_ME_PIC=3, the ME's loop.  4b-2's records, plans, filter, guards and log stay the
   main CPU's; the copies' bytes, the log's laying, the walks, the thinning's ranges, each frame's end, its GE list and each frame's start
   go to the ME's loop (ge/xme.c) through a queue (ge/xme.h).  PPSSPP has no ME, so the loop runs as a thread of the main CPU ("me_pic",
   the main thread's FPU rule): MGS2_ME_PIC_PRI=0 (the default) below the main thread's priority, so that it runs only while the main CPU
   waits (the early copies when the main CPU next waits, the walks by the next hook or the next plan: the latest the x86 check allows,
   me_handoff_design.md 4.6), 1 above it, so that each entry is done as it is posted (4b-2's order, but the GE's waits).  The main CPU
   keeps the GE and the display: it gives the GE each list the ME made, one at a time, and tells the ME (ge_done) when the GE has
   finished one; the ME starts a frame only then.  MGS2_ME_PIC_TURN=N: in every other N vblanks (vb / N odd) the main CPU runs the
   loop's entries itself, the queue emptied at each change.  The take-back: when the ME's heartbeat has not moved for MGS2_ME_PIC_TBK_MS
   (default 1000 ms; the loop's heartbeat also moves inside long walks: me_rt.h ME_BEAT) while the main CPU waits for the loop, the
   thread is ended, the picture made cold (the translator and the map's
   knowledge as at a start again, a new texture cache, the frame dropped, the waiting records and lists dropped) and the main CPU runs
   the entries itself from then on.  MGS2_ME_PIC_HANG=vb (the test): the thread's loop stops at the first frame hook at or after vb with
   its heartbeat stopped.  The check (MGS2_ME_PIC_CHK) and b5's diagnostics are 4b-2's only. ---- */
extern volatile uint32_t *me_beat_p;   /* ge/me_rt.c (me_rt.h ME_BEAT: declared there for the ME-safe units only) */
static XmeCtx *mq;
static int mq_self = 1, mq_hi, mq_turn, mq_turn_k = -1, mq_c4, mq_ft_on, mq_stop_due, mq_stop_sent;
static SceUID mq_th = -1, mq_sem = -1;
#define MQ_THR_STK (16u << 10)   /* gl41: the me_pic thread's stack (below, at its creation) */
static uint32_t mq_walk_seq, mq_thin_seq; static int mq_walk_due, mq_thin_due;   /* the last WALK's and THIN's places in the queue */
static uint32_t mq_cons, mq_subm; static uint64_t mq_ge_t0;   /* results taken from the ME, lists given to the GE; the last one's time */
/* gl22: the GE's finish seen at once.  sceGu's finish callback (the GE's interrupt) stores the time and counts; with MGS2_ME_PIC_GEFAST on
   it also sets ge_done, so the ME may begin its next frame without waiting for the main CPU to notice (the main CPU still shows the frame
   and gives the GE the next list in mq_service).  Timestamps per list: submit -> finish (the GE's drawing; the list is whole when
   submitted) and finish -> the main CPU's notice; by maker: 0 ME turns fast off, 1 ME turns fast on, 2 main CPU turns */
static volatile uint32_t ge_fin_t;   /* (ge_fin_n: above, native step 1) */ static uint32_t ge_sub_t, ge_fin_n0; static int ge_fast_cfg, ge_fast;
static long ge_n[3], ge_nofin; static uint64_t ge_draw[3], ge_note[3]; static uint32_t ge_draw_max[3];
static long ge_tn0; static uint64_t ge_td0, ge_tq0;   /* the ME turn's start (mq_tstart) */
static int ge2_cfg, ge2; static long ge2_tn0;   /* gl23: MGS2_ME_PIC_GE2 (0 off, 1 on, 2 every other ME turn); this turn's; n_ge2 at its start */
static void ge_fin_cb(int id)
{
  (void)id; ge_fin_t = sceKernelGetSystemTimeLow(); ge_fin_n++;
  if (ge_fast && mq && gl_ge && !mq_self) XS(mq)->ge_done = mq_subm;
}
static void ge_noted(void)   /* the main CPU has seen the GE's list finished */
{
  uint32_t now = sceKernelGetSystemTimeLow(); int b = mq_self ? 2 : ge2 ? 1 : 0;   /* gl23: by GE2 (gl22: by GEFAST) */
  if (ge_fin_n != ge_fin_n0) { uint32_t d = ge_fin_t - ge_sub_t; ge_n[b]++; ge_draw[b] += d; ge_note[b] += now - ge_fin_t;
    if (d > ge_draw_max[b]) ge_draw_max[b] = d; }
  else ge_nofin++;
}
static long mq_tbk_n, mq_tbk_cold, mq_tbk_warm, mq_tbk_vb = -1, mq_tbk_rec, mq_tbk_left; static uint32_t mq_tbk_at; static int mq_tbk_why = -1, mq_tbk_hung; static uint32_t mq_tbk_us = 1000000;
static long mq_posted, mq_signals, mq_selfrun, mq_turns, mq_dropped;
enum { MW_NOW, MW_FILL, MW_ARENA, MW_RAM, MW_RING, MW_THIN, MW_DRAIN, MW_PROBE, MW_SYNC, MW_HOOK, MW_N };
static const char *const mq_wname[MW_N] = { "a copy as the transfer ended", "a fill", "the arena (a walk)", "a walk from RAM", "room in the queue",
                                            "the thinning's slot", "the queue emptied (turns, shots, the hang test, the end)",
                                            "a heartbeat (a hook: entries waiting, none since the hook before)", "a hand-over to the main CPU (SYNC)",
                                            "the walk before at a hook (gl24: two copy areas)" };
/* ---- step 4d (me_step4_notes.md 4d), MGS2_ME_PIC=4: the real ME's way; on the emulator (no ME) the loop is 4c's thread.  The main CPU
   cannot reach the ME's memory (the eDRAM: the arena, in 4d-4), so in its own turns and after a take-back it makes the picture with no
   copy: each transfer walked from RAM as it ends (no plan, no image marks, no guards: nothing writes the game's RAM between xlive_chk_dma
   and xlive_chk_end).  The turns change after a hook's FILL, WALK and HOOK (no record waits then): to the main CPU through SYNC (the ME
   writes back and drops its whole data cache, then the main CPU drops its own), back to the ME with no entry (the main CPU writes back
   its cache before every post).  The ME's cache steps go through mq->cache (here a counting stand-in; the ME's own in 4d-4).  A take-back
   is warm only at the hang test's stop (between entries, the ME's cache written back, no record, fill or walk waiting: the main CPU runs
   the entries left and makes the picture itself from then on); any other is cold, and in mode 4 cold stops the picture for the rest of
   the run (on the PSP the ME's unwritten cache lines are lost with it, so what it was writing may be torn), the GE stop's way: the text
   on the screen, the run goes on for the hashes.  MGS2_ME_PIC_HANG=vb: the main CPU empties the queue right after the HOOK of that
   vblank (the ME's stop found before any record is posted after it); MGS2_ME_PIC_HANG_IN=1: the loop stops inside the next walk instead
   (the cold path's test). ---- */
static int mq_m4, mq_dead; static long mq_dead_vb = -1;
/* step 4d-4 (me_step4_notes.md 4d): the loop on the PSP's Media Engine (-DMGS2_ME_REAL, MGS2_ME_PIC_ME=1, the default; never on the
   emulator, where hle_me_start refuses).  The ME runs psp/xme_me.c's entry with a 16 KB region of main RAM as its stack (mecheck's
   deepest chain 5,120 bytes) and a 128-byte control block both cores reach uncached; the arena and the view's four buffers go in its
   eDRAM (0x80000000, which the main CPU cannot reach) when they fit its 4 MB (MGS2_ME_PIC_EDRAM=1, the default).  If the ME does not
   start with the arena there, the main CPU makes the picture itself for the whole run (each transfer walked from RAM as it ends); with
   the arena in main RAM, 4c's thread runs the loop.  A take-back holds the ME in reset first (hle_me_park: its unwritten cache lines
   are lost), so it is cold unless the ME stopped at the hang test with its cache written back; an exception of the ME's (its handler
   records Cause, EPC and BadVAddr and halts) is a take-back at once, found at each round of the main CPU's waits. */
static int mq_me;                      /* 1 the loop on the ME, -1 the ME held after a take-back or the run's end, 0 not started */
static void mq_noed(void)              /* the split off with the arena in the eDRAM: nothing points there any more */
{
#ifdef MGS2_ME_REAL
  if (mq_edram) { xmc.arena = 0; for (int i = 0; i < 4; i++) mp_view.fix[i] = 0; mp_view.fix_n = 0; mp_view.arena = 0; }
  mq_me_try = mq_edram = 0;
#endif
}
#define MQ_LOOP() (mq_th >= 0 || mq_me > 0)   /* the loop runs apart from the main CPU (4c's thread or the ME) */
#ifdef MGS2_ME_REAL
#define MQ_EXC() mq_me_tbk_exc
#else
#define MQ_EXC() 0
#endif
#ifdef MGS2_ME_REAL
#define MQ_ME_REG (16u << 10)
static int mq_me_ack = -1, mq_me_exc, mq_me_tbk_exc;   /* hle_me_stop's answer; an exception seen; a take-back for it */
static XmeMeCtl *mq_mc; static volatile XmeMeCtl *mq_mcu; static uint8_t *mq_mreg;   /* the control block (cached, uncached), the region */
static uint32_t mq_mx[3]; static long mq_mx_vb = -1;   /* the ME's exception: Cause, EPC, BadVAddr; the vblank it was seen */
static char mq_me_why[200];
static int mq_me_x(void)   /* the ME's exception record (hle_me_exception), once: 1 when it took one */
{
  uint32_t c, e, b;
  if (mq_me <= 0 || mq_me_exc) return mq_me_exc;
  if (hle_me_exception(&c, &e, &b)) { mq_me_exc = 1; mq_mx[0] = c; mq_mx[1] = e; mq_mx[2] = b; mq_mx_vb = me_vb; }
  return mq_me_exc;
}
#endif
static long mq_own, mq_syncs, mq_c_wb, mq_c_wbinv;   /* records walked from RAM by the main CPU itself; hand-overs; its cache steps */
static int mq_ext_max = -1; static long mq_ext_max_vb = -1; static uint32_t mq_arena_max; static long mq_arena_max_vb = -1;   /* a walk's extents
                                                                                       and arena bytes at most (4d-3 sizes them) */
static volatile long mq_mc_all, mq_mc_inv; static volatile uint64_t mq_mc_inv_b;   /* the stand-in's counts of the ME's cache steps */
static void mq_cache_thr(XmeCtx *x, int op, const void *p, uint32_t n)   /* the thread's stand-in for the ME's cache steps (one memory here) */
{
  (void)x; (void)p;
  if (op == XME_C_ALL) mq_mc_all++; else { mq_mc_inv++; mq_mc_inv_b += n; }
}
static void mq_cpu_wb(void)
{
  int tp = p9_top(); mq_c_wb++; p9_wbn[tp]++;   /* gl39: by the part the main CPU is in; timed with MGS2_PROBE9 */
  if (p9_on > 0) { uint32_t t0 = sceKernelGetSystemTimeLow(); sceKernelDcacheWritebackAll(); p9_wbus[tp] += sceKernelGetSystemTimeLow() - t0; }
  else sceKernelDcacheWritebackAll();
}                /* what the ME reads, in RAM before it reads it */
static void mq_cpu_wbinv(void) { mq_c_wbinv++; sceKernelDcacheWritebackInvalidateAll(); }   /* before the main CPU reads what the ME wrote */
/* step 4d-2: the ME's heap request (ge/xme.c xme_rq), served at the main CPU's waits, transfers and hooks (mq_service) by the host's
   own hooks; a region given or moved is written back and dropped from the main CPU's cache before the ME has it (no old line of the
   main CPU's can land on what the ME writes there) */
static long mq_rq_n[4], mq_rq_fail;
static void mq_heap_serve(void)
{
  uint32_t s = XS(mq)->rq_seq; int op; void *r = 0; size_t got = 0;
  if (s == XS(mq)->rq_done) return;
  XME_BARRIER(); op = XS(mq)->rq_op;
  if (op == XME_RQ_MORE) { r = me_more(XS(mq)->rq_n, &got); if (r) sceKernelDcacheWritebackInvalidateRange(r, got); }
  else if (op == XME_RQ_LESS) me_less(XS(mq)->rq_p);
  else if (op == XME_RQ_RESIZE) { r = me_resize(XS(mq)->rq_p, XS(mq)->rq_n); if (r) sceKernelDcacheWritebackInvalidateRange(r, (XS(mq)->rq_n + 63) & ~63u); }
  if (op != XME_RQ_LESS && !r) mq_rq_fail++;
  mq_rq_n[op >= 1 && op <= 3 ? op : 0]++;
  XS(mq)->rq_r = r; XS(mq)->rq_got = (uint32_t)got;
  XME_BARRIER(); XS(mq)->rq_done = s;
}
static void mq_heap_own(void) { me_heap_grow(me_more, me_less); me_heap_resize(me_rs_on ? me_resize : 0); }   /* the main CPU's hooks */
static long mq_wn[MW_N]; static uint64_t mq_wus[MW_N];
static void mq_takeback(int why);
static uint64_t mq_now(void) { return (uint64_t)sceKernelGetSystemTimeWide(); }
static int mq_done(uint32_t seq) { return (int32_t)(XS(mq)->tail - seq) > 0; }
#ifdef MGS2_ME_REAL
/* gl28 (MGS2_ME_REST=1; power_design.md section 5 "R1" with power_critique.md section B; v30me5's psp/me_mix.c me_kick_check and
   wait_rd's rekick): the ME rests in HALT (psp/xme_rest.S; psp/xme_me.c xme_rest_once) through the main CPU's turns, never in its own.
   A main turn begins: the rest bit set (rs_allow, after the hand-over).  An ME turn begins (rs_end, first in the switch): the rest bit
   cleared, sync, asleep read; a rest not kicked yet gets one kick (hle_me_kick: 0xBC100044 = 1 in kernel mode, about 137 us of the main
   CPU's in v30me5), then the main CPU waits (at most 2 ms, a rekick every 200 us) until the ME's idle wait that held the rest is
   accounted (XmeSh t_idle_q), so the ME turn's own times on its Count never hold the main turn's rest; then the turn's line.  After each
   post, and while the main CPU waits for the ME (mq_wait, mq_probe: 200 us without a heartbeat and asleep), the same check (expected
   never: rekicks 0).  The 1-s take-back stays the safety net.  Times that span a rest are the main CPU's clock; the ME's Count ticks in
   its rests are shown against them (Count may stop in HALT): Count's test is the turns with one rest woken in HALT (any other turn's
   span from the rest bit to the kick holds awake time too).  Stray wakes (the handler's wakes beyond every kick) are a low bound: a
   kick to an awake ME stays pending and wakes the next rest at once (kicks while one is pending make one wake), so the per-turn line
   shows the handler's wakes beside the kicks.  A rest given up just as the rest bit is cleared (asleep seen, then the ME saw the bit
   off) gets the switch's kick while awake: that turn shows "given up 1", the next "rests 2, before HALT 1" (benign).  When the switch
   gives up its wait (2 ms) or finds the ME awake after a rest, that rest's Count ticks fall in the next ME turn's idle: a line after
   that "me turn" line says so.  MGS2_ME_REST=0 (the default): the rest bit is never set, nothing here runs, the ME never sets the rest
   up (gl27). */
static int rs_cfg;                          /* MGS2_ME_REST: 1 on */
static int rs_on;                           /* the rest bit is set (a main CPU turn) */
static uint32_t rs_kicked = 0xFFFFFFFFu;    /* the ME's sleeps at the last kick */
static uint64_t rs_kicks, rs_rekicks, rs_kick_us, rs_kick_max, rs_kick_tmax, rs_lk;   /* kicks (a switch, a post), kicks in waits, their
                                                                                         main-clock us, the longest (run, turn), the last's end */
static uint64_t rs_t_ok, rs_asleep_us, rs_ticks, rs_sw_us; static long rs_vb0 = -1, rs_turns, rs_slow, rs_awake_sw;
static long rs_cl_n, rs_xs_n; static uint64_t rs_cl_us, rs_cl_ticks, rs_xs;   /* turns with one rest woken in HALT (Count's test): their
                                                                                  main us and ticks; turns with more handler wakes than
                                                                                  kicks, the wakes beyond */
static int rs_tnote;                        /* the ME turn now begun may count the main turn's last rest in its idle: 1 the ME awake at
                                               the switch after a rest, 2 the switch's wait given up (mq_tend's note) */
typedef struct { uint32_t sl, ha, pe, nw, la, tr, w; uint64_t k, rk, kus; } RsSnap;
static RsSnap rs_s0, rs_s1;                 /* at the main turn's start (rest bit set); at its end (the ME turn's start) */
static uint64_t rs_me_sl, rs_me_k; static uint32_t rs_me_sl_t; static uint64_t rs_me_k_t;   /* in the ME turns: rests begun, kicks (all;
                                                                                               the turn before this main turn) */
static void rs_snap(RsSnap *s)
{
  volatile XmeMeCtl *c = mq_mcu;
  s->sl = c->sleeps; s->ha = c->halted; s->pe = c->pending; s->nw = c->nowake; s->la = c->late; s->tr = c->t_rest; s->w = c->wakes;
  s->k = rs_kicks; s->rk = rs_rekicks; s->kus = rs_kick_us;
}
static uint32_t rs_stray(void)              /* the handler's wakes over every kick since the start (critique B: a wake by anyone else; a
                                               low bound: kicks to an awake ME, coalesced or never answered, hide later strays) */
{
  uint64_t w = mq_mcu->wakes, k = rs_kicks + rs_rekicks;
  return w > k ? (uint32_t)(w - k) : 0u;
}
static void rs_kick(int re)                 /* one interrupt to the ME, timed on the main clock */
{
  uint64_t t0 = mq_now(), d;
  hle_me_kick();
  rs_lk = mq_now(); d = rs_lk - t0; rs_kick_us += d;
  if (d > rs_kick_max) rs_kick_max = d;
  if (d > rs_kick_tmax) rs_kick_tmax = d;
  if (re) rs_rekicks++; else rs_kicks++;
}
static int rs_kick_check(void)              /* v30me5's me_kick_check: the ME asleep in a rest not kicked yet gets one kick; 1: asleep */
{
  uint32_t s;
  if (!mq_mcu->asleep) return 0;
  s = mq_mcu->sleeps;                       /* (the ME writes sleeps before asleep, a sync between) */
  if (s != rs_kicked) { rs_kicked = s; rs_kick(0); }
  return 1;
}
static void rs_rekick(uint64_t t, uint64_t prog)   /* in a wait for the ME: asleep, no progress for 200 us and no kick for 200 us */
{
  if (mq_mcu->asleep && t - prog > 200u && t - rs_lk > 200u) rs_kick(1);
}
static void rs_allow(long vb)               /* a main CPU turn begins (the hand-over done): the ME may rest */
{
  if (!rs_cfg || mq_me <= 0 || rs_on) return;
  rs_me_sl_t = 0; rs_me_k_t = 0;
  if (rs_vb0 >= 0) { rs_me_sl_t = mq_mcu->sleeps - rs_s1.sl; rs_me_k_t = (rs_kicks + rs_rekicks) - (rs_s1.k + rs_s1.rk);
    rs_me_sl += rs_me_sl_t; rs_me_k += rs_me_k_t; }
  rs_snap(&rs_s0); rs_kick_tmax = 0; rs_vb0 = vb;
  XME_BARRIER(); mq_mcu->rest = XME_REST_CFG | XME_REST_OK; XME_BARRIER();
  rs_t_ok = mq_now(); rs_on = 1;
}
static void rs_end(long vb)                 /* an ME turn begins (or the run ends in a main turn): no more rests; the ME woken; the line */
{
  uint64_t t, tw, a = 0, k, rk; uint32_t q0, qe, sl, tr, dw; int asl, ok = 1, cl; double r;
  if (!rs_on) return;
  rs_on = 0;
  if (mq_me <= 0) return;   /* (held after a take-back: nothing to wake) */
  mq_mcu->rest = XME_REST_CFG; XME_BARRIER();   /* the rest bit off, in memory before asleep is read (xme_me.c's handshake) */
  q0 = XS(mq)->t_idle_q; XME_BARRIER();
  asl = rs_kick_check(); t = mq_now();
  if (!asl) rs_awake_sw++;
  else {   /* the ME's idle wait that held the rest ends and is accounted (t_idle_q even and two past q0's even value) */
    qe = (q0 + 1u) & ~1u;
    for (;;) { uint32_t q = XS(mq)->t_idle_q; uint64_t n;
      if (!(q & 1u) && (int32_t)(q - (qe + 2u)) >= 0) break;
      n = mq_now();
      if (n - t > 2000u) { ok = 0; rs_slow++; break; }
      rs_rekick(n, t); } }
  tw = mq_now() - t; rs_sw_us += tw;
  rs_snap(&rs_s1);
  sl = rs_s1.sl - rs_s0.sl; tr = rs_s1.tr - rs_s0.tr; k = rs_s1.k - rs_s0.k; rk = rs_s1.rk - rs_s0.rk; dw = rs_s1.w - rs_s0.w;
  if (sl) a = t - rs_t_ok;   /* the ME asleep from (within 5 us of) the rest bit to the kick, on the main clock */
  rs_asleep_us += a; rs_ticks += tr; rs_turns++;
  cl = sl == 1 && rs_s1.ha - rs_s0.ha == 1;   /* one rest, woken in HALT: the span is that rest (Count's test) */
  if (cl) { rs_cl_n++; rs_cl_us += a; rs_cl_ticks += tr; }
  if (dw > k + rk) { rs_xs_n++; rs_xs += dw - (k + rk); }
  rs_tnote = !ok ? 2 : !asl && sl ? 1 : 0;
  r = a ? (double)tr / (331.31 * (double)a) : 0.0;
  fprintf(stderr, "main turn vb %ld-%ld gl28 me rest: rests %u (woken in HALT %u, before HALT %u, no interrupt %u, given up %u), kicks %llu + "
          "rekicks %llu (%.1f us mean, %.1f max), handler wakes %u, stray wakes so far at least %u; ME asleep %.1f ms (main clock: rest allowed "
          "to kick%s), Count ticks in rests %u = %.4f of 331.31 x main us%s%s; flags seen %08x; Sony words %08x %08x %08x %08x %08x; switch "
          "wait %.0f us%s; the ME turn before: rests begun %u, kicks %llu\n", rs_vb0, vb, (unsigned)sl, (unsigned)(rs_s1.ha - rs_s0.ha),
          (unsigned)(rs_s1.pe - rs_s0.pe), (unsigned)(rs_s1.nw - rs_s0.nw), (unsigned)(rs_s1.la - rs_s0.la), (unsigned long long)k,
          (unsigned long long)rk, k + rk ? (double)(rs_s1.kus - rs_s0.kus) / (double)(k + rk) : 0.0, (double)rs_kick_tmax, (unsigned)dw,
          (unsigned)rs_stray(), (double)a / 1000.0, asl ? "" : "; the ME was awake at the switch", (unsigned)tr, r,
          cl ? " (one rest woken in HALT: Count's test)" : sl ? " (not one rest woken in HALT: the span holds awake time, not Count's test)" : "",
          a > 12963000u ? " (over 12.96 s: the tick sum may have wrapped)" : "", (unsigned)mq_mcu->flags_or, (unsigned)mq_mcu->rpc[0],
          (unsigned)mq_mcu->rpc[1], (unsigned)mq_mcu->rpc[2], (unsigned)mq_mcu->rpc[3], (unsigned)mq_mcu->rpc[4], (double)tw,
          ok ? "" : " (GIVEN UP after 2 ms: the ME's idle wait not yet accounted)", (unsigned)rs_me_sl_t, (unsigned long long)rs_me_k_t);
}
#endif
/* the main CPU waits until the loop has done the entry at seq, serving the GE meanwhile; a heartbeat that does not move for
   MGS2_ME_PIC_TBK_MS: the take-back */
static long mq_w_rounds, mq_w_r0, mq_dly_bad; static int mq_dly_e; static uint64_t mq_w_t0, mq_w_hb_t; static uint32_t mq_w_words0;   /* a take-back's facts */
static uint64_t mq_gap_max; static long mq_gap_vb = -1; static uint32_t mq_gap_at;   /* the longest stretch without a heartbeat seen while waiting */
/* gl20: the loop's own time (ge/xme.h XmeCtx.t_k, on mq->clk) and the main CPU's waits, one log line per ME turn; and the overlap: the
   main CPU's time from a hook to its first copy plan of the step after (mq_chk_end waits there for the ME's walk of the step before) */
static uint32_t mq_clk_us(void) { return sceKernelGetSystemTimeLow(); }   /* the loop's clock on the main CPU (its own turns, the thread) */
static uint64_t mq_idle(void);
static uint64_t mq_tk0[XME_T_N], mq_tw0[MW_N], mq_tt0, mq_tkall[XME_T_N], mq_twall[MW_N], mq_ttall;
static uint64_t mq_ts0[XM_TS_N], mq_tsall[XM_TS_N]; static long mq_tsn0[XM_TS_N], mq_tsnall[XM_TS_N];   /* gl26: the walk's split */
static uint32_t mq_tdiv = 1000u;   /* the clock's ticks a ms in the turn (the ME's Count 331,310 [C, probe 2]; the system's us 1,000) */
static long mq_tvb0 = -1, mq_tn, mq_th0, mq_tl0, mq_tg0, mq_tov0, mq_tovn0;
static uint64_t mq_ov_us, mq_ov_t, mq_tovu0; static long mq_ov_n; static uint32_t mq_ov_f;
/* gl24: two copy areas (MGS2_ME_PIC_TWO: 0 off, 1 in every ME turn, 2 in every other ME turn: off, on, ...).  The arena and the extents
   in two halves: a step's copies go to one half while the ME walks the step before from the other, so the step's first copy plan no
   longer waits for that walk; the main CPU waits for it at the next hook instead (MW_HOOK), so a walk reads RAM at the latest as the next
   hook comes (the late-read check gl24d: what the walks read from RAM outside their copies did not change by then, the boot's refill and
   one opening step apart).  A step whose copies do not fit in its half takes the other half once the walk there is done (a spill); the
   step after then waits for its walk at its first plan, as gl23.  Records keep their extents' places (x0), so the ME needs nothing new */
static int two_cfg, two, two_h, two_pend[2], two_spill, two_xh, two_xn, two_next[2]; static uint32_t two_ah, two_an, two_used[2];
static long two_steps, two_spills, two_nohalf, two_full_n, two_t0[3];
static int two_wx; static uint32_t two_wb;   /* gl24b: the extents and arena bytes planned since the last walk posted (the walk peaks) */
static uint32_t two_seq[2]; static long two_nowait, two_older, two_move, two_t1[3];   /* gl25: the walk that holds each half; counts */
static long mq_walk_hk = -1;   /* gl25b: mp_hooks at the hook that posted the last hook walk */
static void two_load(int h) { two_h = h; xmc.next = two_next[h]; xmc.used = two_used[h]; xmc.ext_cap = (h + 1) * two_xh; xmc.arena_n = (uint32_t)(h + 1) * two_ah; }
static void two_keep(void) { two_next[two_h] = xmc.next; two_used[two_h] = xmc.used; }
static void two_empty(int h) { two_next[h] = h * two_xh; two_used[h] = (uint32_t)h * two_ah; two_pend[h] = 0; }
static void two_start(int on)   /* every walk done (an ME turn's start, a hand-over, a take-back): two areas, or gl23's one */
{
  two = on && two_ah; two_spill = 0; two_wx = 0; two_wb = 0;
  if (two) { two_empty(0); two_empty(1); two_load(0); }
  else { two_pend[0] = two_pend[1] = 0; xmc.next = 0; xmc.used = 0; if (two_an) { xmc.ext_cap = two_xn; xmc.arena_n = two_an; } }
}
static int two_on(long vb) { return mq_turn && (two_cfg == 1 || (two_cfg == 2 && ((vb / (2 * mq_turn)) & 1))); }
static void two_done(void)   /* every walk posted is done: the halves they held empty (the step's own copies stay) */
{
  for (int h = 0; h < 2; h++) if (two_pend[h]) { two_empty(h); if (h == two_h) two_load(h); }
}
static void two_poll(void);   /* (gl25c: two_post) */
static void two_post(void)   /* a walk posted with the records so far: their halves its until it is done; the next copies in the other
                                 half when that one is empty, else (a spill before) in this one after the wait */
{
  two_poll();   /* gl25c: halves whose walk a FILL or copy wait already passed */
  if (two_wx > mq_ext_max) { mq_ext_max = two_wx; mq_ext_max_vb = me_vb; }   /* gl24b: this walk's extents and bytes */
  if (two_wb > mq_arena_max) { mq_arena_max = two_wb; mq_arena_max_vb = me_vb; }
  two_wx = 0; two_wb = 0;
  two_keep(); two_pend[two_h] = 1; if (two_spill) two_pend[two_h ^ 1] = 1;
  two_seq[two_h] = mq_walk_seq; if (two_spill) two_seq[two_h ^ 1] = mq_walk_seq;   /* gl25 */
  two_spill = 0; two_steps++;
  if (!two_pend[two_h ^ 1]) two_load(two_h ^ 1); else two_nohalf++;
}
static void mq_wait(uint32_t seq, int why);   /* (gl25: two_wait) */
static void two_poll(void)   /* gl25: the halves whose walk is done emptied, without a wait; the current half held and the other free: the
                                step moves there (a held half holds no copy of this step: its copies wait until the half is free) */
{
  uint32_t t;
  if (!two) return;
  t = XS(mq)->tail;   /* gl25c: one read for the halves and walk_due */
  for (int h = 0; h < 2; h++) if (two_pend[h] && (int32_t)(t - two_seq[h]) > 0) { two_empty(h); if (h == two_h) two_load(h); }
  if (two_pend[two_h] && !two_pend[two_h ^ 1]) { two_load(two_h ^ 1); two_move++; }
  if (mq_walk_due && (int32_t)(t - mq_walk_seq) > 0) mq_walk_due = 0;
}
static void two_wait(int h, int why)   /* gl25: only the walk that holds half h (with two walks out, the older one for the older half) */
{
  if (two_seq[h] != mq_walk_seq) two_older++;
  mq_wait(two_seq[h], why);
  two_poll();
}
static void flip_line(const char *who, long vb0, long vb, int b)   /* gl27: a turn's flip counts */
{
  if (flip_w == 1 + b) { uint32_t n = sceKernelGetSystemTimeLow(); flip_us[b] += n - flip_t0; flip_t0 = n; }   /* gl27b: a hold still on:
                                                                                                               its time so far */
  if (fbc_from < 0 || vb <= fbc_from)   /* gl28d: the suffix only for turns that end after MGS2_GL_FBCHK's vb */
  fprintf(stderr, "%s vb %ld-%ld gl27 display: flip wait %s; lists held for the screen %ld (%.1f ms); lists drawn into the shown buffer %ld\n",
          who, vb0, vb, flip_on ? "on" : "off", flip_hold[b] - flip_h0[b], (double)(flip_us[b] - flip_u0[b]) / 1000.0, flip_race[b] - flip_r0[b]);
  else   /* gl28d: MGS2_GL_FBCHK's counts of the turn */
  fprintf(stderr, "%s vb %ld-%ld gl27 display: flip wait %s; lists held for the screen %ld (%.1f ms); lists drawn into the shown buffer %ld; "
          "gl28 fbchk: lists %ld, into the buffer the display shows now (IMMEDIATE) %ld, into the buffer it shows next (NEXTFRAME) %ld, in "
          "the display vblank of the show %ld, display vblanks since the show at most %u\n", who, vb0, vb, flip_on ? "on" : "off",
          flip_hold[b] - flip_h0[b], (double)(flip_us[b] - flip_u0[b]) / 1000.0, flip_race[b] - flip_r0[b], fbc_n[b] - fbc_n0[b],
          fbc_imm[b] - fbc_i0[b], fbc_next[b] - fbc_x0[b], fbc_same[b] - fbc_s0[b], (unsigned)fbc_dvc_t[b]);
}
static void flip_t0set(int b) { flip_h0[b] = flip_hold[b]; flip_r0[b] = flip_race[b]; flip_u0[b] = flip_us[b];
  fbc_n0[b] = fbc_n[b]; fbc_i0[b] = fbc_imm[b]; fbc_x0[b] = fbc_next[b]; fbc_s0[b] = fbc_same[b]; fbc_dvc_t[b] = 0; }   /* gl28d */
/* gl38 (ge/bound.h): a turn's bound line, with the bound cycle on: the kind of the frames that began in it (hooks vb0 .. vb - 1), F1's
   switch for them, and what was left out (the counts are the turn's: transfers by chain as the main CPU left them out, calls as the
   walks did: ge/gelive.h GL_BD_*) */
static void bd_tline(const char *who, long vb0, long vb)
{
  static const uint16_t mask[BD_KINDS] = BD_MASKS; long tix; int bd, kind = 0;
  if (!g_bd_cfg && !g_nat_f1cfg && !g_bd_rain) return;
  bd = rt_bound_at(vb0, &tix); for (int i = 0; i < BD_KINDS; i++) if (mask[i] == bd) kind = i;
  fprintf(stderr, "%s vb %ld-%ld gl38 bound: turn %ld kind %d (mask %d), F1 %d; frames begun with a mask %ld; transfers left out by chain:", who, vb0, vb, tix, kind, bd,
          rt_nat_f1_at(vb0), bd_fr - bd_fr0);
  for (int i = 0; i <= NS_CH; i++) if (bd_tr[i] != bd_tr0[i]) fprintf(stderr, " %d:%ld", i == NS_CH ? -1 : i, bd_tr[i] - bd_tr0[i]);
  fprintf(stderr, "; native groups recorded and not drawn %ld; rain owners' calls %ld (sheets %ld), left out %ld, near sheet calls kept %ld; chain 6's other "
          "calls left out %ld, its GS primitives %ld; rain owners' blocks over the table %ld, not right %ld\n", bd_grp - bd_grp0,
          gl_bd_n[GL_BD_RAIN] - bd_n0[GL_BD_RAIN], gl_bd_n[GL_BD_SHEET] - bd_n0[GL_BD_SHEET], gl_bd_n[GL_BD_RAIN_OUT] - bd_n0[GL_BD_RAIN_OUT],
          gl_bd_n[GL_BD_NEAR_KEPT] - bd_n0[GL_BD_NEAR_KEPT], gl_bd_n[GL_BD_SORT_OUT] - bd_n0[GL_BD_SORT_OUT], gl_bd_n[GL_BD_PRIM_OUT] - bd_n0[GL_BD_PRIM_OUT],
          gl_ft.b_over - bd_ov0, gl_ft.b_bad - bd_bad0);
  memcpy(bd_tr0, bd_tr, sizeof bd_tr0); memcpy(bd_n0, gl_bd_n, sizeof bd_n0); bd_fr0 = bd_fr; bd_grp0 = bd_grp; bd_ov0 = gl_ft.b_over; bd_bad0 = gl_ft.b_bad;
  {   /* gl39: the probe line (the counts since the line before; the ME's load counts are read after its hand-over) */
    static const char *const nm[P9_N] = { "logic", "iop", "gs", "pipeline", "translator", "picture", "ge-wait", "other" };
    long ln = mq ? mq->n_load : 0, lit[4]; uint64_t lt[4]; const int me = who[1] == 'e';
    const double dv = mq && me ? (double)mq_tdiv : 1000.0;
    for (int i = 0; i < 4; i++) { lit[i] = mq ? mq->n_load_it[i] : 0; lt[i] = mq ? mq->t_load[i] : 0; }
    fprintf(stderr, "%s vb %ld-%ld gl39 probe: turn %ld kind %d digest %s load %d; service calls by part (calls/timed/us):", who, vb0, vb, tix, kind,
            rt_dig_off_at(vb0) ? "off" : "on", rt_load_at(vb0));
    for (int i = 0; i < P9_N; i++) if (p9_sv[i] != p9_0.sv[i]) fprintf(stderr, " %s %ld/%ld/%llu", nm[i], p9_sv[i] - p9_0.sv[i], p9_svt[i] - p9_0.svt[i],
                                                                    (unsigned long long)(p9_svus[i] - p9_0.svus[i]));
    fprintf(stderr, "; of them from transfers:");
    for (int i = 0; i < P9_N; i++) if (p9_dma[i] != p9_0.dma[i]) fprintf(stderr, " %s %ld", nm[i], p9_dma[i] - p9_0.dma[i]);
    fprintf(stderr, "; GE polls %ld, found done %ld, lists to the GE %ld; posts %ld; whole-cache write-backs by part (n/us):", p9_poll - p9_0.poll,
            p9_done - p9_0.done, p9_subm - p9_0.subm, p9_post - p9_0.post);
    for (int i = 0; i < P9_N; i++) if (p9_wbn[i] != p9_0.wbn[i]) fprintf(stderr, " %s %ld/%llu", nm[i], p9_wbn[i] - p9_0.wbn[i], (unsigned long long)(p9_wbus[i] - p9_0.wbus[i]));
    fprintf(stderr, "; ME load: posted %ld, not posted (the loop behind) %ld, run %ld, blocks and ms by flavour:", p9_ldp - p9_0.ldp, p9_ldskip - p9_0.ldskip, ln - p9_0.ln);
    for (int i = 1; i < 4; i++) fprintf(stderr, " %d: %ld %.1f", i, lit[i] - p9_0.lit[i], (double)(lt[i] - p9_0.lt[i]) / dv);
    fprintf(stderr, "\n");
    memcpy(p9_0.sv, p9_sv, sizeof p9_sv); memcpy(p9_0.svt, p9_svt, sizeof p9_svt); memcpy(p9_0.wbn, p9_wbn, sizeof p9_wbn); memcpy(p9_0.dma, p9_dma, sizeof p9_dma);
    memcpy(p9_0.svus, p9_svus, sizeof p9_svus); memcpy(p9_0.wbus, p9_wbus, sizeof p9_wbus); p9_0.poll = p9_poll; p9_0.done = p9_done; p9_0.subm = p9_subm;
    p9_0.post = p9_post; p9_0.ldp = p9_ldp; p9_0.ldskip = p9_ldskip; p9_0.ln = ln; memcpy(p9_0.lit, lit, sizeof lit); memcpy(p9_0.lt, lt, sizeof lt);
  }
  {   /* gl40: the copy plan's line (the counts since the line before; the times in microseconds, 0 without MGS2_PROBE10) */
    const XmCopy *c = &xmc; long wn = 0; uint64_t wu = 0;
    fprintf(stderr, "%s vb %ld-%ld gl40 plan: turn %ld XP %d M5 walks %ld; plans %ld, us total %llu / walk %llu / sort+join %llu / rest %llu; "
            "mq_chk_end %ld us %llu; mp_mark %ld us %llu; xc_add calls %ld; ranges into the merges ra %ld rb %ld mk %ld; sort moves %ld, copied back %ld; "
            "extents %ld; XP check: plans %ld, differing %ld, by result only %ld; waits in mq_chk_end by reason (n/us):", who, vb0, vb, tix, rt_xp_at(vb0), g_k_m5 - p10_0.m5, c->xp_plans - p10_0.plans,
            (unsigned long long)((c->xp_tw - p10_0.tw) + (c->xp_ts - p10_0.ts) + (c->xp_tr - p10_0.tr)), (unsigned long long)(c->xp_tw - p10_0.tw),
            (unsigned long long)(c->xp_ts - p10_0.ts), (unsigned long long)(c->xp_tr - p10_0.tr), p10_ce - p10_0.ce, (unsigned long long)(p10_ceus - p10_0.ceus),
            p10_mk - p10_0.mkn, (unsigned long long)(p10_mkus - p10_0.mkus), c->xp_adds - p10_0.adds, c->xp_ra - p10_0.ra, c->xp_rb - p10_0.rb, c->xp_mk - p10_0.mk,
            c->xp_moves - p10_0.moves, c->xp_cpy - p10_0.cpy, c->exts - p10_0.ext, c->xp_chk - p10_0.chk, c->xp_dif - p10_0.dif, c->xp_room - p10_0.room);
    for (int i = 0; i < MW_N && i < 16; i++) if (p10_wn[i] != p10_0.wn[i]) { fprintf(stderr, " %d %ld/%llu", i, p10_wn[i] - p10_0.wn[i],
                                                                         (unsigned long long)(p10_wus[i] - p10_0.wus[i])); wn += p10_wn[i] - p10_0.wn[i]; wu += p10_wus[i] - p10_0.wus[i]; }
    fprintf(stderr, "; in all %ld/%llu\n", wn, (unsigned long long)wu);
    p10_0.plans = c->xp_plans; p10_0.adds = c->xp_adds; p10_0.moves = c->xp_moves; p10_0.cpy = c->xp_cpy; p10_0.ra = c->xp_ra; p10_0.rb = c->xp_rb; p10_0.mk = c->xp_mk;
    p10_0.chk = c->xp_chk; p10_0.dif = c->xp_dif; p10_0.room = c->xp_room; p10_0.ext = c->exts; p10_0.tw = c->xp_tw; p10_0.ts = c->xp_ts; p10_0.tr = c->xp_tr;
    p10_0.ce = p10_ce; p10_0.ceus = p10_ceus; p10_0.mkn = p10_mk; p10_0.mkus = p10_mkus; p10_0.m5 = g_k_m5;
    memcpy(p10_0.wn, p10_wn, sizeof p10_wn); memcpy(p10_0.wus, p10_wus, sizeof p10_wus);
  }
}
static void mq_tstart(long vb)   /* an ME turn begins (the main CPU's cache holds the counts as they are) */
{
  if (flip_mvb0 >= 0) flip_line("main turn", flip_mvb0, vb, 1);   /* gl27: the main CPU's turn before */
  flip_t0set(0);
  for (int k = 0; k < XME_T_N; k++) mq_tk0[k] = k == XME_T_IDLE ? mq_idle() : mq->t_k[k];   /* gl24: the idle time from the shared words */
  for (int k = 0; k < MW_N; k++) mq_tw0[k] = mq_wus[k];
  two_t0[0] = two_steps; two_t0[1] = two_spills; two_t0[2] = two_nohalf;   /* gl24 */
  two_t1[0] = two_nowait; two_t1[1] = two_older; two_t1[2] = two_move;   /* gl25 */
  mq_tt0 = mq_now(); mq_tvb0 = vb; mq_th0 = mq->n_hook; mq_tl0 = mq->n_list; mq_tg0 = mq->n_gewait;
  for (int k = 0; k < XM_TS_N; k++) { mq_ts0[k] = mq->t_ws[k]; mq_tsn0[k] = mq->n_ws[k]; }   /* gl26 */
  mq_tovu0 = mq_ov_us; mq_tovn0 = mq_ov_n; mq_ov_f = 0;
  { int b = ge2 ? 1 : 0; ge_tn0 = ge_n[b]; ge_td0 = ge_draw[b]; ge_tq0 = ge_note[b]; ge2_tn0 = mq->n_ge2; }   /* gl22; gl23 by GE2 */
#ifdef MGS2_ME_REAL
  mq_tdiv = mq_me > 0 ? 331310u : 1000u;
#endif
  if (flip_mvb0 >= 0) { nat_tline("main turn", flip_mvb0, vb, 0); bd_tline("main turn", flip_mvb0, vb); }   /* native step 1: the main CPU's turn before (gl38: its bound line) */
  nat_t0set();
}
static uint64_t mq_idle(void)   /* gl24: the loop's idle time (XmeSh t_idle_*: q odd while the loop writes them) */
{
  volatile XmeSh *x = XS(mq); uint32_t q, l, h; int n = 0;
  for (;;) {   /* gl24b: yield while q is odd (the emulator's loop thread runs below the main thread: it finishes its write only while
                  the main thread sleeps), and give up after about 20 ms (a loop stopped inside its write: the time is only a count) */
    q = x->t_idle_q; XME_BARRIER(); l = x->t_idle_lo; h = x->t_idle_hi; XME_BARRIER();
    if (!(q & 1) && q == x->t_idle_q) break;
    if (++n > 4) sceKernelDelayThread(10);
    if (n > 2000) break;
  }
  return (uint64_t)h << 32 | l;
}
static void mq_tend(long vb)   /* an ME turn ends, after the hand-over (the ME's cache written back, the main CPU's dropped) */
{
  double ms[XME_T_N], all = 0, w[MW_N], tt; uint64_t t = mq_now();
  if (mq_tvb0 < 0) return;
  for (int k = 0; k < XME_T_N; k++) { uint64_t d = (k == XME_T_IDLE ? mq_idle() : mq->t_k[k]) - mq_tk0[k]; mq_tkall[k] += d; ms[k] = (double)d / mq_tdiv; if (k < 8) all += ms[k]; }
  for (int k = 0; k < MW_N; k++) { uint64_t d = mq_wus[k] - mq_tw0[k]; mq_twall[k] += d; w[k] = (double)d / 1000.0; }
  tt = (double)(t - mq_tt0) / 1000.0; mq_ttall += t - mq_tt0; mq_tn++;
  fprintf(stderr, "me turn vb %ld-%ld (%.0f ms): the ME's own time (ms): records %.1f, thinning %.1f, fills %.1f, walks %.1f, hooks %.1f "
          "(frame ends and lists %.1f, GE waits %.1f, the rest %.1f), hand-over %.1f, idle %.1f, all %.1f; hooks %ld, lists %ld, GE waits %ld; "
          "the main CPU waited (ms): copies as they ended %.1f, fills %.1f, the arena %.1f, the rest %.1f; from a hook to its first copy plan "
          "%ld times, mean %.2f ms; gl22: idle in this turn %.1f; GE2 %d: lists %ld, drawing %.1f ms, seen %.1f ms after its finish; begun while the "
          "GE drew the frame before %ld; gl24 TWO %d: walks posted %ld, spills %ld, no half free %ld, the main CPU waited at hooks for the walk "
          "before %.1f ms; gl25: hooks without a wait %ld, waits for an older walk %ld, half moves %ld\n", mq_tvb0, vb, tt, ms[XE_REC], ms[XE_THIN], ms[XE_FILL], ms[XE_WALK], ms[XE_HOOK], ms[XME_T_LIST],
          ms[XME_T_GEW], ms[XE_HOOK] - ms[XME_T_LIST] - ms[XME_T_GEW], ms[XE_SYNC] + ms[XE_STOP], ms[XME_T_IDLE], all, mq->n_hook - mq_th0,
          mq->n_list - mq_tl0, mq->n_gewait - mq_tg0, w[MW_NOW], w[MW_FILL], w[MW_ARENA],
          w[MW_RAM] + w[MW_RING] + w[MW_THIN] + w[MW_DRAIN] + w[MW_PROBE] + w[MW_SYNC], mq_ov_n - mq_tovn0,
          mq_ov_n > mq_tovn0 ? (double)(mq_ov_us - mq_tovu0) / 1000.0 / (double)(mq_ov_n - mq_tovn0) : 0.0,
          tt - (all - ms[XME_T_IDLE]), ge2, ge_n[ge2 ? 1 : 0] - ge_tn0, (double)(ge_draw[ge2 ? 1 : 0] - ge_td0) / 1000.0,
          (double)(ge_note[ge2 ? 1 : 0] - ge_tq0) / 1000.0, mq->n_ge2 - ge2_tn0, two, two_steps - two_t0[0], two_spills - two_t0[1],
          two_nohalf - two_t0[2], w[MW_HOOK], two_nowait - two_t1[0], two_older - two_t1[1], two_move - two_t1[2]);
#ifdef MGS2_ME_REAL
  if (rs_tnote) fprintf(stderr, "me turn vb %ld-%ld gl28 me rest: this turn's idle may hold the main turn's last rest before it (%s): that "
                        "rest's Count ticks were not yet counted when the turn began\n", mq_tvb0, vb, rs_tnote == 2 ? "the switch's wait was "
                        "given up after 2 ms" : "the ME was awake at the switch after a rest");
  rs_tnote = 0;
#endif
  nat_tline("me turn", mq_tvb0, vb, 1); nat_t0set();   /* native step 1 */
  if (nge_k) nge_turn_line(mq_tvb0, vb);   /* native step 1 (MGS2_NGE): the fill lines and counts of the turn */
  bd_tline("me turn", mq_tvb0, vb);   /* gl38 */
  flip_line("me turn", mq_tvb0, vb, 0); flip_t0set(1); flip_mvb0 = vb;   /* gl27 */
  if (mq->ws_on) { double s[XM_TS_N], in = 0; long n[XM_TS_N];   /* gl26: the walk's split (gl27: only with MGS2_ME_SPLIT) */
    for (int k = 0; k < XM_TS_N; k++) { uint64_t d = mq->t_ws[k] - mq_ts0[k]; mq_tsall[k] += d; s[k] = (double)d / mq_tdiv; in += s[k];
      n[k] = mq->n_ws[k] - mq_tsn0[k]; mq_tsnall[k] += n[k]; }
    fprintf(stderr, "me turn vb %ld-%ld gl26 walk split (ms): calls: lane 1 meshes %.1f (%ld), lane 2 P06 %.1f (%ld), lane 3 effects %.1f (%ld), "
            "lane 4 P07 %.1f (%ld), no lane %.1f (%ld); held calls' flushes %.1f (%ld); calls that ran the held calls %.1f (%ld); "
            "passes 2-3 %.1f (%ld); GS primitives %.1f (%ld); the rest of the walks %.1f (of %.1f)\n", mq_tvb0, vb, s[1], n[1], s[2], n[2],
            s[3], n[3], s[4], n[4], s[0], n[0], s[XM_TS_FLUSH], n[XM_TS_FLUSH], s[XM_TS_CFL], n[XM_TS_CFL], s[XM_TS_PASS], n[XM_TS_PASS],
            s[XM_TS_PRIM], n[XM_TS_PRIM], ms[XE_WALK] - in, ms[XE_WALK]); }
  mq_tvb0 = -1;
}
static void mq_wait(uint32_t seq, int why)
{
  uint64_t t0, t1, t; uint32_t hb;
  if (mq_self || mq_done(seq)) return;
  HOST_ENTER(HOST_GW); bb_set(BB_MW(why));   /* gl44: MW (HOST_LEAVE sets the part below again) */
  uint32_t at = XS(mq)->at;
  t0 = t1 = mq_now(); hb = XS(mq)->heart; mq_wn[why]++;
  mq_w_t0 = t0; mq_w_r0 = mq_w_rounds; mq_w_words0 = (uint32_t)xl_stats()->words;
  while (!mq_self && !mq_done(seq)) {
    mq_service();
    t = mq_now();
#ifdef MGS2_ME_REAL
    if (mq_me > 0 && mq_me_x()) { mq_w_hb_t = t1; mq_me_tbk_exc = 1; mq_takeback(why); break; }   /* step 4d-4: an exception of the ME's */
#endif
    if (XS(mq)->heart != hb) { hb = XS(mq)->heart; if (t - t1 > mq_gap_max) { mq_gap_max = t - t1; mq_gap_vb = me_vb; mq_gap_at = at; } t1 = t; }
    else if (t - t1 >= mq_tbk_us) { mq_w_hb_t = t1; mq_takeback(why); break; }
#ifdef MGS2_ME_REAL
    else if (rs_cfg && mq_me > 0) rs_rekick(t, t1);   /* gl28: a lost wake (not expected) */
#endif
    at = XS(mq)->at;
    { int dr = sceKernelDelayThread(50); if (dr < 0 && !mq_dly_bad++) mq_dly_e = dr; }
    mq_w_rounds++;
  }
  t1 = mq_now() - t0; mq_wus[why] += t1;
  if (p10_in) { p10_wn[why & 15]++; p10_wus[why & 15] += t1; }   /* gl40 probe: a wait inside mq_chk_end */
  HOST_LEAVE(HOST_GW);
  XME_BARRIER();
}
/* at a vblank hook, entries waiting and no heartbeat since the hook before: the loop gets the CPU until its heartbeat moves (at low
   priority it runs only while the main CPU waits; on the real ME it would have moved already), or the take-back after
   MGS2_ME_PIC_TBK_MS: a loop that stops while the main CPU waits for nothing is seen within a hook or two */
static uint32_t mq_hk_hb;
static void mq_probe(void)
{
  uint64_t t0, t; uint32_t hb = XS(mq)->heart;
#ifdef MGS2_ME_REAL
  if (!mq_self && mq_me > 0 && mq_me_x()) { mq_w_t0 = mq_w_hb_t = mq_now(); mq_w_r0 = mq_w_rounds; mq_w_words0 = (uint32_t)xl_stats()->words;
    mq_me_tbk_exc = 1; mq_takeback(MW_PROBE); mq_hk_hb = 0; return; }   /* step 4d-4: an exception of the ME's, found at a hook */
#endif
  if (mq_self || XS(mq)->tail == XS(mq)->head || hb != mq_hk_hb) { mq_hk_hb = hb; return; }
  HOST_ENTER(HOST_GW); bb_set(BB_MW(MW_PROBE));   /* gl44: MW */
  t0 = mq_now(); mq_wn[MW_PROBE]++;
  mq_w_t0 = t0; mq_w_r0 = mq_w_rounds; mq_w_words0 = (uint32_t)xl_stats()->words;
  while (!mq_self && XS(mq)->heart == hb && XS(mq)->tail != XS(mq)->head) {
    mq_service();
    t = mq_now();
#ifdef MGS2_ME_REAL
    if (mq_me > 0 && mq_me_x()) { mq_w_hb_t = t0; mq_me_tbk_exc = 1; mq_takeback(MW_PROBE); break; }   /* step 4d-4 */
#endif
    if (t - t0 >= mq_tbk_us) { mq_w_hb_t = t0; mq_takeback(MW_PROBE); break; }
#ifdef MGS2_ME_REAL
    if (rs_cfg && mq_me > 0) rs_rekick(t, t0);   /* gl28: a lost wake (not expected) */
#endif
    { int dr = sceKernelDelayThread(50); if (dr < 0 && !mq_dly_bad++) mq_dly_e = dr; }
    mq_w_rounds++;
  }
  mq_wus[MW_PROBE] += mq_now() - t0;
  HOST_LEAVE(HOST_GW);
  mq_hk_hb = mq ? XS(mq)->heart : 0;
}
static void mq_wait_self(XmeCtx *x, int why);
/* an entry to the loop: into the queue (its place returned), or, when the main CPU runs the loop itself, run now */
static uint32_t mq_post(const XmeE *e)
{
  if (mq_dead) { mq_dropped++; return XS(mq)->head - 1; }   /* step 4d: the picture stopped */
  if (!mq_self && XS(mq)->head - XS(mq)->tail >= XME_N) {
    long tb = mq_tbk_cold; mq_wait(XS(mq)->head - XME_N, MW_RING);
    if (mq_dead) { mq_dropped++; return XS(mq)->head - 1; }
    if (mq_tbk_cold != tb && (e->kind == XE_REC || e->kind == XE_FILL || e->kind == XE_WALK)) { mq_dropped++; return XS(mq)->head - 1; } }
  if (mq_self) { xme_entry(mq, e, mq_wait_self); mq_selfrun++; return XS(mq)->head - 1; }
  mq->e[XS(mq)->head & (XME_N - 1)] = *e;
  if (mq_m4) mq_cpu_wb();   /* step 4d: the entry and the bytes it has the ME read (the records' RAM, the extents, the log) in RAM
                               before head moves (4d-4: the entry after its write, not before) */
  XME_BARRIER(); XS(mq)->head++; mq_posted++; p9_post++;
#ifdef MGS2_ME_REAL
  if (rs_cfg && mq_me > 0) { XME_BARRIER(); rs_kick_check(); }   /* gl28: head in memory before asleep is read (xme_me.c's handshake;
                                                                     the ME rests only in main turns, so expected never asleep here) */
#endif
  if (mq_th >= 0 && XS(mq)->idle) { sceKernelSignalSema(mq_sem, 1); mq_signals++; }   /* the thread's wait (4c); the real ME looks
                                                                                         at head itself (psp/xme_me.c) */
  return XS(mq)->head - 1;
}
static void mq_released_all(void)   /* the GE stopped (gl_ok 3): the ME no longer waits for it; no more frames */
{
  XS(mq)->ge_done = XS(mq)->listed + (1u << 30); mq_stop_due = 1;
}
/* gl28d: MGS2_GL_FBCHK.  A display address -> its VRAM offset (0xffffffff: not in VRAM); the buffer's name */
static uint32_t fb_off(const void *a) { uint32_t u = (uint32_t)(uintptr_t)a; return (u & 0x0fe00000u) == 0x04000000u ? u & 0x001fffffu : 0xffffffffu; }
static const char *fb_nm(uint32_t o) { return o == GL_FB0 ? "0" : o == GL_FB1 ? "1" : o == GL_TXT ? "text" : "other"; }
/* in mq_submit, before the list goes to the GE: the display's own answers (sceDisplayGetFrameBuf: IMMEDIATE, the buffer it scans now;
   NEXTFRAME, the one it shows from its next vblank) against the list's target buffer, and the display vblanks since the frame before
   was shown (gl_show), by turn.  A line for the first check, for each list into either buffer and for the first answer that names
   neither buffer (50 lines at most) */
static GL28D_COLD void fb_chk(const XmeRes *r)
{
  void *a0 = 0, *a1 = 0; int w0 = 0, w1 = 0, f0 = 0, f1 = 0, r0, r1, b = mq_self ? 1 : 0, imm, nxt, oth;
  uint32_t t = r->d ? GL_FB1 : GL_FB0, o0, o1, dv;
  r0 = sceDisplayGetFrameBuf(&a0, &w0, &f0, PSP_DISPLAY_SETBUF_IMMEDIATE);
  r1 = sceDisplayGetFrameBuf(&a1, &w1, &f1, PSP_DISPLAY_SETBUF_NEXTFRAME);
  dv = sceDisplayGetVcount() - gl_show_vc;
  o0 = r0 < 0 ? 0xffffffffu : fb_off(a0); o1 = r1 < 0 ? 0xffffffffu : fb_off(a1);
  imm = o0 == t; nxt = o1 == t;
  oth = (r0 >= 0 && o0 != GL_FB0 && o0 != GL_FB1) + (r1 >= 0 && o1 != GL_FB0 && o1 != GL_FB1);   /* gl28d2: neither buffer named */
  fbc_n[b]++; fbc_imm[b] += imm; fbc_next[b] += nxt; if (r0 < 0 || r1 < 0) fbc_err++;
  if (gl_shown) { if (!dv) fbc_same[b]++; if (dv > fbc_dvc[b]) fbc_dvc[b] = dv; if (dv > fbc_dvc_t[b]) fbc_dvc_t[b] = dv; }
  if ((imm || nxt || r0 < 0 || r1 < 0 || !fbc_lines || (oth && !fbc_oth)) && fbc_lines < 50) {   /* gl28d2: and the first answer naming
                                                                                                     neither buffer */
    fbc_lines++;
    fprintf(stderr, "picture: gl28 fbchk vb %ld (%s turn): the list of frame vb %ld into buffer %d; the display's answers: IMMEDIATE %08x "
            "(buffer %s, rc %d), NEXTFRAME %08x (buffer %s, rc %d); %u display vblanks after the show of frame vb %ld%s%s\n", me_vb,
            b ? "main" : "me", (long)r->fvb, r->d, (unsigned)(uintptr_t)a0, fb_nm(o0), r0, (unsigned)(uintptr_t)a1, fb_nm(o1), r1,
            (unsigned)dv, gl_shown_vb, imm ? ": INTO THE BUFFER ON THE SCREEN" : nxt ? ": INTO THE BUFFER SHOWN NEXT" : "",
            oth ? "; AN ANSWER NAMES NEITHER BUFFER (the counts above cannot match it)" : "");
  }
  fbc_oth += oth;
}
/* gl_list's part after gx_frame, for a result of the ME's: the counts, the "did not fit" and digest lines, the list to the GE */
static void mq_submit(const XmeRes *r)
{
  const GxCount *c = &r->c;
  if (r->r && ++gl_listfail <= 50)
    fprintf(stderr, "picture: vb %ld: the list did not fit: %ld batches left out (%ld words, %ld aux bytes)\n", (long)r->fvb, c->dropped, c->words, c->aux);
  gx_words_sum += c->words; gx_drop += c->dropped;
  gx_c1p += c->c1_passes; gx_c1b += c->c1_bones; gx_c1w += c->c1_words;
  gx_c4s += c->fx_spr; gx_c4r += c->fx_lruns; gx_c4l += c->fx_lines;
  gx_c2p += c->c2_passes; gx_c2b += c->c2_bones; gx_c2w += c->c2_words;
  if (c->words > gx_words_max) gx_words_max = c->words;
  if (c->aux > gx_aux_max) gx_aux_max = c->aux;
  if (4 * c->words + c->aux > gx_both_max) gx_both_max = 4 * c->words + c->aux;
  if (r->a_nb > nat_fa[0]) nat_fa[0] = r->a_nb; if (r->a_nv > nat_fa[1]) nat_fa[1] = r->a_nv; if (r->a_nd > nat_fa[2]) nat_fa[2] = r->a_nd;   /* native step 1 */
  if (dz_on) dz_acc(r->fvb, &r->dc, r->t_me ? (uint32_t)((uint64_t)r->t_list * 1000u / 331310u) : r->t_list, r->me_turn);   /* gl29 (the ME's
                                                                                                    Count: 331,310 a ms, as mq_tdiv) */
  if (r->dig) {
    uint64_t g = r->dig_frame; dig_all = (dig_all ^ g) * 0x100000001b3ull; dig_n++; char zt[224];
    fprintf(stderr, "picture: vb %ld buffer %d list %016llx frame %016llx: %ld words, %ld aux bytes, %ld batches, %ld PRIMs, %d textures, "
            "%ld dropped%s\n", (long)r->fvb, r->d, (unsigned long long)r->dig_list, (unsigned long long)g, c->words, c->aux, c->batches, c->prims,
            r->nt, c->dropped, dz_on ? dz_txt(zt, sizeof zt, r->dz, r->dzn, &r->dc) : "");
    fprintf(stderr, "picture: vb %ld content %016llx\n", (long)r->fvb, (unsigned long long)r->dig_c);   /* gl41: the loop's content digest */
  }
  sceKernelDcacheWritebackAll();         /* the GE reads the list, the frame and the textures from RAM */
  if (fbc_from >= 0 && me_vb >= fbc_from) fb_chk(r);   /* gl28d: MGS2_GL_FBCHK */
  ge_fin_n0 = ge_fin_n; ge_sub_t = sceKernelGetSystemTimeLow();   /* gl22 */
  sceGuStart(GU_DIRECT, gl_dl); sceGuCallList(r->list); sceGuFinish();
  gl_ge = 1; gl_ge_buf = r->d; gl_ge_vb = r->fvb; mq_subm++; mq_ge_t0 = mq_now();
}
/* the GE and the display: a list the GE has finished shown (and the ME told), the ME's next list to the GE; a GE that has not finished
   a list in GL_GE_LIMIT stops the picture, as gl_sync */
static void mq_service_do(void)
{
  if (!mq) return;
  if (mq_m4) mq_heap_serve();   /* step 4d-2 */
  if (gl_ge) {
    int r = sceGeDrawSync(1); p9_poll++;
    if (!r) { ge_noted(); gl_show(); XS(mq)->ge_done = mq_subm; p9_done++; }
    else if (mq_now() - mq_ge_t0 >= GL_GE_LIMIT) {
      gl_stop_vb = gl_ge_vb; gl_stop_r = r; gl_ok = 3; gl_now = gl_ge = 0;
      sceDisplaySetFrameBuf((void *)(0x44000000u + GL_TXT), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
      fprintf(stderr, "picture: STOPPED: the GE has not finished the list of the frame of vb %ld in %u ms (sceGeDrawSync(1) %08x): no more "
              "frames, the run goes on\n", gl_stop_vb, GL_GE_LIMIT / 1000, (unsigned)r);
      mq_released_all(); }
  }
  if (!gl_ge && gl_ok == 1 && mq_cons != XS(mq)->listed) {   /* step 4d-4: the result read through the uncached alias (the ME wrote it
                                                                 back before listed moved; a result of the main CPU's own turn is
                                                                 written back from its cache first) */
    int fb = mq_self ? 1 : 0;
    if (gl_shown && !flip_force && sceDisplayGetVcount() == gl_show_vc) {   /* gl27: the frame before is not on the screen yet */
      if (flip_on) { if (!flip_w) { flip_w = 1 + fb; flip_t0 = sceKernelGetSystemTimeLow(); flip_hold[fb]++; } return; }
      flip_race[fb]++; }
    if (flip_w) { flip_us[flip_w - 1] += sceKernelGetSystemTimeLow() - flip_t0; flip_w = 0; }   /* gl27b: to the hold's own bucket */
    uint32_t k = mq_cons++ % XME_NRES;
    XME_BARRIER(); sceKernelDcacheWritebackRange(&mq->res[k], sizeof mq->res[k]); XME_BARRIER(); mq_submit(&XR(mq)[k]); p9_subm++; }
}
static void mq_service(void)   /* gl39: the calls counted by the part the main CPU is in; with MGS2_PROBE9 one in 8 timed */
{
  int tp = p9_top(); p9_sv[tp]++;
  if (p9_on > 0 && !(p9_k++ & 7)) { uint32_t t0 = sceKernelGetSystemTimeLow(); mq_service_do(); p9_svus[tp] += sceKernelGetSystemTimeLow() - t0; p9_svt[tp]++; }
  else mq_service_do();
}
static void mq_ge_finish(void)   /* the main CPU waits for the GE's list (gl_sync, its limit), shows it and tells the ME */
{
  int r;
  if (!mq || !gl_ge) return;
  HOST_ENTER(HOST_GW); r = gl_sync(); HOST_LEAVE(HOST_GW);
  if (r) { mq_released_all(); return; }   /* (gl_sync's stop: gl_ok 3, the text on the screen) */
  ge_noted(); gl_show(); XS(mq)->ge_done = mq_subm;
}
/* gl28d: MGS2_GL_SHOT_FLIPWAIT, a shot's drained path from MGS2_GL_FLIPWAIT_FROM on: the loop's entries done (mq_drain) with the flip wait
   kept, then every list made given to the GE, drawn and shown, a list held while the frame before is not on the screen yet (200 us
   rounds).  The loop gives up when it finds 2 s passed since the drain began, checked after each round's mq_ge_finish, which can
   itself wait up to gl_sync's limit (GL_GE_LIMIT; beyond it the picture stops): at worst the drain's own wait + 2 s + GL_GE_LIMIT.
   The frame saved is the last one the loop made; gl27's forced path drew and showed at most one list after the drain, so it saved
   the same frame when at most one list was waiting then (always on the emulator, whose GE is done at once) and an earlier one when
   more were (GE2 on the PSP).  Only the GE no longer draws into the buffer still on the screen.  0: shown; -1: the picture stopped */
static GL28D_COLD int shot_wait(long vb)
{
  uint64_t t0 = mq_now(); long n = 0; int late = 0;
  flip_force = 0; mq_drain();
  while (gl_ok == 1 && mq && (gl_ge || mq_cons != XS(mq)->listed)) {
    mq_service(); mq_ge_finish();
    if (gl_ok != 1 || (!gl_ge && mq_cons == XS(mq)->listed)) break;
    if (mq_now() - t0 > 2000000u) { late = 1; break; }
    sceKernelDelayThread(200); n++;
  }
  fprintf(stderr, "picture: gl28 shot vb %ld (%s turn): the flip wait kept: %.1f ms, %ld rounds of 200 us%s\n", vb, mq_self ? "main" : "me",
          (double)(mq_now() - t0) / 1000.0, n, late ? "; GAVE UP after 2 s with a list not shown (the shot shows the frame before)" : "");
  return gl_ok == 1 ? 0 : -1;
}
/* gl29: the GE idle, every list made given to it, and its last list the frame on the screen (the depth buffer is that frame's) */
static GL28D_COLD int gl29_same(void) { return !gl_ge && gl_ge_vb == gl_shown_vb && (!mq || mq_cons == XS(mq)->listed); }
static GL28D_COLD int gl29_me(void) { return mq_on && !mq_self; }   /* an ME turn now */
static const char *dz_anm(int arm, int n, char *s)   /* an arm's name in s (8 bytes): O, X, Y, W, B<n> */
{
  if (arm == GX_DZ_B) snprintf(s, 8, "B%d", n); else { s[0] = arm >= 0 && arm < GX_DZ_B ? "OXYW"[arm] : '?'; s[1] = 0; }
  return s;
}
static GL28D_COLD const char *dz_fnm(long fvb, char *s)   /* the arm of the frame whose last vblank is fvb (-1: none yet) */
{
  int n = 0, a;
  if (fvb < 0) { s[0] = '-'; s[1] = 0; return s; }
  a = gx_dz_arm(&dz_t, fvb, &n, 0); return dz_anm(a, n, s);
}
static GL28D_COLD const char *dz_txt(char *s, int n, int dz, int dzn, const GxDzCount *c)   /* the digest line's part (the twin's alike) */
{
  char a[8];
  snprintf(s, n, "; dz %s 42:%ld oth:%ld pair:%ld sub:%ld fb:%ld,%ld,%ld,%ld,%ld,%ld bias:%ld w:%ld aux:%lu", dz_anm(dz, dzn, a), (long)c->n42,
           (long)c->noth, (long)c->npair, (long)c->nsub, (long)c->fb[0], (long)c->fb[1], (long)c->fb[2], (long)c->fb[3], (long)c->fb[4],
           (long)c->fb[5], (long)c->nbias, (long)c->nw, (unsigned long)c->aux);   /* gl30: sub (X's subset pairs) */
  return s;
}
static void dz_wadd(int32_t a, int32_t b, int lab)
{
  if (dz_nw >= DZ_NWIN || b < a) return;
  memset(&dz_w[dz_nw], 0, sizeof dz_w[0]); dz_w[dz_nw].vb0 = a; dz_w[dz_nw].vb1 = b; dz_w[dz_nw].lab = lab; dz_nw++;
}
static void dz_gap(int32_t a, int32_t b)   /* a stretch of O: split at MGS2_GL_LIVE's start (the walk) when that lies inside */
{
  if (live_from > a && live_from <= b) { dz_wadd(a, (int32_t)live_from - 1, 0); dz_wadd((int32_t)live_from, b, 0); }
  else dz_wadd(a, b, 0);
}
static int dz_win(long fvb)   /* the window of the frame whose last vblank is fvb, or -1 */
{
  for (int k = 0; k < dz_nw; k++) if (fvb >= dz_w[k].vb0 && fvb <= dz_w[k].vb1) return k;
  return -1;
}
static GL28D_COLD void gl29_cfg(void)
{
  const char *e;
  if ((e = mgs2_getenv("MGS2_GL_DZ"))) {
    if (gx_dz_parse(e, &dz_t) <= 0)
      fprintf(stderr, "gl29 dz (MGS2_GL_DZ %.60s): not <vb0>:<vb1>:<arm>[,...] with arm O, X, Y, W or B1..B255 (at most %d entries), so off\n",
              e, GX_DZ_MAX);
    else {
      int32_t prev = -1; char a[8];
      dz_on = 1; dz_nlab = 1; dz_lab[0].arm = GX_DZ_O; dz_lab[0].n = 0;
      for (int j = 0; j < dz_t.n; j++) {
        const GxDzEnt *x = &dz_t.e[j]; int l, n = x->arm == GX_DZ_B ? x->n : 0;
        for (l = 0; l < dz_nlab && (dz_lab[l].arm != x->arm || dz_lab[l].n != n); l++) {}
        if (l == dz_nlab) { dz_lab[l].arm = (int16_t)x->arm; dz_lab[l].n = (int16_t)n; dz_nlab++; }
        dz_elab[j] = l;
        if (x->vb0 <= prev) continue;   /* (not after the entry before: no window of its own) */
        if (prev >= 0 && x->vb0 > prev + 1) dz_gap(prev + 1, x->vb0 - 1);
        dz_wadd(x->vb0, x->vb1, l); prev = x->vb1;
      }
      if (prev >= 0) dz_gap(prev + 1, 0x7fffffff);
      fprintf(stderr, "gl29 dz (MGS2_GL_DZ): the 0x42 shadow decals' depth arm by frame (its last vblank), O elsewhere: %d entries:", dz_t.n);
      for (int j = 0; j < dz_t.n; j++) fprintf(stderr, " %ld-%ld %s", (long)dz_t.e[j].vb0, (long)dz_t.e[j].vb1, dz_anm(dz_t.e[j].arm, dz_t.e[j].n, a));
      fprintf(stderr, "; %d windows (a 'gl29 dz window' line each once the screen has left it)\n", dz_nw);
    }
  }
  if ((e = mgs2_getenv("MGS2_GL_ZDUMP"))) {
    long a = -1, v = 0, m = -1; int n = sscanf(e, "%ld:%ld:%ld", &a, &v, &m);
    if (n >= 2 && a >= 0 && (v == 1 || v == 2)) { zd_from = a; zd_views = (int)v; zd_mir = n == 3 ? m : -1;
      fprintf(stderr, "gl29 zdump (MGS2_GL_ZDUMP %.40s): at each MGS2_GL_SHOT shot from vb %ld on, the depth buffer (512 x 272 16-bit words, "
              "read only, through the uncached view) to zbuf_<frame vb>.raw next to the EBOOT", e, zd_from);
      if (zd_views == 2) fprintf(stderr, "; through the |0x600000 mirror too (zbm_<frame vb>.raw) at every one\n");
      else if (zd_mir >= 0) fprintf(stderr, "; through the |0x600000 mirror too (zbm_<frame vb>.raw) at the shot of vb %ld only\n", zd_mir);
      else fputc('\n', stderr); }
    else fprintf(stderr, "gl29 zdump (MGS2_GL_ZDUMP %.40s): not <from>:<views 1 or 2>[:<mirror vb>], so off\n", e); }
  if ((e = mgs2_getenv("MGS2_GL_SHOT_RANGE"))) {
    long a = -1, b = -1, st = 0;
    if (sscanf(e, "%ld:%ld:%ld", &a, &b, &st) == 3 && a >= 0 && b >= a && st > 0) { sr_from = a; sr_to = b; sr_step = st;
      fprintf(stderr, "gl29 shot range (MGS2_GL_SHOT_RANGE %.40s): a shot (drained as MGS2_GL_SHOT's, no depth) at every %ld-th vblank hook "
              "from %ld to %ld\n", e, sr_step, sr_from, sr_to); }
    else fprintf(stderr, "gl29 shot range (MGS2_GL_SHOT_RANGE %.40s): not <from>:<to>:<step> with step > 0, so off\n", e); }
}
static GL28D_COLD void dz_acc(long fvb, const GxDzCount *c, uint32_t us, int me)   /* a list made: its window's counts (me: in an ME turn) */
{
  int k = dz_win(fvb), t = me ? 0 : 1;
  if (k < 0) return;
  DzWin *w = &dz_w[k];
  w->fr[t]++; w->n42 += c->n42; w->noth += c->noth; w->npair += c->npair; w->nsub += c->nsub; w->nbias += c->nbias; w->nw += c->nw;
  for (int j = 0; j < GX_DZF_N; j++) w->fb[j] += c->fb[j];
  if (c->aux > w->aux) w->aux = c->aux;
  w->us[t] += us; if (us > w->usmax[t]) w->usmax[t] = us;
}
static GL28D_COLD void dz_r(long vb)   /* MGS2_EYE's peek at this hook found R down: by the arm of the frame on the screen and the turn */
{
  long f = gl_shown_vb; int t = mq_on && !mq_self ? 0 : 1, j = -1, k;
  (void)vb;
  if (f < 0) return;
  gx_dz_arm(&dz_t, f, 0, &j); dz_held[j >= 0 ? dz_elab[j] : 0][t]++;
  if ((k = dz_win(f)) >= 0) { dz_w[k].r[t]++; if (f >= (long)dz_w[k].vb0 + 10) dz_w[k].r10++; }
}
static void dz_wline(FILE *f, const DzWin *w)
{
  char a[8], v1[16];
  if (w->vb1 == 0x7fffffff) snprintf(v1, sizeof v1, "end"); else snprintf(v1, sizeof v1, "%ld", (long)w->vb1);
  fprintf(f, "picture: gl29 dz window vb %ld-%s arm %s: frames %ld (ME turns %ld, main %ld); 0x42 decals %ld (of other kinds %ld), paired %ld, "
          "subset %ld, fallbacks base %ld matrix %ld vertex set %ld triangles %ld uv/colour %ld room %ld, biased %ld, W %ld, the largest copy %lu bytes; "
          "list build: ME turns %.1f ms (the largest %.2f), main %.1f ms (the largest %.2f); R held %ld vblank hooks (ME turns %ld, main %ld), "
          "%ld from the window's 10th vblank on\n", (long)w->vb0, v1, dz_anm(dz_lab[w->lab].arm, dz_lab[w->lab].n, a), (long)(w->fr[0] + w->fr[1]),
          (long)w->fr[0], (long)w->fr[1], (long)w->n42, (long)w->noth, (long)w->npair, (long)w->nsub, (long)w->fb[0], (long)w->fb[1], (long)w->fb[2], (long)w->fb[3],
          (long)w->fb[4], (long)w->fb[5], (long)w->nbias, (long)w->nw, (unsigned long)w->aux, w->us[0] / 1000.0, w->usmax[0] / 1000.0,
          w->us[1] / 1000.0, w->usmax[1] / 1000.0, (long)(w->r[0] + w->r[1]), (long)w->r[0], (long)w->r[1], (long)w->r10);
}
static GL28D_COLD void dz_tick(FILE *f, long upto)   /* the lines of the windows that end before frame vb upto */
{
  while (dz_wcur < dz_nw && dz_w[dz_wcur].vb1 < upto) dz_wline(f, &dz_w[dz_wcur++]);
}
static GL28D_COLD void dz_report(FILE *f)   /* the run's end: the windows not printed yet, then R held by arm and turn, and by window */
{
  char a[8], v1[16];
  while (dz_wcur < dz_nw) dz_wline(f, &dz_w[dz_wcur++]);
  fprintf(f, "gl29 dz: R held (vblank hooks with R down, by the arm of the frame on the screen; ME turns / main turns):");
  for (int l = 0; l < dz_nlab; l++) fprintf(f, " %s %ld/%ld", dz_anm(dz_lab[l].arm, dz_lab[l].n, a), dz_held[l][0], dz_held[l][1]);
  fprintf(f, "\ngl29 dz: R held by window (frame vb, arm: vblank hooks, from the window's 10th vblank on):");
  for (int k = 0; k < dz_nw; k++) { const DzWin *w = &dz_w[k];
    if (w->vb1 == 0x7fffffff) snprintf(v1, sizeof v1, "end"); else snprintf(v1, sizeof v1, "%ld", (long)w->vb1);
    fprintf(f, " %ld-%s %s %ld (%ld)", (long)w->vb0, v1, dz_anm(dz_lab[w->lab].arm, dz_lab[w->lab].n, a), (long)(w->r[0] + w->r[1]), (long)w->r10); }
  fputc('\n', f);
}
/* gl28d: MGS2_EYE, at each vblank hook (xl_psp_gl_vblank, outside the step's time): the R trigger peeked (the latest sample; nothing
   consumed), a line at each press and release with the turn and what the screen holds */
static GL28D_COLD void eye_poll(long vb)
{
  SceCtrlData c; int r; uint64_t t = mq_now(); void *a = 0; int w = 0, f = 0, rc; char g[64], za[8];
  if (eye_vb0 < 0) { eye_vb0 = vb; eye_t0 = t; fprintf(stderr, "eye: t 0 at vb %ld (t: ms from this hook)\n", vb); }
  if (sceCtrlPeekBufferPositive(&c, 1) < 1) return;
  r = (c.Buttons & PSP_CTRL_RTRIGGER) != 0;
  if (dz_on && r) dz_r(vb);   /* gl29: R held, by the arm on the screen */
  if (r == eye_r) return;
  eye_r = r; eye_n[r]++;
  rc = sceDisplayGetFrameBuf(&a, &w, &f, PSP_DISPLAY_SETBUF_IMMEDIATE);
  if (gl_ge) snprintf(g, sizeof g, "draws frame vb %ld into %s", gl_ge_vb, gl_ge_buf == gl_shown_buf ? "THE SHOWN BUFFER" : "the other buffer");
  else snprintf(g, sizeof g, "idle");
  fprintf(stderr, "eye: R %s vb %ld t %llu turn %s two %d ge2 %d flip %s shown frame %ld buffer %d (shown #%ld, %u display vblanks ago); "
          "the display scans buffer %s; the GE %s%s%s\n", r ? "down" : "up", vb, (unsigned long long)((t - eye_t0) / 1000),
          mq_on && !mq_self ? "me" : "main", mq_on && !mq_self ? two : 0, mq_on && !mq_self ? ge2 : 0, flip_on ? "on" : "off", gl_shown_vb,
          gl_shown_buf, gl_shown, (unsigned)(sceDisplayGetVcount() - gl_show_vc), rc < 0 ? "?" : fb_nm(fb_off(a)), g, dz_on ? "; dz " : "",
          dz_on ? dz_fnm(gl_shown_vb, za) : "");   /* gl29: the arm of the frame on the screen */
}
/* gl28d: MGS2_GL_LIVE, at each vblank hook after the shots (outside the step's time): the buffer handed to the display (gl_shown_buf;
   on the screen, or from the next display vblank, before which the flip wait gives the GE nothing) copied as it is through the uncached
   view, as gl_shot reads it, with gl_shot's writer and its 23 KB buffer (no heap): no drain, no GE wait, nothing given to the GE, so
   the GE's flow is the same; the GE may draw the next frame into the other buffer meanwhile.  r: R went down at this hook.  Both kinds
   only inside from..to (gl28d2: an R press outside the window is only the 'eye' line: no capture, nothing taken from max_r) */
static GL28D_COLD void gl_live(long vb, int r)
{
  int in = vb >= live_from && vb <= live_to, per = in && (vb - live_from) % live_every == live_every / 2, rp = 0, b = gl_shown_buf, ok, rc;
  uint64_t t0; uint32_t rd = 0, dv; void *a = 0; int w = 0, f = 0; char p[256], g[64], za[8];
  if (in && r && eye_on && live_nr < live_rmax && vb - live_rlast >= 30) rp = 1;
  if (!per && !rp) return;
  if (b < 0) { fprintf(stderr, "picture: gl28 live vb %ld: no frame shown yet\n", vb); return; }
  if (rp) { live_nr++; live_rlast = vb; }
  if (per) live_np++;
  t0 = mq_now(); dv = sceDisplayGetVcount() - gl_show_vc;
  rc = sceDisplayGetFrameBuf(&a, &w, &f, PSP_DISPLAY_SETBUF_IMMEDIATE);
  snprintf(p, sizeof p, "%slive_%ld.bmp", host_dir(), vb);
  if (gl_ge) snprintf(g, sizeof g, "draws frame vb %ld into %s", gl_ge_vb, gl_ge_buf == b ? "THIS BUFFER" : "the other buffer");
  else snprintf(g, sizeof g, "idle");
  { uint32_t o = bb_set(BB_SH); ok = gl_bmp(p, b, &rd); bb_set(o); } if (ok != 1) live_bad++;   /* gl44: SH */
  fprintf(stderr, "picture: gl28 live vb %ld (%s%s) turn %s: buffer %d frame vb %ld (shown #%ld, %u display vblanks ago); the display scans "
          "buffer %s; the other buffer frame vb %ld; the GE %s; lists waiting %u; VRAM read %.1f ms, all %.1f ms: %s %s%s%s\n", vb,
          per ? "periodic" : "", rp ? (per ? ", R" : "R") : "", mq_on && !mq_self ? "me" : "main", b, gl_buf_vb[b & 1], gl_buf_n[b & 1],
          (unsigned)dv, rc < 0 ? "?" : fb_nm(fb_off(a)), gl_buf_vb[(b & 1) ^ 1], g, mq ? (unsigned)(XS(mq)->listed - mq_cons) : 0u,
          rd / 1000.0, (double)(mq_now() - t0) / 1000.0, ok == 1 ? "saved as" : ok == 0 ? "NOT saved (a write failed):" : "NOT saved (not opened):",
          p, dz_on ? "; dz " : "", dz_on ? dz_fnm(gl_buf_vb[b & 1], za) : "");   /* gl29: the arm of the frame captured */
}
/* the loop's waits when the main CPU runs its entries itself: the GE (why 1) waited for here */
static void mq_wait_self(XmeCtx *x, int why)
{
  (void)x;
  if (why == 3) { mq_heap_serve(); return; }   /* step 4d-2 (the main CPU's own turns use its own hooks: not expected) */
  if (why != 1) return;
  mq_service(); mq_ge_finish();
  if (gl_ok != 1) mq_released_all(); else XS(mq)->ge_done = mq_subm;
}
/* the thread's waits: the queue empty (the main CPU signals when it posts and sees idle; 2 ms at most, for the heartbeat), the GE, the
   hang test (for ever) */
static void mq_wait_thr(XmeCtx *x, int why)
{
  (void)x;
  if (why == 0) { SceUInt us = 2000; sceKernelWaitSema(mq_sem, 1, &us); }
  else if (why == 1) sceKernelDelayThread(500);
  else if (why == 3) sceKernelDelayThread(100);   /* step 4d-2: the heap's answer */
  else for (;;) sceKernelSleepThread();
}
static int mq_thread(SceSize n, void *a)
{
  (void)n; (void)a; { SceKernelThreadInfo ti; memset(&ti, 0, sizeof ti); ti.size = sizeof ti; if (sceKernelReferThreadStatus(sceKernelGetThreadId(), &ti) >= 0 && ti.stack && (uint8_t *)&ti - 512 > (uint8_t *)ti.stack) memset(ti.stack, 0xff, (size_t)((uint8_t *)&ti - 512 - (uint8_t *)ti.stack)); }   /* gl41: the stack's bottom marked (0xff) for the end's deepest-use line, with imports the ELF has */
  pspFpuSetFCR31(0x01000001u);          /* the main thread's rule (main_psp.c, lessons A9): the picture's floats as on the main CPU */
  xme_run(mq, mq_wait_thr);
  return 0;
}
/* step 4d-3: the line check at set-up (tools/dev/me18/mesec_chk.py checks the ELF the same way): the ME-safe objects' .data and .bss and
   this file's objects the ME writes (ME_LN) in psp/me_sec.ld's .me_data and .me_bss, each section on whole 64-byte lines; the memory the
   ME writes outside them (the queue's ctx and its parts, the arena, the frame memory, the texture pool, the picture heap's first region,
   the view's buffers) on whole lines too.  The symbols are weak: 0 when the link had no psp/me_sec.ld */
extern char __me_data_start[] __attribute__((weak)), __me_data_end[] __attribute__((weak));
extern char __me_bss_start[] __attribute__((weak)), __me_bss_end[] __attribute__((weak));
static int mq_ln_bad;
static int ln_ok(const void *p, size_t n) { return p && !((uintptr_t)p & 63) && !(n & 63); }
static const char *ln_yn(int ok) { if (!ok) mq_ln_bad++; return ok ? "yes" : "NO"; }
static void mq_lines(void)
{
  const struct { const char *nm; const void *p; size_t n; } o[] = { { "M", &M, sizeof M }, { "glf", &glf, sizeof glf },
    { "gxl", &gxl, sizeof gxl }, { "gxh", &gxh, sizeof gxh }, { "xmp", &xmp, sizeof xmp }, { "mp_view", &mp_view, sizeof mp_view } };
  const char *b0 = __me_bss_start, *b1 = __me_bss_end, *d0 = __me_data_start, *d1 = __me_data_end; int k, in = 0, fx = 1;
  mq_ln_bad = 0;
  fprintf(stderr, "me picture lines (task #180 step 4d-3): .me_data %08x-%08x, .me_bss %08x-%08x (%u KB); on whole lines: %s, %s; this file's "
          "objects the ME writes inside .me_bss:", (unsigned)(uintptr_t)d0, (unsigned)(uintptr_t)d1, (unsigned)(uintptr_t)b0,
          (unsigned)(uintptr_t)b1, (unsigned)((b1 - b0) >> 10), ln_yn(d0 && d1 >= d0 && !((uintptr_t)d0 & 63) && !((uintptr_t)d1 & 63)),
          ln_yn(b0 && b1 > b0 && !((uintptr_t)b0 & 63) && !((uintptr_t)b1 & 63)));
  for (k = 0; k < (int)(sizeof o / sizeof o[0]); k++) {
    int ok = b0 && (const char *)o[k].p >= b0 && (const char *)o[k].p + o[k].n <= b1; in += ok;
    fprintf(stderr, " %s %08x %s", o[k].nm, (unsigned)(uintptr_t)o[k].p, ln_yn(ok)); }
  for (k = 0; k < 4; k++) if (mp_view.fix[k] && !ln_ok(mp_view.fix[k], mp_view.fix_n)) fx = 0;
  fprintf(stderr, "; on whole lines: the queue's ctx %08x (%u bytes) %s, its parts at +%u (shared words) +%u (entries) +%u (results) +%u "
          "(thinning slot) +%u (the ME's own) %s; the arena %s, the frame memory %s, the texture pool %s, the picture heap's first region %s, "
          "the view's %d fixed buffers %s; problems %d\n", (unsigned)(uintptr_t)mq, (unsigned)sizeof *mq, ln_yn(ln_ok(mq, sizeof *mq)),
          (unsigned)offsetof(XmeCtx, sh), (unsigned)offsetof(XmeCtx, e), (unsigned)offsetof(XmeCtx, res), (unsigned)offsetof(XmeCtx, thin_r),
          (unsigned)offsetof(XmeCtx, rec), ln_yn(!((offsetof(XmeCtx, sh) | offsetof(XmeCtx, e) | offsetof(XmeCtx, res) | offsetof(XmeCtx, thin_r)
          | offsetof(XmeCtx, rec)) & 63)), ln_yn(ln_ok(xmc.arena, xmc.arena_n)), ln_yn(ln_ok(gl_vol, (gl_vol_n + 63) & ~63u)),
          ln_yn(ln_ok(gl_pool, (size_t)gl_texkb << 10)), ln_yn(ln_ok(me_hb, me_hn)), mp_view.fix_n ? 4 : 0, ln_yn(fx), mq_ln_bad);
  (void)in;
}
void mq_setup(void)   /* mp_setup in mode 3, after 4b-2's set-up */
{
  const char *e;
#ifdef MGS2_ME_REAL
  rs_cfg = (e = mgs2_getenv("MGS2_ME_REST")) && atoi(e) == 1;   /* gl28: before the ME starts, and before this set-up can give up (the
                                                                   switch's value always in the log, as the cfg gave it) */
  fprintf(stderr, "gl28 me rest (MGS2_ME_REST %.16s): %s\n", e ? e : "not set", rs_cfg ? "the ME rests in HALT (psp/xme_rest.S) through the "
          "main CPU's turns, woken by the main CPU's interrupt (0xBC100044) when its own turn begins; never inside its turns; a 'main turn "
          "vb A-B gl28 me rest' line per main turn, 'gl28 me rest totals' at the end (the emulator has no ME: no rests there)" : e && atoi(e)
          ? "not 1, so off (only 1 turns it on): the ME spins when idle (gl27)" : "off: the ME spins when idle (gl27)");
#endif
  if (MQ_ED() && (mp_mode != 2 || !(mq = memalign(64, sizeof *mq)))) {   /* step 4d-4: the main CPU cannot copy into the eDRAM */
    fprintf(stderr, "me picture loop: %s with the arena in the ME's eDRAM, which the main CPU cannot reach: the split off, today's walk\n",
            mp_mode != 2 ? "4b-2's set-up failed" : "no memory for its queue");
    mp_on = 0; mp_want = 0; mq_noed(); return; }
  if (mp_mode != 2) { fprintf(stderr, "me picture loop: 4b-2's set-up failed: 4b-1 on the main CPU\n"); return; }
  if (!mq && !(mq = memalign(64, sizeof *mq))) { fprintf(stderr, "me picture loop: no memory for its queue (%u KB): 4b-2 on the main CPU\n",
                                                  (unsigned)(sizeof *mq >> 10)); return; }
  memset(mq, 0, sizeof *mq); mq->clk = mq_clk_us;   /* gl20 */
  mq->w_thr = mq_wait_thr;   /* gl26: the loop's thread times the walk's split (the emulator), set once */
  mq->ws_on = (e = mgs2_getenv("MGS2_ME_SPLIT")) && atoi(e) > 0;   /* gl27: before the ME starts */
  fprintf(stderr, "me picture walk split (gl26's timers, MGS2_ME_SPLIT): %s\n", mq->ws_on ? "on" : "off");
  flip_cfg = (e = mgs2_getenv("MGS2_GL_FLIPWAIT")) ? atoi(e) : 0; flip_from = (e = mgs2_getenv("MGS2_GL_FLIPWAIT_FROM")) ? atol(e) : -1;
  flip_on = flip_cfg == 1 || flip_cfg == 2;   /* gl27c: as flip_sel before the turns (other values: off, as printed) */
  fprintf(stderr, "picture display (gl27): the next list waits for the screen to show the frame before (MGS2_GL_FLIPWAIT): %s", flip_cfg == 2 ?
          "in every other ME turn and the main CPU turn after it (on in odd pairs, as GE2=2)" : flip_cfg == 1 ? "always" : "never (counted)");
  if (flip_cfg == 2 && flip_from >= 0) fprintf(stderr, ", always from vb %ld", flip_from);
  fprintf(stderr, "\n");
  if ((e = mgs2_getenv("MGS2_GL_FBCHK"))) {   /* gl28d */
    fbc_from = atol(e) >= 0 ? atol(e) : -1;
    fprintf(stderr, "gl28 fbchk (MGS2_GL_FBCHK %.16s): %s", e, fbc_from >= 0 ? "on from vb " : "off (not a vblank)\n");
    if (fbc_from >= 0) fprintf(stderr, "%ld: at each list given to the GE, the display's answers (sceDisplayGetFrameBuf IMMEDIATE and "
                               "NEXTFRAME) against the list's target buffer and the display vblanks since the frame before was shown, "
                               "by turn on the 'gl27 display' lines and at the end\n", fbc_from); }
  if ((e = mgs2_getenv("MGS2_GL_SHOT_FLIPWAIT"))) {   /* gl28d */
    shot_fw = atoi(e) == 1;
    fprintf(stderr, "gl28 shot flip wait (MGS2_GL_SHOT_FLIPWAIT %.16s): %s\n", e, !shot_fw ? "not 1, so off (the shots force the flip, "
            "as gl27)" : flip_from >= 0 && flip_cfg ? "on: from MGS2_GL_FLIPWAIT_FROM on, a shot's drained path keeps the flip wait and "
            "gives the GE every list made (the last frame made saved; no list drawn into the buffer still on the screen)" : "on, but "
            "MGS2_GL_FLIPWAIT or MGS2_GL_FLIPWAIT_FROM is not set: no shot waits"); }
#ifdef MGS2_ME_REAL
  mq->clk_me = xme_me_clk; mq->w_me = xme_me_wait;   /* gl21: set once, before the ME starts (xme.c XME_CLK) */
#endif
  sceKernelDcacheWritebackInvalidateRange(mq, sizeof *mq);   /* step 4d-4: the zeros in RAM before the shared words are reached uncached
                                                                (no dirty line of the memset's left to land on them later) */
  mq->p = &xmp; mq->m = &M; mq->v = &mp_view; mq->log = mp_log; mq->hang_vb = -1;
  me_beat_p = &XS(mq)->heart;   /* the heartbeat inside long walks (me_rt.h ME_BEAT; step 4d-4: its uncached alias) */
  mq->fc.mem = M.mem; mq->fc.spr = M.spr; mq->fc.mem_size = XL_GSZ; mq->fc.ext = xmc.ext; mq->fc.ext_cap = xmc.ext_cap;
  mq->fc.arena = xmc.arena; mq->fc.arena_n = xmc.arena_n;
  mq_hi = (e = mgs2_getenv("MGS2_ME_PIC_PRI")) && atoi(e);
  mq_turn = (e = mgs2_getenv("MGS2_ME_PIC_TURN")) && atoi(e) > 0 ? atoi(e) : 0;
  ge_fast_cfg = (e = mgs2_getenv("MGS2_ME_PIC_GEFAST")) ? atoi(e) : 0; ge_fast = ge_fast_cfg == 1;   /* gl22 */
  fprintf(stderr, "me picture GE (gl22): the GE's finish timed by sceGu's finish callback; the ME told of it at once (MGS2_ME_PIC_GEFAST): %s\n",
          ge_fast_cfg == 2 ? "in every other ME turn (off, on, ...)" : ge_fast_cfg == 1 ? "always" : "never (the main CPU tells it, as gl21)");
  ge2_cfg = (e = mgs2_getenv("MGS2_ME_PIC_GE2")) ? atoi(e) : 0;   /* gl23 */
  two_cfg = (e = mgs2_getenv("MGS2_ME_PIC_TWO")) ? atoi(e) : 0;   /* gl24: the halves (the arena's at 64 bytes) */
  two_xn = xmc.ext_cap; two_an = xmc.arena_n; two_xh = two_xn / 2; two_ah = (two_an / 2) & ~63u; two = 0;
  fprintf(stderr, "me picture copies (gl24): two copy areas (a step's copies in one half while the ME walks the step before from the other; "
          "MGS2_ME_PIC_TWO): %s; each %u KB and %d extents\n", !mq_turn && two_cfg ? "off (no turns)" : two_cfg == 2 ? "in every other ME turn "
          "(off, on, ...)" : two_cfg == 1 ? "in every ME turn" : "off", (unsigned)(two_ah >> 10), two_xh);
  if (gl_vol2) { mq->fm[0] = gl_vol; mq->fm[1] = gl_vol2; mq->fl[0] = &gxl; mq->fl[1] = &gxl2;
    mq->fm_arena = GL_PSP_ARENA - nge_k; mq->fm_nb = GL_PSP_NB; mq->fm_nv = GL_PSP_NV; }
  fprintf(stderr, "me picture GE (gl23): two frames on the GE side (the ME begins a frame while the GE draws the one before; MGS2_ME_PIC_GE2): "
          "%s\n", !gl_vol2 ? "off (one frame memory)" : ge2_cfg == 2 ? "in every other ME turn (off, on, ...)" : ge2_cfg == 1 ? "in every ME turn" : "off");
  if ((e = mgs2_getenv("MGS2_ME_PIC_TBK_MS")) && atoi(e) > 0) mq_tbk_us = (uint32_t)atoi(e) * 1000u;
  if ((e = mgs2_getenv("MGS2_ME_PIC_HANG"))) mq->hang_vb = (int32_t)atol(e);
  mq_m4 = mp_want == 4;   /* step 4d */
  mq->hang_in = mq_m4 && (e = mgs2_getenv("MGS2_ME_PIC_HANG_IN")) && atoi(e);
  if ((e = mgs2_getenv("MGS2_ME_PIC_GE2_AB"))) {   /* gl28d (gl28d2: read once mq_m4 and mq_turn are known: it acts in mq_turn_hook,
                                                      mode 4's ME turns only) */
    ge2_ab = atol(e) >= 0 && mq_m4 && mq_turn ? atol(e) : -1;
    fprintf(stderr, "gl28 ge2 a/b (MGS2_ME_PIC_GE2_AB %.16s): %s", e, atol(e) < 0 ? "off (not a vblank)\n" : ge2_ab < 0 ? "off: it acts "
            "only at the ME turns of mode 4 (MGS2_ME_PIC=4 with MGS2_ME_PIC_TURN), which this run does not have\n" : "from vb ");
    if (ge2_ab >= 0) fprintf(stderr, "%ld GE2 only in every other ME turn: on when vb / (2 * MGS2_ME_PIC_TURN) is odd (it needs "
                             "MGS2_ME_PIC_GE2 for the second frame memory); a 'gl28 ge2 a/b' line at each ME turn's start from then on\n",
                             ge2_ab); }
  mq_ft_on = glf.ft_on;
  nat_psp_setup();   /* native step 1 */
  nge_psp_setup();   /* native step 1 (MGS2_NGE): after the native arrays, before the ME starts */
  xm_install(&xmp);   /* the translator's callbacks are the loop's (ge/xlme.c xm_on_*: its frame, xmp.now), no longer on_call's */
  mq_on = 1; mq_self = 1;   /* (the thread starts after the translator's reset: mq_start) */
  if (mq_m4) mq_lines();   /* step 4d-3 */
}
#ifdef MGS2_ME_REAL
/* step 4d-4: the loop on the ME: the control block and the region, the loop's ctx and wait for the heap's requests bound, every cache
   line the ME will read written back (the control block reached uncached from then on), then hle_me_start (Sony's boot steps, then
   psp/xme_me.c's entry at its kseg0 alias; it waits up to 100 ms for the ME's heartbeat and holds the ME in reset when it does not
   come).  1: running */
static int mq_me_start(void)
{
  char pg[400], sl[600]; int r, pw;
  mq_mc = memalign(64, sizeof *mq_mc); mq_mreg = memalign(64, MQ_ME_REG);
  if (!mq_mc || !mq_mreg) {
    snprintf(mq_me_why, sizeof mq_me_why, "no memory for its control block and its region (%u KB)", (unsigned)(MQ_ME_REG >> 10));
    free(mq_mc); free(mq_mreg); mq_mc = 0; mq_mreg = 0; return 0; }
  memset(mq_mc, 0, sizeof *mq_mc);
  xme_bind(mq, xme_me_wait);
  sceKernelDcacheWritebackInvalidateAll();
  mq_mcu = (volatile XmeMeCtl *)((uintptr_t)mq_mc | 0x40000000u);
  mq_mcu->x = mq; XS(mq)->me_ctl = (uint32_t)(uintptr_t)mq_mcu;
  if (rs_cfg) mq_mcu->rest = XME_REST_CFG;   /* gl28: the ME sets its rest up at its start (psp/xme_me.c xme_me_main) */
  pw = hle_me_power_init();
  r = hle_me_start(xme_me_main, &mq_mcu->h, mq_mreg, MQ_ME_REG, MQ_ME_REG, mq_me_why, sizeof mq_me_why);
  hle_me_progress(pg, sizeof pg); hle_me_sony_line(sl, sizeof sl);
  fprintf(stderr, "me picture loop (step 4d-4): the ME's start: %s; %s; power callback %d, scePowerLock %d; control block %08x, region "
          "%08x (%u KB)\n", pg, sl, pw, hle_me_power_lock_rc(), (unsigned)(uintptr_t)mq_mc, (unsigned)(uintptr_t)mq_mreg, (unsigned)(MQ_ME_REG >> 10));
  if (!r) {   /* gl19: where the loop got to (gl18me's ME stopped between its start and its first heartbeat, place unknown) */
    volatile XmeSh *s = XS(mq);
    fprintf(stderr, "me picture loop (gl19): the loop's shared words after a failed start: heart %u, place %u, idle %u, tail %u, head %u, "
            "hung %u, stopped %u; the control block's Count %08x\n", (unsigned)s->heart, (unsigned)s->at, (unsigned)s->idle,
            (unsigned)s->tail, (unsigned)s->head, (unsigned)s->hung, (unsigned)s->stopped, (unsigned)mq_mcu->h.count);
    XS(mq)->me_ctl = 0; mq_mcu = 0; free(mq_mreg); free(mq_mc); mq_mreg = 0; mq_mc = 0; return 0; }
  return 1;
}
#endif
void mq_start(void)   /* xlive_chk_dma's start, after xl_reset: the loop's thread (step 4d-4: or the ME) */
{
  int pri = mq_hi ? 0x18 : 0x30;   /* the main thread's is 0x20 (a lower number runs first) */
#ifdef MGS2_ME_REAL
  if (mq_me_try) {
    if (mq_me_start()) {
      mq_me = 1;
      if (mq_turn) { mq_turn_k = (int)((me_vb / mq_turn) & 1); mq_self = mq_turn_k; } else mq_self = 0;
      flip_on = flip_sel(me_vb);   /* gl27b: the first turn's */
      mq->cache = mq_self ? 0 : xme_me_cache;   /* gl21: the clocks are set once (xme.c XME_CLK) */
      if (!mq_self) { xme_heap_ask(me_rs_on); mq_tstart(me_vb); two_start(two_on(me_vb)); }
      else rs_allow(me_vb);   /* gl28: a first turn of the main CPU's rests too (MGS2_ME_REST=1) */
      fprintf(stderr, "me picture loop (task #180 step 4d-4): on: the loop on the PSP's Media Engine, queue of %u entries, %d results, at "
              "%08x (%u KB); the arena and the view's buffers %s; %s; take-back after %u ms without a heartbeat while the main CPU waits, or "
              "at once on an exception of the ME's (MGS2_ME_PIC_TBK_MS)%s\n", XME_N, XME_NRES, (unsigned)(uintptr_t)mq, (unsigned)(sizeof *mq >> 10),
              mq_edram ? "in its eDRAM" : "in main RAM", mq_turn ? "turns: every other block of MGS2_ME_PIC_TURN vblanks the main CPU makes "
              "the picture itself" : "no turns", (unsigned)(mq_tbk_us / 1000), mq->hang_vb >= 0 ? (mq->hang_in ? "; the hang test inside the "
              "walk after the hook at or after MGS2_ME_PIC_HANG (MGS2_ME_PIC_HANG_IN: a cold take-back)" : "; the hang test: the loop stops at "
              "the first frame hook at or after MGS2_ME_PIC_HANG, its cache written back (a warm take-back)") : "");
      if (mq_turn) fprintf(stderr, "me picture loop: turns of %d vblanks, the first %s\n", mq_turn, mq_self ? "on the main CPU" : "the ME's");
      return; }
    fprintf(stderr, "me picture loop (step 4d-4): the ME did not start: %s\n", mq_me_why);
    if (mq_edram) {   /* the main CPU cannot reach the arena: no copies, each transfer walked from RAM as it ends */
      mq_self = 1; mq->cache = 0; mq->clk = mq_clk_us; xme_bind(mq, mq_wait_self);
      fprintf(stderr, "me picture loop (step 4d-4): the arena is in the ME's eDRAM, which the main CPU cannot reach: the main CPU makes the "
              "picture itself for the whole run (each transfer walked from RAM as it ends)\n");
      return; }
    fprintf(stderr, "me picture loop (step 4d-4): the arena is in main RAM: the loop runs as 4c's thread\n"); }
#endif
  xme_bind(mq, mq_wait_thr);   /* step 4d-4: the heap's requests' ctx and wait (the thread's) */
  mq_sem = sceKernelCreateSema("me_pic", 0, 0, 1, 0);
  /* gl41: the thread's stack 16 KB (was 64 KB), the real ME's own stack size (mecheck's deepest chain 5,120 bytes): with native step 1's
     tables and the cooker's scratch outside newlib's heap, partition 2 keeps 27 KB in its largest block, and a 64 KB stack was refused
     (80020190) on the emulator, where the main CPU then ran every entry itself (no turns).  The ME on hardware uses no thread. */
  mq_th = mq_sem >= 0 ? sceKernelCreateThread("me_pic", mq_thread, pri, MQ_THR_STK, PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU, 0) : -1;
  if (mq_th < 0 || sceKernelStartThread(mq_th, 0, 0) < 0) {
    fprintf(stderr, "me picture loop: no thread (semaphore %08x, thread %08x): the main CPU runs the loop's entries itself\n", (unsigned)mq_sem,
            (unsigned)mq_th);
    if (mq_th >= 0) sceKernelDeleteThread(mq_th);
    mq_th = -1; return; }
  if (mq_turn) { mq_turn_k = (int)((me_vb / mq_turn) & 1); mq_self = mq_turn_k; } else mq_self = 0;
  flip_on = flip_sel(me_vb);   /* gl27b: the first turn's */
  if (mq_m4) { mq->cache = mq_self ? 0 : mq_cache_thr; if (!mq_self) { xme_heap_ask(me_rs_on); mq_tstart(me_vb); two_start(two_on(me_vb)); } }   /* step 4d: the ME's cache steps and
                                                                                                   heap requests while it makes the picture */
  fprintf(stderr, "me picture loop (task #180 step 4c): on: the ME's loop (ge/xme.c) as a thread of the main CPU, priority %#x (%s the main "
          "thread's; MGS2_ME_PIC_PRI), queue of %u entries, %d results, at %08x (%u KB); %s; take-back after %u ms without a heartbeat while "
          "the main CPU waits (MGS2_ME_PIC_TBK_MS)%s\n", pri, mq_hi ? "above" : "below", XME_N, XME_NRES, (unsigned)(uintptr_t)mq,
          (unsigned)(sizeof *mq >> 10), mq_turn ? "turns: every other block of MGS2_ME_PIC_TURN vblanks the main CPU runs the entries itself"
          : "no turns", (unsigned)(mq_tbk_us / 1000), mq->hang_vb >= 0 ? "; the hang test on (MGS2_ME_PIC_HANG)" : "");
  if (mq_turn) fprintf(stderr, "me picture loop: turns of %d vblanks, the first %s\n", mq_turn, mq_self ? "on the main CPU" : "the thread's");
  if (mq->hang_vb >= 0) fprintf(stderr, "me picture loop: the hang test: the thread's loop stops at the first frame hook at or after vb %ld\n", (long)mq->hang_vb);
  if (mq_m4) fprintf(stderr, "me picture loop (task #180 step 4d-1): mode 4, the real ME's way on the thread: the main CPU's own turns walk "
                     "each transfer from RAM as it ends; the turns change after a hook's walk and list (SYNC); the caches handed over (the "
                     "main CPU's write-backs, the ME's steps counted by a stand-in); a cold take-back stops the picture%s\n",
                     mq->hang_vb >= 0 ? (mq->hang_in ? "; the hang test inside the walk after the hook (MGS2_ME_PIC_HANG_IN: a cold "
                     "take-back)" : "; the hang test's stop found right after the hook (a warm take-back)") : "");
}
static void mq_drain(void) { if (mq && !mq_self) mq_wait(XS(mq)->head - 1, MW_DRAIN); }
static void mq_arena_waitw(int why)   /* the walks posted last are done: the arena and its extents are the main CPU's again (gl24: with two
                                         areas, the halves those walks held: the step's own copies stay) */
{
  if (!mq_walk_due) { two_poll(); return; }   /* gl25c */
  mq_wait(mq_walk_seq, why);
  if (!two && xmc.next > mq_ext_max) { mq_ext_max = xmc.next; mq_ext_max_vb = me_vb; }   /* step 4d: for 4d-3's sizes (gl24b: one area only;
                                                                                 two areas: two_post) */
  if (!two && xmc.used > mq_arena_max) { mq_arena_max = xmc.used; mq_arena_max_vb = me_vb; }
  mq_walk_due = 0;
  if (two) two_done(); else { xmc.next = 0; xmc.used = 0; }
}
static void mq_arena_wait(void) { mq_arena_waitw(MW_ARENA); }
static void mq_sum(void)   /* ge/xme.c xme_sum on the main CPU (the take-back) */
{
  const XlStats *S = xl_stats(); uint32_t q = (uint32_t)M.qw, w = (uint32_t)S->words, c = (uint32_t)S->calls, a = (uint32_t)S->ad_kicks;
  mq->t_qw += q - mq->l_qw; mq->l_qw = q; mq->t_words += w - mq->l_words; mq->l_words = w;
  mq->t_calls += c - mq->l_calls; mq->l_calls = c; mq->t_ad += a - mq->l_ad; mq->l_ad = a;
}
static const char *mq_at_name(uint32_t a)   /* xme.h XmeCtx.at */
{
  if (!a) return "between entries";
  if (a == XE_REC) return "a record's copy"; if (a == XE_THIN) return "the thinning"; if (a == XE_FILL) return "a fill";
  if (a == XE_WALK) return "a walk"; if (a == XE_HOOK) return "a hook"; if (a == XE_STOP) return "the end";
  if (a == XE_SYNC) return "a hand-over to the main CPU";
  if (a >= XME_AT_WALK && a < XME_AT_WALK + XME_NREC) return "a walk's record";
  if (a >= XME_AT_LAY && a < XME_AT_LAY + XME_NREC) return "a fill's laying";
  if (a == XME_AT_RESFULL) return "a hook, waiting for room for its list"; if (a == XME_AT_GEWAIT) return "a hook, waiting for the GE";
  if (a == XME_AT_END) return "a hook, the frame's end"; if (a == XME_AT_LIST) return "a hook, the frame's GE list";
  if (a == XME_AT_BEGIN) return "a hook, the frame's start"; if (a == XME_AT_TKFRAME) return "a hook, the texture cache's frame";
  if (a == XME_AT_IDLE) return "idle (the queue empty)"; if (a == XME_AT_HANG) return "the hang test";
  return "unknown";
}
/* step 4d: mode 4's cold take-back: the picture stops for the rest of the run (gl_ok 4), as when the GE stops: the GE's list drawn (a
   whole one: the ME posts a list only after its cache is written back), the harness's text on the screen, the translator and the split
   off (state -2), the run goes on for the hashes */
static void mq_stop_pic(int why, uint32_t left, int busy)
{
  mq_dead = 1; mq_dead_vb = me_vb; XS(mq)->tail = XS(mq)->head;
  if (gl_ge && gl_ok == 1) { HOST_ENTER(HOST_GW); (void)gl_sync(); HOST_LEAVE(HOST_GW); }
  if (gl_ok == 1) {
    gl_ok = 4; gl_now = gl_ge = 0;
    sceDisplaySetFrameBuf((void *)(0x44000000u + GL_TXT), 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME); }
  mq_on = 0; mp_on = 0; state = -2;
  g_g0_on = 0; g_xl_img = g_xl_end = 0; mp_nrec = 0; mp_unfilled = 0; mp_nlog = 0; mq_walk_due = 0; mq_thin_due = 0; thin_due = 0;
  two = 0;   /* (gl24) */
  char lead[160];
  snprintf(lead, sizeof lead, "the heartbeat did not move for %u ms", (unsigned)(mq_tbk_us / 1000));
#ifdef MGS2_ME_REAL
  if (mq_me_tbk_exc) snprintf(lead, sizeof lead, "the ME took an exception (Cause %08x, EPC %08x, BadVAddr %08x)", (unsigned)mq_mx[0],
                              (unsigned)mq_mx[1], (unsigned)mq_mx[2]);
#endif
  fprintf(stderr, "me picture loop: TAKE-BACK at vb %ld (cold, mode 4): %s while the main CPU waited for %s "
          "(the loop %s, at %#x: %s; %u entries not done, %d of them records, fills or walks): the picture STOPPED for the rest of the run "
          "(what the ME was writing may be torn: on the PSP its cache's unwritten lines are lost with it); the run goes on\n", me_vb,
          lead, mq_wname[why], mq_tbk_hung ? "stopped by the hang test" : "not in the hang test",
          (unsigned)mq_tbk_at, mq_at_name(mq_tbk_at), (unsigned)left, busy);
}
static void mq_takeback4(int why, uint32_t left)   /* step 4d: mode 4's take-back, after the ME is held (4d-4) or the thread ended */
{
  int busy = 0; uint32_t n = 0;
  for (uint32_t s = XS(mq)->tail; s != XS(mq)->head; s++) { int k = mq->e[s & (XME_N - 1)].kind; if (k == XE_REC || k == XE_FILL || k == XE_WALK) busy++; }
  mq_cpu_wbinv(); mq->cache = 0; mq->clk = mq_clk_us; mq_heap_own();   /* the main CPU reads what the ME wrote back (its cache dropped first, step 4d-3: the
                                                     lines it writes next are read fresh); its own heap hooks */
  if (mq_tbk_at != XME_AT_HANG || busy || mq->nrec || MQ_EXC()) { mq_tbk_cold++; mq_tbk_rec = mp_nrec; mq_stop_pic(why, left, busy); return; }
  /* warm: the hang test's stop (between entries, the ME's cache written back before it, no record waiting for a copy) */
  mq_tbk_warm++; mq_tbk_rec = 0;
  while (XS(mq)->tail != XS(mq)->head) { uint32_t t = XS(mq)->tail; xme_entry(mq, &mq->e[t & (XME_N - 1)], mq_wait_self); XS(mq)->tail = t + 1; mq_selfrun++; n++; }
  two_start(0);   /* gl24b: every walk done, the main CPU's own walks from RAM from now on: one area (the counts stay the patch's) */
  fprintf(stderr, "me picture loop: TAKE-BACK at vb %ld (warm, mode 4): the heartbeat did not move for %u ms while the main CPU waited for %s; "
          "the loop had stopped at the hang test, its cache written back, no record waiting: %s, the main CPU ran the %u entries "
          "left and makes the picture itself from now on (each transfer walked from RAM as it ends); nothing dropped\n", me_vb,
          (unsigned)(mq_tbk_us / 1000), mq_wname[why], mq_me ? "the ME held in reset" : "the thread ended", (unsigned)n);
}
static void mq_takeback(int why)
{
  uint32_t left = XS(mq)->head - XS(mq)->tail;
  mq_tbk_hung = (int)XS(mq)->hung;
#ifdef MGS2_ME_REAL
  if (mq_me > 0) {   /* step 4d-4: the ME held in reset first (the lines its cache had not written back are lost with it), then the facts */
    uint32_t at, ca, ep, bv; uint64_t t = mq_now(); int ex;
    hle_me_park(); mq_me = -1;
    at = XS(mq)->at;
    ex = hle_me_exception(&ca, &ep, &bv);
    if (ex && !mq_me_exc) { mq_me_exc = 1; mq_mx[0] = ca; mq_mx[1] = ep; mq_mx[2] = bv; mq_mx_vb = me_vb; }
    mq_cpu_wbinv();   /* (what the ME wrote back, read fresh) */
    fprintf(stderr, "me picture loop: the take-back's facts (the ME, held in reset): its control block: state %u, heartbeat %u, Count at its "
            "last wait %08x, boots %u, exit request %u; the loop at %#x (%s), heartbeat %u, idle %u, %d records held (as last written back); "
            "this wait %ld rounds in %llu us (the heartbeat last moved %llu us before), %ld delays failed in all (the first %08x); the "
            "translator's words %+ld in this wait; the ME's exception: ", (unsigned)mq_mcu->h.state, (unsigned)mq_mcu->h.heartbeat,
            (unsigned)mq_mcu->h.count, (unsigned)mq_mcu->h.boots, (unsigned)mq_mcu->h.exit_req, (unsigned)at, mq_at_name(at),
            (unsigned)XS(mq)->heart, (unsigned)XS(mq)->idle, mq->nrec, mq_w_rounds - mq_w_r0, (unsigned long long)(t - mq_w_t0),
            (unsigned long long)(t - mq_w_hb_t), mq_dly_bad, (unsigned)mq_dly_e, (long)(int32_t)((uint32_t)xl_stats()->words - mq_w_words0));
    if (mq_me_exc) fprintf(stderr, "Cause %08x (code %u), EPC %08x, BadVAddr %08x (seen at vb %ld)", (unsigned)mq_mx[0],
                           (unsigned)((mq_mx[0] >> 2) & 31), (unsigned)mq_mx[1], (unsigned)mq_mx[2], mq_mx_vb);
    else fprintf(stderr, "none");
    fprintf(stderr, "; the entries not done (kinds):");
    for (uint32_t q = XS(mq)->tail; q != XS(mq)->head; q++) fprintf(stderr, " %d", mq->e[q & (XME_N - 1)].kind);
    fputc('\n', stderr);
    mq_tbk_at = at; mq_self = 1; mq_tbk_n++; mq_tbk_vb = me_vb; mq_tbk_why = why; mq_tbk_left = (long)left;
    mq_takeback4(why, left); return; }
#endif
  {   /* the facts, before the thread ends: its state, where the loop is, the main CPU's waiting */
    SceKernelThreadInfo ti; int rr; uint32_t at = XS(mq)->at; uint64_t t = mq_now();
    memset(&ti, 0, sizeof ti); ti.size = sizeof ti; rr = sceKernelReferThreadStatus(mq_th, &ti);
    fprintf(stderr, "me picture loop: the take-back's facts: the thread (refer %08x) status %#x, wait type %d (id %08x), priority %#x, the main "
            "thread's %#x; the loop at %#x (%s), heartbeat %u, idle %u, %d records held; this wait %ld rounds in %llu us (the heartbeat last moved "
            "%llu us before), %ld delays failed in all (the first %08x); the translator's words %+ld in this wait; the entries not done (kinds):",
            (unsigned)rr, (unsigned)ti.status, ti.waitType, (unsigned)ti.waitId, (unsigned)ti.currentPriority,
            (unsigned)sceKernelGetThreadCurrentPriority(), (unsigned)at, mq_at_name(at), (unsigned)XS(mq)->heart, (unsigned)XS(mq)->idle, mq->nrec,
            mq_w_rounds - mq_w_r0, (unsigned long long)(t - mq_w_t0), (unsigned long long)(t - mq_w_hb_t), mq_dly_bad, (unsigned)mq_dly_e,
            (long)(int32_t)((uint32_t)xl_stats()->words - mq_w_words0));
    for (uint32_t s = XS(mq)->tail; s != XS(mq)->head; s++) fprintf(stderr, " %d", mq->e[s & (XME_N - 1)].kind);
    if (at >= XME_AT_WALK && at < XME_AT_WALK + XME_NREC) {
      const XmeRec *r = &mq->rec[at - XME_AT_WALK];
      fprintf(stderr, "; the record walked: channel %d, chcr %08x madr %08x qwc %u tadr %08x, %d extents, restart %d, now %d", r->ch,
              (unsigned)r->chcr, (unsigned)r->madr, (unsigned)r->qwc, (unsigned)r->tadr, r->nx, r->restart, r->now); }
    fputc('\n', stderr);
  }
  mq_tbk_at = XS(mq)->at;   /* (the thread does not run while the main CPU does, here: the word holds) */
  sceKernelTerminateDeleteThread(mq_th); mq_th = -1;
  mq_self = 1; mq_tbk_n++; mq_tbk_vb = me_vb; mq_tbk_why = why; mq_tbk_left = (long)left;
  if (mq_m4) { mq_takeback4(why, left); return; }   /* step 4d */
  if (mq_tbk_at == 0 || mq_tbk_at == XME_AT_IDLE || mq_tbk_at == XME_AT_HANG) {
    /* warm: the loop stopped between entries (its tail moves before its place word goes to 0, ge/xme.c), so everything it did is whole:
       the main CPU runs the entries left, in order, and goes on with the picture as it is (nothing dropped) */
    uint32_t n = 0;
    mq_tbk_warm++; mq_tbk_rec = 0;
    while (XS(mq)->tail != XS(mq)->head) { uint32_t t = XS(mq)->tail; xme_entry(mq, &mq->e[t & (XME_N - 1)], mq_wait_self); XS(mq)->tail = t + 1; mq_selfrun++; n++; }
    fprintf(stderr, "me picture loop: TAKE-BACK at vb %ld (warm): the heartbeat did not move for %u ms while the main CPU waited for %s; the "
            "loop had stopped between entries (%s): the thread ended, the main CPU ran the %u entries left and runs the loop's entries itself "
            "from now on; nothing dropped\n", me_vb, (unsigned)(mq_tbk_us / 1000), mq_wname[why], mq_at_name(mq_tbk_at), (unsigned)n);
    return;
  }
  mq_tbk_cold++; mq_tbk_rec = mp_nrec;
  XS(mq)->tail = XS(mq)->head;
  if (gl_ge) mq_ge_finish();                       /* the GE's list drawn and shown; the ME's lists not taken dropped */
  mq_cons = XS(mq)->listed; flip_w = 0;   /* gl27b: a held result dropped too */
  if (gl_ok == 1) XS(mq)->ge_done = XS(mq)->listed; else mq_released_all();
  /* the picture cold, as at a start again (xlive_chk_dma), with a new texture cache (the ME may have stopped inside it) */
  mq_sum(); gu_zero_empty(map, 0); xl_reset(); mq->l_words = mq->l_calls = mq->l_ad = 0;
  tk_free(tk);
  if (!(tk = tk_new(map, (gl_texkb << 10) / 8 * 7)) || tk_pool(tk, gl_pool, gl_texkb << 10)) {
    fprintf(stderr, "me picture loop: TAKE-BACK: no texture cache: no picture\n"); gl_ok = -1; tk = 0; }
  glf.tk = tk; xm_objects(&xmp, map, tk);
  gl_begin(&glf); xmp.now = 0; gl_now = 0; thin_due = 0;   /* the frame dropped (empty, not fed) */
  mq->nrec = 0;
  for (int i = 0; i < mp_nrec; i++) if (mp_rec[i].nx >= 0 && !mp_rec[i].filled) mp_mark(&mp_rec[i], 0);
  mp_nrec = 0; mp_nlog = 0; mp_unfilled = 0; if (!++mp_hgen) { memset(mp_h, 0, (size_t)mp_hn * sizeof *mp_h); mp_hgen = 1; }
  xmc.next = 0; xmc.used = 0; mq_walk_due = 0; mq_thin_due = 0; g_g0_on = 0; two_start(0);   /* (gl24) */
  fprintf(stderr, "me picture loop: TAKE-BACK at vb %ld (cold): the heartbeat did not move for %u ms while the main CPU waited for %s (the loop %s; %u "
          "entries not done, %ld records dropped; the loop in an entry: %s): the thread ended, the picture made cold (the frame and the "
          "dropped records' uploads into the GS memory are missing until the game uploads them again; the GS memory picture and the texture "
          "map stay, the texture cache starts empty), the main CPU runs the loop's entries itself from now on\n", me_vb,
          (unsigned)(mq_tbk_us / 1000), mq_wname[why], mq_tbk_hung ? "stopped by the hang test" : "not in the hang test", (unsigned)left,
          mq_tbk_rec, mq_at_name(mq_tbk_at));
}
/* 4b-2's mp_fill_all: the ME lays the log over the early copies (FILL), the main CPU waits for it, then empties the log and the filter.
   1: a take-back meanwhile (the records dropped) */
/* ---- native step 1 (native_renderer_design.md 6.1; ge/natsnap.h, boot/nat_stub.h, ge/xlme.h XmPic): the main CPU's hand-off.
   The stub (boot.c) writes a picture's ship-mesh groups into the array of its packet buffer.  As a transfer ends, its copy's plan
   (xm_plan) takes the groups its tags reach and keeps their data as extents, and the groups themselves get room in the arena: the
   ME copies them there with the extents (REC) and its walk draws them (merged by key).  A record walked from RAM takes them in place
   and answers how many.  Nothing here is read by the game. ---- */
static int nat_on;                       /* the arrays made (mode 4): arm B's groups are drawn */
static SceUID nat_rsys = -1;   /* gl36 */
static SceUID nat_sys[2] = { -1, -1 }; static uint32_t nat_sys_free[4], nat_sys_n; static int nat_one;   /* nat_one: one array for both buffers */
static uint8_t nat_unf[2];               /* a buffer's groups are in a copy the ME may not have made yet (a record posted without a wait) */
static long nat_n_sync, nat_n_plan, nat_n_plang, nat_n_dirg, nat_n_room; static uint32_t nat_ca_max;
/* native step 2: the people's arrays (boot/np6_stub.h g_np6), beside the ship's and by the same rules: a block each of the system's memory
   outside newlib's heap (NP6_MAXGRP records of 72 B: 72 KB each); a transfer's plan takes the people its tags reach with the ship's, the
   ME copies them after the ship's (ge/xme.h XME_POFF) */
extern NatP6Buf g_np6[2]; extern int g_np6_cfg; extern long g_np6_c[N6_N];
static int np6_on; static SceUID np6_sys[2] = { -1, -1 }; static long np6_n_plang, np6_n_dirg;
static long np6_c0[N6_N], np6_t0n[8];   /* step 2: the people's counts at the turn's start (nat_t0set) */
static int np6_pend(int b, const NatP6 **g)
{
  const NatP6Buf *q = &g_np6[b];
  if (!np6_on || !q->g || !q->draw || q->seq != g_nat_bseq[b] || q->cur >= q->n) return 0;
  *g = q->g + q->cur; return (int)(q->n - q->cur);
}
static int nat_pend(int b, const NatGrp **g)   /* buffer b's groups no transfer has taken: the pipeline call that last started in b wrote them */
{
  const NatBuf *s = &g_nat[b];
  if (!nat_on || !s->g || !s->draw || s->seq != g_nat_bseq[b] || s->cur >= s->n) return 0;
  *g = s->g + s->cur; return (int)(s->n - s->cur);
}
static void nat_direct_pre(MpRec *e)     /* a record walked from RAM: both buffers' waiting groups offered (the walk binds one) */
{
  const NatGrp *g;
  e->nb = -1; e->noff = 0;
  for (int b = 0; b < 2; b++) { e->ngn[b] = (int16_t)(e->ch == 1 && e->lanes ? nat_pend(b, &g) : 0); e->ng0[b] = (int16_t)g_nat[b].cur; e->nru[b] = (uint8_t)g_nat[b].ru; }   /* gl36: nru */
  if (e->ngn[0] | e->ngn[1]) { XS(mq)->nat_b = -1; XS(mq)->nat_used = 0; }
  { const NatP6 *q; for (int b = 0; b < 2; b++) { e->pn[b] = (int16_t)(e->ch == 1 && e->lanes ? np6_pend(b, &q) : 0); e->p0[b] = (int16_t)g_np6[b].cur; } }   /* step 2 */
  if (e->pn[0] | e->pn[1]) { XS(mq)->np6_b = -1; XS(mq)->np6_used = 0; }
}
static void nat_direct_post(const MpRec *e)   /* after the wait for that walk: the groups it took */
{
  int b; uint32_t u;
  if (e->pn[0] | e->pn[1]) {   /* step 2: the people's */
    XME_BARRIER(); b = XS(mq)->np6_b; u = XS(mq)->np6_used;
    if (b >= 0 && b < 2 && u <= (uint32_t)e->pn[b]) { g_np6[b].cur += u; np6_n_dirg += (long)u; } }
  if (!(e->ngn[0] | e->ngn[1])) return;
  XME_BARRIER(); b = XS(mq)->nat_b; u = XS(mq)->nat_used;
  if (b >= 0 && b < 2 && u <= (uint32_t)e->ngn[b]) { g_nat[b].cur += u; nat_n_dirg += (long)u; }
}
static void nat_psp_sync(int b)          /* the stub's (g_nat_sync): buffer b's array is about to be written again */
{
  if (!mq_on || mq_dead || !(nat_unf[b] | (nat_one ? nat_unf[b ^ 1] : 0))) return;   /* (one array for both: either one's copy still owed) */
  nat_n_sync++;
  if (mp_unfilled && !mq_self) (void)mq_fill(3);   /* the ME's copies made first (an early fill.  With an array each none is
                                                       expected: a buffer's transfers end a hook before its next pipeline call.  With
                                                       one array every picture of an ME turn has one: the next pipeline call begins
                                                       before the hook that lays the sent picture's copies [C emulator q3]) */
  nat_unf[0] = nat_unf[1] = 0;
}
int rt_np6_arm_at(long vb);
static void np6_psp_setup(void)          /* native step 2 (nat_psp_setup's start): the people's two arrays, when an arm or MGS2_NGE_CYCLE's digit 2 asks */
{
  const uint32_t one = ((uint32_t)NP6_MAXGRP * (uint32_t)sizeof(NatP6) + 63u) & ~63u; uint8_t *bl[2] = { 0, 0 };
  (void)rt_np6_arm_at(0);   /* (the switches read: g_np6_cfg) */
  if (!g_np6_cfg) return;
  if (nge_np6_bytes()) { fprintf(stderr, "native step 2: the people's arrays come from the carve after the opening (nge_psp_setup)\n"); return; }
  if (!mq_m4) { fprintf(stderr, "native step 2: the people's groups are drawn only by mode 4's loop (MGS2_ME_PIC=4): their calls stay stubbed "
                        "and NOTHING DRAWS them in this run\n"); return; }
  for (int i = 0; i < 2; i++) {
    np6_sys[i] = sceKernelAllocPartitionMemory(2, i ? "np6_groups1" : "np6_groups0", PSP_SMEM_Low, one, 0);
    if (np6_sys[i] >= 0) bl[i] = (uint8_t *)sceKernelGetBlockHeadAddr(np6_sys[i]);
    if (bl[i] && ((uintptr_t)bl[i] & 63)) bl[i] = 0; }
  if (bl[0] && bl[1]) { g_np6[0].g = (NatP6 *)(void *)bl[0]; g_np6[1].g = (NatP6 *)(void *)bl[1]; mq->np6_g[0] = g_np6[0].g; mq->np6_g[1] = g_np6[1].g; np6_on = 1;
    g_nat_sync = nat_psp_sync; }   /* (the stub's, before a people array is written again: as the ship's) */
  fprintf(stderr, "native step 2: the people's arrays: %d groups of %u B a packet buffer, 2 x %u KB of the system's memory outside newlib's heap: %s "
          "(%08x, %08x); largest free block now %u KB, total %u KB\n", NP6_MAXGRP, (unsigned)sizeof(NatP6), (unsigned)(one >> 10),
          np6_on ? "made" : "NOT MADE: the people's calls in arm 3 are stubbed with no record kept (NOTHING DRAWS them)",
          (unsigned)(uintptr_t)bl[0], (unsigned)(uintptr_t)bl[1], (unsigned)(sceKernelMaxFreeMemSize() >> 10), (unsigned)(sceKernelTotalFreeMemSize() >> 10));
}
static void nat_psp_setup(void)          /* mq_setup's end */
{
  const char *e; uint32_t one = ((uint32_t)NAT_MAXGRP * (uint32_t)sizeof(NatGrp) + 63u) & ~63u;
  (void)rt_nat_arm_at(0, 0);   /* (the schedule read, its line printed) */
  (void)rt_bound_at(0, 0);     /* gl38: the bound cycle and F1's switches read, their lines printed */
  p9_setup();                  /* gl39 */
  p10_setup();                 /* gl40 */
  gl_bd_near = (e = mgs2_getenv("MGS2_GL_RAIN_NEAR")) ? atoi(e) : 0;   /* gl38 (ge/gelive.c gl_bd_near_call), before the ME starts */
  if (g_bd_cfg || g_bd_rain) fprintf(stderr, "bound: MGS2_GL_RAIN_NEAR %d (the drop sheets' calls a frame without the rain keeps: the last n of its sheet calls, the nearest; 0 all of them)\n", gl_bd_near);
  mq->nat_t_on = !((e = mgs2_getenv("MGS2_NAT_T")) && !atoi(e));
  mq->nh_chk = (e = mgs2_getenv("MGS2_NAT_HCHK")) ? atoi(e) : 0;   /* gl34 (ge/xlme.c lw_nptr_h): a check for the emulator's runs */
  mq->ns_on = mq->nat_t_on && (e = mgs2_getenv("MGS2_NAT_SPLIT")) && atoi(e) > 0;   /* gl34 (ge/natsplit.h), before the ME starts */
  fprintf(stderr, "nat split (gl34, MGS2_NAT_SPLIT; the lines 'nat split' and 'me chains' of each ME turn): %s; a group in about %d timed by stage\n",
          mq->ns_on ? "on" : "off", NS_EVERY);
  np6_psp_setup();   /* native step 2: the people's arrays */
  if (!g_nat_cfg) { fprintf(stderr, "native step 1: off (no arm in MGS2_NAT or MGS2_NAT_CYCLE): the builder 0x1214a0 runs as always\n"); return; }
  if (!mq_m4) { fprintf(stderr, "native step 1: the groups are drawn only by mode 4's loop (MGS2_ME_PIC=4): arm 3 stubs the builder and "
                        "NOTHING DRAWS its groups in this run\n"); return; }
  nat_sys_free[0] = (uint32_t)sceKernelMaxFreeMemSize(); nat_sys_free[1] = (uint32_t)sceKernelTotalFreeMemSize();
  { uint8_t *bl[2] = { 0, 0 };   /* a block each (two small blocks fit where one of twice the size may not); one only: both buffers share it */
    for (int i = 0; i < 2; i++) {
      if (i && (e = mgs2_getenv("MGS2_NAT_ONE")) && atoi(e)) break;   /* (a test: one array as if the second block were refused) */
      nat_sys[i] = sceKernelAllocPartitionMemory(2, i ? "nat_groups1" : "nat_groups0", PSP_SMEM_Low, one, 0);
      if (nat_sys[i] >= 0) bl[i] = (uint8_t *)sceKernelGetBlockHeadAddr(nat_sys[i]);
      if (bl[i] && ((uintptr_t)bl[i] & 63)) { fprintf(stderr, "native step 1: block %d is not on a 64-byte line (%08x): not used\n", i, (unsigned)(uintptr_t)bl[i]); bl[i] = 0; }
      if (bl[i]) nat_sys_n += one; }
    if (!bl[0]) { bl[0] = bl[1]; bl[1] = 0; }
    if (bl[0]) { nat_one = !bl[1]; g_nat[0].g = (NatGrp *)(void *)bl[0]; g_nat[1].g = (NatGrp *)(void *)(bl[1] ? bl[1] : bl[0]);
      mq->nat_g[0] = g_nat[0].g; mq->nat_g[1] = g_nat[1].g; g_nat_sync = nat_psp_sync; nat_on = 1; } }
  nat_sys_free[2] = (uint32_t)sceKernelMaxFreeMemSize(); nat_sys_free[3] = (uint32_t)sceKernelTotalFreeMemSize();
  fprintf(stderr, "native step 1: the groups' arrays: %d groups of %u B a packet buffer, %u KB of the system's memory outside newlib's heap: %s "
          "(%08x, %08x); that memory before: largest block %u KB, total %u KB; after: %u KB, %u KB; the merge runs timed on the ME "
          "(MGS2_NAT_T): %s\n", NAT_MAXGRP, (unsigned)sizeof(NatGrp), (unsigned)(nat_sys_n >> 10),
          !nat_on ? "NOT MADE: arm 3's calls stay stubbed (the run's state as planned) and NOTHING DRAWS their groups" :
          nat_one ? "ONE ARRAY ONLY, shared by both buffers (the pictures are the same; each ME-turn picture then has an early fill: arm "
                    "3's hold is not the design's)" : "made, an array each",
          (unsigned)(uintptr_t)g_nat[0].g, (unsigned)(uintptr_t)g_nat[1].g,
          (unsigned)(nat_sys_free[0] >> 10), (unsigned)(nat_sys_free[1] >> 10), (unsigned)(nat_sys_free[2] >> 10),
          (unsigned)(nat_sys_free[3] >> 10), mq->nat_t_on ? "yes" : "no");
  if (nat_on && g_nat_rcfg) {   /* gl36: the reuse's table (ge/xlate.h NrEnt), a block of the system's memory outside newlib's heap */
    static const int cap_try[4] = { NAT_MAXGRP, 1536, 1024, 512 }; uint8_t *bl = 0; int cap = 0;
    uint32_t f0 = (uint32_t)sceKernelMaxFreeMemSize(), f1 = (uint32_t)sceKernelTotalFreeMemSize();
    for (int k = 0; k < 4 && !bl; k++) {
      uint32_t nb = (uint32_t)XL_NR_BYTES(cap_try[k]);   /* gl36 fix B: + the last writers (ge/xlate.h XL_NR_LW_N) */
      nat_rsys = sceKernelAllocPartitionMemory(2, "nat_reuse", PSP_SMEM_Low, nb, 0);
      if (nat_rsys >= 0) { bl = (uint8_t *)sceKernelGetBlockHeadAddr(nat_rsys); cap = cap_try[k];
        if (bl && ((uintptr_t)bl & 63)) { bl = 0; cap = 0; break; } } }   /* (not on a 64-byte line: not used; the system's blocks are) */
    if (bl) { memset(bl, 0, XL_NR_BYTES(cap)); sceKernelDcacheWritebackAll(); xl_nr_table((NrEnt *)(void *)bl, cap, g_nat_rchk); }
    fprintf(stderr, "nat reuse (gl36): the table: %s, %d entries of %u B = %u KB and the last writers' 2 KB (%08x); the system's memory before: largest block %u KB, total %u KB; "
            "after: %u KB, %u KB; check mode (MGS2_NAT_RCHK) %d\n", bl ? "made" : "NOT MADE: nothing is reused", cap, (unsigned)sizeof(NrEnt),
            (unsigned)((uint32_t)cap * (uint32_t)sizeof(NrEnt) >> 10), (unsigned)(uintptr_t)bl, (unsigned)(f0 >> 10), (unsigned)(f1 >> 10),
            (unsigned)((uint32_t)sceKernelMaxFreeMemSize() >> 10), (unsigned)((uint32_t)sceKernelTotalFreeMemSize() >> 10), g_nat_rchk);
  }
}
/* a line per turn (ME turn: mq_tend; main CPU turn: mq_tstart), the counts since the turn before */
static long nat_c0[NC_N], nat_t0n[16]; static uint64_t nat_t0t, nat_w0[2];
static uint32_t ns_cha[NS_CH]; static int ns_chn; static long ns_ch_lists, ns_ch_unl, ns_ch_in;
static NatSplit ns_s0; static long ns_fst0, ns_nf0[8], ns_km0[4], ns_l0[3], ns_nh0[3]; static uint64_t ns_rec0, ns_wk0, ns_b0, ns_q0, ns_nb0;   /* gl34: the counts at the turn's start */
static const char *const ns_name[NS_PN] = { "walk and pointers", "register", "A+D packet", "call's rest", "host call and GS state", "transform", "positions",
  "cut test", "attributes", "texture", "strips", "GE transform and batch data", "depth range", "into the frame", "way back", "drawn areas", "walk's end",
  /* gl38, the people's own stages */ "to the hold: test, GS state and texture", "the call kept", "passes' look-ups", "header, bones and lights", "vertices",
  "strips, GE data and passes' records", "into the frame", "passes into the frame" };
static uint64_t tm_h0[XME_H_N], tm_gx0[GX_T_N], tm_r0[XME_R_N], tm_l0; static long tm_n0[4], tm_rec0, tm_hk0, tm_ls0, tm_gl0, tm_gc0, f1_n0[12];   /* gl38: the timers' and F1's counts at the turn's start */
static void nat_t0set(void)
{
  for (int k = 0; k < NC_N; k++) nat_c0[k] = g_nat_c[k];
  g_nat_c[NC_GRP_MAX] = 0;   /* (the turn's own most) */
  nat_t0n[0] = nat_n_plan; nat_t0n[1] = nat_n_plang; nat_t0n[2] = nat_n_dirg; nat_t0n[3] = nat_n_room; nat_t0n[4] = nat_n_sync;
  nat_t0n[5] = mq->n_natrec; nat_t0n[6] = mq->n_natg; nat_t0n[7] = xmp.nat_runs; nat_t0n[8] = xmp.nat_core; nat_t0n[9] = xmp.nat_fed;
  nat_t0n[10] = xmp.nat_bad; nat_t0n[11] = xmp.nat_late; nat_t0n[12] = xmp.nat_stop; nat_t0n[13] = xmp.nat_in; nat_t0n[14] = xmp.nat_out;
  nat_t0n[15] = xmp.nat_part; nat_t0t = xmp.nat_t; nat_w0[0] = mq_wus[MW_FILL]; nat_w0[1] = mq_wus[MW_NOW];
  nat_ca_max = 0; nat_fa[0] = nat_fa[1] = nat_fa[2] = 0;
  for (int k = 0; k < N6_N; k++) np6_c0[k] = g_np6_c[k];   /* step 2 */
  g_np6_c[N6_GRP_MAX] = 0;
  np6_t0n[0] = np6_n_plang; np6_t0n[1] = np6_n_dirg; np6_t0n[2] = mq->n_np6g; np6_t0n[3] = xmp.np6_runs; np6_t0n[4] = xmp.np6_core;
  np6_t0n[5] = xmp.np6_fed; np6_t0n[6] = xmp.np6_bad; np6_t0n[7] = xmp.np6_late;
  nat_stc0 = g_nat_stc;   /* gl33 */
  nk_kpt0 = g_nat_kpt; for (int k = 0; k < 9; k++) nk_n0[k] = xl_nk_n[k];   /* gl35 */
  for (int k = 0; k < 20; k++) nr_n0[k] = xl_nr_n[k];   /* gl36 */
  for (int k = 0; k < 8; k++) nr_r0[k] = xl_nr_r[k];   /* gl37 */
  for (int k = 0; k < XME_H_N; k++) tm_h0[k] = mq->t_h[k];   /* gl38 */
  for (int k = 0; k < GX_T_N; k++) tm_gx0[k] = mq->t_gx[k];
  for (int k = 0; k < XME_R_N; k++) tm_r0[k] = mq->t_r[k];
  for (int k = 0; k < 4; k++) tm_n0[k] = mq->n_gx[k];
  tm_l0 = mq->t_k[XME_T_LIST]; tm_rec0 = mq->n_rec; tm_hk0 = mq->n_hook; tm_ls0 = mq->n_list; tm_gl0 = mq->n_gxl; tm_gc0 = mq->n_gxc;
  for (int k = 0; k < 12; k++) f1_n0[k] = xl_f1_n[k];
  ns_s0 = g_ns; ns_fst0 = g_nat_fst; for (int k = 0; k < 8; k++) ns_nf0[k] = xl_nf_n[k]; for (int k = 0; k < 4; k++) ns_km0[k] = vc_km_n[k];   /* gl34 */
  for (int k = 0; k < 3; k++) ns_nh0[k] = lw_nh_n[k];
  ns_wk0 = mq->t_k[XE_WALK]; ns_rec0 = mq->t_k[XE_REC]; ns_b0 = xmc.bytes; ns_q0 = xmc.nat_qw; ns_nb0 = mq->n_natb; ns_l0[0] = ns_ch_lists; ns_l0[1] = ns_ch_unl; ns_l0[2] = ns_ch_in;
}
static void nat_tline(const char *who, long vb0, long vb, int me)
{
  long turn = -1, pics; int arm;
  if (!g_nat_cfg) return;
  arm = rt_nat_arm_at(vb0 + 1, &turn); pics = g_nat_c[NC_PICS] - nat_c0[NC_PICS];
  if (turn < 0 && !arm && !pics && g_nat_c[NC_STUBC] == nat_c0[NC_STUBC]) return;   /* before the cycle, the builder's arm: no line */
  fprintf(stderr, "nat turn vb %ld-%ld %s arm %d (%s; cycle turn %ld): calls recorded %ld, groups %ld, pictures with records %ld; snapshot bytes a "
          "picture: mean %ld, most %ld; stubbed with no record %ld; calls with no group %ld; the builder run instead (dropped records) %ld: "
          "array full %ld, key order %ld, near the buffer's end %ld, other %ld; recorded with no array %ld; groups left untaken %ld; "
          "transfers planned with groups %ld (%ld groups), groups taken by walks from RAM %ld, plans undone for the groups' room %ld, early "
          "fills for an array's reuse %ld; ",
          vb0, vb, who, arm, arm == 3 ? "native" : arm == 4 ? "stubbed, not drawn" : arm ? "?" : "the builder", turn,
          g_nat_c[NC_REC] - nat_c0[NC_REC], g_nat_c[NC_GRP] - nat_c0[NC_GRP], pics,
          pics ? (long)sizeof(NatGrp) * (g_nat_c[NC_GRP] - nat_c0[NC_GRP]) / pics : 0, (long)sizeof(NatGrp) * g_nat_c[NC_GRP_MAX],
          g_nat_c[NC_STUBC] - nat_c0[NC_STUBC], g_nat_c[NC_EMPTY] - nat_c0[NC_EMPTY],
          g_nat_c[NC_FB_FULL] - nat_c0[NC_FB_FULL] + g_nat_c[NC_FB_ORDER] - nat_c0[NC_FB_ORDER] + g_nat_c[NC_FB_END] - nat_c0[NC_FB_END] +
          g_nat_c[NC_FB_OTHER] - nat_c0[NC_FB_OTHER], g_nat_c[NC_FB_FULL] - nat_c0[NC_FB_FULL], g_nat_c[NC_FB_ORDER] - nat_c0[NC_FB_ORDER],
          g_nat_c[NC_FB_END] - nat_c0[NC_FB_END], g_nat_c[NC_FB_OTHER] - nat_c0[NC_FB_OTHER], g_nat_c[NC_NOMEM] - nat_c0[NC_NOMEM],
          g_nat_c[NC_LEFT] - nat_c0[NC_LEFT], nat_n_plan - nat_t0n[0], nat_n_plang - nat_t0n[1], nat_n_dirg - nat_t0n[2],
          nat_n_room - nat_t0n[3], nat_n_sync - nat_t0n[4]);
  fprintf(stderr, "the picture's side: records' groups copied %ld (%ld groups), merge runs %ld, groups to the core %ld, fed word by word %ld, "
          "dropped %ld, drawn at a walk's end %ld, run lanes stopped at a key %ld, the groups' data in the copy %ld, read from RAM %ld, partly "
          "(dropped) %ld; native time %.2f ms (%s); hold: the main CPU waited for fills %.1f ms, for copies as they ended %.1f ms; frame "
          "arena, most: %ld of %d batches, %ld of %d vertices, %ld KB of %u KB data; copy arena, most in a step: %u KB of %u KB\n",
          mq->n_natrec - nat_t0n[5], mq->n_natg - nat_t0n[6], xmp.nat_runs - nat_t0n[7], xmp.nat_core - nat_t0n[8], xmp.nat_fed - nat_t0n[9],
          xmp.nat_bad - nat_t0n[10], xmp.nat_late - nat_t0n[11], xmp.nat_stop - nat_t0n[12], xmp.nat_in - nat_t0n[13],
          xmp.nat_out - nat_t0n[14], xmp.nat_part - nat_t0n[15], me ? (double)(xmp.nat_t - nat_t0t) / mq_tdiv : 0.0,
          !me ? "not timed in the main CPU's turns" : mq->nat_t_on ? "on the ME; the emulator: its loop thread" : "MGS2_NAT_T=0: not timed",
          (double)(mq_wus[MW_FILL] - nat_w0[0]) / 1000.0, (double)(mq_wus[MW_NOW] - nat_w0[1]) / 1000.0, (long)nat_fa[0], GL_PSP_NB,
          (long)nat_fa[1], GL_PSP_NV, (long)(nat_fa[2] >> 10), (unsigned)(GL_PSP_ARENA >> 10), (unsigned)(nat_ca_max >> 10),
          (unsigned)((two ? two_ah : two_an ? two_an : xmc.arena_n) >> 10));
  if (g_np6_cfg)   /* step 2: the people's records (boot/np6_stub.h) and their side of the picture */
    fprintf(stderr, "np6 turn vb %ld-%ld %s arm %d: calls recorded %ld, groups %ld (most in a picture %ld), pictures %ld; stubbed with no record "
            "%ld; calls with no group %ld; the builder run instead: array full %ld, key order %ld, near the buffer's end %ld, other %ld (the game's "
            "memo behind the stub's %ld); recorded with no array %ld; groups left untaken %ld; groups planned with transfers %ld, taken by walks "
            "from RAM %ld; the picture's side: groups copied %ld, runs %ld, groups to the core %ld, fed word by word %ld, dropped %ld, drawn at a "
            "walk's end %ld; calls in arm 3 that ran the builder %ld (the growth late %ld)\n", vb0, vb, who, rt_np6_arm_at(vb0 + 1), g_np6_c[N6_REC] - np6_c0[N6_REC], g_np6_c[N6_GRP] - np6_c0[N6_GRP],
            g_np6_c[N6_GRP_MAX], g_np6_c[N6_PICS] - np6_c0[N6_PICS], g_np6_c[N6_STUBC] - np6_c0[N6_STUBC], g_np6_c[N6_EMPTY] - np6_c0[N6_EMPTY],
            g_np6_c[N6_FB_FULL] - np6_c0[N6_FB_FULL], g_np6_c[N6_FB_ORDER] - np6_c0[N6_FB_ORDER], g_np6_c[N6_FB_END] - np6_c0[N6_FB_END],
            g_np6_c[N6_FB_OTHER] - np6_c0[N6_FB_OTHER], g_np6_c[N6_FB_MEMO] - np6_c0[N6_FB_MEMO], g_np6_c[N6_NOMEM] - np6_c0[N6_NOMEM],
            g_np6_c[N6_LEFT] - np6_c0[N6_LEFT], np6_n_plang - np6_t0n[0], np6_n_dirg - np6_t0n[1], mq->n_np6g - np6_t0n[2],
            xmp.np6_runs - np6_t0n[3], xmp.np6_core - np6_t0n[4], xmp.np6_fed - np6_t0n[5], xmp.np6_bad - np6_t0n[6], xmp.np6_late - np6_t0n[7],
            g_np6_c[N6_NOARR] - np6_c0[N6_NOARR], g_np6_c[N6_LATE] - np6_c0[N6_LATE]);
  fprintf(stderr, "nat inpl vb %ld-%ld %s arm %d inpl %d (gl33; 1: a static group's three streams stay in RAM): groups flagged static %ld\n",
          vb0, vb, who, arm, rt_nat_inpl_at(vb0 + 1), g_nat_stc - nat_stc0);
  /* gl34: the fast front end's counts (the picture's side counts the groups it drew in the turn; the flags are the recorder's) */
  fprintf(stderr, "nat fast vb %ld-%ld %s arm %d fast %d (gl34; 1: arm 3's groups go to the fast front end): groups flagged %ld; the picture's side: "
          "taken %ld, declined: state %ld, entry %ld, buffers %ld, stream counts %ld, material tag %ld, tables full %ld, path 1 busy %ld; "
          "transforms taken from the last keyed call %ld, computed and kept %ld; pointers by their own hints %ld (MGS2_NAT_HCHK %d: checked %ld, "
          "differing %ld)\n", vb0, vb, who, arm, rt_nat_fast_at(vb0 + 1), g_nat_fst - ns_fst0,
          xl_nf_n[0] - ns_nf0[0], xl_nf_n[1] - ns_nf0[1], xl_nf_n[2] - ns_nf0[2], xl_nf_n[3] - ns_nf0[3], xl_nf_n[4] - ns_nf0[4],
          xl_nf_n[5] - ns_nf0[5], xl_nf_n[6] - ns_nf0[6], xl_nf_n[7] - ns_nf0[7], vc_km_n[0] - ns_km0[0], vc_km_n[1] - ns_km0[1],
          lw_nh_n[0] - ns_nh0[0], mq->nh_chk, lw_nh_n[2] - ns_nh0[2], lw_nh_n[1] - ns_nh0[1]);
  /* gl35: the keeps' counts (the picture's side; the flags are the recorder's) */
  fprintf(stderr, "nat keep vb %ld-%ld %s arm %d keep %d (gl35; bits: 1 drawn areas, 2 A+D packet, 4 / 8 texture): groups flagged %ld; the picture's "
          "side: drawn areas replayed %ld, checked %ld, differing %ld; textures replayed %ld, checked %ld, differing %ld, records %ld; CLUT calls not "
          "made %ld, PRIM writes in line %ld (MGS2_NAT_KCHK %d)\n", vb0, vb, who, arm, rt_nat_keep_at(vb0 + 1), g_nat_kpt - nk_kpt0,
          xl_nk_n[0] - nk_n0[0], xl_nk_n[1] - nk_n0[1], xl_nk_n[2] - nk_n0[2], xl_nk_n[3] - nk_n0[3], xl_nk_n[4] - nk_n0[4], xl_nk_n[5] - nk_n0[5],
          xl_nk_n[6] - nk_n0[6], xl_nk_n[7] - nk_n0[7], xl_nk_n[8] - nk_n0[8], xl_keep_chk);
  if (xl_nr_cap) { long d[20]; for (int k = 0; k < 20; k++) d[k] = xl_nr_n[k] - nr_n0[k];   /* gl36: the reuse's counts (the picture's side) */
    fprintf(stderr, "nat reuse vb %ld-%ld %s arm %d reuse %d (gl36; 1: a fast run that matches its kept entries is replayed): runs replayed %ld (groups %ld, "
            "skipping %ld, in S %ld), recorded %ld (groups %ld; kept %ld, not kept %ld), checked %ld (groups %ld; S differing %ld, boxes differing %ld); boxes "
            "recorded %ld, kept boxes taken %ld, cut %ld, positions then needed %ld; no table room %ld; replays with a group not taken %ld, with a decline %ld "
            "(MGS2_NAT_RCHK %d)\n", vb0, vb, who, arm, rt_nat_reuse_at(vb0 + 1), d[0], d[1], d[18], d[2], d[3], d[4], d[5], d[13], d[6], d[7], d[8], d[9],
            d[16], d[10], d[11], d[12], d[14], d[15], d[17], g_nat_rchk);
    long q[8]; for (int k = 0; k < 8; k++) q[k] = xl_nr_r[k] - nr_r0[k];   /* gl37 */
    fprintf(stderr, "nat reuse rule vb %ld-%ld %s arm %d reuse %d rule %d (gl37; 1: a run that does not match is recorded only while the last two pictures "
            "replayed well): groups tried %ld, replayed or the same as the picture before %ld; runs not recorded %ld (groups %ld), the same as their shadow %ld "
            "(groups %ld); pictures recording %ld, not recording %ld\n", vb0, vb, who, arm, rt_nat_reuse_at(vb0 + 1), xl_nr_rule, q[6], q[7], q[0], q[1], q[2],
            q[3], q[4], q[5]); }
  if (g_nat_f1cfg || g_nat_f1chk) { long d[12]; for (int k = 0; k < 12; k++) d[k] = xl_f1_n[k] - f1_n0[k];   /* gl38: F1's counts (the picture's side) */
    fprintf(stderr, "nat f1 vb %ld-%ld %s arm %d F1 %d (gl38; 1: in a fast run that is not replayed, a group whose VU writes a later group of the run "
            "overwrites does not write them): runs planned %ld (groups %ld, of them not written %ld, without the fast form %ld), runs not planned %ld (state %ld, a group without an "
            "MSCAL %ld), planned runs with a problem %ld; check mode: groups compared %ld, DIFFERING %ld, written by the plan and not by the notes %ld, "
            "skipped by the plan ahead of a group that declined when it ran %ld (MGS2_NAT_F1CHK %d)\n", vb0, vb, who, arm, rt_nat_f1_at(vb0), d[0], d[1], d[2],
            d[10], d[3], d[8], d[9], d[4], d[5], d[6], d[7], d[11], g_nat_f1chk); }
  if (me) {   /* gl38: the hooks', lists' and records' parts (ge/xme.h XME_H_*, XME_R_*; ge/gelist.h GX_T_*), the ME's own time in ms */
    double h[XME_H_N], g[GX_T_N], r[XME_R_N], gs = 0, d = (double)mq_tdiv, l = (double)(mq->t_k[XME_T_LIST] - tm_l0) / d;
    for (int k = 0; k < XME_H_N; k++) h[k] = (double)(mq->t_h[k] - tm_h0[k]) / d;
    for (int k = 0; k < GX_T_N; k++) { g[k] = (double)(mq->t_gx[k] - tm_gx0[k]) / d; gs += g[k]; }
    for (int k = 0; k < XME_R_N; k++) r[k] = (double)(mq->t_r[k] - tm_r0[k]) / d;
    fprintf(stderr, "me turn vb %ld-%ld gl38 timers (ms, the ME's own; each part holds one clock read a hook or a record): hooks %ld, lists %ld: frame ends "
            "%.2f, frame ends and lists %.2f, results and cache out %.2f, frame starts %.2f, texture cache's frame %.2f; records %ld: out of the cache %.2f, "
            "extents copied %.2f, native groups copied %.2f; the list build by part, in %ld of the lists (one in %d; a clock read at each change of the "
            "batches' kind, %ld of them; ms in those lists): depth %.2f, set-up and clear %.2f, GS primitives' batches %.2f (%ld), ship batches %.2f (%ld), "
            "effects' batches %.2f (%ld), people's batches %.2f (%ld), list's end %.2f, digests %.2f, sum %.2f\n", vb0, vb,
            mq->n_hook - tm_hk0, mq->n_list - tm_ls0, h[XME_H_END], l, h[XME_H_RES], h[XME_H_BEGIN], h[XME_H_TKF], mq->n_rec - tm_rec0, r[XME_R_INV],
            r[XME_R_COPY], r[XME_R_NAT], mq->n_gxl - tm_gl0, XME_GX_EVERY, mq->n_gxc - tm_gc0, g[GX_T_DEPTH], g[GX_T_SETUP], g[GX_T_KIND],
            mq->n_gx[0] - tm_n0[0], g[GX_T_KIND + 1], mq->n_gx[1] - tm_n0[1], g[GX_T_KIND + 2], mq->n_gx[2] - tm_n0[2], g[GX_T_KIND + 3],
            mq->n_gx[3] - tm_n0[3], g[GX_T_TAIL], g[GX_T_DIG], gs); }
  if (me && mq->ns_on) {   /* gl38 (ge/natsplit.h): a people group (lane 2, P06) by stage, at the walk and at its flush */
    const NatSplit *s = &g_ns; long pn = s->pn - ns_s0.pn, hn = s->phn - ns_s0.phn; double us = (double)mq_tdiv / 1000.0, sw = 0, sf = 0;
    fprintf(stderr, "people split vb %ld-%ld me turn (gl38; ticks a ms %u): lane 2's groups in the timed walks %ld, of them timed at the walk %ld, at their "
            "flush %ld; us a timed group by stage (each holds one read); at the walk:", vb0, vb, (unsigned)mq_tdiv, s->pseen - ns_s0.pseen, pn, hn);
    for (int k = 0; k < NS_PN; k++) { double v; if (k >= NS_P_C2) continue;
      v = pn ? (double)(s->t[NS_PN + k] - ns_s0.t[NS_PN + k]) / us / (double)pn : 0.0; if (v == 0.0) continue;
      sw += v; fprintf(stderr, " %s %.2f,", ns_name[k], v); }
    fprintf(stderr, " sum %.2f; at the flush:", sw);
    for (int k = NS_P_C2; k < NS_PN; k++) { double v = hn ? (double)(s->t[NS_PN + k] - ns_s0.t[NS_PN + k]) / us / (double)hn : 0.0;
      sf += v; fprintf(stderr, " %s %.2f,", ns_name[k], v); }
    fprintf(stderr, " sum %.2f\n", sf); }
  if (me && mq->ns_on) {   /* gl34 (ge/natsplit.h): the ME's time inside the native path, and the walks' time by chain */
    const NatSplit *s = &g_ns; long n = s->n - ns_s0.n, seen = s->seen - ns_s0.seen, cn = s->cal_n - ns_s0.cal_n; double us = (double)mq_tdiv / 1000.0, sum = 0;
    double nat = (double)(xmp.nat_t - nat_t0t) / mq_tdiv;
    fprintf(stderr, "nat split vb %ld-%ld me turn arm %d fast %d (gl34; ticks a ms %u): groups in the timed merge runs %ld, of them timed by stage %ld "
            "(vertices %ld, left out by the cut %ld); clock read pairs %ld, mean %.3f us a read; us a timed group by stage (each holds one read):",
            vb0, vb, arm, rt_nat_fast_at(vb0 + 1), (unsigned)mq_tdiv, seen, n, s->nv - ns_s0.nv, s->ncut - ns_s0.ncut, cn,
            cn ? (double)(s->cal_t - ns_s0.cal_t) / us / (double)cn : 0.0);
    for (int k = 0; k < NS_N; k++) { double v = n ? (double)(s->t[k] - ns_s0.t[k]) / us / (double)n : 0.0; sum += v; fprintf(stderr, " %s %.2f%s", ns_name[k], v, k + 1 < NS_N ? "," : ";"); }
    fprintf(stderr, " sum %.2f; the turn's native time %.2f ms = %.2f us a group; hand-off: the ME's records %.1f ms, the plans' bytes %lu KB, of them asked "
            "for by the groups' data %lu KB, the groups' array %lu KB\n", sum, nat, seen ? nat * 1000.0 / (double)seen : 0.0,
            (double)(mq->t_k[XE_REC] - ns_rec0) / mq_tdiv, (unsigned long)((xmc.bytes - ns_b0) >> 10), (unsigned long)((xmc.nat_qw - ns_q0) >> 6),
            (unsigned long)((mq->n_natb - ns_nb0) >> 10));
    fprintf(stderr, "me chains vb %ld-%ld me turn arm %d fast %d (gl34; a chain is a transfer of the frame's list): lists read %ld, transfers in a list "
            "%ld, not in one %ld; chain: transfers, ms, converter calls, native ms (groups):", vb0, vb, arm, rt_nat_fast_at(vb0 + 1),
            ns_ch_lists - ns_l0[0], ns_ch_in - ns_l0[2], ns_ch_unl - ns_l0[1]);
    for (int k = 0; k < NS_CH_N; k++) { long w = s->ch_n[k] - ns_s0.ch_n[k];
      if (!w && k < NS_CH) continue;
      if (k < NS_CH) fprintf(stderr, " %d:", k); else fprintf(stderr, k == NS_CH_FLUSH ? " the transfers' ends (held calls):" : " other transfers:");
      fprintf(stderr, " %ld, %.2f, %ld, %.2f (%ld);", w, (double)(s->ch_t[k] - ns_s0.ch_t[k]) / mq_tdiv, s->ch_calls[k] - ns_s0.ch_calls[k],
              (double)(s->ch_nat[k] - ns_s0.ch_nat[k]) / mq_tdiv, s->ch_grp[k] - ns_s0.ch_grp[k]); }
    fprintf(stderr, " the ME's walks %.1f ms\n", (double)(mq->t_k[XE_WALK] - ns_wk0) / mq_tdiv);
  }
}
static int mq_fill(int why)
{
  long tb = mq_tbk_cold; int n = 0; XmeE e;
  memset(&e, 0, sizeof e); e.kind = XE_FILL; e.why = why; e.nlog = mp_nlog;
  mq_wait(mq_post(&e), MW_FILL);
  if (mq_tbk_cold != tb) return 1;
  for (int i = 0; i < mp_nrec && mp_unfilled; i++) {
    MpRec *r = &mp_rec[i];
    if (r->nx < 0 || r->filled) continue;
    r->filled = 1; n++; mp_mark(r, 0); }
  if (why) mp_fill_early[why - 1] += n; else mp_fill_dl += n;
  if (mp_nlog > mp_nlog_max) mp_nlog_max = mp_nlog;
  mp_nlog = 0; mp_unfilled = 0;
  nat_unf[0] = nat_unf[1] = 0;   /* native step 1: every record before this fill has its groups' copy */
  if (!++mp_hgen) { memset(mp_h, 0, (size_t)mp_hn * sizeof *mp_h); mp_hgen = 1; }
  g_g0_on = g_xl_end;   /* (a record in its walk: a write there is counted) */
  return 0;
}
/* 4b-2's mp_late: the waiting copies laid (FILL), then the records walked by the ME (WALK); the arena is the ME's until that walk is
   done (wait: the main CPU waits for it now) */
static void mq_late(int why, int wait)
{
  XmeE e;
  if (mp_unfilled && mq_fill(why)) return;
  memset(&e, 0, sizeof e); e.kind = XE_WALK;
  mq_walk_seq = mq_post(&e); mq_walk_due = 1;
  if (two) two_post();   /* gl24 */
  mp_nrec = 0; mp_late_runs++; mp_chn = 0;
  if (wait) mq_arena_wait();
}
/* gl34 (ge/natsplit.h): the transfer's place in its frame's chain list.  The list is read at the transfer whose tadr is the list's
   first start (the frame's first chain: the list is still the frame's then) and kept for the frame's other transfers */
static int ns_chain_at(const uint8_t *mem, int ch, uint32_t tadr)   /* gl38: at the transfer's start (xlive_chk_dma), with the bound turns or the split on */
{
  uint32_t a, lp, n;
  if (ch != 1 || (tadr & 0x80000000u) || !mem) return -1;
  a = tadr & (XL_GSZ - 1) & ~15u;
  lp = *(const uint32_t *)(mem + 0x1b9348) & (XL_GSZ - 1) & ~3u;
  if (lp && lp + 12 + 16 * NS_CH + 16 <= XL_GSZ) {
    n = *(const uint32_t *)(mem + lp);
    if (n && n <= NS_CH && (*(const uint32_t *)(mem + lp + 16) & (XL_GSZ - 1) & ~15u) == a) {
      for (uint32_t i = 0; i < n; i++) ns_cha[i] = *(const uint32_t *)(mem + lp + 16 + 16 * i) & (XL_GSZ - 1) & ~15u;
      ns_chn = (int)n; ns_ch_lists++; ns_ch_in++; return 0; } }
  for (int i = 1; i < ns_chn; i++) if (ns_cha[i] == a) { ns_cha[i] = 0xffffffffu; ns_ch_in++; return i; }   /* once: an old list names no later frame's transfer */
  ns_ch_unl++; return -1;
}
static void mq_rec(const MpRec *r, int now)   /* a record to the ME as its transfer ends; now: the main CPU waits for its copy (or walk) */
{
  XmeE e; uint32_t s;
  memset(&e, 0, sizeof e); e.kind = XE_REC;
  e.r.ch = r->ch; e.r.lanes = r->lanes; e.r.restart = r->restart; e.r.x0 = r->x0; e.r.nx = r->nx; e.r.t = r->t; e.r.now = now;
  e.r.chcr = r->chcr; e.r.madr = r->madr; e.r.qwc = r->qwc; e.r.tadr = r->tadr; e.r.opt = r->opt; e.r.opt2 = r->opt2;
  e.r.nb = r->nb; e.r.ng0[0] = r->ng0[0]; e.r.ng0[1] = r->ng0[1]; e.r.ngn[0] = r->ngn[0]; e.r.ngn[1] = r->ngn[1]; e.r.noff = r->noff;   /* native step 1 */
  e.r.p0[0] = r->p0[0]; e.r.p0[1] = r->p0[1]; e.r.pn[0] = r->pn[0]; e.r.pn[1] = r->pn[1];   /* step 2 */
  e.r.nru[0] = r->nru[0]; e.r.nru[1] = r->nru[1];   /* gl36 */
  e.r.chi = r->chi;   /* gl34 (gl38: found as the transfer started) */
  if (r->nx >= 0 && r->nb >= 0 && !now) nat_unf[r->nb] = 1;
  s = mq_post(&e);
  if (now) mq_wait(s, MW_NOW);
}
static int mq_plan(MpRec *e, int *x0, int *nx)   /* gl24b: xm_plan, counting a walk's extents and arena bytes with two areas */
{
  uint32_t u0 = xmc.used; int r;
  e->nb = -1; e->ngn[0] = e->ngn[1] = 0; e->noff = 0; e->nru[0] = e->nru[1] = 0;   /* native step 1: the groups this transfer's tags reach, planned with it */
  e->p0[0] = e->p0[1] = e->pn[0] = e->pn[1] = 0;   /* step 2: the people's */
  xmc.nat_n[0] = xmc.nat_n[1] = 0; xmc.np6_n[0] = xmc.np6_n[1] = 0;
  if (nat_on && e->ch == 1 && e->lanes) { xmc.nat_n[0] = nat_pend(0, &xmc.nat_g[0]); xmc.nat_n[1] = nat_pend(1, &xmc.nat_g[1]); }
  if (np6_on && e->ch == 1 && e->lanes) { xmc.np6_n[0] = np6_pend(0, &xmc.np6_g[0]); xmc.np6_n[1] = np6_pend(1, &xmc.np6_g[1]); }
  xmc.xp = rt_xp_at(me_vb); xmc.clk = p10_on > 0 ? p10_clk : 0;   /* gl40 */
  { const long d0 = xmc.xp_dif;
    r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, x0, nx);
    if (!d0 && xmc.xp_dif) xmc.xp_dif_vb = (int32_t)me_vb; }
  if (!r && xmc.nat_b >= 0 && (xmc.nat_cur > 0 || xmc.np6_cur > 0)) {   /* (step 2: the people's records right after the ship's: XME_POFF) */
    const int b = xmc.nat_b; uint32_t off = 0;
    int bad = xm_nat_room(&xmc, XME_NATBYTES(b, xmc.nat_cur, xmc.np6_cur), &off);   /* (step 2: both kinds' groups and references, ge/xme.h) */
    if (bad) {   /* no room for their copy: the plan undone, as an arena too full for the extents (xm_plan's 3) */
      xmc.copies--; xmc.exts -= *nx; xmc.bytes -= xmc.used - u0; xmc.next = *x0; xmc.used = u0; xmc.fails[3]++; nat_n_room++; r = 3; }
    else { e->nb = b; e->ng0[b] = (int16_t)g_nat[b].cur; e->ngn[b] = (int16_t)xmc.nat_cur; e->noff = off; e->nru[b] = (uint8_t)g_nat[b].ru;   /* gl36: nru */
      e->p0[b] = (int16_t)g_np6[b].cur; e->pn[b] = (int16_t)xmc.np6_cur; } }
  if (!r) { uint32_t h = xmc.used - (two ? (uint32_t)two_h * two_ah : 0u); if (h > nat_ca_max) nat_ca_max = h; }
  if (two && !r) { two_wx += *nx; two_wb += xmc.used - u0; }
  return r;
}
static void mq_chk_end_do(void);
static void mq_chk_end(void)   /* xlive_chk_end in mode 3 (gl40: timed whole with MGS2_PROBE10, its waits by reason) */
{
  uint32_t t0;
  if (p10_on <= 0) { mq_chk_end_do(); return; }
  t0 = sceKernelGetSystemTimeLow(); p10_in = 1;
  mq_chk_end_do();
  p10_in = 0; p10_ce++; p10_ceus += sceKernelGetSystemTimeLow() - t0;
}
static void mq_chk_end_do(void)
{
  MpRec *e; int r, x0 = 0, nx = 0; long tb = mq_tbk_cold;
  if (mq_m4 && mq_self) {   /* step 4d: the main CPU makes the picture itself and cannot reach the ME's arena: walked from RAM now */
    if (!mp_nrec) return;
    e = &mp_rec[mp_nrec - 1]; e->nx = -1; mq_own++; { MpRec keep; nat_direct_pre(e); keep = *e; mq_rec(e, 0); mq_late(3, 1); nat_direct_post(&keep); } return; }
  { uint64_t ov = mq_now(); mq_ov_us += (ov - mq_ov_t) & (0 - (uint64_t)mq_ov_f); mq_ov_n += mq_ov_f; mq_ov_f = 0; }   /* gl20: the overlap (no branch) */
  if (!two) mq_arena_wait();   /* (no plan while the ME walks the records before; gl24: in the half it walks) */
  else { two_poll(); if (two_pend[two_h]) two_wait(two_h, MW_ARENA); }   /* gl25: only the walk that holds this half */
  if (mq_tbk_cold != tb || !mp_nrec) return;   /* (a take-back: the record dropped) */
  e = &mp_rec[mp_nrec - 1];
  r = mq_plan(e, &x0, &nx);
  if (two && (r == 2 || r == 3) && !two_spill) {   /* gl24: the half full: the other half, once the walk there is done (a spill) */
    two_poll(); if (two_pend[two_h ^ 1]) { two_wait(two_h ^ 1, MW_ARENA); if (mq_tbk_cold != tb || !mp_nrec) return; }   /* gl25 */
    if (two) { xmc.fails[r]--; two_keep(); two_load(two_h ^ 1); two_spill = 1; two_spills++;
      r = mq_plan(e, &x0, &nx); }
  }
  if (two && (r == 2 || r == 3) && mp_nrec == 1 && !two_pend[0] && !two_pend[1]) {   /* gl24b: one record larger than a half, every walk
                                                                                   done: the whole arena (gl23 copied it there too) */
    xmc.fails[r]--; two_h = 0; xmc.next = 0; xmc.used = 0; xmc.ext_cap = two_xn; xmc.arena_n = two_an; two_spill = 1; two_full_n++;
    r = mq_plan(e, &x0, &nx);
  }
  if ((r == 2 || r == 3) && mp_nrec > 1) {   /* no room: the records before laid and walked now, the copy planned again */
    MpRec keep = *e; mp_drain_room++; mp_nrec--; mq_late(3, 1);
    if (mq_tbk_cold != tb) return;
    if (two) { two_h = 0; xmc.next = 0; xmc.used = 0; xmc.ext_cap = two_xn; xmc.arena_n = two_an; two_spill = 1; two_full_n++; }   /* gl24:
                                                                   every walk done: the whole arena for the rest of the step */
    mp_rec[0] = keep; mp_nrec = 1; e = &mp_rec[0];
    r = mq_plan(e, &x0, &nx);
  }
  if (r) {   /* none: walked from RAM now (nx -1), with the records before */
    MpRec keep; mp_direct++; e->nx = -1; nat_direct_pre(e); keep = *e; mq_rec(e, 0); mq_late(3, 1); nat_direct_post(&keep); return; }
  e->x0 = x0; e->nx = nx;
  if (e->nb >= 0) { g_nat[e->nb].cur += (uint32_t)e->ngn[e->nb]; nat_n_plan++; nat_n_plang += e->ngn[e->nb];   /* native step 1: taken */
    g_np6[e->nb].cur += (uint32_t)e->pn[e->nb]; np6_n_plang += e->pn[e->nb]; }   /* step 2 */
  if (me_vb >= mp_dl && !mp_spr(e)) { e->filled = 0;   /* an early copy, laid at the FILL */
    if (p10_on > 0) { uint32_t t0 = sceKernelGetSystemTimeLow(); mp_mark(e, 1); p10_mk++; p10_mkus += sceKernelGetSystemTimeLow() - t0; } else mp_mark(e, 1);
    mp_unfilled++; mq_rec(e, 0); }
  else { e->filled = 1; mp_fill_end[me_vb < mp_dl ? 0 : 1]++; mq_rec(e, 1); }   /* before vb 60, or scratchpad bytes: as it ends */
  if (mq_tbk_cold != tb) return;
  g_g0_on = mp_unfilled > 0;
}
static void mq_thin(void)   /* the thinning's scan for the frame begun last (it reads the game's RAM), its ranges to the ME (THIN) */
{
  XmeE e; int n, nd;
  if (mq_thin_due) { mq_wait(mq_thin_seq, MW_THIN); mq_thin_due = 0; }   /* (the slot is the ME's until it has taken the last ranges) */
  n = fx_thin_scan(&gl_ft, M.mem, mq_c4); if (n > 16) n = 16; if (n < 0) n = 0;   /* (gl_thin's limits) */
  nd = gl_ft.nd; if (nd > 512) nd = 512; if (nd < 0) nd = 0;
  memcpy(mq->thin_r, gl_ft.r, (size_t)n * sizeof gl_ft.r[0]); mq->thin_n = n; mq->thin_keep3 = gl_ft.keep[3];
  if (nd) memcpy(mq->thin_d, gl_ft.d, (size_t)nd * sizeof gl_ft.d[0]);
  mq->thin_nd = nd;
  mq->bd_n = gl_ft.all ? gl_ft.nb : 0; if (mq->bd_n > 0) memcpy(mq->bd_r, gl_ft.b, (size_t)mq->bd_n * sizeof gl_ft.b[0]);   /* gl38: the rain owners' blocks */
  memset(&e, 0, sizeof e); e.kind = XE_THIN;
  mq_thin_seq = mq_post(&e); mq_thin_due = 1;
}
/* step 4d: to the main CPU: the ME's cache written back and dropped (SYNC), the queue emptied, then the main CPU's cache dropped */
static void mq_sync(void)
{
  XmeE e;
  memset(&e, 0, sizeof e); e.kind = XE_SYNC;
  mq_wait(mq_post(&e), MW_SYNC); mq_syncs++;
  if (!mq_dead) mq_cpu_wbinv();
}
static int flip_sel(long vb)   /* gl27: the flip wait in the turn that begins at vb */
{
  return flip_cfg == 1 || (flip_cfg == 2 && ((flip_from >= 0 && vb >= flip_from) || !mq_turn || ((vb / (2 * mq_turn)) & 1)));
}
/* gl28d: MGS2_ME_PIC_GE2_AB at an ME turn's start from its vblank: GE2 on when vb / (2 * TURN) is odd (after the cfg's own choice) */
static GL28D_COLD void ge2_ab_turn(long vb)
{
  ge2 = gl_vol2 && ((vb / (2 * mq_turn)) & 1);
  fprintf(stderr, "me turn vb %ld gl28 ge2 a/b: GE2 %s (MGS2_ME_PIC_GE2 %d, the second frame memory %s)\n", vb, ge2 ? "on" : "off",
          ge2_cfg, gl_vol2 ? "made" : "NOT made");
}
/* step 4d, mode 4, after the hook's FILL, WALK and HOOK (no record waits): the hang test's stop found, the turns changed */
/* gl38 (ge/bound.h): the bound cycle's turns.  From its first vblank every turn is the ME's (no main turn between two of them: the
   hand-over and back at once, for the turn's lines), but a turn of kind 7 (want 1), which is the main CPU's with the ME resting; the
   frame that began at this hook already has the new turn's mask (mq_frame) */
static void mq_bd_turn(long vb, long tix, int want)
{
  if (tix == bd_tix) return;
  if (!mq_turn_k) {   /* an ME turn ends: as the switch to the main CPU */
    mq_sync(); if (!MQ_LOOP()) return;
    mq_arena_wait(); if (!MQ_LOOP()) return;
    XS(mq)->ge2 = 0; mq->cache = 0; mq->clk = mq_clk_us; mq_heap_own(); mq_self = 1; mq_tend(vb); two_start(0); flip_on = flip_sel(vb);
    if (!want) flip_mvb0 = -1;   /* (no main turn: no main turn lines at the ME turn's start below) */
#ifdef MGS2_ME_REAL
    else rs_allow(vb);
#endif
  } else if (want) {   /* a main turn after a main turn: the lines of the one that ends, as mq_tstart prints them */
#ifdef MGS2_ME_REAL
    rs_end(vb);
#endif
    if (flip_mvb0 >= 0) { flip_line("main turn", flip_mvb0, vb, 1); nat_tline("main turn", flip_mvb0, vb, 0); bd_tline("main turn", flip_mvb0, vb); }
    flip_t0set(1); nat_t0set(); flip_mvb0 = vb;
#ifdef MGS2_ME_REAL
    rs_allow(vb);
#endif
  }
  if (!want) {   /* to the ME: as mq_turn_hook's */
#ifdef MGS2_ME_REAL
    rs_end(vb);
    mq->cache = mq_me > 0 ? xme_me_cache : mq_cache_thr;
#else
    mq->cache = mq_cache_thr;
#endif
    if (ge_fast_cfg == 2) ge_fast = (int)((vb / (2 * mq_turn)) & 1);
    ge2 = gl_vol2 && (ge2_cfg == 1 || (ge2_cfg == 2 && ((vb / (2 * mq_turn)) & 1)));
    if (ge2_ab >= 0 && vb >= ge2_ab) ge2_ab_turn(vb);
    XS(mq)->ge2 = (uint32_t)ge2;
    xme_heap_ask(me_rs_on); mq_self = 0; mq_tstart(vb); two_start(two_on(vb)); flip_on = flip_sel(vb); }
  mq_turn_k = want; bd_tix = tix; mq_turns++; mq_thin_due = 0; mq_walk_due = 0;
}
static void mq_turn_hook(long vb)
{
  int k;
  if (mq_dead) return;
  if (!mq_self && mq->hang_vb >= 0 && !mq->hang_in && vb >= mq->hang_vb) mq_drain();
  if (!mq_turn || !MQ_LOOP()) return;
  if (g_bd_cfg) { long tix; int bd = rt_bound_at(vb, &tix);   /* gl38 */
    if (tix >= 0) { mq_bd_turn(vb, tix, (bd & (BD_ALL | BD_ME)) == BD_ALL); return; } }   /* gl39: kind 8 (BD_ALL | BD_ME) stays the ME's */
  k = (int)((vb / mq_turn) & 1);
  if (k == mq_turn_k) return;
  if (k) {   /* to the main CPU */
    mq_sync(); if (!MQ_LOOP()) return;   /* (a take-back meanwhile: the main CPU's from now on, or the picture stopped) */
    mq_arena_wait(); if (!MQ_LOOP()) return;
    XS(mq)->ge2 = 0;   /* gl23: the main CPU's turns: one frame on the GE side (the ME has handed over: it reads nothing until the next turn) */
    mq->cache = 0; mq->clk = mq_clk_us; mq_heap_own(); mq_self = 1; mq_tend(vb); two_start(0); flip_on = flip_sel(vb);
#ifdef MGS2_ME_REAL
    rs_allow(vb);   /* gl28: the ME may rest (MGS2_ME_REST=1) */
#endif
  }
  else {   /* to the ME (the main CPU writes back its cache before each post) */
#ifdef MGS2_ME_REAL
    rs_end(vb);     /* gl28: the rest ends first: the ME woken (MGS2_ME_REST=1) */
    mq->cache = mq_me > 0 ? xme_me_cache : mq_cache_thr;   /* gl21: the clocks are set once (xme.c XME_CLK) */
#else
    mq->cache = mq_cache_thr;
#endif
    if (ge_fast_cfg == 2) ge_fast = (int)((vb / (2 * mq_turn)) & 1);   /* gl22: off, on, off, ... by ME turn */
    ge2 = gl_vol2 && (ge2_cfg == 1 || (ge2_cfg == 2 && ((vb / (2 * mq_turn)) & 1)));   /* gl23: off, on, off, ... by ME turn */
    if (ge2_ab >= 0 && vb >= ge2_ab) ge2_ab_turn(vb);   /* gl28d: MGS2_ME_PIC_GE2_AB: on, off, ... by ME turn from that vblank */
    XS(mq)->ge2 = (uint32_t)ge2;
    xme_heap_ask(me_rs_on); mq_self = 0; mq_tstart(vb); two_start(two_on(vb)); flip_on = flip_sel(vb); }   /* gl24: two areas or one, by ME
                                                                                                             turn; gl27: the flip wait */
  mq_turn_k = k; mq_turns++; mq_thin_due = 0; mq_walk_due = 0;
}
/* xl_psp_frame in mode 3: the walks posted at the last hook done by now (at the latest), the turns, the step's records laid and walked
   (FILL, WALK), then the hook (HOOK: the frame's end and list, a frame's start, the texture cache's frame) */
static void mq_frame(long vb)
{
  uint32_t kb; XmeE e;
  mq_probe();
  if (!two) mq_arena_wait();
  else if (mp_nrec || (mq_walk_due && mp_hooks != mq_walk_hk))   /* gl24: with two areas the walk before ran while this step's game code did */
    mq_arena_waitw(MW_HOOK);   /* gl25: a hook with records waits for it, before its FILL and WALK; gl25b: so does the second hook after
                                  the walk's own (lchk4: the RAM a hook walk reads stays unchanged for two hooks, xc_keep's ranges apart) */
  else { two_poll(); two_nowait++; }   /* gl25: a hook without records (the lite step's): no wait */
  if (!mq_m4 && mq_turn && mq_th >= 0) { int k = (int)((vb / mq_turn) & 1);
    if (k != mq_turn_k) { mq_drain(); if (mq_th >= 0) { mq_turn_k = k; mq_self = k; mq_turns++; mq_thin_due = 0; mq_walk_due = 0;
        flip_on = flip_sel(vb); } } }   /* gl27c: mode 3's turns choose the flip wait too */
  mp_hooks++; kb = (uint32_t)((xmc.bytes - mp_bytes_hook) >> 10); mp_bytes_hook = xmc.bytes;   /* (mp_hook's counts) */
  if (kb > mp_step_max) { mp_step_max = kb; mp_step_max_vb = vb; }
  if (kb) mp_hist[kb < 512 ? 0 : kb < 1024 ? 1 : kb < 1536 ? 2 : kb < 2048 ? 3 : kb < 2560 ? 4 : kb < 3072 ? 5 : kb < 3584 ? 6 : kb < 4096 ? 7 : 8]++;
  if (mp_nrec) { mq_late(0, 0); if (mq_walk_due) mq_walk_hk = mp_hooks; }   /* gl25b */
  memset(&e, 0, sizeof e); e.kind = XE_HOOK; e.h.vb = (int32_t)vb;
  if (gl_ok != 1) {   /* the GE stopped: the ME told once, no more frames */
    if (mq_stop_due && !mq_stop_sent) { e.h.stop = 1; mq_post(&e); mq_stop_sent = 1; }
    return; }
  HOST_ENTER(HOST_GL);
  if (gl_now) { gl_now = 0; e.h.end = 1; e.h.d = (int)(gl_built++ & 1); e.h.dig = vb - 1 >= dig_from && vb - 1 <= dig_to && !rt_dig_off_at(vb - 1);   /* gl39: MGS2_DIG_CYCLE */
    if (dz_on) e.h.dz = gx_dz_arm(&dz_t, vb - 1, &e.h.dzn, 0); }   /* gl29: MGS2_GL_DZ's arm for the frame that ended */
  if (gl_full && state == 1 && (g_opt & (OPT_XL | OPT_GL)) == (OPT_XL | OPT_GL)) {
    bd_cur = rt_bound_at(vb, 0); gl_ft.all = (bd_cur & (BD_SORT | BD_RAIN)) != 0; bd_fr += bd_cur != 0;   /* gl38: the frame's mask, to the ME in begin's bits 16-23 */
    if (nge_kg && !nge_gst && vb >= nge_grow_vb()) { nge_gst = 1; nge_gfin = ge_fin_n; nge_gvb0 = vb;   /* step 2: frames from this one with the smaller arena */
      fprintf(stderr, "picture: step 2: vb %ld: frames begin with the smaller arena (%u KB); the GE's finishes %u\n", vb,
              (unsigned)((GL_PSP_ARENA - nge_kg) >> 10), (unsigned)ge_fin_n); }
    e.h.begin = 1 | rt_nat_cull_at(vb) << 8 | (bd_cur & 0xff) << 16 | (nge_gst ? 1u << 24 : 0u);   /* (step 2: BD_NATIVE the main CPU's) */ e.h.opt = g_opt; e.h.opt2 = g_opt2; gl_now = 1; thin_due = 1; mq_c4 = (g_opt2 & OPT2_C4) != 0;
    if (gl_first_vb < 0) { gl_first_vb = vb; fprintf(stderr, "picture: the first frame begins at vb %ld\n", vb); } }
  mq_post(&e);
  if (nge_gst == 1 && nge_gok && ge_fin_n - nge_gfin >= 3) { nge_gst = 2; nge_grow(nge_p6b[0] ? nge_p6b : 0, vb);
    fprintf(stderr, "picture: step 2: vb %ld: grown (a frame begun at vb %ld or later drawn; the GE's finishes %u)\n", vb, nge_gvb0, (unsigned)ge_fin_n); }   /* step 2 (the guard above) */
  if ((bd_cur & BD_ME) && gl_now && !mq_self && mq_m4 && !mq_dead) {   /* gl39, kind 8: the ME's load for this step, posted in every flavour (0: an entry
                                                                        that does nothing), only when the loop has caught up */
    if (XS(mq)->head - XS(mq)->tail <= 1) { XmeE l; memset(&l, 0, sizeof l); l.kind = XE_LOAD; l.why = rt_load_at(vb); l.nlog = (int)p9_load_us;
      l.h.vb = (int32_t)vb; l.h.opt = ((uint32_t)(uintptr_t)_ftext + 63u) & ~63u; l.h.opt2 = 2u << 20; mq_post(&l); p9_ldp++; }
    else p9_ldskip++; }
  mq_service();
  HOST_LEAVE(HOST_GL);
  mq_ov_t = mq_now(); mq_ov_f = 1;   /* gl20 */
  if (mq_m4) mq_turn_hook(vb);   /* step 4d */
}
static void mq_end(void)   /* xl_psp_gl_end (the run's end): the last step's records walked, the loop stopped, its thread ended */
{
  XmeE e;
  if (mp_nrec) mq_late(0, 0);
  if (MQ_LOOP()) {   /* (in a turn of the main CPU's the thread or the ME idles, the queue empty) */
    uint32_t s; memset(&e, 0, sizeof e); e.kind = XE_STOP;
#ifdef MGS2_ME_REAL
    rs_end(me_vb);   /* gl28: a main turn's rest ends (its line) before the STOP */
#endif
    mq_self = 0; s = mq_post(&e); mq_wait(s, MW_DRAIN);
    if (mq_m4 && !mq_dead) { mq_cpu_wbinv(); mq->cache = 0; mq->clk = mq_clk_us; mq_heap_own(); mq_tend(me_vb); }   /* step 4d: the ME's counts read (4d-3: the INV first) */
#ifdef MGS2_ME_REAL
    if (mq_me > 0) {   /* step 4d-4: the ME left the loop at the STOP (its cache written back, state 2): held in reset */
      mq_me_ack = hle_me_stop(50); mq_me = -1;
      fprintf(stderr, "me picture loop (step 4d-4): the ME stopped: %s (state %u, heartbeat %u, boots %u)\n", mq_me_ack ? "it answered the "
              "exit request" : "NO ANSWER in 50 ms: held in reset", (unsigned)mq_mcu->h.state, (unsigned)mq_mcu->h.heartbeat,
              (unsigned)mq_mcu->h.boots); }
#endif
    if (mq_th >= 0) { SceUInt us = 1000000; int r = sceKernelWaitThreadEnd(mq_th, &us), fs = -1, sz = 0; SceKernelThreadInfo ti; memset(&ti, 0, sizeof ti); ti.size = sizeof ti;
      if (sceKernelReferThreadStatus(mq_th, &ti) >= 0 && ti.stack) { const uint8_t *p = (const uint8_t *)ti.stack; sz = ti.stackSize; fs = 0; while (fs < sz && p[fs] == 0xff) fs++; }
      fprintf(stderr, "me picture loop (gl41): the thread's stack %u KB, never touched at its bottom %d B (deepest use %d B; the bottom marked at its start)\n",
              MQ_THR_STK >> 10, fs, fs >= 0 ? sz - fs : -1);
      if (r < 0) fprintf(stderr, "me picture loop: its thread did not end (%08x)\n", (unsigned)r);
      sceKernelDeleteThread(mq_th); mq_th = -1; } }
  mq_self = 1;
  if (mq_sem >= 0) { sceKernelDeleteSema(mq_sem); mq_sem = -1; }
  flip_force = 1; mq_service(); mq_ge_finish(); flip_force = 0;   /* gl27b: the run's last list, no flip wait (the text goes up next) */
}
static void mq_report(FILE *f)
{
  if (g_nat_cfg)   /* native step 1: the run's totals */
    fprintf(f, "native step 1 totals: arrays %s; calls recorded %ld, groups %ld, pictures with records %ld; stubbed with no record %ld; calls "
            "with no group %ld; the builder run instead: array full %ld, key order %ld, near the buffer's end %ld, other %ld; recorded with no "
            "array %ld; groups left untaken %ld (pictures %ld); transfers planned with groups %ld (%ld groups), groups taken by walks from RAM "
            "%ld, plans undone for the groups' room %ld, early fills for an array's reuse %ld; records' groups copied %ld (%ld groups, %llu "
            "bytes); merge runs %ld, groups to the core %ld, fed word by word %ld, dropped %ld, drawn at a walk's end %ld, run lanes stopped "
            "at a key %ld; the groups' data in the copy %ld, read from RAM %ld, partly (dropped) %ld\n", !nat_on ? "NOT MADE" : nat_one ? "ONE, shared" : "made, one a buffer",
            g_nat_c[NC_REC], g_nat_c[NC_GRP], g_nat_c[NC_PICS], g_nat_c[NC_STUBC], g_nat_c[NC_EMPTY], g_nat_c[NC_FB_FULL], g_nat_c[NC_FB_ORDER],
            g_nat_c[NC_FB_END], g_nat_c[NC_FB_OTHER], g_nat_c[NC_NOMEM], g_nat_c[NC_LEFT], g_nat_c[NC_LEFT_PICS], nat_n_plan, nat_n_plang,
            nat_n_dirg, nat_n_room, nat_n_sync, mq->n_natrec, mq->n_natg, (unsigned long long)mq->n_natb, xmp.nat_runs, xmp.nat_core, xmp.nat_fed,
            xmp.nat_bad, xmp.nat_late, xmp.nat_stop, xmp.nat_in, xmp.nat_out, xmp.nat_part);
  if (g_nat_cfg) fprintf(f, "static streams in place totals (gl33): groups flagged static %ld\n", g_nat_stc);
  if (g_nat_f1cfg || g_nat_f1chk) fprintf(f, "F1 totals (gl38): runs planned %ld (groups %ld, of them not written %ld, without the fast form %ld), runs not planned %ld (state %ld, a "
                         "group without an MSCAL %ld), planned runs with a problem %ld; check mode: groups compared %ld, DIFFERING %ld, written by the plan and not "
                         "by the notes %ld, skipped by the plan ahead of a group that declined when it ran %ld\n", xl_f1_n[0], xl_f1_n[1], xl_f1_n[2], xl_f1_n[10],
                         xl_f1_n[3], xl_f1_n[8], xl_f1_n[9], xl_f1_n[4], xl_f1_n[5], xl_f1_n[6], xl_f1_n[7], xl_f1_n[11]);
  fprintf(f, "gl40 XP totals: plans %ld (failed: ranges %ld, extents %ld, arena %ld, chain %ld), xc_add calls %ld, sort moves %ld, copied back %ld; "
          "XP check: plans compared %ld, DIFFERING %ld (first at vb %ld: results %x, extents %u / %u), by result only %ld; M5 walks %ld\n", xmc.xp_plans,
          xmc.fails[1], xmc.fails[2], xmc.fails[3], xmc.fails[4], xmc.xp_adds, xmc.xp_moves, xmc.xp_cpy, xmc.xp_chk, xmc.xp_dif, (long)(xmc.xp_dif ? xmc.xp_dif_vb : -1),
          (unsigned)xmc.xp_dif_w[0], (unsigned)xmc.xp_dif_w[1], (unsigned)xmc.xp_dif_w[2], xmc.xp_room, g_k_m5);
  if (g_bd_cfg || g_bd_rain) { fprintf(f, "bound totals (gl38): frames begun with a mask %ld; transfers left out by chain:", bd_fr);
    for (int i = 0; i <= NS_CH; i++) if (bd_tr[i]) fprintf(f, " %d:%ld", i == NS_CH ? -1 : i, bd_tr[i]);
    fprintf(f, "; native groups recorded and not drawn %ld; rain owners' calls %ld (sheets %ld), left out %ld, near sheet calls kept %ld; chain 6's other calls "
            "left out %ld, its GS primitives %ld; rain owners' blocks over the table %ld, not right %ld\n", bd_grp, gl_bd_n[GL_BD_RAIN], gl_bd_n[GL_BD_SHEET],
            gl_bd_n[GL_BD_RAIN_OUT], gl_bd_n[GL_BD_NEAR_KEPT], gl_bd_n[GL_BD_SORT_OUT], gl_bd_n[GL_BD_PRIM_OUT], gl_ft.b_over, gl_ft.b_bad); }
  if (g_nat_cfg && xl_nr_cap) fprintf(f, "reuse totals (gl36): table %d entries; runs replayed %ld (groups %ld, skipping %ld), recorded %ld (groups %ld; kept %ld, "
                         "not kept %ld), checked %ld (groups %ld; S differing %ld, boxes differing %ld); in S %ld; boxes recorded %ld, taken %ld, cut %ld, "
                         "positions then needed %ld; no table room %ld; replays with a group not taken %ld, with a decline %ld\n", xl_nr_cap, xl_nr_n[0],
                         xl_nr_n[1], xl_nr_n[18], xl_nr_n[3], xl_nr_n[4], xl_nr_n[5], xl_nr_n[13], xl_nr_n[6], xl_nr_n[7], xl_nr_n[8], xl_nr_n[9], xl_nr_n[2],
                         xl_nr_n[16], xl_nr_n[10], xl_nr_n[11], xl_nr_n[12], xl_nr_n[14], xl_nr_n[15], xl_nr_n[17]);
  if (g_nat_cfg && xl_nr_cap) fprintf(f, "reuse rule totals (gl37): rule %d; groups tried %ld, replayed or the same as the picture before %ld; runs not "
                         "recorded %ld (groups %ld), the same as their shadow %ld (groups %ld); pictures recording %ld, not recording %ld\n", xl_nr_rule,
                         xl_nr_r[6], xl_nr_r[7], xl_nr_r[0], xl_nr_r[1], xl_nr_r[2], xl_nr_r[3], xl_nr_r[4], xl_nr_r[5]);
  if (g_nat_cfg) fprintf(f, "keeps for fast native groups totals (gl35): groups flagged %ld; drawn areas replayed %ld, checked %ld, differing %ld; textures "
                         "replayed %ld, checked %ld, differing %ld, records %ld; CLUT calls not made %ld, PRIM writes in line %ld\n", g_nat_kpt, xl_nk_n[0],
                         xl_nk_n[1], xl_nk_n[2], xl_nk_n[3], xl_nk_n[4], xl_nk_n[5], xl_nk_n[6], xl_nk_n[7], xl_nk_n[8]);
  if (g_nat_cfg) fprintf(f, "fast native front end totals (gl34): groups flagged %ld; taken %ld, declined: state %ld, entry %ld, buffers %ld, stream counts "
                         "%ld, material tag %ld, tables full %ld, path 1 busy %ld; transforms taken %ld, computed and kept %ld; pointers by their own hints %ld "
                         "(checked %ld, differing %ld); nat split: groups timed by "
                         "stage %ld of %ld, clock read pairs %ld; chain lists read %ld, transfers in a list %ld, not in one %ld\n", g_nat_fst, xl_nf_n[0],
                         xl_nf_n[1], xl_nf_n[2], xl_nf_n[3], xl_nf_n[4], xl_nf_n[5], xl_nf_n[6], xl_nf_n[7], vc_km_n[0], vc_km_n[1], lw_nh_n[0], lw_nh_n[2], lw_nh_n[1], g_ns.n, g_ns.seen,
                         g_ns.cal_n, ns_ch_lists, ns_ch_in, ns_ch_unl);
  fprintf(f, "picture display (gl27): flip wait MGS2_GL_FLIPWAIT %d (from vb %ld): ME turns: lists held for the screen %ld (%.2f s), lists drawn "
          "into the shown buffer %ld; main CPU turns: held %ld (%.2f s), drawn into the shown buffer %ld%s", flip_cfg, flip_from, flip_hold[0],
          flip_us[0] / 1e6, flip_race[0], flip_hold[1], flip_us[1] / 1e6, flip_race[1], fbc_from < 0 ? "\n" : "");
  if (fbc_from >= 0)   /* gl28d: MGS2_GL_FBCHK's totals, on the same line */
    fprintf(f, "; gl28 fbchk from vb %ld: ME turns: lists %ld, into the buffer the display shows now (IMMEDIATE) %ld, into the buffer it "
            "shows next (NEXTFRAME) %ld, in the display vblank of the show %ld, display vblanks since the show at most %u; main CPU turns: "
            "lists %ld, IMMEDIATE %ld, NEXTFRAME %ld, in the vblank of the show %ld, at most %u; answers refused %ld; answers naming neither "
            "buffer %ld\n", fbc_from, fbc_n[0], fbc_imm[0], fbc_next[0], fbc_same[0], (unsigned)fbc_dvc[0], fbc_n[1], fbc_imm[1], fbc_next[1],
            fbc_same[1], (unsigned)fbc_dvc[1], fbc_err, fbc_oth);   /* gl28d2: fbc_oth */
  if (mq_tn && mq->ws_on) { double s[XM_TS_N], in = 0, wk = (double)mq_tkall[XE_WALK] / (mq_tdiv ? mq_tdiv : 1);   /* gl26: the walk's split over the ME's
                                                                                                   turns (gl26b: only when there were some) */
    for (int k = 0; k < XM_TS_N; k++) { s[k] = (double)mq_tsall[k] / (mq_tdiv ? mq_tdiv : 1) / 1000.0; in += s[k]; }
    fprintf(f, "me picture walk split (gl26), the ME's turns (s): walks %.1f; calls: lane 1 meshes %.1f (%ld), lane 2 P06 %.1f (%ld), "
            "lane 3 effects %.1f (%ld), lane 4 P07 %.1f (%ld), no lane %.1f (%ld); held calls' flushes %.1f (%ld); calls that ran the held calls "
            "%.1f (%ld); passes 2-3 %.1f (%ld); GS primitives %.1f (%ld); the rest %.1f\n", wk / 1000.0, s[1], mq_tsnall[1], s[2], mq_tsnall[2],
            s[3], mq_tsnall[3], s[4], mq_tsnall[4], s[0], mq_tsnall[0], s[XM_TS_FLUSH], mq_tsnall[XM_TS_FLUSH], s[XM_TS_CFL], mq_tsnall[XM_TS_CFL],
            s[XM_TS_PASS], mq_tsnall[XM_TS_PASS], s[XM_TS_PRIM], mq_tsnall[XM_TS_PRIM], wk / 1000.0 - in); }
  if (two_cfg) fprintf(f, "me picture copies (gl24): two copy areas: walks posted %ld, spills into the other half %ld, steps with no half free "
                       "after their walk %ld, steps given the whole arena after a walk at once %ld; the main CPU waited at hooks for the walk "
                       "before %.1f s; gl25: hooks without a wait %ld, waits for an older walk %ld, half moves %ld\n", two_steps, two_spills, two_nohalf,
                       two_full_n, mq_wus[MW_HOOK] / 1e6, two_nowait, two_older, two_move);
  { static const char *nm[3] = { "ME turns, one frame on the GE side", "ME turns, two frames on the GE side (GE2)", "main CPU turns" };   /* gl22, gl23 */
    for (int b = 0; b < 3; b++) if (ge_n[b])
      fprintf(f, "me picture GE (gl22): %s: %ld lists, the GE drawing a list %.2f ms on average (at most %.2f), the main CPU seeing its finish "
              "%.2f ms after it on average\n", nm[b], ge_n[b], (double)ge_draw[b] / 1000.0 / ge_n[b], ge_draw_max[b] / 1000.0,
              (double)ge_note[b] / 1000.0 / ge_n[b]);
    fprintf(f, "me picture GE (gl22): finishes counted by the callback %u, lists seen finished with no callback %ld\n", (unsigned)ge_fin_n, ge_nofin);
    if (mq) fprintf(f, "me picture GE (gl23): frames begun while the GE drew the frame before: %ld\n", mq->n_ge2); }
  if (mq_tn) {   /* gl20: the ME turns' sums (each turn's own clock) */
    double d = (double)mq_tdiv;
    fprintf(f, "me turns (gl20): %ld turns, %.1f s: the ME's own time (s): records %.1f, thinning %.1f, fills %.1f, walks %.1f, hooks %.1f "
            "(frame ends and lists %.1f, GE waits %.1f), hand-over %.1f, idle %.1f; the main CPU waited (s): copies as they ended %.1f, fills %.1f, "
            "the arena %.1f; from a hook to its first copy plan %ld times, mean %.2f ms\n", mq_tn, (double)mq_ttall / 1e6,
            mq_tkall[XE_REC] / d / 1000, mq_tkall[XE_THIN] / d / 1000, mq_tkall[XE_FILL] / d / 1000, mq_tkall[XE_WALK] / d / 1000,
            mq_tkall[XE_HOOK] / d / 1000, mq_tkall[XME_T_LIST] / d / 1000, mq_tkall[XME_T_GEW] / d / 1000,
            (mq_tkall[XE_SYNC] + mq_tkall[XE_STOP]) / d / 1000, mq_tkall[XME_T_IDLE] / d / 1000, mq_twall[MW_NOW] / 1e6, mq_twall[MW_FILL] / 1e6,
            mq_twall[MW_ARENA] / 1e6, mq_ov_n, mq_ov_n ? (double)mq_ov_us / 1000.0 / mq_ov_n : 0.0); }
  fprintf(f, "me picture loop (task #180 step 4c): %s; entries posted %ld (the thread woken %ld "
          "times), run by the main CPU itself %ld, dropped %ld; turns %ld (of %d vblanks); the loop: records %ld (copies as the transfer ended "
          "%ld, early %ld, walked from RAM %ld, over %ld), fills %ld (copies laid %ld), walks %ld (%ld records, started again %ld), thinning "
          "%ld, hooks %ld (lists %ld, frames begun %ld; waits for the GE %ld, results full %ld); lists to the GE %u; take-backs %ld",
          mq_me ? "the loop on the ME (step 4d-4)" : mq_hi ? "the thread's priority above the main thread's" : "the thread's priority below the "
          "main thread's", mq_posted, mq_signals, mq_selfrun, mq_dropped, mq_turns, mq_turn, mq->n_rec, mq->n_now, mq->n_early,
          mq->n_ram, mq->n_over, mq->n_fill, mq->n_lay, mq->n_walk, mq->n_walked, mq->n_restart, mq->n_thin, mq->n_hook, mq->n_list,
          mq->n_begin, mq->n_gewait, mq->n_resfull, (unsigned)mq_subm, mq_tbk_n);
  if (mq_tbk_n) fprintf(f, " (%ld warm, %ld cold; the last at vb %ld, waiting for %s; the loop %s, at %#x: %s; %ld entries not done, %ld records "
                        "dropped)", mq_tbk_warm, mq_tbk_cold, mq_tbk_vb, mq_wname[mq_tbk_why], mq_tbk_hung ? "stopped by the hang test" :
                        "not in the hang test", (unsigned)mq_tbk_at, mq_at_name(mq_tbk_at), mq_tbk_left, mq_tbk_rec);
  fprintf(f, "\nme picture loop: the main CPU's waits for the loop (times, ms):");
  for (int k = 0; k < MW_N; k++) fprintf(f, "%s %s %ld (%llu)", k ? ";" : "", mq_wname[k], mq_wn[k], (unsigned long long)(mq_wus[k] / 1000));
  fprintf(f, "\nme picture loop: the longest stretch without a heartbeat while the main CPU waited: %llu us (vb %ld, the loop at %#x: %s); "
          "the main CPU's rounds %ld, delays failed %ld (the first %08x)\n", (unsigned long long)mq_gap_max, mq_gap_vb, (unsigned)mq_gap_at,
          mq_at_name(mq_gap_at), mq_w_rounds, mq_dly_bad, (unsigned)mq_dly_e);
  if (mq_m4)   /* step 4d */
    fprintf(f, "me picture loop (step 4d, mode 4): records walked from RAM by the main CPU itself %ld; hand-overs to the main CPU (SYNC) %ld "
            "(the loop's %ld); the ME's cache steps (the stand-in's counts): the whole cache %ld, ranges %ld (%llu KB); the main CPU's: "
            "write-backs %ld, write-backs with the cache dropped %ld; a walk's extents at most %d (vb %ld, of %d), its arena bytes at most %u KB "
            "(vb %ld, of %u KB); the picture %s; the ME's heap requests (step 4d-2): more %ld, give back %ld, resize %ld (refused %ld; the "
            "loop's count %ld); the lines (step 4d-3): problems at set-up %d, the heap's regions moved by realloc and copied to a line start "
            "%ld, left off one %ld\n", mq_own, mq_syncs, mq->n_sync, mq_mc_all, mq_mc_inv,
            (unsigned long long)(mq_mc_inv_b >> 10), mq_c_wb, mq_c_wbinv, mq_ext_max, mq_ext_max_vb, two_an ? two_xn : xmc.ext_cap, (unsigned)(mq_arena_max >> 10),
            mq_arena_max_vb, (unsigned)((two_an ? two_an : xmc.arena_n) >> 10), mq_dead ? "stopped (a cold take-back)" : "made to the end", mq_rq_n[1], mq_rq_n[2],
            mq_rq_n[3], mq_rq_fail, mq->n_rq, mq_ln_bad, me_realn, me_misal);
  if (mq_m4)
    fprintf(f, "me picture loop (step 4d-4): the loop's cache steps through the hook: the whole cache %ld, ranges %ld (%llu KB)\n", mq->n_call,
            mq->n_cinv, (unsigned long long)(mq->n_cinv_b >> 10));
#ifdef MGS2_ME_REAL
  if (mq_m4) {
    fprintf(f, "me picture loop (step 4d-4): the loop on the ME %s", !mq_me_try ? (mq_emu() ? "not asked for (the emulator)" : "not asked "
            "for (MGS2_ME_PIC_ME=0)") : mq_mc ? "started" : "NOT started");
    if (mq_mc) fprintf(f, " (control block %08x, region %08x; its heartbeat %u, state %u, boots %u)", (unsigned)(uintptr_t)mq_mc,
                       (unsigned)(uintptr_t)mq_mreg, (unsigned)mq_mcu->h.heartbeat, (unsigned)mq_mcu->h.state, (unsigned)mq_mcu->h.boots);
    else if (mq_me_try) fprintf(f, " (%s)", mq_me_why);
    fprintf(f, "; the arena and the view's buffers %s; the ME's exception: ", mq_edram ? "in its eDRAM" : "in main RAM");
    if (mq_me_exc) fprintf(f, "Cause %08x (code %u), EPC %08x, BadVAddr %08x (seen at vb %ld)%s", (unsigned)mq_mx[0], (unsigned)((mq_mx[0] >> 2) & 31),
                           (unsigned)mq_mx[1], (unsigned)mq_mx[2], mq_mx_vb, mq_me_tbk_exc ? ": the take-back's cause" : "");
    else fprintf(f, "none");
    fprintf(f, "; its stop: %s\n", mq_me_ack < 0 ? "none (a take-back held it, or it never started)" : mq_me_ack ? "it answered the exit "
            "request" : "no answer in 50 ms, held in reset");
    /* gl28: the rest's totals (MGS2_ME_REST): the ME's counts since its start, the main CPU's kicks and clocks */
    if (!rs_cfg) ;   /* (MGS2_ME_REST=0: no line, as gl27) */
    else if (!mq_mcu) fprintf(f, "gl28 me rest totals: MGS2_ME_REST %d: the ME did not run, no rests\n", rs_cfg);
    else { volatile XmeMeCtl *c = mq_mcu; uint64_t kk = rs_kicks + rs_rekicks;
      fprintf(f, "gl28 me rest totals: MGS2_ME_REST %d; main turns with a rest line %ld; rests %u (woken in HALT %u, before HALT %u, no "
              "interrupt %u, given up %u); kicks %llu + rekicks %llu (%.1f us mean, %.1f max, %.3f s in all); stray wakes at least %u (the "
              "handler's wakes %u; turns with more handler wakes than kicks %ld, %llu wakes beyond: 1 in a turn can be a kick left pending "
              "from before it); ME asleep %.1f s (main clock), Count ticks in those rests %llu = %.4f of 331.31 x main us; Count's test (the "
              "%ld turns with one rest woken in HALT): %.1f s, ticks %llu = %.4f; switch waits %.1f ms in all, given up %ld, the ME awake at "
              "%ld switches; in the ME turns: rests begun %llu, kicks %llu; flags seen %08x; Sony words %08x %08x %08x %08x %08x\n", rs_cfg,
              rs_turns, (unsigned)c->sleeps, (unsigned)c->halted, (unsigned)c->pending,
              (unsigned)c->nowake, (unsigned)c->late, (unsigned long long)rs_kicks, (unsigned long long)rs_rekicks,
              kk ? (double)rs_kick_us / (double)kk : 0.0, (double)rs_kick_max, rs_kick_us / 1e6, (unsigned)rs_stray(), (unsigned)c->wakes,
              rs_xs_n, (unsigned long long)rs_xs,
              rs_asleep_us / 1e6, (unsigned long long)rs_ticks, rs_asleep_us ? (double)rs_ticks / (331.31 * (double)rs_asleep_us) : 0.0,
              rs_cl_n, rs_cl_us / 1e6, (unsigned long long)rs_cl_ticks, rs_cl_us ? (double)rs_cl_ticks / (331.31 * (double)rs_cl_us) : 0.0,
              rs_sw_us / 1000.0, rs_slow, rs_awake_sw, (unsigned long long)rs_me_sl, (unsigned long long)rs_me_k, (unsigned)c->flags_or,
              (unsigned)c->rpc[0], (unsigned)c->rpc[1], (unsigned)c->rpc[2], (unsigned)c->rpc[3], (unsigned)c->rpc[4]); } }
#endif
}
/* gl28 (MGS2_BAT_REC=1): the picture loop's state for psp/v26me_bat.c's 5-s line, read by the battery thread (user mode, priority 0x11,
   above the main thread's 0x20): plain reads of this file's counters and uncached reads of the words the ME and the main CPU share; no
   lock, no call, nothing written.  v[0] bits: 1 the loop is set up, 2 it runs on the ME, 4 the ME held after a take-back, 64 the ME
   stopped at the run's end, 8 the main CPU makes the picture now (its turn), 16 the picture stopped (a cold take-back), 32 the ME may
   rest now (gl28); v[1] head,
   v[2] tail, v[3] the loop's heartbeat, v[4] its place word, v[5] the ME's own heartbeat (its waits), v[6] asleep, v[7] rests begun,
   v[8] the rest word, v[9] the ME's state, v[10] lists given to the GE, v[11] the GE's finishes (callback), v[12] lists held for the
   screen (gl27), v[13] lists the loop made, v[14] turns, v[15] kicks (gl28, rekicks included) */
void xl_psp_rec(uint32_t v[16])
{
  XmeCtx *q = mq; int i;
  for (i = 0; i < 16; i++) v[i] = 0;
  if (!q) return;
  { volatile XmeSh *s = XS(q);
    v[0] = 1u | (mq_self ? 8u : 0u) | (mq_dead ? 16u : 0u);
    v[1] = s->head; v[2] = s->tail; v[3] = s->heart; v[4] = s->at; v[13] = s->listed; }
  v[10] = mq_subm; v[11] = ge_fin_n; v[12] = (uint32_t)(flip_hold[0] + flip_hold[1]); v[14] = (uint32_t)mq_turns;
#ifdef MGS2_ME_REAL
  { volatile XmeMeCtl *c = mq_mcu;
    v[0] |= (mq_me > 0 ? 2u : 0u) | (mq_me < 0 ? (mq_tbk_n ? 4u : 64u) : 0u) | (rs_on ? 32u : 0u);
    if (c) { v[5] = c->h.heartbeat; v[6] = c->asleep; v[7] = c->sleeps; v[8] = c->rest; v[9] = c->h.state; }
    v[15] = (uint32_t)(rs_kicks + rs_rekicks); }
#endif
}
#else
void xl_psp_rec(uint32_t v[16]) { for (int i = 0; i < 16; i++) v[i] = 0; }   /* gl28: no picture loop in this build */
#endif
#ifndef MGS2_GL   /* task #180 step 4b-2: the guarded game units' and runtime's calls, idle without the picture */
int g_g0_on;
void rt_g0_pre(uint32_t a, uint32_t n, int fn) { (void)a; (void)n; (void)fn; }
void rt_me_guard(uint32_t a, uint32_t n) { (void)a; (void)n; }
#endif
void xlive_chk_dma(const uint8_t *mem, const uint8_t *spr, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int one)
{
#if defined(ME_WATCH) && defined(MGS2_GL)
  if (g_mpw_armed) rt_me_watch(1, (uint32_t)ch, madr);
#endif
  if (!(g_opt & OPT_XL)) { if (state == 1) state = 2; return; }
  cfg();
#ifdef MGS2_GL
  if (mq_on) { p9_dma[p9_top()]++; mq_service(); }   /* task #180 step 4c: a list the GE finished shown, the ME's next list to the GE (gl39: counted) */
#endif
  if (!mp_want) {   /* (task #180: with the split each record's switches are set as it is walked, ge/xlme.c xm_switch) */
  { static int ud = 1; int m = (g_opt & OPT_XL_VMUD) != 0; if (m != ud) { xl_vm_mode(m); ud = m; } }   /* (xlate.c's default: 1) */
  { static int v4 = 1; int m = (g_opt & OPT_XL_V48) != 0; if (m != v4) { xl_ud_v48(m); v4 = m; } }   /* (xlate.c's default: 1) */
  { static int eg = 1; int m = (g_opt & OPT_XL_EAGER) != 0; if (m != eg) { xl_ud_eager(m); eg = m; } }   /* sb8 (xlate.c's default: 1) */
  { static int mk = 1; int m = (g_opt & OPT_XL_MARK) != 0; if (m != mk) { xl_ud_mark(m); mk = m; } }    /* sb8 (xlate.c's default: 1) */
  { static int ln = 0; int m = ((g_opt & OPT_XL_LANE) ? 1 : 0) | ((g_opt & OPT_XL_LANE2) ? 2 : 0) | ((g_opt & OPT_XL_LANE3) ? 4 : 0) |
                              ((g_opt & OPT_XL_LANE4) ? 8 : 0);   /* sb1, sb2, sb6, sb5 lanes (xlate.c starts with them off) */
    if (m != ln) { xl_lane(m != 0); g_xl_lane = m; ln = m; } }
#ifdef SB_NEED   /* sb1 direct reads: exact, but slower in the model (renderer_notes 11.22), so only in builds made with -DSB_NEED */
  { static int nd = 0; int m = (g_opt & OPT_XL_NEED) != 0; if (m != nd) { xl_need(m); gl_need = m; nd = m; } }   /* (off at start) */
#endif
  if (map) { static int am = 1, cm = 1; int a = (g_opt & OPT_GU_MEMO) != 0, c = (g_opt & OPT_GU_CLUT) != 0;   /* (gsup2.c's defaults: 1) */
    if (a != am) { gu_area_memo(map, a); am = a; }
    if (c != cm) { gu_clut_memo(map, c); cm = c; } }
  if (map) { static int li = 1; int m = (g_opt2 & OPT2_GU_LDID) != 0; if (m != li) { gu_ld_ident(map, m); li = m; } }   /* sb9 0b (gsup2.c's default: 1) */
#ifdef MGS2_GL
  if (tk) { static int tb = 1; int m = (g_opt & OPT_TK_BLK) != 0; if (m != tb) { tk_blk(tk, m); tb = m; } }   /* (texcache.c's default: 1) */
  if (tk) { static int tf = 1; int m = (g_opt & OPT_TK_FLAGS) != 0; if (m != tf) { tk_flags(tk, m); tf = m; } }   /* (texcache.c's default: 1) */
  if (tk) { static int um = 1; int m = (g_opt2 & OPT2_TX_UMEMO) != 0; if (m != um) { tk_umemo(tk, m); um = m; } }   /* sb9 0a (texcache.c's default: 1) */
  if (tk) { static int uf = 1; int m = (g_opt2 & OPT2_TX_FRONT) != 0; if (m != uf) { tk_ufront(tk, m); uf = m; } }   /* sb10 T1 (texcache.c's default: 1) */
#endif
  }
  if (state != 1) {
    if (state < 0) return;
    if (!state) {
      M.mem = mem; M.mem_size = XL_GSZ; M.spr = spr; M.seen = 0; M.fast = 1;
      { const char *e = mgs2_getenv("MGS2_ME_HEAP_KB"); me_hn = (size_t)(e && atoi(e) >= 64 ? atoi(e) : ME_HEAP_KB) << 10;   /* task #149 */
        if (!(me_hb = memalign(64, me_hn)) || me_heap_init(me_hb, me_hn)) {   /* (step 4d-3: on whole lines; me_hn is whole KB) */
          state = -1; fprintf(stderr, "translator: no memory for the picture heap (%u KB), off\n", (unsigned)(me_hn >> 10)); return; }
        { const char *g = mgs2_getenv("MGS2_ME_HEAP_GROW_KB"); if (g) me_grow = (size_t)atoi(g) << 10; }
        me_heap_grow(me_more, me_less);
        { const char *z = mgs2_getenv("MGS2_ME_HEAP_RESIZE"); if (!z || atoi(z)) { me_heap_resize(me_resize); me_rs_on = 1; } }
        fprintf(stderr, "translator: the picture code built ME-safe (task #149): its heap a region of %u KB at %08x (MGS2_ME_HEAP_KB), more "
                "from newlib's heap when nothing fits (regions of at least %u KB: MGS2_ME_HEAP_GROW_KB), given back when empty, %s\n",
                (unsigned)(me_hn >> 10), (unsigned)(uintptr_t)me_hb, (unsigned)(me_grow >> 10),
                mgs2_getenv("MGS2_ME_HEAP_RESIZE") && !atoi(mgs2_getenv("MGS2_ME_HEAP_RESIZE")) ? "not resized (MGS2_ME_HEAP_RESIZE=0)"
                : "resized by realloc when one block fills it"); }
#ifdef MGS2_GL
      xmp.m = &M;   /* task #180 step 4c: in mode 3 the map reads through the ME's records' views (ge/xlme.c xm_ram, as ram()) */
      if (!(map = mp_want >= 3 ? gu_new(xm_ram, &xmp) : gu_new(ram, 0))) { state = -1; fprintf(stderr, "translator: no memory for the texture map, off\n"); return; }
#else
      if (!(map = gu_new(ram, 0))) { state = -1; fprintf(stderr, "translator: no memory for the texture map, off\n"); return; }
#endif
      gu_keys(map, 1);
      xl_on_call(on_call, 0); xl_on_prim(on_prim, 0); xl_uploads(map);
      cfg();
#ifdef MGS2_GL
      xl_on_pass(on_pass, 0); xl_on_flush(on_flush, 0);
      gl_setup();
      mp_setup();   /* task #180 */
      nge_psp_setup();   /* native step 1 (MGS2_NGE): when mq_setup did not (no ME) */
#endif
      xl_reset(); l_words = l_calls = l_ad = 0;
#ifdef MGS2_GL
      if (mq_on) { void mq_start(void); mq_start(); }   /* task #180 step 4c: the loop's thread, after the translator's reset */
#endif
    } else { nrestart++;
#ifdef MGS2_GL
      if (mp_on) mp_restart_due = 1;     /* task #180: the start again comes with the next record's late walk (mp_late) */
      else
#endif
      { sum();
#ifdef MGS2_GL
        gu_zero_empty(map, 0);           /* the map did not see the transfers while OPT_XL was off: what it never saw written is not known */
#endif
        xl_reset(); l_words = l_calls = l_ad = 0; }
    }
    state = 1;
  }
  if (skip_now) { sk_tr++; return; }   /* after reduced calls only: the shown frame does not need it */
  ntransfer++;
#ifdef MGS2_GL
  if (thin_due && (mq_on ? mq_ft_on : glf.ft_on)) {   /* task #149: the thinning's scan on the host's side, at the frame's first walked
                                    transfer (it was gl_call's at the frame's first effect call); the ME will not read the game's RAM for it */
    thin_due = 0; HOST_ENTER(HOST_GL);   /* task #180: gl17's scan (the streaks' too with C4 on) and the rain's sprite blocks */
    if (mq_on) mq_thin();   /* step 4c: the ranges to the ME through the slot */
    else { const int n = fx_thin_scan(glf.ft, glf.ram, glf.h.c4); gl_thin(&glf, glf.ft->r, n, glf.ft->keep[3], glf.ft->d, glf.ft->nd);
      gl_bd_ranges(&glf, glf.ft->b, glf.ft->all ? glf.ft->nb : 0); }   /* gl38 */
    HOST_LEAVE(HOST_GL); }
  if (state < 0) return;   /* task #180 step 4d: the picture stopped in a wait above (mode 4's cold take-back) */
#endif
#ifdef MGS2_GL
  { int chi = -1;   /* gl38: the transfer's chain (ge/natsplit.h), and the frame's mask: a kind's transfer is left out here, never queued */
    if (mq_on && (g_bd_cfg || mq->ns_on)) chi = ns_chain_at(mem, ch, tadr);
    bd_chi = chi;
    if (bd_cur && bd_drop_chain(bd_cur, chi)) {
      bd_tr[chi < 0 || chi >= NS_CH ? NS_CH : chi]++;
      if (bd_cur & BD_ALL) for (int b = 0; b < 2; b++) { const NatGrp *g; int n = nat_pend(b, &g);   /* the picture's native groups: none will be drawn */
        if (n > 0) { g_nat[b].cur += (uint32_t)n; bd_grp += n; }
        { const NatP6 *q; int m = np6_pend(b, &q); if (m > 0) { g_np6[b].cur += (uint32_t)m; bd_grp += m; } } }   /* (step 2: the people's) */
      return; } }
  if (mp_on) {   /* task #180: queued; its copy as it ends (xlive_chk_end) */
    MpRec *r; int own;
    if (mp_nrec == MP_NREC) { mp_drain_full++; mp_late(2); if (!mp_on) return; }
    r = &mp_rec[mp_nrec++]; r->ch = ch; r->chcr = chcr; r->madr = madr; r->qwc = qwc; r->tadr = tadr; r->opt = g_opt; r->opt2 = g_opt2;
    r->lanes = one_mode && one ? ((g_opt & OPT_XL_LANE) ? 1 : 0) | ((g_opt & OPT_XL_LANE2) ? 2 : 0) | ((g_opt & OPT_XL_LANE3) ? 4 : 0) | ((g_opt & OPT_XL_LANE4) ? 8 : 0) : 0;
    r->restart = mp_restart_due; mp_restart_due = 0; r->x0 = 0; r->nx = -1;
    r->t = ++mp_seg; r->filled = 0; r->h0 = 0; r->nh = -1;   /* step 4b-2 */
    r->nb = -1; r->ng0[0] = r->ng0[1] = r->ngn[0] = r->ngn[1] = 0; r->noff = 0;   /* native step 1 */
    r->p0[0] = r->p0[1] = r->pn[0] = r->pn[1] = 0;   /* step 2 */
    r->chi = (int16_t)bd_chi;   /* gl38 */
    own = mq_m4 && mq_self;   /* step 4d: walked from RAM as it ends (mq_chk_end): no image marks, no guards */
    xmc.nmk = 0; xmc.mk_full = 0; g_xl_img = !own; g_xl_end = 1; mp_queued++;
    if (mp_mode == 2 && !own) g_g0_on = 1;   /* 4b-2: a guarded write in the walk is counted (none is expected) */
    return; }
#endif
  if (one_mode && one) { none++; g_xl_one = 1; return; }   /* the exact walk feeds it, then xlive_one_end */
  xm_tsplit(0, 0, 0);   /* gl26 */
  HOST_ENTER(HOST_XL); xlv_dma(&M, ch, chcr, madr, qwc, tadr); HOST_LEAVE(HOST_XL);
  sum();
}
void xlive_one_end(void) { xl_flush(); g_xl_one = 0; sum(); }   /* the game may change this RAM before the next transfer */

void xl_psp_report(FILE *f)
{
  if (!f) return;
  if (!state) { fprintf(f, "translator: never ran (OPT_XL off)\n"); return; }
  { const MeHeapStats *h = me_heap_stats();   /* task #149 */
    fprintf(f, "picture heap: %u KB in %d regions now (the first %u KB; %u KB added now, %u KB at most: the hook gave %ld regions, took back "
            "%ld, resized %ld, refused %ld), peak %u KB in use (extent %u KB "
            "in the first), now %u KB; blocks peak %ld; allocations %ld, frees %ld, reallocs %ld (moved %ld); refused %ld (first %u bytes); check %d; "
            "depth by zk %ld\n", (unsigned)(h->size >> 10), h->pools, (unsigned)((h->size - h->added) >> 10), (unsigned)(h->added >> 10),
            (unsigned)(h->added_peak >> 10), h->grows, h->returned, h->resized, h->grow_fails, (unsigned)(h->peak >> 10), (unsigned)(h->extent >> 10),
            (unsigned)(h->used >> 10), h->blocks_peak, h->allocs, h->frees,
            h->reallocs, h->moved, h->fails, (unsigned)h->fail_first, me_heap_check(), me_faults.zk);
    fprintf(f, "picture heap: the hooks' %d events (vb: + KB given at, ! refused, - taken back, = resized in place, > resized and moved "
            "from, ? resize refused; newlib's break after it; its free top given back after a resize %ld times)", me_nev_all, me_trim);
    for (int k = 0; k < me_nev; k++) {
      fprintf(f, "%s vb %ld: %c", k ? ";" : ":", me_ev[k].vb, me_ev[k].op);
      if (me_ev[k].op != '-') fprintf(f, "%u.%u KB ", me_ev[k].n >> 10, (me_ev[k].n & 1023) * 10 >> 10);
      if (me_ev[k].op == '+' || me_ev[k].op == '!') fprintf(f, "(for %u B) ", me_ev[k].w);
      fprintf(f, "at %08x", me_ev[k].at);
      if (me_ev[k].op == '>') fprintf(f, " from %08x", me_ev[k].from);
      fprintf(f, " (%08x; room %u KB, top %u KB, free %u KB)", me_ev[k].brk, me_ev[k].room >> 10, me_ev[k].top >> 10, me_ev[k].fr >> 10); }
    fprintf(f, "\n");
    fprintf(f, "picture heap (gl41): newlib's room at the hooks' events (after each; room = above newlib's break, top = its free top chunk, "
            "free = all its free bytes): the lowest room + top %u KB, at vb %ld (event %c)%s\n", me_room_min == ~0u ? 0u : me_room_min >> 10, me_room_vb,
            me_room_op ? me_room_op : '-', me_room_min == ~0u ? " (no event)" : "");
    if (tk) {   /* gl41 heap fix (b): the texture table's allocation: the last event whose region holds the table now */
      uint32_t t = (uint32_t)(uintptr_t)tk_table(tk); int j = -1;
      for (int k = 0; k < me_nev; k++) if ((me_ev[k].op == '+' || me_ev[k].op == '=' || me_ev[k].op == '>') && t >= me_ev[k].at && t < me_ev[k].at + me_ev[k].n) j = k;
      if (j >= 0) fprintf(f, "picture heap (gl41): the texture table (at %08x, %ld entries) came with the event at vb %ld (%c %u KB): newlib's room then "
                          "%u KB above the break + %u KB free top = %u KB\n", (unsigned)t, tk_stats(tk)->tab_cap, me_ev[j].vb, me_ev[j].op,
                          me_ev[j].n >> 10, me_ev[j].room >> 10, me_ev[j].top >> 10, (me_ev[j].room + me_ev[j].top) >> 10);
      else fprintf(f, "picture heap (gl41): the texture table (at %08x, %ld entries) came from a region given before (no newlib event of its "
                   "own)%s\n", (unsigned)t, tk_stats(tk)->tab_cap, me_nev_all > me_nev ? "; or from an event past the 48 logged" : ""); } }
  if (state == -1) { fprintf(f, "translator: no memory for the texture map, it did not run\n"); return; }
#ifdef MGS2_GL
  if (state == -2) fprintf(f, "translator: stopped at vb %ld with the picture (task #180 step 4d: mode 4's cold take-back); the counts below "
                           "are up to there\n", mq_dead_vb);
#endif
#ifdef MGS2_GL
  if (mq) {   /* task #180 step 4c: the ME's loop summed its walks, its callbacks counted, its copies were laid */
    t_qw += mq->t_qw; t_words += mq->t_words; t_calls += mq->t_calls; t_ad += mq->t_ad; ncall += xmp.calls; nprim += xmp.prims;
    xmc.fills += mq->fc.fills; xmc.laid += mq->fc.laid; xmc.laid_lines += mq->fc.laid_lines; xmc.laid_b += mq->fc.laid_b; }
#endif
  const XlStats *S = xl_stats(); const GfGs *F = xl_front(); const GuStats *U = gu_stats(map);
  fprintf(f, "translator: one walk %s, shown frames only %s: %ld vblanks after reduced calls only, %ld transfers not handed over\n",
          one_mode ? "on" : "off", skip_mode ? "on" : "off", sk_vb, sk_tr);
  fprintf(f, "translator: %ld transfers (%ld fed by the exact walk: their tags and quadwords not counted), %ld tags, %llu quadwords; %llu VIF1 words, %llu VU1 calls (%ld callbacks), %llu A+D packets; "
          "front end %ld primitives; texture map %ld transfers (%ld not modelled, %ld in pieces), %ld CLUT loads, %ld IMAGE quadwords "
          "without a transfer, %ld block entries dropped; started again %ld times; since the last start: %ld calls without an entry, %ld "
          "A+D packets without an end, GIF tags path 1/2/3 %ld/%ld/%ld, %ld unknown VIF commands, %ld zero-area primitives\n", ntransfer,
          none, M.tags, (unsigned long long)t_qw, (unsigned long long)t_words, (unsigned long long)t_calls, ncall, (unsigned long long)t_ad, nprim,
          U->xfers, U->xfer_unmod, U->xfer_split, U->loads, U->image_orphan, U->dropped, nrestart, S->calls_noentry, S->ad_bad,
          S->gif_tags[1], S->gif_tags[2], S->gif_tags[3], S->unk, F->zero_area);
#ifdef MGS2_GL
  if (mp_on || mq_dead) {   /* task #180 (4d-2: after mode 4's cold stop too, its counts up to there) */
    fprintf(f, "me picture split: %ld records queued, walked in %ld runs at %ld hooks; runs early: the queue full %ld, no room %ld; walked at once "
            "from RAM %ld; copies %ld (not made: ranges %ld, extents %ld, arena %ld, chains %ld), %ld extents (at most %d a copy), %llu KB copied, "
            "%llu KB of image data left in RAM (%ld pieces); a step's copy at most %u KB (vb %ld); steps by its KB: <512 %ld, <1024 %ld, <1536 %ld, "
            "<2048 %ld, <2560 %ld, <3072 %ld, <3584 %ld, <4096 %ld, more %ld; ranges at most %d / %d, marks %d; image runs %ld (in the scratchpad "
            "%ld, under 16 bytes %ld, lists full %ld)\n", mp_queued, mp_late_runs, mp_hooks, mp_drain_full, mp_drain_room, mp_direct, xmc.copies,
            xmc.fails[1], xmc.fails[2], xmc.fails[3], xmc.fails[4], xmc.exts, xmc.nx_max, (unsigned long long)(xmc.bytes >> 10),
            (unsigned long long)(xmc.marked >> 10), xmc.rb_cut, (unsigned)mp_step_max, mp_step_max_vb, mp_hist[0], mp_hist[1], mp_hist[2], mp_hist[3],
            mp_hist[4], mp_hist[5], mp_hist[6], mp_hist[7], mp_hist[8], xmc.na_max, xmc.nb_max, xmc.nmk_max, mp_img_runs, mp_img_spr, mp_img_small, mp_img_full);
    fprintf(f, "me picture split: reads through the view (from the copy / from RAM / split or refused): tags %ld %ld %ld, runs %ld %ld %ld, "
            "the lanes' data %ld %ld %ld, the map's %ld %ld %ld (put together in the fixed buffers %ld (%u KB each), in the picture heap %ld)\n",
            mp_view.in[XV_TAG], mp_view.out[XV_TAG], mp_view.part[XV_TAG], mp_view.in[XV_SPAN],
            mp_view.out[XV_SPAN], mp_view.part[XV_SPAN], mp_view.in[XV_LANE], mp_view.out[XV_LANE], mp_view.part[XV_LANE], mp_view.in[XV_RAM],
            mp_view.out[XV_RAM], mp_view.part[XV_RAM], mp_view.n_fix, (unsigned)(mp_view.fix_n >> 10), mp_view.n_tmp);
    if (mp_mode == 2) {   /* step 4b-2 */
      long st = 0, sk = 0, du = 0, lg = 0, wk = 0;
      for (int k = 1; k <= 12; k++) { st += mg_st[k]; sk += mg_skip[k]; du += mg_dup[k]; lg += mg_log[k]; wk += mg_walk[k]; }
      fprintf(f, "me picture deadline: from vb %ld: copies filled at the deadline %ld, early %ld / %ld / %ld (the log full %ld times / the "
              "queue full / no room), as the transfer ended %ld (before vb %ld) and %ld (scratchpad bytes); log entries laid %ld (%ld on the "
              "copies' lines, %llu bytes), at most %d at once (of %d); G0 guards while on: %ld stores (%ld in a walk), lines out of the filter "
              "%ld, in the log already %ld, logged %ld; the runtime's writes while on: %ld (%llu KB, %ld in a walk), lines out of the filter "
              "%ld, in the log already %ld, logged %ld\n", mp_dl, mp_fill_dl, mp_fill_early[0], mp_fill_early[1], mp_fill_early[2],
              mp_full_log, mp_fill_end[0], mp_dl, mp_fill_end[1], xmc.laid, xmc.laid_lines, (unsigned long long)xmc.laid_b, mp_nlog_max,
              mp_logcap, st, wk, sk, du, lg, mg_st[0], (unsigned long long)(mg_rtb >> 10), mg_walk[0], mg_skip[0], mg_dup[0], mg_log[0]);
      fprintf(f, "me picture deadline: G0 function, stores while on / lines logged:");
      for (int k = 1; k <= 12; k++) fprintf(f, " %d %ld/%ld", k, mg_st[k], mg_log[k]);
      fprintf(f, "\n");
      if (mp_chk) mpc_flush();   /* b3: the last window */
      if (mp_chk) fprintf(f, "me picture check: %ld lines of the deadline copies compared with the bytes as their transfers ended (%ld of them "
                          "holding bytes the transfers read, the others only the margins after tags), %ld differ (in %ld copies): %ld in bytes "
                          "the transfers read (b4: by 16-byte quadword; %ld of them in the margin too), %ld only in a margin; %ld copies not "
                          "checked (hashes full); second walk failed %ld times\n",
                          mp_chk_lines, mp_chk_lrd, mp_chk_diff, mp_chk_rec, mp_chk_drd, mp_chk_dboth, mp_chk_dmg, mp_chk_skip, mp_chk_rdfail);
      if (mp_nolay || mp_scr) fprintf(f, "me picture diagnostic b5: the guards' log %s; margin quadwords turned over %ld (in %ld copies)\n",
                                      mp_nolay ? "not laid" : "laid", mp_scr_q, mp_scr_rec);
#ifdef ME_WATCH
      if (mpw_on) {
        fprintf(f, "me picture watch: %d lines watched; %ld changes seen at the checkpoints (%ld lines printed), %ld lines let go as a guard logged "
                "them; the check's read lines that differ: %ld seen first (then watched), %ld changed in RAM at a checkpoint, %ld with no change "
                "seen and RAM as at the plan (the copy itself), %ld with no change seen and RAM changed since, %ld let go by a guard, %ld planned "
                "by another copy\n", mpw_n, mpw_chg, mpw_prn, mpw_logd, mpw_d_new, mpw_d_chg, mpw_d_same, mpw_d_ram, mpw_d_logd, mpw_d_other);
        fprintf(f, "me picture watch (b4): %ld of the changes in quadwords the transfer reads, %ld only in the margin\n", mpw_chg_rd, mpw_chg - mpw_chg_rd);
        for (int i = 0; i < 20; i++) for (int j = 0; j < 20; j++) if (mpw_iv[i][j] | mpw_ivm[i][j])
          fprintf(f, "me picture watch: changes between %s and %s: %ld in read quadwords, %ld only in the margin\n", mpw_name[i], mpw_name[j],
                  mpw_iv[i][j], mpw_ivm[i][j]);
        for (int i = 0; i < mpw_n; i++) fprintf(f, "me picture watch: line %07x\n", (unsigned)(mpw[i].l << 6)); }
#endif
    }
    if (nge_k) nge_end_line();   /* native step 1 */
    if (mq) mq_report(f); }   /* step 4c */
  if (gl_ok < 0) { fprintf(f, "picture: none (no memory or no set-up)\n"); return; }
  if (!gl_ok) { fprintf(f, "picture: never set up\n"); return; }
  if (gl_ok == 4) fprintf(f, "picture: STOPPED at vb %ld: the ME's picture taken back cold (task #180 step 4d, mode 4); the totals are up "
                          "to there\n", mq_dead_vb);
  if (gl_ok == 3)
    fprintf(f, "picture: STOPPED: the GE did not finish %s%ld in %u ms (sceGeDrawSync(1) %08x); the totals are up to there\n",
            gl_stop_vb < 0 ? "its set-up list" : "the list of the frame of vb ", gl_stop_vb < 0 ? 0 : gl_stop_vb, GL_GE_LIMIT / 1000, (unsigned)gl_stop_r);
  fprintf(f, "picture: %ld frames made (the first began at vb %ld), %ld shown, %ld saved; lists: %ld that did not fit, %ld batches left out, "
          "%llu words (the largest %ld), the largest aux %ld bytes, the largest list and aux together %ld bytes (of %u); digests %ld "
          "(vb %ld..%ld), all %016llx\n", gl_built, gl_first_vb, gl_shown, gl_shots, gl_listfail, gx_drop, (unsigned long long)gx_words_sum,
          gx_words_max, gx_aux_max, gx_both_max, GL_PSP_LIST, dig_n, dig_from, dig_to,
          (unsigned long long)dig_all);
  if (!mq) fprintf(f, "picture display (gl27): lists drawn into the shown buffer (the main CPU's own picture, no loop; counted only) %ld\n",
                   flip_race_dir);
  if (eye_on) fprintf(f, "gl28 eye: R pressed %ld times, let go %ld times (from vb %ld)\n", eye_n[1], eye_n[0], eye_vb0);   /* gl28d */
  if (live_from >= 0) fprintf(f, "gl28 live: captures %ld periodic, %ld at R presses (of %ld at most); not saved %ld\n", live_np, live_nr,
                              live_rmax, live_bad);
  if (dz_on) dz_report(f);   /* gl29 */
  gl_line(f, &glf.all, glf.all_dbytes, glf.all_tbytes, "picture: the frames:"); fprintf(f, "; memo answers that differed %ld\n", glf.memo_bad);
  gl_line(f, &glf.max, glf.max.dbytes, glf.max.tbytes, "picture: the largest of each in one frame:"); fputc('\n', f);
  { const TkStats *k = tk_stats(tk);
    fprintf(f, "picture: textures: %ld uses, textures %ld hits / %ld conversions, palettes %ld / %ld; render targets %ld, not known %ld, refused "
            "%ld (no room in the pool %ld), texels or entries the map could not give %ld; converted %lld KB, kept %ld (%lld KB, peak %lld KB), "
            "evicted %ld, the heaviest frame %lld KB; pool %u KB, budget %u KB\n", k->uses, k->hits, k->misses, k->pal_hits, k->pal_misses, k->rt,
            k->lost, k->unsup, k->nomem, k->unknown, (long long)(k->conv_bytes >> 10), k->live, (long long)(k->live_bytes >> 10),
            (long long)(k->peak_bytes >> 10), k->evicted, (long long)(k->peak_frame_bytes >> 10), (unsigned)gl_texkb, (unsigned)gl_texkb / 8 * 7);
    fprintf(f, "picture: texture conversions: %ld a block at a time, %ld handed back to the texel path; GS blocks read %ld, %ld of them whole "
            "from one upload\n", k->blk, k->blk_back, gu_stats(map)->blocks, gu_stats(map)->blocks_one);
    fprintf(f, "picture: the texture cache's table: grown %ld times, %ld entries now; newlib's free top given back after %ld of the "
            "growths (task #159); gl41: the table made 128 KB at its first entry, growths refused %ld; textures refused for want of memory: %ld (the pool %ld, the table %ld)\n",
            k->tab_grows, k->tab_cap, k->tab_trims, k->tab_fail, k->nomem + k->tab_fail, k->nomem, k->tab_fail);
    { const GuStats *u = gu_stats(map);   /* sb9: Stage C step 0 */
      fprintf(f, "picture: step 0: texture look-ups by upload %ld, answered %ld; not served: TEX0 / CLAMP %ld, TEXA %ld, pages %ld %ld %ld %ld %ld "
              "%ld, no slot %ld, entry gone %ld; slots filled %ld, geometry slots made %ld; CLUT loads by identity %ld (joined %ld; no block "
              "list %ld, records not final %ld)\n", k->um_asks, k->um_hits, k->um_geo, k->um_texa, k->um_sig[1], k->um_sig[2], k->um_sig[3],
              k->um_sig[4], k->um_sig[5], k->um_sig[6], k->um_slot, k->um_gone, k->um_fill, k->um_gnew, u->ld_ident, u->ld_ident_put,
              u->ld_ident_nolist, u->ld_ident_nosig);
      fprintf(f, "picture: step 1: the texture memo's front answered %ld look-ups, entries put %ld\n", k->uf_hits, k->uf_fill); } }   /* sb10 T1 */
  { const XcStats *x = xc_stats();
    fprintf(f, "picture: converters: df0 calls %ld, batches %ld, left out off the scissor %ld (%ld vertices); effect calls %ld / %ld, batches "
            "%ld / %ld; P06 calls %ld, batches %ld, passes 2 and 3 drawn %ld / %ld; P07 calls %ld, batches %ld, bad %ld (paths not seen %ld), "
            "in the exact form %ld, passes 2 and 3 drawn %ld / %ld\n", x->calls[0], x->batches[0], x->cut[0], x->cut_v[0], x->calls[1],
            x->calls[2], x->batches[1], x->batches[2], x->calls[3], x->batches[3], x->lit_pass[2], x->lit_pass[3], x->calls[4], x->batches[4],
            x->bad[4], x->face_unseen, x->face_exact, x->face_pass[2], x->face_pass[3]);
    fprintf(f, "picture: effect vertices %ld, %ld of them through the clamps with the fast path on\n", x->clip[0], x->clip[1]);
    { const long *u = x->cull_n;   /* cull study v3 (ge/xconv.h XcStats.cull_n) */
      fprintf(f, "natcull: totals (v3): effect calls tested %ld, left out %ld (%ld vertices); kept: vertex 0 inside %ld, no plane proven %ld, "
              "a bad attribute %ld, count %ld, out of the clamps' range %ld, thinned %ld; check mode: left-out calls converted %ld, DIFFERING "
              "%ld, kept calls that drew nothing %ld; vertices read %ld, %ld of them in calls kept; left-out vertices on the clamped path %ld, "
              "check mode: converted through the clamps %ld\n", u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[11], u[8], u[9], u[10], u[12], u[13], u[14], u[15]); }
    fprintf(f, "picture: step c: %ld calls held (%ld of them P07); flushes at a transfer's end %ld, for a call's room %ld, records full %ld, "
            "P07 copies full %ld, at a frame's end %ld; faults: held calls not placed %ld, passes after their call ran %ld\n", glf.held_n,
            glf.held7_n, glf.hflush[0], glf.hflush[1], glf.hflush[2], glf.hflush_log, glf.hflush[3], glf.held_bad, glf.held_late);
    fprintf(f, "picture: P07 held (sb10 T2): %ld calls; slots copied at the hold %ld, placed at the run %ld; vertices rebuilt %ld\n",
            xc_c7_held, xc_c7_snap, xc_c7_put, xc_c7_rb);
    if (x->c1_calls || x->c1_fall[0] || x->c1_fall[1] || x->c1_fall[2] || x->c1_fall[3] || gx_c1p)   /* sb11 C1 (g_opt2 OPT2_C1) */
      fprintf(f, "picture: P07 C1 (the GE's skinning and lighting): calls %ld, vertices %ld, by weights a vertex 1..8 %ld %ld %ld %ld %ld %ld "
              "%ld %ld, data %ld KB; to today's path: %ld (its fast form's conditions), %ld (the clamp, a light, the ambient), %ld (an offset, "
              "a slot's weights, a UV), %ld (a bone); in the GE lists: passes %ld, bone matrices sent %ld, their own words %ld; the depth range by the bound alone %ld calls, %ld vertices\n",
              x->c1_calls, x->c1_v, x->c1_nw[1], x->c1_nw[2], x->c1_nw[3], x->c1_nw[4], x->c1_nw[5], x->c1_nw[6], x->c1_nw[7], x->c1_nw[8],
              (long)(x->c1_bytes >> 10), x->c1_fall[0], x->c1_fall[1], x->c1_fall[2], x->c1_fall[3], gx_c1p, gx_c1b, gx_c1w, x->c1_zin, x->c1_zin_v);
    if (x->c2_calls || x->c2_fall[0] || x->c2_fall[1] || x->c2_fall[2] || x->c2_fall[3] || gx_c2p)   /* sb12 C2 (g_opt2 OPT2_C2) */
      fprintf(f, "picture: P06 C2 (the GE's skinning and lighting): calls %ld (skinned %ld), vertices %ld, data %ld KB; to today's path: %ld (its "
              "fast form's conditions), %ld (the clamp, a light's colour, the ambient), %ld (a weight, a tint, a UV), %ld (room); in the GE lists: "
              "passes %ld, bone matrices sent %ld, their own words %ld; the depth range by the bound alone %ld calls, %ld vertices\n",
              x->c2_calls, x->c2_skin, x->c2_v, (long)(x->c2_bytes >> 10), x->c2_fall[0], x->c2_fall[1], x->c2_fall[2], x->c2_fall[3], gx_c2p,
              gx_c2b, gx_c2w, x->c2_zin, x->c2_zin_v);
    if (x->c3v_calls || x->c3z_in)   /* sb14 step 3 (g_opt2 OPT2_C3V, OPT2_C3Z) */
      fprintf(f, "picture: df0 C3 (two passes, the depth range by the box): lane calls in two passes %ld (straight into the frame %ld), cut "
              "before the second pass %ld calls, %ld vertices; the depth range by the box alone %ld calls, %ld vertices\n",
              x->c3v_calls, x->c3w_calls, x->c3v_cut, x->c3v_cut_v, x->c3z_in, x->c3z_in_v);
    if (x->c4_calls[0] || x->c4_calls[1] || x->c4_calls[2])   /* sb15 step 4 (g_opt2 OPT2_C4) */
      fprintf(f, "picture: effects C4: calls sprites %ld, quads %ld, streaks %ld; bad %ld %ld %ld; batches %ld %ld %ld; nothing drawn %ld %ld %ld; "
              "sprites %ld (dropped %ld, corners moved %ld, drawn %ld); quads %ld (dropped %ld, clipped %ld, whole %ld; polygons %ld, their "
              "vertices %ld, empty %ld); streak segments %ld (dropped %ld, to the clipper %ld, in the strip %ld; lines %ld, the clipper's drops "
              "%ld); no room %ld; GE list: sprites %ld, line strips %ld, lines %ld\n",
              x->c4_calls[0], x->c4_calls[1], x->c4_calls[2], x->c4_bad[0], x->c4_bad[1], x->c4_bad[2], x->c4_batches[0], x->c4_batches[1],
              x->c4_batches[2], x->c4_empty[0], x->c4_empty[1], x->c4_empty[2], x->c4_n[0], x->c4_drop[0], x->c4_clip[0], x->c4_drawn[0],
              x->c4_n[1], x->c4_drop[1], x->c4_clip[1], x->c4_drawn[1], x->c4_poly, x->c4_poly_v, x->c4_poly_empty, x->c4_n[2], x->c4_drop[2],
              x->c4_clip[2], x->c4_drawn[2], x->c4_lines, x->c4_line_drop, x->c4_full, gx_c4s, gx_c4r, gx_c4l);
    if (glf.ft) { const FxThin *t = glf.ft;   /* task #114 */
      fprintf(f, "picture: thinned effects (sheets 1 in %d, splash 1 in %d): scans %ld, actors walked %ld, names read %ld, cache emptied %ld; "
              "blocks found: sheets %ld, splash %ld; bad %ld, over %ld; calls thinned %ld, their strips %ld, left out %ld (%ld triangles)\n",
              t->keep[0], t->keep[1], t->scans, t->actors, t->named, t->flushes, t->found[0], t->found[1], t->bad, t->over, x->thin_calls,
              x->thin_strips, x->thin_drop, x->thin_tris);
      if (t->keep[2] != 1 || t->keep_calls > 1)   /* task #154 (0: none, calls 1 in N: task #158) */
        fprintf(f, "picture: thinned streaks (1 in %d, calls 1 in %d): actors %ld; their tables' blocks %ld, entries 0 %ld, blocks not right %ld; ranges %ld; "
                "calls thinned %ld, their strips %ld, left out %ld (%ld segments); calls left out whole %ld\n", t->keep[2], t->keep_calls, t->found[2], t->tab_blocks, t->tab_empty,
                t->tab_bad, t->tab_ranges, x->thin_lcalls, x->thin_lstrips, x->thin_ldrop, x->thin_lseg, t->line_none + glf.line_none);   /* (task #180: the ME-safe gl_call counts on the frame) */
      if (!t->keep[3])   /* task #161 */
        fprintf(f, "picture: the rain's sprites left out: actors %ld; blocks %ld (not right %ld, over %ld); ranges %ld; calls left out whole %ld\n",
                t->found[3], t->spr_blocks, t->spr_bad, t->spr_over, t->spr_ranges, t->spr_none + glf.spr_none); } }
#endif
}
