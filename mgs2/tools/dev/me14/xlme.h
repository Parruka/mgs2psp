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
  /* counts */
  long calls, prims, lists, list_fail;
  long lw_tr, lw_tags[5], lw_grp[5], lw_bad;   /* the late walk: transfers; tags and groups each lane took; walks that missed a group's end */
} XmPic;

extern long xm_lw7_miss[8], xm_lw3_miss[8];  /* lanes 4 and 3: why a recognizer declined (lw_lanes.h) */
#ifndef __psp__
extern long xm_lw_ncmd[256];                 /* the VIF commands the lanes' groups held (as runtime/ee_io.h vif1_n_cmd counts them) */
#endif

void xm_init(XmPic *p);                      /* after the host's fields: the switches as the picture code starts them, the counts 0 */
void xm_install(XmPic *p);                   /* the translator's callbacks (xl_on_call, xl_on_pass, xl_on_flush, xl_on_prim) with ctx p */
void xm_objects(XmPic *p, GuMap *map, TkCache *tk);   /* a map and a texture cache made again (the take-back): their switches not known */
void xm_switch(XmPic *p, uint32_t opt, uint32_t opt2);
void xm_walk(XmPic *p, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int lanes, uint32_t opt);
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
const uint8_t *xm_ram(void *ctx, uint32_t ee, uint32_t n);
#endif
