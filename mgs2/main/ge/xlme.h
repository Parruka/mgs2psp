/* xlme.h: the picture's loop on the Media Engine, the parts the ME runs between the records and the translator (task #149; build order
   step 2 of me_handoff_design.md: 4.5 the Stage B lanes in the ME's walk, 4.8 the ME-safe picture code, "the picture half of
   psp/xl_psp.c").  Until step 2 these lived in the hosts: the late walk with the lanes in the x86 twin's glue (me13 b,
   boot/xlive_chk_g.c lw_walk), the switches, the translator's callbacks and a frame's end and start in psp/xl_psp.c and the twin's glue.
   As a ge/ file they are built with the rest of the picture code (-DMGS2_ME_SAFE, ge/me_rt.h) into the ME's unit, which the unit's check
   (tools/dev/me14/mecheck.py) reads, and the x86 twin's late path runs these same functions.

   - xm_walk: one transfer.  A VIF1 transfer the runtime's walk would have fed to the translator (the record's lanes not 0) goes through
     the late walk: ge/xlive.c's walk of the chain with runtime/ee_io.h vif1_chain_fast's lane block, whose recognizers lw_lanes.h holds
     (generated from the runtime's by tools/mk_lw.py), so the converters get the same groups and records as from the one walk; any other
     transfer through xlv_dma.  The walk reads the bytes through lw_q and lw_ptr only (the ME's copy will be read there).
   - xm_switch: the switches a record carries (g_opt and g_opt2 at its transfer), set as psp/xl_psp.c xlive_chk_dma sets them; follow
     says which ones follow the records (the PSP: all; the x86 twin: those its late path followed in step 1).
   - xm_call, xm_pass, xm_flush, xm_prim: the translator's callbacks' picture work (xm_on_*: the callbacks themselves, ctx the XmPic).
   - xm_end, xm_list, xm_begin: a frame's end (gl_end), its GE list (gx_frame and, when asked, its digest) and a frame's start (its
     switches, gl_begin).
   - xm_ram: the GS map's reader of EE RAM (gu_new's fn, ctx the XmPic).
   No printing and no system calls: the counts are the host's to print. */
#ifndef MGS2_XLME_H
#define MGS2_XLME_H
#include <stdint.h>
#include "xlive.h"
#include "xlate.h"
#include "gsup.h"
#include "texcache.h"
#include "gelive.h"
#include "gelist.h"
#include "xlview.h"   /* task #180 */
#include "natsnap.h"  /* native step 1 */
#include "natp06.h"   /* native step 2: the people's records (its data part: ge/xlme.c includes it again for the walk's) */
#include "nge.h"      /* step 2: NGE_NAT_ROOM (XME_NATBYTES) */
/* step 2 (gl41's rule; gl42: on gl41's layout): a record's copy of its native groups: gl41's ship copy (its groups, then their references:
   ge/nge.h NGE_NAT_ROOM), then on 16 bytes the people's groups (ge/xme.h XME_POFF) and their references (XME_P6ROFF); its room */
#define XME_R16(n) (((uint32_t)(n) + 15u) & ~15u)
#define XME_NATBYTES(b, ng, pn) (XME_R16(NGE_NAT_ROOM((b), (ng))) + XME_R16((uint32_t)(pn) * (uint32_t)sizeof(NatP6)) + 4u * (uint32_t)(pn))
#include "natsplit.h" /* gl34 */

#define XM_F_LANES 1u   /* the translator's lanes (OPT_XL_LANE, LANE2, LANE3, LANE4 within XmPic.lanes), OPT_XL_EAGER and OPT_XL_MARK */
#define XM_F_STEP0 2u   /* OPT2_TX_UMEMO, OPT2_GU_LDID, OPT2_TX_FRONT (with the x86 check modes chk_um, chk_li, chk_uf) */
#define XM_F_REST  4u   /* OPT_XL_VMUD, OPT_XL_V48, OPT_GU_MEMO, OPT_GU_CLUT, OPT_TK_BLK, OPT_TK_FLAGS (and OPT_XL_NEED in builds made with
                           -DSB_NEED, as psp/xl_psp.c) */
#define XM_F_CUT   8u   /* a frame's start takes OPT_GL_CUT too */
#define XM_F_ALL  15u   /* the PSP's: psp/xl_psp.c sets every one of these from g_opt and g_opt2 */

enum { XS_VMUD, XS_V48, XS_EAGER, XS_MARK, XS_LANE, XS_NEED, XS_GUMEMO, XS_GUCLUT, XS_LDID, XS_TKBLK, XS_TKFLAGS, XS_UMEMO, XS_UFRONT, XS_N };

typedef struct XmPic {
  /* the host's, before xm_init */
  XlvMem *m; GuMap *map; TkCache *tk; GlFrame *glf; GxList *gxl; const GxHost *gxh;
  unsigned follow;            /* XM_F_* */
  int lanes;                  /* the lanes the translator may take (1, 2, 4, 8: lanes 1 to 4; the PSP 15) */
  int lw_on;                  /* 1: the late walk with the lanes (xm_walk); 0: every transfer through xlv_dma */
  int chk_um, chk_li, chk_uf; /* x86 check modes: a step 0 or T1 bit's "on" is 1 + chk (0 on the PSP) */
  /* the state */
  int now;                    /* 1 from xm_begin to xm_end: the callbacks feed the frame */
  int sw[XS_N];               /* each switch as last set; -1: not known (set at the next record) */
  GxFrame gxf;                /* the last list's frame */
  uint64_t dig_frame;         /* the last list's frame digest (gl_digest), when xm_list was asked for it */
#ifdef __psp__
  uint64_t dig_c;             /* gl41: its content digest (gl_digest_c), for the PSP's "picture: vb N content" line */
#endif
  /* counts */
  long calls, prims, lists, list_fail;
  long lw_tr, lw_tags[5], lw_grp[5], lw_bad;   /* the late walk: transfers; tags and groups each lane took; walks that missed a group's end */
  /* native step 1 (native_renderer_design.md 6.1): ship-mesh groups the transfer's walk draws from records, merged by key: before a
     top-level tag of the groups' packet buffer, every group whose key is at or below the tag's address.  The host sets nat_g / nat_n
     before each xm_walk (ge/xme.c xme_walk; 0, 0: none): per packet buffer (NAT_BUF) the groups not yet taken.  nat_b >= 0: the
     buffer the transfer's plan bound (xc_walk), nat_n[nat_b] the count it took (the walk draws what is left of them at its end,
     counted in nat_late); nat_b -1: a walk with no plan (from RAM): the walk binds the buffer of the first such tag where groups
     wait, as the plan does, and takes what its tags reach.  After the walk: nat_b the buffer, nat_used the groups taken */
  const NatGrp *nat_g[2]; int nat_n[2], nat_b, nat_used;
  const void *nat_rf[2];       /* gl41: native step 1's references of nat_g[b][0..] when the record copied them (ge/nge.h nge_walk_r), else 0 */
  uint32_t (*nat_clk)(void);  /* the merge runs' clock (0: not timed) */
  uint64_t nat_t;             /* their time in its ticks */
  long nat_runs, nat_core, nat_fed, nat_bad, nat_late, nat_stop;   /* merge runs; groups the core took, fed word by word, dropped (a
                                                                      count or pointer out of range); drawn at the walk's end; run lanes
                                                                      stopped at a key */
  long nat_in, nat_out, nat_part;   /* the groups' data pieces found in the transfer's copy, outside it (RAM as it is), partly (dropped) */
  /* gl34 (ge/natsplit.h): ns_on 1: the split's timers run in the walks that have a clock (nat_clk).  ch_i: the transfer's place in
     its frame's chain list, set by the host before xm_walk; -1: none */
  int ns_on, ch_i;
  int nh_chk;                       /* gl34: lw_nptr_h's check (ge/xlme.c) */
  /* gl36 (ge/xlate.h xl_nr_*): per packet buffer, the index in its picture's array of nat_g[b][0], and the picture's reuse switch
     (boot/nat_stub.h NatBuf.ru), set by the host with nat_g */
  int nat_i0[2]; int nat_ru[2];
  /* native step 2 (ge/natp06.h): the people's groups, beside the ship's and by the same rule (np6_g / np6_n / np6_b / np6_used / np6_i0
     as nat_g / nat_n / nat_b / nat_used / nat_i0; one buffer bound for both: a transfer walks one packet buffer).  At a tag, the people
     whose key is at or below both the tag and the next ship group's key first, then the ship's, then the people interleaved (as the
     twin's walk, boot/nat_x86.c) */
  const NatP6 *np6_g[2]; int np6_n[2], np6_b, np6_used, np6_i0[2];
  const uint32_t *np6_r[2];   /* step 2: each np6_g's references (ge/nge.h xc_nge_ref6), index for index: copied with the groups (ge/xme.c
                                 XME_P6ROFF), or the main CPU's for a walk it waits for; 0: none (today's path) */
  long np6_runs, np6_core, np6_fed, np6_bad, np6_late;
} XmPic;
extern long lw_nh_n[3];             /* gl34 (ge/xlme.c lw_nptr_h): hinted look-ups, of them differing from lw_nptr's, checked */

extern long xm_lw7_miss[8], xm_lw3_miss[8];  /* lanes 4 and 3: why a recognizer declined (lw_lanes.h) */
#ifndef __psp__
extern long xm_lw_ncmd[256];                 /* the VIF commands the lanes' groups held (as runtime/ee_io.h vif1_n_cmd counts them) */
#endif

void xm_init(XmPic *p);                      /* after the host's fields: the switches as the picture code starts them, the counts 0 */
void xm_install(XmPic *p);                   /* the translator's callbacks (xl_on_call, xl_on_pass, xl_on_flush, xl_on_prim) with ctx p */
void xm_objects(XmPic *p, GuMap *map, TkCache *tk);   /* a map and a texture cache made again (the take-back): their switches not known */
void xm_switch(XmPic *p, uint32_t opt, uint32_t opt2);
void xm_walk(XmPic *p, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int lanes, uint32_t opt);
/* gl26: the walk's split: each callback's picture work timed by kind into t[] (clk's ticks) and counted in n[], while clk is not 0.
   ge/xme.c xme_walk sets it for each walk (the ME's Count on the ME, the system clock on the emulator's loop thread, 0 on the main CPU)
   and sets 0 after; psp/xl_psp.c sets 0 before its own direct walks.  Lanes as XlCall.lane (0: none); XM_TS_FLUSH the held calls run
   at a transfer's end (xm_flush); XM_TS_CFL (gl26b) a call that ran the held calls inside gl_call (records full, a P07 call's copies'
   room, a call's room), whatever its lane */
enum { XM_TS_L0 = 0, XM_TS_L4 = 4, XM_TS_FLUSH = 5, XM_TS_PASS = 6, XM_TS_PRIM = 7, XM_TS_CFL = 8, XM_TS_N = 9 };
void xm_tsplit(uint32_t (*clk)(void), uint64_t *t, long *n);
void xm_end(XmPic *p);                       /* the frame ends: gl_end */
int xm_list(XmPic *p, int buf, int dig);     /* its GE list, drawn into frame buffer buf; dig: the digests too.  gx_frame's 0 or -1 */
void xm_begin(XmPic *p, uint32_t opt, uint32_t opt2);   /* a frame starts with the switches of its hook */
void xm_call(XmPic *p, const XlCall *c);
void xm_pass(XmPic *p, const XlCall *c, int pass);
void xm_flush(XmPic *p);
void xm_prim(XmPic *p, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n);
void xm_on_call(void *ctx, const XlCall *c);
void xm_on_pass(void *ctx, const XlCall *c, int pass);
void xm_on_flush(void *ctx);
void xm_on_prim(void *ctx, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n);
const uint8_t *xm_ram(void *ctx, uint32_t ee, uint32_t n);   /* through m->view when set (task #180) */
const uint8_t *xv_ram(XmView *v, uint32_t k, uint32_t n);    /* task #180: n bytes at RAM key k through the view */

/* ---- task #180 (step 4b): a transfer's copy (me_step4_notes.md 4b-1).  xm_copy walks the transfer's DMA chain in RAM as the late walk
   will (ge/xlive.c walk()), keeps what that walk and its lanes read and copies it into the arena as the transfer's extents: every tag with
   XC_MARGIN bytes after it (what the lanes may read on from a tag: at most 384 for lane 4's slot), the data after the tags (CNT, NEXT,
   CALL, RET, END, and a chain's first QWC quadwords), the data the tags point to (REF, REFE, REFS) and a normal transfer's, less the image
   data the runtime's walk marked (mk) outside the two ranges the game rewrites (me_handoff_design.md 4.4); all the scratchpad's.  It
   reads RAM and the scratchpad and writes only the arena and its own arrays. ---- */
#define XC_MARGIN 416u
typedef struct { uint32_t k0, k1; } XcR;   /* keys [k0, k1) (xlview.h) */
#define XC_CAPS(c) ((c)->cap_s ? (c)->cap_s : (c)->cap)   /* step 4d-3: ra's and mk's room */
typedef struct XmCopy {
  /* the host's */
  const uint8_t *mem, *spr; uint32_t mem_size;
  XcR *ra, *rb, *mk, *tmp; int cap;    /* cap entries each: tags and the data after them; the data the tags point to; the image marks
                                          (the host fills mk and nmk as the runtime's walk goes, mk_full when they did not fit); a scratch */
  int cap_s;                           /* task #180 step 4d-3: ra's and mk's entries when they are fewer (XC_CAPS; 0: cap) */
  int nmk, mk_full;
  int rd_only;   /* step 4b-2's check (xl_psp.c): 1 = the tags without the margin after them, so the extents are the bytes the
                    transfer itself reads (with nmk 0: its image data too) */
  XvExt *ext; int ext_cap; uint8_t *arena; uint32_t arena_n;
  /* the state: extents and arena bytes in use (the host empties them once the copies are walked) */
  int next; uint32_t used;
  /* counts */
  long copies, fails[5], exts, rb_cut, mk_cut; int na_max, nb_max, nmk_max, nx_max; uint64_t bytes, marked;
  long fills, laid, laid_lines; uint64_t laid_b;   /* task #180 step 4b-2: xm_fill's: fills, log entries laid, their lines in the
                                                      extents, bytes */
  /* the walk's */
  int na, nb, over;
  /* native step 1: the host's, before xm_plan (0, 0: none): per packet buffer the groups not yet taken; the plan's walk (xc_walk) takes
     them by the walk's rule (XmPic above) and keeps their data (the constant block, the material, the streams) as extents.  After
     xm_plan: nat_b the buffer bound (-1: none), nat_cur the groups taken; the host gives them room in the arena (xm_nat_room) */
  const NatGrp *nat_g[2]; int nat_n[2], nat_b, nat_cur;
  long nat_grp;                        /* groups planned */
  const NatP6 *np6_g[2]; int np6_n[2], np6_b, np6_cur;   /* native step 2: the people's, as nat_* (ge/natp06.h; XmPic) */
  long np6_grp;
  uint64_t nat_qw;                     /* gl34: the quadwords their ranges asked for (before the merge with the transfer's own) */
  /* gl40 lever XP (attic/gl40/gl40_plan.md 1d), the host's, set before each xm_plan (0: gl39's plan): XP_PP the merge sort's passes
     alternate the array and the scratch (the copy back after every pass dropped; the same merges in the same order), XP_LOOK a range that
     does not meet the last entry joins the first of the XP_K - 1 entries before it that it meets (xc_add's miss path), XP_CHK the check:
     after the plan, gl39's ranges made again (xp 0) and compared with the plan's extents.  The extents depend only on the union of the
     ranges, which both parts keep. */
  int xp;
  long xp_plans, xp_adds, xp_moves, xp_cpy, xp_ra, xp_rb, xp_mk;   /* gl40 probe: plans, xc_add calls, the sorts' element moves and the
                                                                      elements copied back, the ranges into the merges (ra, rb, mk) */
  long xp_chk, xp_dif, xp_room; int32_t xp_dif_vb; uint32_t xp_dif_w[3];   /* XP_CHK: plans compared, differing, compared by their result
                                                                      only (the extents' or the arena's room); the first difference */
  uint32_t (*clk)(void);               /* gl40 probe (MGS2_PROBE10): the main CPU's microseconds, or 0 (the twin): the plan's time by part */
  uint64_t xp_tw, xp_ts, xp_tr;        /* the walk (xc_walk), the sorts and joins of ra and rb, the rest (marks, extents) */
} XmCopy;
#define XP_PP 1
#define XP_LOOK 2
#define XP_CHK 4
#define XP_K 8
/* 0: the copy made, its extents ext[*x0 .. *x0 + *nx); else nothing kept: 1 the ranges did not fit, 2 the extents, 3 the arena, 4 the
   chain did not end */
int xm_copy(XmCopy *c, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int *x0, int *nx);
/* task #180 step 4b-2 (me_step4_notes.md 4b-2; me_handoff_design.md 4.2, 4.3): the copy in two parts.  xm_plan is xm_copy without the
   bytes: the extents and their places in the arena, as the transfer ends.  xm_fill, at the copy's deadline, copies the extents' bytes
   from RAM and the scratchpad as they are then and lays over them the guards' log entries made since the transfer started (their t, the
   count of transfers started when the store came, is the transfer's own number t or more), the newest first, so that where a line was
   logged twice its oldest bytes win: the bytes as they were when the transfer started.  The log is in the order the stores came (t never
   goes down).  xm_copy = xm_plan + xm_fill with no log. */
typedef struct { uint32_t l; int32_t t; uint8_t old[64]; } XmLogE;   /* a guarded store's entry: RAM line l (bytes 64 l ..), the count of
                                                                         transfers started when it came, the line's 64 bytes before it */
int xm_plan(XmCopy *c, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int *x0, int *nx);
/* native step 1: n bytes of the arena after the last plan's extents, for the copy of its groups (*off; 16-byte aligned).  0, or 3: no room
   (the host then undoes the plan) */
int xm_nat_room(XmCopy *c, uint32_t n, uint32_t *off);
void xm_fill(XmCopy *c, int x0, int nx, int t, const XmLogE *log, int nlog);
/* task #180 step 4c: xm_fill's second part alone, the log entries with t or more laid over a copy made earlier (xm_fill with no log) at
   any time after its transfer ended: a line written since then is in the log (the filter holds the copy's lines until the lay), so
   the copy ends as the one made at the deadline */
void xm_lay(XmCopy *c, int x0, int nx, int t, const XmLogE *log, int nlog);
#endif
