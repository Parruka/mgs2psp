/* xlive_chk.c - dev, x86 check build only (boot.c and runtime/ee_io.h built with -DMGS2_XLIVE; tools/dev/build_xlive.sh): the packet
   translator live (renderer_notes 11.10, step 6).  io_dma_start hands each VIF1 / GIF transfer here before its exact walk; ge/xlive.c
   walks it a second time and feeds ge/xlate.c, whose GS front end tells ge/gsup.c of the texture uploads (the texels are read from guest
   RAM as it is when they are read).  Nothing of the game's is written.
   MGS2_XLIVE=<from vb>:<to vb>[:<digest file>] (vblanks as k_vblanks; the translator starts at <from> knowing nothing, as tools/gsref.c
   does on a capture that starts there; <from> 0: from the first transfer, before the first vblank, whose packets no MGS2_PKT_HASH line covers):
   - "xlive: vb <n> <hash>" for each vblank of the window: FNV-1a 64 of the quadwords this walk took, by ee_io.h's packet hash rules
     (PKT_HASH, PKT_TAG).  With MGS2_PKT_HASH=<from>:<to> in the same run each must equal the exact walk's "pkt: vb <n> <hash>";
   - the digest file (ge/xldig.c): a line per VU1 call and per vblank, for the same file from gsref (GSREF_XLATE=1 GSREF_XLDIG=<file>)
     on a GS capture of the window (MGS2_GSCAP=<file>:<from>:<to - from> in the same run);
   - totals when the window ends.
   MGS2_XLIVE_TEX=1 adds the texture check (renderer_notes 11.2 "Textures"; 11.10 step 5, "Open"):
   - each host -> local transfer, once all its IMAGE data is in: its key as the PSP's texture cache would take it (EE address, size, a hash
     of 64 words spread over it) and a hash of all of it; a known key with other content is a stale texture on the PSP;
   - the bytes of each upload as sent (a 32 MB copy by EE address); every word the texture sources read from guest RAM is compared with
     it: a texture is made from RAM when it is used, so any difference is a texel the GE would get wrong;
   - each draw that textures: the VU1 calls with a rule (TEX0 / CLAMP of the context their tag template's PRIM names, after their A+D
     packet, and after each P06 pass packet) and the front end's primitives.  A call's texels (once per vblank and upload per TEX0 /
     CLAMP, through ge/texconv.c as the PSP will convert them), a primitive's texels where it samples them (its vertices' texel box, one
     texel more each side; the whole texture when that box leaves the texture or the wrap mode is a region repeat) and the palette
     entries they use are read through the texture sources, from EE RAM now and as sent;
   - a line every 100 vblanks of the window, and totals;
   - at the end, by source (EE address and size): its uploads and keys, and what the rule "hash a source in full once it shows a second
     key" would leave stale and hash in full (bytes a vblank), with the uploads a vblank by size.
   MGS2_XLIVE_TEX=2 also prints, for the first 64 textures (by TBP, TBW, PSM) with a texel the map cannot tell or that texconv cannot
   convert, the texture and its first such texel's block list (gu_why).
   MGS2_XLIVE_WATCH=<block, hex>:<from vb>:<to vb> prints each entry the map pushes on that block's list in those vblanks (gu_watch).
   MGS2_XLIVE_RTLOG=<from vb>:<to vb> prints up to 80 textured front-end primitives of those vblanks with their texture registers, texel
   box and the map's flags for the texture's area (the whole, the box, the 64 x 64 squares holding a drawn area).
   With MGS2_XLIVE_TEX the totals also list the 24 textures (TBP, TBW, PSM) with the most texels the map cannot tell.
   MGS2_XLIVE_TRX=1 prints each GS local -> local transfer and each host -> local transfer in a format other than the 32-bit layout's
   (buffer, format, rectangle; a local copy's source too); MGS2_XLIVE_TRX=2 every host -> local transfer with its number and, with
   MGS2_XLIVE_TEX, the EE address of its data once all of it is in.
   MGS2_XLIVE_TK=1 also runs the PSP's texture path (ge/texcache.h) on every indexed texture use: the cache's texture and palette are
   compared texel by texel and entry by entry with the map, and the totals give its hit rate, its bytes and what it refused.
   MGS2_XLIVE_TK=<budget in KB> (over 1) sets the cache's budget, so eviction runs.
   MGS2_XLIVE_TKBLK=0 / 1 / 2: the texture path converts texel by texel / a GS block at a time (the default) / both, compared (tk_blk,
   task #59); a line after the texture totals counts them.
   The exact speed-ups of codec_speed.md steps 1 and 2 (on by default, as on the PSP) are switched off one by one with MGS2_XLIVE_GUMEMO=0
   (gu_area_memo, the PSP's OPT_GU_MEMO), MGS2_XLIVE_TKFLAGS=0 (tk_flags, OPT_TK_FLAGS), MGS2_XLIVE_GUCLUT=0 (gu_clut_memo, OPT_GU_CLUT)
   and MGS2_XLIVE_V48=0 (xl_ud_v48, OPT_XL_V48); ge/ built with -DGU_MEMO_CHECK checks the map's memos against its plain walks.
   MGS2_XLIVE_T16=0 (tk_t16) turns task #83's 16-bit textures and DIRECT primitive regions off (the pictures before; the PSP builds keep
   them on).
   The VU1 entry census (renderer_notes 11.2): the first call of each entry is printed with whether the translator has a rule for it;
   MGS2_XLIVE_CENSUS=<vb>[,<vb>...] prints the calls of each entry so far (and those without a tag template) after those vblanks (the
   route's stage borders), and the end of the window prints them too.  A run from vblank 0 also prints what each upload of the boot sent.
   MGS2_XLIVE_VCEN=1 (ge/ built with -DXL_CENSUS, as build_xlive.sh does): the volume census (renderer_notes 11.12), a line per vblank
   of the window, "xlive vc: vb <n>" with the VIF1 words (w), command words (cw), data words of UNPACK, MPG, DIRECT and the rest (up, mpg,
   dir, oth), GIF quadwords of paths 1-3 (g1-g3; path 1 = the A+D packets the translator kicks) and their IMAGE quadwords (i2, i3), the
   DMA quadwords the walk took (qw), VU1 calls (with a converter, a rule only, no rule), the front end's primitives, and the vectors the
   UNPACKs wrote by format (uq <command bits 0-4, hex>:<vectors>).
   MGS2_XLIVE_FAST=1: the walk hands the translator spans (xl_vif1_span: UNPACKs decoded when read, IMAGE runs in one piece; renderer_notes
   11.12 step 1) instead of single words; everything above prints as with single words (the digests hash the VU memory as it reads,
   xl_vm_logical), and the totals add a line "xlive lazy:" with the UNPACKs left to decode later and the quadwords decoded later.
   MGS2_XLIVE_SYNC=1 decodes all pending quadwords before each call's callbacks (the path the converters will take).
   MGS2_XLIVE_NOSEEN=1 leaves out the walk's packet hash (for timing).  The totals give the time spent in the walk with the translator
   and the callbacks ("xlive time:").
   MGS2_XLIVE_CG=1 (a build where valgrind/callgrind.h is found): callgrind's instrumentation on for the window only, for
   valgrind --tool=callgrind --instr-atstart=no --toggle-collect=xlv_dma (the translator's own profile).
   MGS2_XLIVE_DIGUSED=1: the digests take only what is read (ge/xldig.h xdg_used), to compare a run with MGS2_XLIVE_SKIP=1 against one
   without on the shown vblanks.
   MGS2_XLIVE_SKIP=1: shown frames only (renderer_notes 11.12 "Shown frames only"): the transfers of a vblank whose previous vblank had
   reduced calls of the draw pipeline only (boot.c g_dg_calls) are not handed over, as the PSP translator will skip them; their packet
   hash lines carry the empty hash, and the totals count what was skipped.
   MGS2_XLIVE_ONE=1 (with MGS2_XLIVE_FAST=1): the one walk (renderer_notes 11.12, task #47): a VIF1 transfer that ee_io.h's
   vif1_chain_fast walks is not walked again here; that walk hands the translator the same runs of words (XL_ONE) and the transfer's end
   (xlive_one_end).  GIF transfers keep the second walk.  The packet hash lines then cover the GIF transfers only, the walk's tag and
   quadword counts the GIF transfers only, and "xlive time:" holds the exact walk of the VIF1 transfers too.
   MGS2_XLIVE_GB=1 (renderer_notes 11.13, #30 step 3): the live renderer's frame (ge/gelive.h) made on each shown vblank of the window
   (one whose previous vblank had full draw-pipeline calls): the front end's path 2 / 3 primitives and the converters' VU1 calls into GE
   batches, textures from the PSP texture path (its own cache when MGS2_XLIVE_TK is not set; no budget).  A line per frame ("xlive gb:")
   and totals with the largest frame.  MGS2_XLIVE_GB=2 also asks the texture path when the last lookup's answer is taken (the memo) and
   counts the differences; MGS2_XLIVE_GB=3 also counts what each frame's depth map leaves outside the GE's range (gelive.h zchk; with
   MGS2_XLIVE_GB_ZK=0).  MGS2_XLIVE_GB_ZK=<k>: GE depth = Z24 x k (default 0.2, the replay files' since v4); 0: the PSP's depth (gelive.h
   zraw: Z24 as it is, the frame's range mapped by the GE list; the frame's depth range on each line).  MGS2_XLIVE_GER=<prefix>:<vb>[,<vb>...] saves those frames as replay files
   <prefix><vb>.ger (ge/replay_fmt.h, textures copied in) for psp/ge_replay and the check against gsref (GSREF_GELIVE).
   MGS2_XLIVE_GX=<a>:<b> (renderer_notes 11.13 "3e", task #56; with MGS2_XLIVE=0:<to>, and no MGS2_XLIVE_TK): the PSP harness's frames
   (psp/xl_psp.c MGS2_GL) made here the same way: the PSP's depth, its texture pool (MGS2_XLIVE_GX_TEXKB, default GL_PSP_TEXKB) and the
   cache's budget, its frame arena, texture table and list sizes (gelive.h GL_PSP_*), a frame begun only after the first transfer, and
   each frame's GE list (ge/gelist.c, the draw buffer alternating from the first frame); the frames whose last vblank lies in a..b print
   "picture:" lines with the list digest and the frame digest (gelive.h gl_digest) in the format of the PSP's log.txt, and the end a
   total, so the two can be compared line for line (the word counts differ by BASE words: the memory lies elsewhere).  A list that does
   not fit prints a line (the first 50), as on the PSP.  MGS2_XLIVE_GX_LIST=<KB>: another size of the list's block (the list and its aux
   share it, gelist.h), to measure what the lists need (those frames' lists then differ from the PSP's). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../ge/xlive.h"
#include "../ge/xlate.h"
#include "../ge/gsfront.h"
#include "../ge/gsup.h"
#include "../ge/xldig.h"
#include "../ge/texconv.h"
#include "../ge/texcache.h"
#include "../ge/gslayout.h"
#include "../ge/gelive.h"
#include "../ge/vu1conv.h"   /* sb4: vc_p06_exact for MGS2_SB_P6EXACT */
#include "../ge/fxthin.h"    /* task #114 */
#include "../ge/c1model.h"   /* sb11: C1 census and GE model */
#include "../ge/c2model.h"   /* sb12: C2 census and GE model */
#include "../ge/c3model.h"   /* sb13: C3 census of the rooms' meshes */
#include "../runtime/host_time.h"   /* sb3: g_opt's OPT_CLIP_FAST (boot.c MGS2_OPT, MGS2_OPT_CYCLE), as psp/xl_psp.c reads it */
#if defined(__has_include)
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#endif
#endif
#ifndef CALLGRIND_START_INSTRUMENTATION
#define CALLGRIND_START_INSTRUMENTATION
#define CALLGRIND_STOP_INSTRUMENTATION
#endif

#define GSZ (1u << 25)
static long from = -2, to; static int on, done; static XlvMem M; static GuMap *map; static FILE *dig;
extern uint32_t g_dg_calls[2];   /* boot.c: draw-pipeline calls, full / reduced */
static int skip_on, skip_now; static uint32_t sk_c0, sk_c1; static long sk_vb, sk_tr;   /* MGS2_XLIVE_SKIP */
extern int g_xl_one;                    /* runtime/ee_io.h: the exact walk feeds the translator (MGS2_XLIVE_ONE) */
extern int g_xl_lane;                   /* sb1, sb2 runtime/ee_io.h: Stage B lanes (MGS2_SB_LANE: bit 0 lane 1, bit 1 lane 2, sb5 bit 3 lane 4,
                                           sb6 bit 2 lane 3; MGS2_SB_CHK=1 checks the lanes' df0, P06, P07 and effect input) */
static int lane_opt, lane_base;           /* gl11 MGS2_SB_LANE_OPT=1: the lanes follow g_opt's lane bits at each transfer, as psp/xl_psp.c's do
                                           (MGS2_OPT_CYCLE's turns switch them; lane_base = MGS2_SB_LANE's mask) */
extern long lane7_miss[8];              /* sb5 runtime/ee_io.h lane_p07: tags with STCYCL 7 / 1 where no slot was taken, by the failed test */
extern long lane3_miss[8], lane3_gen[8];   /* sb6 runtime/ee_io.h lane_fx: tags that start an effect prim where it took none, by the failed
                                           test; such tags the generic walk took, by why the lane did not look (vif1_chain_fast) */
static int one_on; static long one_tr; static struct timespec one_t0;
static uint64_t ph = 0xcbf29ce484222325ull;
static long cur_vb;
static const uint8_t *ram(void *ctx, uint32_t ee, uint32_t n) { (void)ctx; ee &= GSZ - 1; return M.mem && ee + n <= GSZ ? M.mem + ee : 0; }

/* ---- the texture check (MGS2_XLIVE_TEX=1; ge/gsup.c built with GU_SENT keeps each transfer's bytes as sent) ---- */
static int tx_on, tx_detail, trx_all, wbn = -1; static long wfrom, wto;
typedef struct {
  long xfers, bytes, keynew, keyhit, keybad;            /* finished uploads; their keys as the PSP's texture cache would take them */
  long scanned, scan_bad, scan_badx, scan_bytes;        /* each vblank: transfers the map can still read, those whose EE bytes now differ
                                                           from as sent (transfer-vblanks, distinct transfers, differing bytes) */
  long uses, calls, passes, prims, nonidx, notpl, mip;  /* texture uses */
  long conv, boxes, unsup, texels, tmiss, tone, tdiff;  /* conversions (calls), primitive boxes; texels read from EE RAM now and as sent:
                                                           unknown both ways, known one way only, different */
  long pchecks, pal, pmiss, pone, pdiff;                /* palettes, the same way */
  long direct, dtexels, dmiss, done1, ddiff;            /* direct-colour textures (CT32, CT24, CT16, CT16S): uses, texels the same way */
  long tk_uses, tk_tex, tk_rt, tk_lost, tk_unsup, tk_cmp, tk_bad, tk_pbad;   /* the PSP texture path (MGS2_XLIVE_TK): uses and their kind,
                                                           textures compared with the map, those with a differing texel or palette entry */
} TxCount;
static TxCount ck, ck_all;
static TkCache *tkc; static int tk_on, shown_tk;
static int um_chk, li_chk;   /* sb9: MGS2_TX_UMEMO_CHK=1, MGS2_GU_LDID_CHK=1: the step 0 bits' "on" is their check mode */
static int uf_chk;           /* sb10 T1: MGS2_TX_FRONT_CHK=1: OPT2_TX_FRONT's "on" is its check mode */
/* MGS2_XLIVE_GB: the live renderer's frame */
static GlFrame glf; static int gb_on, gb_now; static char ger_pre[200]; static long ger_vb[512]; static int ger_n;   /* gl29: 512 (64) */
/* MGS2_XLIVE_GX: the PSP harness's frames and GE lists */
static int gx_on, gx_seen; static long gx_from = -1, gx_to = -1, gx_count, gx_n, gx_fail, gx_first = -1, gx_wmax, gx_amax, gx_bmax, gx_drop;
static long gx_c1p, gx_c1b, gx_c1w;   /* sb11 C1: the lists' GxCount c1_passes, c1_bones, c1_words */
static long gx_c4s, gx_c4r, gx_c4l;   /* sb15 step 4: the lists' GxCount fx_spr, fx_lruns, fx_lines */
static long gx_c2p, gx_c2b, gx_c2w;   /* sb12 C2: c2_passes, c2_bones, c2_words */
static uint64_t gx_all = 0xcbf29ce484222325ull, gx_call = 0xcbf29ce484222325ull; static GxList gxl; static GxHost gxh;   /* gx_call: sb4's
                                                       content digests (gelive.h gl_digest_c) folded as gx_all */
static uint8_t *gx_arena, *gx_pool, *gx_lb; static uint32_t gx_pool_n, gx_listn = GL_PSP_LIST;
static uint32_t gx_addr(void *ctx, const void *p)   /* a GE address space like the PSP's: the frame memory (the arena, then the list's
                                                       block), the pool */
{
  const uint8_t *q = p; (void)ctx;
  if (q >= gx_arena && q <= gx_arena + GL_PSP_ARENA) return 0x08600000u + (uint32_t)(q - gx_arena);
  if (q >= gx_lb && q <= gx_lb + gx_listn) return 0x08600000u + GL_PSP_ARENA + (uint32_t)(q - gx_lb);
  if (q >= gx_pool && q <= gx_pool + gx_pool_n) return 0x09000000u + (uint32_t)(q - gx_pool);
  fprintf(stderr, "xlive gx: an address outside the frame memory, the aux and the pool\n"); exit(2);
}
/* sb15 (task #152): MGS2_GX_SEM=1, the GE list as the GE draws it, independent of where its data lies and of how its primitives are
   grouped: every state word in order (the address words VADDR, IADDR and BASE only followed, not hashed) and every primitive each PRIM
   makes, its vertices' bytes as the GE reads them (by VTYPE, an index list if any; the vertex or index address following each PRIM as
   the GE's does), a line strip's segments and GU_LINES' pairs alike.  Two lists with the same digest draw the same picture */
static int gx_sem_on; static uint64_t gx_sem_all = 0xcbf29ce484222325ull; static long gx_sem_n, gx_sem_bad;
static const uint8_t *gx_ptr(uint32_t a, uint32_t len)   /* gx_addr's inverse; 0 outside the frame memory, the list's block and the pool */
{
  if (a >= 0x08600000u && a + len <= 0x08600000u + GL_PSP_ARENA) return gx_arena + (a - 0x08600000u);
  if (a >= 0x08600000u + GL_PSP_ARENA && a + len <= 0x08600000u + GL_PSP_ARENA + gx_listn) return gx_lb + (a - 0x08600000u - GL_PSP_ARENA);
  if (a >= 0x09000000u && a + len <= 0x09000000u + gx_pool_n) return gx_pool + (a - 0x09000000u);
  return 0;
}
static uint32_t gx_vsize(uint32_t vt)   /* a vertex's bytes (GE VTYPE: weights, texture, colour, normal, position, each aligned) */
{
  static const uint32_t W[4] = { 0, 1, 2, 4 }, C[8] = { 0, 0, 0, 0, 2, 2, 2, 4 };
  const uint32_t parts[5][2] = { { W[vt >> 9 & 3], (vt >> 14 & 7) + 1 }, { W[vt & 3], 2 }, { C[vt >> 2 & 7], 1 }, { W[vt >> 5 & 3], 3 }, { W[vt >> 7 & 3], 3 } };
  uint32_t o = 0, al = 1;
  for (int k = 0; k < 5; k++) { const uint32_t z = parts[k][0]; if (!z) continue; o = (o + z - 1) & ~(z - 1); o += z * parts[k][1]; if (z > al) al = z; }
  return ((o + al - 1) & ~(al - 1)) * ((vt >> 18 & 7) + 1);
}
static uint64_t gx_sem(const GxList *l)
{
  uint64_t h = 0xcbf29ce484222325ull; uint32_t base = 0, va = 0, ia = 0, vt = 0;
#define SH(x) do { uint32_t x_ = (x); for (int k_ = 0; k_ < 4; k_++) { h ^= x_ >> 8 * k_ & 0xff; h *= 0x100000001b3ull; } } while (0)
  for (uint32_t n = 0; n < l->n; n++) {
    const uint32_t w = l->w[n], c = w >> 24;
    if (c == 0x10) { base = (w & 0x0f0000) << 8; continue; }      /* BASE */
    if (c == 0x01) { va = base | (w & 0xffffff); continue; }       /* VADDR */
    if (c == 0x02) { ia = base | (w & 0xffffff); continue; }       /* IADDR */
    if (c == 0x12) vt = w & 0xffffff;                              /* VTYPE (hashed below, as state) */
    if (c != 0x04) { SH(w); continue; }
    const uint32_t cnt = w & 0xffff, ty = w >> 16 & 7, vs = gx_vsize(vt), ix = vt >> 11 & 3, isz = ix == 1 ? 1 : ix == 2 ? 2 : ix == 3 ? 4 : 0;
    const uint8_t *vb = 0, *ib = 0; uint32_t maxi = 0;
    if (isz) {
      if (!(ib = gx_ptr(ia, cnt * isz))) { gx_sem_bad++; SH(0xdead0001u); continue; }
      for (uint32_t i = 0; i < cnt; i++) { uint32_t x = isz == 1 ? ib[i] : isz == 2 ? (uint32_t)(ib[2 * i] | ib[2 * i + 1] << 8) : (uint32_t)(ib[4 * i] | ib[4 * i + 1] << 8 | ib[4 * i + 2] << 16 | (uint32_t)ib[4 * i + 3] << 24); if (x > maxi) maxi = x; }
      vb = gx_ptr(va, (maxi + 1) * vs);
    } else vb = gx_ptr(va, cnt * vs);
    if (!vb) { gx_sem_bad++; SH(0xdead0002u); continue; }
#define VX(i) (isz == 1 ? ib[i] : isz == 2 ? (uint32_t)(ib[2 * (i)] | ib[2 * (i) + 1] << 8) : isz == 4 ? (uint32_t)(ib[4 * (i)] | ib[4 * (i) + 1] << 8 | ib[4 * (i) + 2] << 16 | (uint32_t)ib[4 * (i) + 3] << 24) : (uint32_t)(i))
#define SV(i) do { const uint8_t *p_ = vb + (size_t)VX(i) * vs; for (uint32_t b_ = 0; b_ < vs; b_ += 4) { uint32_t u_; memcpy(&u_, p_ + b_, 4); SH(u_); } } while (0)
    /* the primitive's kind: points 0, lines 1 (GU_LINES and the line strip's segments), triangles 3 (and the strip's and fan's), sprites 6 */
    if (ty == 0) for (uint32_t i = 0; i < cnt; i++) { SH(0x50000000u); SV(i); }
    else if (ty == 1) for (uint32_t i = 0; i + 1 < cnt; i += 2) { SH(0x50000001u); SV(i); SV(i + 1); }
    else if (ty == 2) for (uint32_t i = 0; i + 1 < cnt; i++) { SH(0x50000001u); SV(i); SV(i + 1); }
    else if (ty == 3) for (uint32_t i = 0; i + 2 < cnt; i += 3) { SH(0x50000003u); SV(i); SV(i + 1); SV(i + 2); }
    else if (ty == 4) for (uint32_t i = 0; i + 2 < cnt; i++) { SH(0x50000004u); SV(i); SV(i + 1); SV(i + 2); }   /* (the strip's winding kept) */
    else if (ty == 5) for (uint32_t i = 1; i + 1 < cnt; i++) { SH(0x50000005u); SV(0); SV(i); SV(i + 1); }
    else if (ty == 6) for (uint32_t i = 0; i + 1 < cnt; i += 2) { SH(0x50000006u); SV(i); SV(i + 1); }
    else { SH(w); gx_sem_bad++; }
    if (isz) ia += cnt * isz; else va += cnt * vs;
#undef SV
#undef VX
  }
#undef SH
  return h;
}
static GxDzSched gx_dz; static int gx_dz_on;   /* gl29: MGS2_GL_DZ (ge/gelist.h), as psp/xl_psp.c reads it from mgs2.cfg */
static void gx_list(long vb)   /* psp/xl_psp.c gl_list's work on the frame that ended (its last vblank vb) */
{
  int d = (int)(gx_count++ & 1), dg = vb >= gx_from && vb <= gx_to;
  GxFrame F = { glf.o.b, (uint32_t)glf.o.nb, glf.o.v, (uint32_t)glf.o.nv, glf.o.d, glf.o.t, (uint32_t)glf.o.nt, glf.tdata, glf.tpal, 1, 0 };
  gx_depth_map(glf.map_lo, glf.map_hi, 1.0f, &F.zs, &F.zo);
  gxl.dig_on = dg;
  if (gx_dz_on) { int n = 0; gxl.dz = gx_dz_arm(&gx_dz, vb, &n, 0); gxl.dzn = n; }   /* gl29: the frame's arm (xl_psp.c's mq_frame and gl_list) */
  if (gx_frame(&gxl, &gxh, &F, d) && ++gx_fail <= 50)
    fprintf(stderr, "picture: vb %ld: the list did not fit: %ld batches left out (%ld words, %ld aux bytes)\n", vb, gxl.c.dropped, gxl.c.words, gxl.c.aux);
  gx_drop += gxl.c.dropped; if (gxl.c.words > gx_wmax) gx_wmax = gxl.c.words; if (gxl.c.aux > gx_amax) gx_amax = gxl.c.aux;
  gx_c1p += gxl.c.c1_passes; gx_c1b += gxl.c.c1_bones; gx_c1w += gxl.c.c1_words;
  gx_c4s += gxl.c.fx_spr; gx_c4r += gxl.c.fx_lruns; gx_c4l += gxl.c.fx_lines;
  gx_c2p += gxl.c.c2_passes; gx_c2b += gxl.c.c2_bones; gx_c2w += gxl.c.c2_words;
  if (4 * gxl.c.words + gxl.c.aux > gx_bmax) gx_bmax = 4 * gxl.c.words + gxl.c.aux;
  if (dg) {
    uint64_t g = gl_digest(&glf, &gxl); gx_all = (gx_all ^ g) * 0x100000001b3ull; gx_n++;
    char zt[224] = "";   /* gl29: the arm and its counts, as psp/xl_psp.c dz_txt prints them */
    if (gx_dz_on) { const GxDzCount *c = &gxl.dc; const int a = gxl.dz; char an[8];
      if (a == GX_DZ_B) snprintf(an, sizeof an, "B%d", gxl.dzn); else { an[0] = a >= 0 && a < GX_DZ_B ? "OXYW"[a] : '?'; an[1] = 0; }
      snprintf(zt, sizeof zt, "; dz %s 42:%ld oth:%ld pair:%ld fb:%ld,%ld,%ld,%ld,%ld,%ld bias:%ld w:%ld aux:%lu", an, (long)c->n42,
               (long)c->noth, (long)c->npair, (long)c->fb[0], (long)c->fb[1], (long)c->fb[2], (long)c->fb[3], (long)c->fb[4], (long)c->fb[5],
               (long)c->nbias, (long)c->nw, (unsigned long)c->aux); }
    fprintf(stderr, "picture: vb %ld buffer %d list %016llx frame %016llx: %ld words, %ld aux bytes, %ld batches, %ld PRIMs, %d textures, "
            "%ld dropped%s\n", vb, d, (unsigned long long)gxl.dig, (unsigned long long)g, gxl.c.words, gxl.c.aux, gxl.c.batches, gxl.c.prims,
            glf.o.nt, gxl.c.dropped, zt);
    uint64_t gc = gl_digest_c(&glf); gx_call = (gx_call ^ gc) * 0x100000001b3ull;   /* sb4: the content, for step c on against off */
    fprintf(stderr, "picture: vb %ld content %016llx\n", vb, (unsigned long long)gc);
    if (gx_sem_on) { uint64_t s = gx_sem(&gxl); gx_sem_all = (gx_sem_all ^ s) * 0x100000001b3ull; gx_sem_n++;
      fprintf(stderr, "picture: vb %ld drawn %016llx\n", vb, (unsigned long long)s); }
  }
}
static void gb_line(const GlCount *n, int64_t db, int64_t tb, const char *what)   /* db, tb: n's data and texture bytes (psp/xl_psp.c gl_line) */
{
  fprintf(stderr, "%s %ld batches (%ld transform), %ld vertices, %lld KB data, %ld textures (%lld KB, %ld new 16-bit); primitives %ld (%ld "
          "textured; left out: %ld uncertain, %ld path 1; state drawn otherwise %ld/%ld/%ld/%ld); calls %ld: converted %ld, nothing drawn %ld, "
          "bad %ld, no room %ld; other calls %ld; texture lookups %ld (memo %ld): frame buffers %ld, untextured: render targets %ld, not "
          "known %ld, refused %ld, table full %ld; dropped %ld", what, n->batches, n->xbatches, n->verts, (long long)(db >> 10), n->ntex,
          (long long)(tb >> 10), n->tex16, n->prims, n->prims_tex, n->uncertain, n->path1, n->unrep[0], n->unrep[1], n->unrep[2], n->unrep[3],
          n->calls, n->conv, n->conv_empty, n->conv_bad, n->conv_full, n->other, n->tex_asks, n->tex_memo, n->tex_fb, n->tex_rt,
          n->tex_lost, n->tex_unsup, n->tex_full, n->dropped);
  fprintf(stderr, "; P06 and P07 calls %ld, passes 2 and 3 drawn %ld, not drawn %ld", n->lit_calls, n->lit_passes, n->lit_nopass);
}
/* sb15 4t census (stagec_notes.md step 4): what sharing transforms would save in a frame's data, batches in draw order.  A: a step 4
   effect batch (sprites, quads, streaks) whose GerXform (world .. fog colour) equals the last such batch's would carry a 32-byte
   reference (texture scale, counts) instead of its 188 bytes; B: the same over all the effects (25f0 / 2870's too); C: any transform
   batch whose camera (projection, viewport, fog) equals the last transform batch's would share it (100 bytes) */
static long cx_n[3], cx_same[3], cx_fx_n, cx_fx_same, cx_cam_n, cx_cam_same, cx_frames;
static long long cx_s[3], cx_max[4]; static long cx_maxvb[4];
static void cx_frame(long vb, long long db)
{
  const GbOut *o = &glf.o;
  const size_t PRE = offsetof(GerXform, su), C0 = offsetof(GerXform, proj);
  const GerXform *l4 = 0, *lf = 0, *lc = 0; long long sA = 0, sB = 0, sC = 0;
  for (int i = 0; i < o->nb; i++) {
    const GerBatch *b = &o->b[i]; const GerXform *x;
    if (b->kind == GERK_XFORM || b->kind == GERK_XFORMF) x = (const GerXform *)(o->d + b->first);
    else if (b->kind == GERK_LIT) { const GerLit *l = (const GerLit *)(o->d + b->first); if ((l->pass & 15) != 1) continue; x = (const GerXform *)((const uint8_t *)l + l->xf); }
    else continue;
    const int k = b->entry == VC_SPR ? 0 : b->entry == VC_QUAD ? 1 : b->entry == VC_LINE ? 2 : -1;
    if (k >= 0) { cx_n[k]++; if (l4 && !memcmp(l4, x, PRE)) { cx_same[k]++; sA += (long long)sizeof(GerXform) - 32; } l4 = x; }
    if (k >= 0 || b->entry == XL_FX || b->entry == XL_FX2) { cx_fx_n++; if (lf && !memcmp(lf, x, PRE)) { cx_fx_same++; sB += (long long)sizeof(GerXform) - 32; } lf = x; }
    cx_cam_n++; if (lc && !memcmp((const uint8_t *)lc + C0, (const uint8_t *)x + C0, PRE - C0)) { cx_cam_same++; sC += (long long)(PRE - C0); } lc = x;
  }
  cx_s[0] += sA; cx_s[1] += sB; cx_s[2] += sC; cx_frames++;
  static long cx_vb = -2; if (cx_vb == -2) cx_vb = getenv("MGS2_CX_VB") ? atol(getenv("MGS2_CX_VB")) : -1;
  if (vb == cx_vb) {   /* the frame's data by kind: each batch owns the bytes from its first to the next first in address order */
    static int ord[16384]; int nb = 0;
    for (int i = 0; i < o->nb && nb < 16384; i++) if (o->b[i].kind != GERK_THROUGH) ord[nb++] = i;
    for (int i = 1; i < nb; i++) { int t = ord[i], j = i; while (j > 0 && o->b[ord[j - 1]].first > o->b[t].first) { ord[j] = ord[j - 1]; j--; } ord[j] = t; }
    static const char *KN[9] = { "df0", "effect meshes 25f0/2870", "sprites", "quads", "streaks", "P06", "P07", "other", "" };
    long long kb[9] = { 0 }; long kn[9] = { 0 };
    for (int i = 0; i < nb; i++) {
      const GerBatch *b = &o->b[ord[i]]; const uint32_t end = i + 1 < nb ? o->b[ord[i + 1]].first : (uint32_t)o->nd;
      const int k = b->kind == GERK_XFORM ? 0 : b->entry == XL_FX || b->entry == XL_FX2 ? 1 : b->entry == VC_SPR ? 2 : b->entry == VC_QUAD ? 3 :
                    b->entry == VC_LINE ? 4 : b->kind == GERK_LIT && b->entry != VC_P07 ? 5 : b->entry == VC_P07 ? 6 : 7;
      kb[k] += end - b->first; kn[k]++;
    }
    fprintf(stderr, "cx vb %ld: data %lld KB in %d transform batches (nd %u):", vb, db >> 10, nb, (unsigned)o->nd);
    for (int k = 0; k < 8; k++) fprintf(stderr, " %s %ld / %lld KB;", KN[k], kn[k], kb[k] >> 10);
    fputc('\n', stderr);
  }
  const long long v[4] = { db, db - sA, db - sB, db - sC };
  for (int j = 0; j < 4; j++) if (v[j] > cx_max[j]) { cx_max[j] = v[j]; cx_maxvb[j] = vb; }
}
static long tp_vb, tp_to;   /* (the probe window, set below) */
static void gb_frame_end(long vb)
{
  if (glf.ft && vb >= tp_vb && vb <= tp_to) { fprintf(stderr, "thin vb %ld n %d", vb, glf.h.nthin);   /* (dev, task #114) */
    for (int k = 0; k < glf.h.nthin; k++) fprintf(stderr, " %07x-%07x/%d%s", glf.h.thin[k].lo, glf.h.thin[k].hi, glf.h.thin[k].keep,
                                                  glf.h.thin[k].line ? "s" : "");
    fputc('\n', stderr); }
  gl_end(&glf);
  cx_frame(vb, (long long)glf.n.dbytes);   /* sb15 4t census */
  if (gx_on) gx_list(vb);
  char w[64]; snprintf(w, sizeof w, "xlive gb: vb %ld:", vb); gb_line(&glf.n, glf.n.dbytes, glf.n.tbytes, w);
  if (glf.zraw) {
    const GlCount *n = &glf.n;
    if (glf.zr.hi >= glf.zr.lo) fprintf(stderr, "; depth: meshes %.0f..%.0f", glf.zr.lo, glf.zr.hi); else fprintf(stderr, "; depth: meshes none");
    fprintf(stderr, " (%ld calls, %ld vertices; %ld ZTST ALWAYS left out)", n->zr_calls, glf.zr.verts, n->zr_always);
    if (glf.thi >= glf.tlo) fprintf(stderr, ", GS draws %.0f..%.0f", glf.tlo, glf.thi); else fprintf(stderr, ", GS draws none");
    fprintf(stderr, " (vertices at 0 %ld, at the top %ld); map %.0f..%.0f; Z16 primitives %ld", glf.tz0, glf.tztop, glf.map_lo, glf.map_hi, n->z16);
    if (glf.zchk) fprintf(stderr, "; outside the map: class 3 effect triangles %ld, df0 triangles of depth writers %ld, of others %ld", n->bent, n->out_w, n->out_o);
  }
  fputc('\n', stderr);
  for (int k = 0; k < ger_n; k++) if (ger_vb[k] == vb) {
    char fn[256]; snprintf(fn, sizeof fn, "%s%ld.ger", ger_pre, vb); FILE *f = fopen(fn, "wb");
    if (!f || gl_write(&glf, f, (uint32_t)vb)) fprintf(stderr, "xlive gb: cannot write %s\n", fn); else fprintf(stderr, "xlive gb: vb %ld saved as %s\n", vb, fn);
    if (f) fclose(f);
  }
}
static int shown_key, shown_scan, shown_tex, shown_pal;
static uint64_t h64(uint64_t h, const uint8_t *b, uint32_t n)
{ for (uint32_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; } return h; }
/* the PSP texture cache's key for an upload (renderer_notes 11.2): its EE address and size and 64 words spread over it (all words when
   it has fewer) */
static uint64_t sparse64(const uint8_t *b, uint32_t n)
{
  uint32_t nw = n / 4, st = nw >= 64 ? nw / 64 : 1, k; uint64_t h = 0xcbf29ce484222325ull;
  for (k = 0; k < 64 && k * st < nw; k++) h = h64(h, b + 4 * (k * st), 4);
  return h;
}
typedef struct { uint32_t ee, n; uint64_t key, full; long vb; } Up;
#define UP_N (1u << 21)
static Up *up; static long up_n;
static int32_t *xvb; static uint8_t *xbad; static uint32_t xcap;   /* by transfer number: the vblank its data started in; it differed */
/* by source (EE address and size): the rule a PSP cache could keep for sources whose content changes: once a source has shown a second
   key it is hashed in full from then on; a stale key before that is one the rule misses */
typedef struct { uint32_t ee, n; long ups, keys, stale, pmiss, vol_vb, vbytes; } Src;
#define SRC_N (1u << 16)
static Src *srcs; static long src_n, pol_miss, pol_bytes, sz_bytes[4];
static Src *src_of(uint32_t ee, uint32_t n)
{
  uint32_t i = (uint32_t)((((uint64_t)ee << 20) ^ n) * 0x9e3779b97f4a7c15ull >> 48) & (SRC_N - 1);
  for (;; i = (i + 1) & (SRC_N - 1)) {
    Src *s = &srcs[i];
    if (!s->n) { if (src_n >= SRC_N / 2) { fprintf(stderr, "xlive tex: the source table is full\n"); exit(1); }
      s->ee = ee; s->n = n; s->vol_vb = -1; src_n++; return s; }
    if (s->ee == ee && s->n == n) return s;
  }
}
static void on_data(void *ctx, uint32_t no, uint32_t ee, uint32_t n, uint32_t got, uint32_t need)
{
  (void)ctx;
  if (no >= xcap) { uint32_t c = xcap ? xcap : 1u << 16; while (c <= no) c *= 2;
    int32_t *q = realloc(xvb, c * sizeof *q); uint8_t *f = q ? realloc(xbad, c) : 0; if (!q || !f) { fprintf(stderr, "xlive tex: no memory\n"); exit(1); }
    memset(q + xcap, 0, (c - xcap) * sizeof *q); memset(f + xcap, 0, c - xcap); xvb = q; xbad = f; xcap = c; }
  if (got == n) xvb[no] = (int32_t)cur_vb;
  if (got != need) return;
  uint32_t e0 = (ee + n - got) & (GSZ - 1); if (e0 + need > GSZ) return;
  if (trx_all) fprintf(stderr, "xlive trx: vb %ld: transfer %u data: EE %07x, %u bytes\n", cur_vb, no, e0, need);
  if (cur_vb == 0) { uint32_t nz = 0; for (uint32_t k = 0; k < need; k++) nz += M.mem[e0 + k] != 0;   /* the boot's uploads: what they sent */
    fprintf(stderr, "xlive boot: transfer %u: EE %07x, %u bytes, %u not zero\n", no, e0, need, nz); }
  uint64_t key = sparse64(M.mem + e0, need), full = h64(0xcbf29ce484222325ull, M.mem + e0, need);
  ck.xfers++; ck.bytes += need;
  Src *s = src_of(e0, need); s->ups++; sz_bytes[need <= 16384 ? 0 : need <= 65536 ? 1 : need <= 262144 ? 2 : 3] += need;
  uint32_t i = (uint32_t)((((uint64_t)e0 << 20) ^ need ^ key) * 0x9e3779b97f4a7c15ull >> 43) & (UP_N - 1);
  for (;; i = (i + 1) & (UP_N - 1)) {
    Up *u = &up[i];
    if (!u->n) { if (up_n >= UP_N / 2) { fprintf(stderr, "xlive tex: the upload table is full\n"); exit(1); }
      u->ee = e0; u->n = need; u->key = key; u->full = full; u->vb = cur_vb; up_n++; ck.keynew++; if (++s->keys == 2) s->vol_vb = cur_vb; break; }
    if (u->ee == e0 && u->n == need && u->key == key) {
      if (u->full != full) { ck.keybad++; s->stale++; if (s->vol_vb < 0) { s->pmiss++; pol_miss++; }
        if (shown_key++ < 20) fprintf(stderr, "xlive tex: vb %ld transfer %u: EE %07x, %u bytes: the same key as at vb %ld, other content "
                                      "(the source's upload %ld, its keys so far %ld)\n", cur_vb, no, e0, need, u->vb, s->ups, s->keys);
        u->full = full; }
      else ck.keyhit++;
      u->vb = cur_vb; break; }
  }
  if (s->vol_vb >= 0) { s->vbytes += need; pol_bytes += need; }
}
static int src_cmp(const void *a, const void *b) { const Src *x = a, *y = b; return (y->vbytes > x->vbytes) - (y->vbytes < x->vbytes); }
static void src_report(long nvb)
{
  long k = 0, shown = 0, nvol = 0; Src *l = malloc((src_n ? src_n : 1) * sizeof *l); if (!l || nvb < 1) { free(l); return; }
  for (uint32_t i = 0; i < SRC_N; i++) if (srcs[i].n) l[k++] = srcs[i];
  qsort(l, k, sizeof *l, src_cmp);
  for (long j = 0; j < k; j++) if (l[j].keys >= 2 || l[j].stale) { nvol += l[j].keys >= 2;
    if (shown++ < 40) fprintf(stderr, "xlive tex: source EE %07x, %u bytes: %ld uploads, %ld keys, hashed in full from vb %ld (%.1f KB a vblank), "
                              "%ld stale (%ld before that)\n", l[j].ee, l[j].n, l[j].ups, l[j].keys, l[j].vol_vb, l[j].vbytes / 1024.0 / nvb, l[j].stale, l[j].pmiss); }
  fprintf(stderr, "xlive tex: %ld sources, %ld with a second key; the rule \"hash a source in full once it shows a second key\" leaves %ld stale "
          "keys and hashes %.1f KB a vblank in full; uploads a vblank by size: to 16 KB %.1f KB, to 64 KB %.1f, to 256 KB %.1f, larger %.1f\n",
          k, nvol, pol_miss, pol_bytes / 1024.0 / nvb, sz_bytes[0] / 1024.0 / nvb, sz_bytes[1] / 1024.0 / nvb, sz_bytes[2] / 1024.0 / nvb,
          sz_bytes[3] / 1024.0 / nvb);
  free(l);
}
static void scan_fn(void *ctx, uint32_t no, uint32_t ee, uint32_t got, uint32_t first, uint32_t nd)
{
  (void)ctx; ck.scan_bad++; ck.scan_bytes += nd;
  if (no < xcap && !xbad[no]) { xbad[no] = 1; ck.scan_badx++;
    if (shown_scan++ < 20) fprintf(stderr, "xlive tex: vb %ld: transfer %u (data from vb %d), EE %07x, %u bytes: %u bytes differ from as sent, the first at +%u\n",
                                   cur_vb, no, no < xcap ? xvb[no] : -1, ee, got, nd, first); }
}
/* the textures the draws use: once per vblank and per state of what they read (transfers for the texels, CLUT loads for the palette) */
typedef struct { uint64_t a, b; long e, vb; } Seen;
#define SEEN_N 4096u
static Seen seen_tex[SEEN_N], seen_pal[SEEN_N];
static int seen_before(Seen *t, uint64_t a, uint64_t b, long e)
{
  uint64_t h = a * 0x9e3779b97f4a7c15ull ^ b * 0xff51afd7ed558ccdull ^ (uint64_t)e * 0xc4ceb9fe1a85ec53ull;
  Seen *s = &t[(h >> 40) & (SEEN_N - 1)];
  if (s->a == a && s->b == b && s->e == e && s->vb == cur_vb) return 1;
  s->a = a; s->b = b; s->e = e; s->vb = cur_vb; return 0;
}
static uint8_t txbuf[1 << 20];
typedef struct { int psm; uint32_t tbp, tbw; uint64_t t0, cl; int told, direct; long n, unk; } Fetch;
/* texels read and not known, by texture (TBP0 TBW PSM) */
typedef struct { uint32_t k; long n, unk, first; } KeyCnt;
#define KC_N 4096u
static KeyCnt kc[KC_N]; static long kc_used;
static void kc_add(const Fetch *f)
{
  uint32_t k = (uint32_t)(f->t0 & 0x3ffffff) | 1u << 31, i = (k * 0x9e3779b1u) >> 20 & (KC_N - 1);
  if (!f->n) return;
  for (;; i = (i + 1) & (KC_N - 1)) {
    KeyCnt *c = &kc[i];
    if (!c->k) { if (kc_used >= (long)KC_N / 2) return; c->k = k; c->first = -1; kc_used++; }
    if (c->k == k) { c->n += f->n; c->unk += f->unk; if (f->unk && c->first < 0) c->first = cur_vb; return; }
  }
}
static int kc_cmp(const void *a, const void *b) { const KeyCnt *x = a, *y = b; return (y->unk > x->unk) - (y->unk < x->unk); }
static void kc_report(void)
{
  KeyCnt *l = malloc(KC_N * sizeof *l); long n = 0; if (!l) return;
  for (uint32_t i = 0; i < KC_N; i++) if (kc[i].k && kc[i].unk) l[n++] = kc[i];
  qsort(l, (size_t)n, sizeof *l, kc_cmp);
  for (long j = 0; j < n && j < 24; j++)
    fprintf(stderr, "xlive tex: texture tbp %04x tbw %u psm %02x: %ld texels read, %ld not known (the first in vb %ld)\n", l[j].k & 0x3fff,
            l[j].k >> 14 & 63, l[j].k >> 20 & 63, l[j].n, l[j].unk, l[j].first);
  free(l);
}
/* MGS2_XLIVE_TEX=2: the first time a texture (TBP, TBW, PSM) has an unknown texel or cannot be converted */
static uint32_t told_k[64]; static int told_n;
static int first_tell(uint64_t t0)
{
  uint32_t k = (uint32_t)(t0 & 0x3ffffff); int i;   /* TBP0 TBW PSM */
  for (i = 0; i < told_n; i++) if (told_k[i] == k) return 0;
  if (told_n == 64) return 0;
  told_k[told_n++] = k; return 1;
}
static void tell(const char *what, uint64_t t0, uint64_t cl, int x, int y)
{
  fprintf(stderr, "xlive tex: vb %ld: %s: tbp %04x tbw %u psm %02x tw %u th %u cbp %04x cpsm %x csa %u; clamp wms %u wmt %u %u..%u %u..%u",
          cur_vb, what, (unsigned)(t0 & 0x3fff), (unsigned)(t0 >> 14 & 63), (unsigned)(t0 >> 20 & 63), (unsigned)(t0 >> 26 & 15), (unsigned)(t0 >> 30 & 15),
          (unsigned)(t0 >> 37 & 0x3fff), (unsigned)(t0 >> 51 & 15), (unsigned)(t0 >> 56 & 31), (unsigned)(cl & 3), (unsigned)(cl >> 2 & 3),
          (unsigned)(cl >> 4 & 1023), (unsigned)(cl >> 14 & 1023), (unsigned)(cl >> 24 & 1023), (unsigned)(cl >> 34 & 1023));
  if (x >= 0) {
    int psm = (int)(t0 >> 20 & 63); uint32_t tbp = (uint32_t)(t0 & 0x3fff), tbw = (uint32_t)(t0 >> 14 & 63), wa; char b[1024];
    x &= 2047; y &= 2047;
    wa = psm == GS_T8 ? gs_pa8(tbp, tbw, x, y) >> 2 : psm == GS_T4 ? gs_pa4(tbp, tbw, x, y) >> 3
       : psm == GS_CT16 || psm == GS_CT16S ? gs_pa16(tbp, tbw, x, y, psm == GS_CT16S, 0) >> 2 : gs_pa32(tbp, tbw, x, y, 0) >> 2;
    gu_why(map, wa, b, sizeof b); fprintf(stderr, "; texel (%d, %d), word %05x, %s", x, y, wa, b);
  }
  fprintf(stderr, "\n");
}
static int fetch(void *ctx, int x, int y)
{
  Fetch *f = ctx; int64_t v = gu_texel(map, f->psm, f->tbp, f->tbw, x, y), r;
  gu_read_sent(map, 1); r = gu_texel(map, f->psm, f->tbp, f->tbw, x, y); gu_read_sent(map, 0);
  long *n = f->direct ? &ck.dtexels : &ck.texels, *miss = f->direct ? &ck.dmiss : &ck.tmiss, *one = f->direct ? &ck.done1 : &ck.tone;
  ++*n; f->n++;
  if (v < 0 || r < 0) { if (v < 0 && r < 0) ++*miss; else ++*one;
    f->unk++;
    if (tx_detail && !f->told++ && first_tell(f->t0)) tell(v < 0 && r < 0 ? "a texel the map cannot tell" : "a texel known one way only", f->t0, f->cl, x, y);
    return v < 0 ? 0 : (int)v; }
  if (v != r) { if (f->direct) ck.ddiff++; else ck.tdiff++;
    if (shown_tex++ < 20) fprintf(stderr, "xlive tex: vb %ld: texel (%d, %d) of tbp %04x tbw %u psm %02x: %llx from EE RAM now, %llx as sent\n", cur_vb, x, y,
                                  f->tbp, f->tbw, f->psm, (unsigned long long)v, (unsigned long long)r); }
  return (int)v;
}
/* the texels a front-end primitive samples: its vertices' texel coordinates, one more each side (bilinear); 0: the whole texture */
static int prim_box(const GfDraw *d, const GfVtx *const *v, int n, int box[4])
{
  float a = 1e30f, b = 1e30f, c = -1e30f, e = -1e30f; int tw = 1 << d->tw, th = 1 << d->th;
  if (d->wms == 3 || d->wmt == 3) return 0;
  for (int i = 0; i < n; i++) {
    float s, t;
    if (d->fst) { s = v[i]->u / 16.0f; t = v[i]->v / 16.0f; }
    else { if (!(v[i]->q > 0)) return 0; s = v[i]->s / v[i]->q * (float)tw; t = v[i]->t / v[i]->q * (float)th; }
    if (!(s > -4096 && s < 4096 && t > -4096 && t < 4096)) return 0;
    if (s < a) a = s;
    if (s > c) c = s;
    if (t < b) b = t;
    if (t > e) e = t;
  }
  box[0] = (int)floorf(a) - 1; box[1] = (int)floorf(b) - 1; box[2] = (int)ceilf(c) + 1; box[3] = (int)ceilf(e) + 1;
  if (d->wms == 2) { if (box[0] < d->minu) box[0] = d->minu; if (box[2] > d->maxu) box[2] = d->maxu; }
  if (d->wmt == 2) { if (box[1] < d->minv) box[1] = d->minv; if (box[3] > d->maxv) box[3] = d->maxv; }
  if (box[0] < 0) box[0] = 0;
  if (box[1] < 0) box[1] = 0;
  if (box[2] >= tw) box[2] = tw - 1;
  if (box[3] >= th) box[3] = th - 1;
  return box[0] <= box[2] && box[1] <= box[3];
}
/* the PSP texture path (ge/texcache.h) on this use: its texture and palette against the map's (t, data: what tc_texture_cb made from
   the map; f: its unknown-texel count) */
static void tk_check(const uint64_t *gr, int ctxt, const TcTex *t, const uint8_t *data, const Fetch *f)
{
  uint64_t t0 = gr[0x06 + ctxt]; TkTex k; int kind = tk_get(tkc, gr, ctxt, &k);
  int n = (t->psm == 8) ? 256 : 16, csa = (int)(t0 >> 56 & 31), cpsm = (int)(t0 >> 51 & 15);
  if (t->psm == 16) n = 0;   /* direct colour: its texels carry the colours */
  ck.tk_uses++;
  switch (kind) {
    case TK_RT: ck.tk_rt++; return;
    case TK_LOST: ck.tk_lost++; return;
    case TK_UNSUP: ck.tk_unsup++; return;
    default: ck.tk_tex++;
  }
  if (f->unk) return;   /* the map could not give every texel of this region: the cache may hold an older, complete conversion */
  ck.tk_cmp++;
  int bad = 0;
  if (k.t.psm != t->psm || k.t.w != t->w || k.t.h != t->h || k.t.tw != t->tw || k.t.th != t->th || k.t.bw != t->bw || k.t.rows != t->rows
      || k.t.u0 != t->u0 || k.t.v0 != t->v0 || k.t.repeat != t->repeat) bad = -1;
  else for (int y = 0; y < t->rows && !bad; y++) for (int x = 0; x < t->bw; x++)   /* the texels it stores: the region plus its edge */
    if (tc_texel(&k.t, k.data, x, y) != tc_texel(t, data, x, y)) { bad = 1; if (shown_tk++ < 20)
        fprintf(stderr, "xlive tk: vb %ld: texture tbp %04x psm %02x: texel (%d, %d) %d in the cache, %d from the map\n", cur_vb,
                (unsigned)(t0 & 0x3fff), (unsigned)(t0 >> 20 & 63), x, y, tc_texel(&k.t, k.data, x, y), tc_texel(t, data, x, y)); break; }
  if (bad) { ck.tk_bad++; if (bad < 0 && shown_tk++ < 20)
      fprintf(stderr, "xlive tk: vb %ld: texture tbp %04x psm %02x: the cache's layout differs (%d x %d of %d x %d at %d,%d, stored %d x %d, repeat %d;"
              " the map's %d x %d of %d x %d at %d,%d, stored %d x %d, repeat %d)\n", cur_vb, (unsigned)(t0 & 0x3fff), (unsigned)(t0 >> 20 & 63),
              k.t.w, k.t.h, k.t.tw, k.t.th, k.t.u0, k.t.v0, k.t.bw, k.t.rows, k.t.repeat, t->w, t->h, t->tw, t->th, t->u0, t->v0, t->bw, t->rows, t->repeat); }
  if (!n) return;
  if (k.npal != n) { ck.tk_pbad++; return; }
  for (int i = 0; i < n; i++) {
    int64_t v = gu_clut(map, cpsm, csa * 16 + i); uint32_t want;
    if (v < 0) continue;   /* an entry the map does not know: as above */
    if (cpsm == GS_CT32 || cpsm == GS_CT24) { uint32_t a = cpsm == GS_CT24 ? (uint32_t)(gr[0x3b] & 0xff) : (uint32_t)v >> 24;
      want = ((uint32_t)v & 0xffffff) | (a >= 0x80 ? 0xffu : a * 2) << 24; }
    else { uint32_t c = (uint32_t)v, col = (c & 31) << 3 | (c >> 5 & 31) << 11 | (c >> 10 & 31) << 19, texa = (uint32_t)gr[0x3b],
             a = (c >> 15 & 1) ? (uint32_t)(gr[0x3b] >> 32 & 0xff) : ((texa >> 15 & 1) && !(c & 0x7fff)) ? 0 : (texa & 0xff);
           want = col | (a >= 0x80 ? 0xffu : a * 2) << 24; }
    if (k.pal[i] != want) {
      ck.tk_pbad++;
      if (shown_tk++ < 20)
        fprintf(stderr, "xlive tk: vb %ld: palette entry %d (cpsm %x csa %d): %08x in the cache, %08x from the map\n", cur_vb, i, cpsm, csa, k.pal[i], want);
      break;
    }
  }
}
static void tex_use(const uint64_t *gr, int ctxt, const int *box)
{
  uint64_t t0 = gr[0x06 + ctxt], cl = gr[0x08 + ctxt]; int psm = (int)(t0 >> 20 & 63), cpsm = (int)(t0 >> 51 & 15), csa = (int)(t0 >> 56 & 31);
  const GuStats *U = gu_stats(map);
  ck.uses++;
  if (psm == GS_CT32 || psm == GS_CT24 || psm == GS_CT16 || psm == GS_CT16S) {   /* direct colour: the texels its box samples, else the
                                                                                    whole texture (its clamp region) */
    int tw = 1 << (t0 >> 26 & 15), th = 1 << (t0 >> 30 & 15), wms = (int)(cl & 3), wmt = (int)(cl >> 2 & 3), all[4];
    Fetch f = { psm, (uint32_t)(t0 & 0x3fff), (uint32_t)(t0 >> 14 & 63), t0, cl, 0, 1, 0, 0 };
    if (!box) { all[0] = wms == 2 ? (int)(cl >> 4 & 1023) : 0; all[2] = wms == 2 ? (int)(cl >> 14 & 1023) : tw - 1;
                all[1] = wmt == 2 ? (int)(cl >> 24 & 1023) : 0; all[3] = wmt == 2 ? (int)(cl >> 34 & 1023) : th - 1; box = all; }
    uint64_t bk = (uint64_t)box[0] | (uint64_t)box[1] << 11 | (uint64_t)box[2] << 22 | (uint64_t)box[3] << 33;
    if (seen_before(seen_tex, t0 & 0x3ffffffffull, bk ^ cl * 0x9e3779b97f4a7c15ull, U->xfers + U->xfer_local + U->drawn)) return;
    ck.direct++;
    for (int y = box[1]; y <= box[3]; y++) for (int x = box[0]; x <= box[2]; x++) fetch(&f, x, y);
    kc_add(&f);
    if (tk_on && (psm == GS_CT16 || psm == GS_CT16S)) {   /* the texture path takes 16-bit textures as GE 5551 */
      size_t sz = tc_texture_size(t0, cl); TcTex t; Fetch g = { psm, (uint32_t)(t0 & 0x3fff), (uint32_t)(t0 >> 14 & 63), t0, cl, 0, 1, 0, 0 };
      if (sz && sz <= sizeof txbuf && !tc_texture_cb(t0, cl, fetch, &g, &t, txbuf)) tk_check(gr, ctxt, &t, txbuf, &g);
    }
    return;
  }
  if (psm != GS_T8 && psm != GS_T4 && psm != GS_T8H && psm != GS_T4HL && psm != GS_T4HH) { ck.nonidx++; return; }
  if (gr[0x14 + ctxt] >> 2 & 7) ck.mip++;   /* TEX1 MXL: only level 0 is read here */
  int n = (psm == GS_T8 || psm == GS_T8H) ? 256 : 16;
  if (!seen_before(seen_pal, (uint64_t)cpsm << 8 | (uint64_t)csa, (uint64_t)n, U->loads)) {   /* entries csa * 16 + i, as gsref reads them */
    ck.pchecks++;
    for (int i = 0; i < n; i++) {
      int k = csa * 16 + i; int64_t v = gu_clut(map, cpsm, k), r;
      gu_read_sent(map, 1); r = gu_clut(map, cpsm, k); gu_read_sent(map, 0);
      ck.pal++;
      if (v < 0 || r < 0) { if (v < 0 && r < 0) ck.pmiss++; else ck.pone++; continue; }
      if (v != r) { ck.pdiff++;
        if (shown_pal++ < 20) fprintf(stderr, "xlive tex: vb %ld: palette entry %d (CPSM %x): %llx from EE RAM now, %llx as sent\n", cur_vb, k, cpsm, (unsigned long long)v, (unsigned long long)r); }
    }
  }
  size_t sz = tc_texture_size(t0, cl); TcTex t; Fetch f = { psm, (uint32_t)(t0 & 0x3fff), (uint32_t)(t0 >> 14 & 63), t0, cl, 0, 0, 0, 0 };
  if (box) {   /* a primitive: the texels in its box */
    uint64_t bk = (uint64_t)box[0] | (uint64_t)box[1] << 11 | (uint64_t)box[2] << 22 | (uint64_t)box[3] << 33;
    if (seen_before(seen_tex, t0 & 0x3ffffffffull, bk ^ cl * 0x9e3779b97f4a7c15ull, U->xfers + U->xfer_local + U->drawn)) return;
    if (!sz) { ck.unsup++; if (tx_detail && first_tell(t0)) tell("not convertible", t0, cl, -1, -1); }
    ck.boxes++;
    for (int y = box[1]; y <= box[3]; y++) for (int x = box[0]; x <= box[2]; x++) fetch(&f, x, y);
    kc_add(&f); return;
  }
  if (seen_before(seen_tex, t0 & 0x3ffffffffull, cl, U->xfers + U->xfer_local + U->drawn)) return;   /* TBP0 TBW PSM TW TH, CLAMP */
  if (!sz || sz > sizeof txbuf || tc_texture_cb(t0, cl, fetch, &f, &t, txbuf)) { ck.unsup++; if (tx_detail && first_tell(t0)) tell("not convertible", t0, cl, -1, -1); kc_add(&f); return; }
  ck.conv++; kc_add(&f);
  if (tk_on) tk_check(gr, ctxt, &t, txbuf, &f);
}
static int call_ctxt = -1;   /* the current call's PRIM context, for its P06 passes (-1: the call does not texture) */
static void on_pass(void *ctx, const XlCall *c, int pass)
{
  (void)ctx;
  if (call_ctxt >= 0) { ck.passes++; tex_use(xl_front()->gr, call_ctxt, 0); }
  if (gb_now) gl_pass(&glf, c, pass);
}
static void tex_call(const XlCall *c)
{
  uint64_t prim = c->gr[0x00]; call_ctxt = -1;
  if (c->tag_qa == ~0u) { ck.notpl++; return; }   /* no tag template: the PRIM it draws with is not known */
  if (!(prim >> 4 & 1)) return;
  call_ctxt = (int)(prim >> 9 & 1); ck.calls++; tex_use(c->gr, call_ctxt, 0);
}
static void tx_line(const TxCount *k, long a, long b)
{
  fprintf(stderr, "xlive tex: vb %ld..%ld: uploads %ld (%ld KB), keys new %ld, seen %ld, seen with other content %ld; transfers compared %ld, "
          "differing %ld (%ld distinct, %ld bytes); texture uses %ld (calls %ld, P06 passes %ld, primitives %ld), other formats %ld, call without "
          "a template %ld, mipmapped %ld; conversions %ld, primitive boxes %ld, not convertible %ld: texels %ld, unknown %ld, known one way %ld, differing %ld; "
          "palettes %ld: entries %ld, unknown %ld, known one way %ld, differing %ld; direct colour %ld: texels %ld, unknown %ld, known one way %ld, "
          "differing %ld; the texture path: %ld uses (%ld textures, %ld render targets, %ld not known, %ld refused), %ld compared, %ld with a "
          "differing texel, %ld with a differing palette\n", a, b, k->xfers, k->bytes >> 10, k->keynew, k->keyhit,
          k->keybad, k->scanned, k->scan_bad, k->scan_badx, k->scan_bytes, k->uses, k->calls, k->passes, k->prims, k->nonidx, k->notpl, k->mip,
          k->conv, k->boxes, k->unsup, k->texels, k->tmiss, k->tone, k->tdiff, k->pchecks, k->pal, k->pmiss, k->pone, k->pdiff,
          k->direct, k->dtexels, k->dmiss, k->done1, k->ddiff,
          k->tk_uses, k->tk_tex, k->tk_rt, k->tk_lost, k->tk_unsup, k->tk_cmp, k->tk_bad, k->tk_pbad);
}
static void tx_add(void)
{
  long *s = (long *)&ck, *d = (long *)&ck_all;
  for (size_t i = 0; i < sizeof ck / sizeof (long); i++) d[i] += s[i];
  memset(&ck, 0, sizeof ck);
}

/* each quadword as ee_io.h's pkt_fnv / pkt_tag hash it (a tag: only the fields that act) */
static void seen(void *ctx, int ch, uint32_t a, const uint8_t *q, int tag, uint32_t chcr)
{
  uint32_t w[4]; const uint8_t *p = q; (void)ctx; (void)a;
  if (tag) { uint32_t id; memcpy(w, q, 16); id = (w[0] >> 28) & 7;
    w[0] &= 0xf000ffffu; if (id == 1 || id == 6 || id == 7) w[1] = 0; if (ch != 1 || !(chcr & 0x40)) w[2] = w[3] = 0; p = (const uint8_t *)w; }
  for (int i = 0; i < 16; i++) { ph ^= p[i]; ph *= 0x100000001b3ull; }
}
static long ncall, nprim; static int sync_on, dig_on, cg_on; static double t_walk; static uint8_t lvm[16384] __attribute__((aligned(16)));
#define EC_N 256
typedef struct { uint32_t entry; long calls, notpl, first, lane[5]; } EntCnt;   /* (sb6: lane[k] calls of lane k, 0 the generic walk) */
static EntCnt ec[EC_N]; static int ec_n, ec_last, census_n; static long census_at[32];
static void ec_add(const XlCall *c)
{
  int i = ec_last;
  if (i >= ec_n || ec[i].entry != c->entry) {
    for (i = 0; i < ec_n && ec[i].entry != c->entry; i++) {}
    if (i == ec_n) { if (ec_n == EC_N) return; ec[ec_n++] = (EntCnt){ c->entry, 0, 0, cur_vb, { 0, 0, 0, 0, 0 } };
      fprintf(stderr, "xlive: vb %ld: entry %06x called first (%s)\n", cur_vb, (unsigned)c->entry, xl_rule(c->entry) ? "a rule" : "no rule"); }
    ec_last = i;
  }
  ec[i].calls++; if (c->tag_qa == ~0u) ec[i].notpl++;
  ec[i].lane[c->lane >= 0 && c->lane <= 4 ? c->lane : 0]++;
}
static void ec_report(long vb)
{
  for (int i = 0; i < ec_n; i++) fprintf(stderr, "xlive census: to vb %ld: entry %06x (%s): %ld calls, %ld without a tag template, the first in vb %ld\n",
                                         vb, (unsigned)ec[i].entry, xl_rule(ec[i].entry) ? "a rule" : "no rule", ec[i].calls, ec[i].notpl, ec[i].first);
  if (g_xl_lane) for (int i = 0; i < ec_n; i++) if (ec[i].calls != ec[i].lane[0])   /* sb6: the entries a lane called, by lane */
    fprintf(stderr, "sb census: to vb %ld: entry %06x: %ld calls: the generic walk %ld, lane 1 %ld, lane 2 %ld, lane 3 %ld, lane 4 %ld\n", vb,
            (unsigned)ec[i].entry, ec[i].calls, ec[i].lane[0], ec[i].lane[1], ec[i].lane[2], ec[i].lane[3], ec[i].lane[4]);
}
static void on_trx(void *ctx, const uint64_t *gr)
{
  uint64_t b = gr[0x50], p = gr[0x51], r = gr[0x52]; int dir = (int)(gr[0x53] & 3), dpsm = (int)(b >> 56 & 63); (void)ctx;
  if (dir == 2)
    fprintf(stderr, "xlive trx: vb %ld: local copy: sbp %04x sbw %u spsm %02x at %u,%u -> dbp %04x dbw %u dpsm %02x at %u,%u, %ux%u, dir %u\n", cur_vb,
            (unsigned)(b & 0x3fff), (unsigned)(b >> 16 & 63), (unsigned)(b >> 24 & 63), (unsigned)(p & 0x7ff), (unsigned)(p >> 16 & 0x7ff),
            (unsigned)(b >> 32 & 0x3fff), (unsigned)(b >> 48 & 63), (unsigned)dpsm, (unsigned)(p >> 32 & 0x7ff), (unsigned)(p >> 48 & 0x7ff),
            (unsigned)(r & 0xfff), (unsigned)(r >> 32 & 0xfff), (unsigned)(p >> 59 & 3));
  else if (trx_all || (dpsm != GS_CT32 && dpsm != GS_CT24 && dpsm != GS_T8H && dpsm != GS_T4HL && dpsm != GS_T4HH))
    fprintf(stderr, "xlive trx: vb %ld: upload %ld: dbp %04x dbw %u dpsm %02x at %u,%u, %ux%u\n", cur_vb, gu_stats(map)->xfers + 1, (unsigned)(b >> 32 & 0x3fff), (unsigned)(b >> 48 & 63),
            (unsigned)dpsm, (unsigned)(p >> 32 & 0x7ff), (unsigned)(p >> 48 & 0x7ff), (unsigned)(r & 0xfff), (unsigned)(r >> 32 & 0xfff));
}
/* the volume census (MGS2_XLIVE_VCEN=1) */
static int vc_on; static XlStats vc_prev; static long vc_calls[3], vc_pcalls[3], vc_pprim, vc_pqw;
static void vc_cline(long vb)   /* (sb15: was vc_line, now ge/vu1conv.h's streak converter) */
{
  const XlStats *S = xl_stats(); long cw = 0, d[4] = { 0, 0, 0, 0 };   /* data words: UNPACK, MPG, DIRECT, the rest */
  for (int c = 0; c < 128; c++) {
    cw += S->cmds[c] - vc_prev.cmds[c];
    d[(c & 0x60) == 0x60 ? 0 : c == 0x4a ? 1 : (c == 0x50 || c == 0x51) ? 2 : 3] += S->wd[c] - vc_prev.wd[c]; }
  fprintf(stderr, "xlive vc: vb %ld w %ld cw %ld up %ld mpg %ld dir %ld oth %ld g1 %ld g2 %ld g3 %ld i2 %ld i3 %ld qw %ld calls %ld %ld %ld prims %ld uq",
          vb, S->words - vc_prev.words, cw, d[0], d[1], d[2], d[3], S->gq[1] - vc_prev.gq[1], S->gq[2] - vc_prev.gq[2], S->gq[3] - vc_prev.gq[3],
          S->img_qw[2] - vc_prev.img_qw[2], S->img_qw[3] - vc_prev.img_qw[3], M.qw - vc_pqw, vc_calls[0] - vc_pcalls[0], vc_calls[1] - vc_pcalls[1],
          vc_calls[2] - vc_pcalls[2], nprim - vc_pprim);
  for (int f = 0; f < 32; f++) if (S->uq[f] != vc_prev.uq[f]) fprintf(stderr, " %02x:%ld", f, S->uq[f] - vc_prev.uq[f]);
  fputc('\n', stderr);
  vc_prev = *S; memcpy(vc_pcalls, vc_calls, sizeof vc_calls); vc_pprim = nprim; vc_pqw = M.qw;
}
static void vc_start(void) { vc_prev = *xl_stats(); memcpy(vc_pcalls, vc_calls, sizeof vc_calls); vc_pprim = nprim; vc_pqw = M.qw; }
static long tp_vb = -1, tp_to = -1;   /* sb1 probe: MGS2_SB_PROBE=<vb>: every tag of that vblank's walks, and the calls between them */
static const uint8_t *tp_mem;
void xl_tag_probe(const uint32_t *t, uint32_t tadr)
{
  if (cur_vb < tp_vb || cur_vb > tp_to) return;
  uint32_t id = t[0] >> 28 & 7, n = t[0] & 0xffff;
  fprintf(stderr, "tag %07x id %u qwc %3u addr %08x tte %08x %08x", tadr, id, n, t[1], t[2], t[3]);
  if (id == 1 && n <= 4) for (uint32_t k = 0; k < 4 * n; k++) fprintf(stderr, " %08x", t[4 + k]);
  fputc('\n', stderr);
}
static void fx_dump(const XlCall *c)   /* dev, task #114 (MGS2_SB_PROBE window): an effect call's vertices in short */
{
  static VcVertF v[1024]; static uint16_t fl[1024]; VcXform x;
  xl_vm_sync();
  int n = vc_fpos(c->vm, c->top, &x, v, fl, 1024, 1);
  if (n <= 0) { fprintf(stderr, "fx n %d\n", n); return; }
  int st = 0, tri = 0, same = 1, a0 = 255, a1 = 0, beh = 0;
  float b[16]; for (int k = 0; k < 16; k += 2) { b[k] = 1e30f; b[k + 1] = -1e30f; }
  for (int i = 0; i < n; i++) {
    if ((fl[i] & 0x8000) && i + 1 < n && (fl[i + 1] & 0x8000) && (i == 0 || !(fl[i - 1] & 0x8000))) st++;
    if (i >= 2 && !(fl[i] & 0x8000)) tri++;
    if (i && (v[i].x != v[0].x || v[i].y != v[0].y || v[i].z != v[0].z)) same = 0;
    float p[3] = { v[i].x, v[i].y, v[i].z }, cc[4]; vc_vu_clip(x.m, p, cc);
    float q[8] = { cc[3] > 0 ? cc[0] / cc[3] : 0, cc[3] > 0 ? cc[1] / cc[3] : 0, cc[3], p[0], p[1], p[2], v[i].u, v[i].v };
    if (cc[3] <= 0) beh++;
    for (int k = 0; k < 8; k++) { if (q[k] < b[2 * k]) b[2 * k] = q[k]; if (q[k] > b[2 * k + 1]) b[2 * k + 1] = q[k]; }
    int a = (int)(v[i].c >> 24); if (a < a0) a0 = a; if (a > a1) a1 = a;
  }
  fprintf(stderr, "fx entry %06x n %d strips %d tris %d same %d behind %d ndc x %.2f..%.2f y %.2f..%.2f w %.0f..%.0f world %.0f..%.0f %.0f..%.0f "
          "%.0f..%.0f uv %.3f..%.3f %.3f..%.3f a %d..%d\n", c->entry, n, st, tri, same, beh, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8],
          b[9], b[10], b[11], b[12], b[13], b[14], b[15], a0, a1);
  static int fs = -1; if (fs < 0) fs = getenv("MGS2_FX_STRIPS") != 0;   /* each strip of the speckle texture's calls: centre, size, alpha */
  int cx = (int)(c->gr[0x00] >> 9 & 1);
  if (fs && (c->gr[0x06 + cx] & 0x3fff) == 0x2000)
    for (int i = 0; i < n; ) {
      int e = i + 1; while (e < n && !((fl[e] & 0x8000) && e + 1 < n && (fl[e + 1] & 0x8000) && !(fl[e - 1] & 0x8000))) e++;
      float s[3] = { 0, 0, 0 }, lo = 1e30f, hi = -1e30f;
      for (int j = i; j < e; j++) { s[0] += v[j].x; s[1] += v[j].y; s[2] += v[j].z; if (v[j].x < lo) lo = v[j].x; if (v[j].x > hi) hi = v[j].x; }
      fprintf(stderr, "fs %d n %d c %.0f %.0f %.0f size %.0f a %u\n", i, e - i, s[0] / (e - i), s[1] / (e - i), s[2] / (e - i), hi - lo, v[i].c >> 24);
      i = e;
    }
}
/* task #114 check (MGS2_THIN_CHK=1, x86): each thinned effect call converted again with no thinning, and the two compared: the same
   vertices and state, every triangle of a kept strip in the same class, every one of a left-out strip class 0, and the polygons of the
   kept strips the same, in the same order */
static long tc_n, tc_bad, tc_empty, tc_drop_tris, tc_keep_tris, tc_fans;
static void thin_chk(const XlCall *c)
{
  static uint8_t b0[GL_XCAP], b1[GL_XCAP]; static VcVertF v[1024]; static uint16_t fl[1024]; static uint8_t drop[1024];
  int keep = 1;
  for (int k = 0; k < glf.h.nthin; k++) if (!glf.h.thin[k].line && c->src - glf.h.thin[k].lo < glf.h.thin[k].hi - glf.h.thin[k].lo) { keep = glf.h.thin[k].keep; break; }
  if (keep < 2) return;
  xl_vm_sync();
  XcHost h0 = glf.h; h0.nthin = 0;
  GerBatch g0, g1; uint32_t s0 = 0, s1 = 0; memset(&g0, 0, sizeof g0); memset(&g1, 0, sizeof g1);
  int r0 = xc_call(c, &h0, &g0, b0, GL_XCAP, &s0), r1 = xc_call(c, &glf.h, &g1, b1, GL_XCAP, &s1);
  tc_n++;
  const int tme = (int)(((c->gr[0x1a] & 1) ? c->gr[0x00] : c->gr[0x1b]) >> 4 & 1);   /* (xconv.c xc_gs's TME) */
  VcXform x; int n = vc_fpos(c->vm, c->top, &x, v, fl, 1024, glf.h.c4x ? tme : 1);   /* (sb15 4x: see below) */
  for (int i = 0, st = 0; i < n; i++) { if (i && (fl[i] & 0x8000) && i + 1 < n && (fl[i + 1] & 0x8000) && !(fl[i - 1] & 0x8000)) st++; drop[i] = st % keep != 0; }
  if (r1 == 0 && r0 == 1) {   /* nothing of the kept strips drawn: then no kept triangle of the full call is drawn either */
    const GerXform *X = (const GerXform *)b0; const uint8_t *cl = b0 + sizeof *X + (X->nv + X->nfv) * sizeof(VcVertF);
    const uint16_t *fa = (const uint16_t *)(cl + ((X->nv + 3) & ~3u)); int bad = 0;
    const VcVertF *w = (const VcVertF *)(b0 + sizeof *X);   /* (class 1 with three equal positions: zero area, not drawn) */
    for (uint32_t i = 2; i < X->nv; i++) if (!drop[i] && cl[i] == 1 && !(w[i].x == w[i - 1].x && w[i].y == w[i - 1].y && w[i].z == w[i - 1].z
        && w[i].x == w[i - 2].x && w[i].y == w[i - 2].y && w[i].z == w[i - 2].z)) bad = 1;
    for (uint32_t k = 0; k < X->nfan; k++) if (!drop[fa[2 * k]]) bad = 1;
    tc_empty++; tc_bad += bad; return; }
  if (r0 != r1 || r0 != 1) { if (r0 != r1) tc_bad++; return; }
  const GerXform *X0 = (const GerXform *)b0, *X1 = (const GerXform *)b1; int bad = 0;
  if (X0->nv != X1->nv || (int)X0->nv != n) bad |= 1;   /* sb15 4x: the vertices of the kept strips (a left-out strip's hold its world
                                                           positions with C4X, the full call's their clip positions; neither is drawn) */
  else for (int i = 0; i < n; i++) if (!drop[i] && memcmp(b0 + sizeof *X0 + i * sizeof(VcVertF), b1 + sizeof *X1 + i * sizeof(VcVertF), sizeof(VcVertF))) { bad |= 1; break; }
  GerXform t0 = *X0, t1 = *X1; t0.nfan = t1.nfan = 0; t0.nfv = t1.nfv = 0; if (memcmp(&t0, &t1, sizeof t0)) bad |= 2;
  GerBatch q0 = g0, q1 = g1; q0.first = q1.first = 0; if (memcmp(&q0, &q1, sizeof q0)) bad |= 4;   /* (the state, twopass, the count) */
  const uint8_t *c0 = b0 + sizeof *X0 + (X0->nv + X0->nfv) * sizeof(VcVertF), *c1 = b1 + sizeof *X1 + (X1->nv + X1->nfv) * sizeof(VcVertF);
  const VcVertF *p0 = (const VcVertF *)(b0 + sizeof *X0 + X0->nv * sizeof(VcVertF)), *p1 = (const VcVertF *)(b1 + sizeof *X1 + X1->nv * sizeof(VcVertF));
  const uint16_t *f0 = (const uint16_t *)(c0 + ((X0->nv + 3) & ~3u)), *f1 = (const uint16_t *)(c1 + ((X1->nv + 3) & ~3u));
  for (int i = 2; i < n && !(bad & 8); i++) {
    if (drop[i]) { if (c1[i]) bad |= 8; if (!(fl[i] & 0x8000)) tc_drop_tris++; }
    else { if (c1[i] != c0[i]) bad |= 8; if (!(fl[i] & 0x8000)) tc_keep_tris++; } }
  uint32_t k1 = 0, v0 = 0, v1 = 0;
  for (uint32_t k = 0; k < X0->nfan; k++) {   /* the full call's polygons, the kept ones in order */
    uint32_t m = f0[2 * k + 1];
    if (!drop[f0[2 * k]]) {
      if (k1 >= X1->nfan || f1[2 * k1] != f0[2 * k] || f1[2 * k1 + 1] != m || memcmp(p0 + v0, p1 + v1, m * sizeof(VcVertF))) { bad |= 16; break; }
      k1++; v1 += m; tc_fans++; }
    v0 += m; }
  if (k1 != X1->nfan) bad |= 16;
  if (bad) { tc_bad++; if (tc_bad <= 10) fprintf(stderr, "thin check: vb %ld call src %08x differs (%d)\n", cur_vb, c->src, bad); }
}
/* task #154 check (MGS2_THIN_CHK=1, x86, C4 on): a streak call in a thinned range converted again with no thinning, and its thinned form
   built from that one: the pieces (runs and pairs) and the clipper's lines of the kept strips (strips 0, keep, 2 keep ... by the ADC
   flags), in the same order, byte for byte; the GerXform and the batch the same but for the counts.  And each streak call's owner by
   the census's walk (fxthin.c fx_who_name): rain.c's and rain_slow.c's calls thinned, no other's */
extern uint16_t xc_lchk_sv[1024][2], xc_lchk_pv[1024]; extern int xc_lchk_nsv, xc_lchk_npv;
static long tl_n, tl_bad, tl_empty, tl_kseg, tl_dseg, tl_own[4][2];   /* tl_own: [rain.c or rain_slow.c / another actor / no block / no
                                                                         owner or no source][thinned] */
/* task #161 check (MGS2_THIN_CHK=1, x86, C4 on): each sprite and quad call's owner by the census's walk, and whether the rain's sprite
   filter left it out: body_sph.c's, charaspread.c's, line_sph.c's and rain_gas_pers.c's left out, no other's */
static long ts_own[4][2];   /* [one of the rain's four / another actor / no block / no owner or no source][left out] */
static long ts_reg[4][3], ts_rvb[4][2];   /* patch_rainspr4: [body_sph, charaspread, line_sph, rain_gas_pers][opening, deck, inside]; first and last vb */
static long ts_nb_n, ts_nb_pool, ts_nb_vb0 = -1, ts_nb_vb1;   /* patch_rainspr3: left out with no census block: all; on a header whose
                                                               third word is not 0; the first and last vblank */
static void thin_chk_s(const XlCall *c)
{
  const int out = glf.ft->nd && fx_drop_has(glf.ft, c->src);
  char nm[40]; const int w = fx_who_name(glf.ram, c->src, nm, sizeof nm);
  const int rs = w == 0 && (!strcmp(nm, "body_sph.c") || !strcmp(nm, "charaspread.c") || !strcmp(nm, "line_sph.c") || !strcmp(nm, "rain_gas_pers.c"));
  ts_own[w == 0 ? !rs : w == -2 ? 2 : 3][out]++;
  if (rs) { const int a = !strcmp(nm, "body_sph.c") ? 0 : !strcmp(nm, "charaspread.c") ? 1 : !strcmp(nm, "line_sph.c") ? 2 : 3;   /* patch_rainspr4 */
    ts_reg[a][cur_vb < 16000 ? 0 : cur_vb < 31500 ? 1 : 2]++; if (cur_vb < ts_rvb[a][0] || !ts_rvb[a][0]) ts_rvb[a][0] = cur_vb;
    if (cur_vb > ts_rvb[a][1]) ts_rvb[a][1] = cur_vb; }
  static int shown;   /* (the first calls left out that the census gives to another actor, or drawn that it gives to the four) */
  if (w == 0 && rs != out && shown < 12) { shown++; fprintf(stderr, "thin check sprites: vb %ld entry %06x src %08x owner %s left out %d\n", cur_vb, c->entry, c->src, nm, out); }
  static int shown2;   /* (the first left out with no census block: the nearest header below src that a block's end check accepts) */
  if (w == -2 && out) { uint32_t h = (c->src & ~3u) - 0x10, lo = c->src > 0x180000u ? c->src - 0x80000u : 0x100000u;
    for (; h >= lo; h -= 4) { const uint32_t nx = *(const uint32_t *)(glf.ram + h + 4);
      if (nx > c->src && nx - h <= 0x100000u && nx < (32u << 20) && !(nx & 3) && *(const uint32_t *)(glf.ram + nx) == h) break; }
    ts_nb_n++; if (h >= lo && *(const uint32_t *)(glf.ram + h + 8)) ts_nb_pool++;   /* patch_rainspr3 */
    if (ts_nb_vb0 < 0 || cur_vb < ts_nb_vb0) ts_nb_vb0 = cur_vb; if (cur_vb > ts_nb_vb1) ts_nb_vb1 = cur_vb;
    if (shown2 >= 12) return; shown2++;
    fprintf(stderr, "thin check sprites: no census block, left out: vb %ld src %08x", cur_vb, c->src);
    if (h >= lo) fprintf(stderr, " header %08x: %08x %08x %08x %08x", h, *(const uint32_t *)(glf.ram + h), *(const uint32_t *)(glf.ram + h + 4),
                         *(const uint32_t *)(glf.ram + h + 8), *(const uint32_t *)(glf.ram + h + 12));
    fputc('\n', stderr); }
}
static void thin_chk_l(const XlCall *c)
{
  int keep = 1;
  for (int k = 0; k < glf.h.nthin; k++) if (glf.h.thin[k].line && c->src - glf.h.thin[k].lo < glf.h.thin[k].hi - glf.h.thin[k].lo) { keep = glf.h.thin[k].keep; break; }
  { char nm[40]; const int w = fx_who_name(glf.ram, c->src, nm, sizeof nm);
    tl_own[w == 0 ? (strcmp(nm, "rain.c") && strcmp(nm, "rain_slow.c") ? 1 : 0) : w == -2 ? 2 : 3][keep > 1]++;
    static int shown;   /* (the first calls whose block the census does not find) */
    if (w == -2 && shown < 12) { shown++; fprintf(stderr, "thin check streaks: no block: vb %ld src %08x thinned %d; words before:", cur_vb, c->src, keep > 1);
      for (int q = 8; q >= 1; q--) fprintf(stderr, " %08x", *(const uint32_t *)(glf.ram + ((c->src & ~3u) - 4u * (uint32_t)q)));
      fputc('\n', stderr); } }
  if (keep < 2) return;
  static uint8_t b0[GL_XCAP], b1[GL_XCAP], be[GL_XCAP]; static uint16_t p0[1024][2], q0[1024], trun[1024][2];
  static VcVertF v[1024], tsv[2048], tpv[2048]; static uint16_t fl[1024]; static float cc[1024][4]; static uint8_t cls[1024]; static int st[1024];
  xl_vm_sync();
  XcHost h0 = glf.h; h0.nthin = 0;
  GerBatch g0, g1; uint32_t s0 = 0, s1 = 0; memset(&g0, 0, sizeof g0); memset(&g1, 0, sizeof g1);
  const int r0 = xc_call(c, &h0, &g0, b0, GL_XCAP, &s0), np0 = xc_lchk_nsv, nq0 = xc_lchk_npv;
  memcpy(p0, xc_lchk_sv, (size_t)np0 * sizeof p0[0]); memcpy(q0, xc_lchk_pv, (size_t)nq0 * sizeof q0[0]);
  const int r1 = xc_call(c, &glf.h, &g1, b1, GL_XCAP, &s1);
  tl_n++;
  const int tme = (int)(((c->gr[0x1a] & 1) ? c->gr[0x00] : c->gr[0x1b]) >> 4 & 1);   /* (xconv.c xc_gs's TME) */
  VcXform x; const int n = vc_line(c->vm, c->top, &x, v, fl, cc, cls, 1024, tme);
  int bad = 0;
  if (n < 0 || r0 != 1) { if (r1 != r0) bad = 1; goto done; }
  st[0] = 0; for (int i = 1; i < n; i++) st[i] = st[i - 1] + !cls[i];   /* each vertex's strip, by the PS2's classes */
  const GerXform *X0 = (const GerXform *)b0; const VcVertF *sv0 = (const VcVertF *)(b0 + sizeof *X0), *pv0 = sv0 + X0->nv;
  const uint16_t (*run0)[2] = (const uint16_t (*)[2])(pv0 + X0->nfv);
  uint32_t nsv = 0, nst = 0, npv = 0, o0 = 0, ri = 0;
  for (int j = 0; j < np0; j++) {   /* the full call's pieces (their runs as the blob lists them), the kept strips' in order */
    const int f = p0[j][0], k = p0[j][1];
    if (k > 2) { if (ri >= X0->nst || run0[ri][0] != o0 || run0[ri][1] != k) bad |= 2; ri++; }
    if (st[f] % keep == 0) {
      memcpy(&tsv[nsv], &sv0[o0], (size_t)k * sizeof *tsv);
      if (k > 2) { trun[nst][0] = (uint16_t)nsv; trun[nst][1] = (uint16_t)k; nst++; }
      nsv += (uint32_t)k; tl_kseg += k - 1;
    } else tl_dseg += k - 1;
    o0 += (uint32_t)k;
  }
  if (o0 != X0->nv || ri != X0->nst || 2u * (uint32_t)nq0 != X0->nfv) bad |= 2;
  for (int j = 0; j < nq0; j++) {   /* the clipper's lines of the kept strips */
    if (st[q0[j]] % keep == 0) { tpv[npv++] = pv0[2 * j]; tpv[npv++] = pv0[2 * j + 1]; tl_kseg++; } else tl_dseg++; }
  if (!nsv && !npv) { if (r1 != 0) bad |= 4; else tl_empty++; goto done; }
  if (r1 != 1) { bad |= 4; goto done; }
  { GerXform Xe = *X0; Xe.nv = nsv; Xe.nst = nst; Xe.nfv = npv;
    uint32_t o = 0;
    memcpy(be, &Xe, sizeof Xe); o += sizeof Xe;
    memcpy(be + o, tsv, nsv * sizeof *tsv); o += nsv * (uint32_t)sizeof *tsv;
    memcpy(be + o, tpv, npv * sizeof *tpv); o += npv * (uint32_t)sizeof *tpv;
    memcpy(be + o, trun, nst * 4); o += nst * 4;
    if (s1 != o || memcmp(b1, be, o)) bad |= 8; }
  { GerBatch q0b = g0, q1b = g1; q0b.first = q1b.first = 0; q0b.count = q1b.count = 0;
    if (memcmp(&q0b, &q1b, sizeof q0b) || g1.count != nsv + npv) bad |= 16; }
done:
  if (bad) { tl_bad++; if (tl_bad <= 10) fprintf(stderr, "thin check streaks: vb %ld call src %08x differs (%d)\n", cur_vb, c->src, bad); }
}
static void on_call(void *ctx, const XlCall *c)
{
  (void)ctx; ncall++; ec_add(c);
  if (cur_vb >= tp_vb && cur_vb <= tp_to) {   /* (dev, task #114: the call's PRIM, its context's TEX0, ALPHA, TEST, and a lane's streams) */
    uint64_t pr = c->gr[0x00]; int cx = (int)(pr >> 9 & 1);
    fprintf(stderr, "call entry %06x top %03x cont %d vb %ld prim %03x tex0 %016llx alpha %010llx test %05llx src %08x", c->entry, c->top, c->cont, cur_vb,
            (unsigned)(pr & 0x7ff), (unsigned long long)c->gr[0x06 + cx], (unsigned long long)c->gr[0x42 + cx], (unsigned long long)c->gr[0x47 + cx], c->src);
    if (c->rec) { fprintf(stderr, " lane %d", c->rec->lane);
      for (int k = 0; k < 6; k++) if (c->rec->p[k]) { const uint8_t *q = (const uint8_t *)c->rec->p[k];
        uint32_t ga = tp_mem && q >= tp_mem && q < tp_mem + GSZ ? (uint32_t)(q - tp_mem) : 0xffffffffu;
        fprintf(stderr, " s%d %08x n %u k %02x", k, ga, c->rec->num[k], c->rec->kind[k]); } }
    fputc('\n', stderr);
    if (c->entry == XL_FX || c->entry == XL_FX2) fx_dump(c);
  }
  if (sync_on) xl_vm_sync();
  if (dig) { XlCall l = *c; xl_vm_logical(lvm); l.vm = lvm; xdg_call(&l); }   /* the VU memory as it reads (spans: pending quadwords decoded) */
  if (tx_on) tex_call(c);
  if (vc_on) vc_calls[gl_conv_entry(c->entry) ? 0 : xl_rule(c->entry) ? 1 : 2]++;
  if (gb_now) gl_call(&glf, c);
  { static int tc = -1; if (tc < 0) tc = getenv("MGS2_THIN_CHK") && atoi(getenv("MGS2_THIN_CHK"));   /* task #114 check */
    if (tc && gb_now && glf.ft && (c->entry == XL_FX || c->entry == XL_FX2) && !c->cont) thin_chk(c);
    if (tc && gb_now && glf.ft && glf.h.c4 && c->entry == VC_LINE && !c->cont) thin_chk_l(c);   /* task #154 */
    if (tc && gb_now && glf.ft && glf.h.c4 && (c->entry == VC_SPR || c->entry == VC_QUAD) && !c->cont) thin_chk_s(c); }   /* task #161 */
}
static void on_flush(void *ctx) { (void)ctx; if (glf.nheld) { glf.hflush[0]++; gl_flush(&glf); } }   /* sb4: a transfer's end */
/* MGS2_XLIVE_RTLOG (dev): a textured primitive's TEX0 / CLAMP / TEXA / FRAME / ZBUF, texel box and what the map says of its texture's
   area: the whole, the box, and the 64 x 64 squares that hold a drawn area */
static long rt_from = -1, rt_to = -1; static int rt_shown;
static void rt_log(const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n)
{
  const uint64_t *gr = g->gr; int c = d->ctxt, box[4], fw = 0, fx = -1, nd = 0;
  uint64_t t0 = gr[0x06 + c], cl = gr[0x08 + c], texa = gr[0x3b];
  int psm = (int)(t0 >> 20 & 63), tw = 1 << (t0 >> 26 & 15), th = 1 << (t0 >> 30 & 15), hb = prim_box(d, v, n, box);
  uint32_t tbp = (uint32_t)(t0 & 0x3fff), tbw = (uint32_t)(t0 >> 14 & 63);
  gu_area_id(map, psm, tbp, tbw, 0, 0, tw - 1, th - 1, &fw);
  if (hb) gu_area_id(map, psm, tbp, tbw, box[0] < 0 ? 0 : box[0], box[1] < 0 ? 0 : box[1], box[2], box[3], &fx);
  fprintf(stderr, "xlive rt: vb %ld path %d prim %d: TEX0 tbp %04x tbw %u psm %02x %dx%d tcc %d tfx %d, CLAMP %d/%d, TEXA ta0 %02x aem %d ta1 %02x, "
          "FRAME fbp %03x fbw %u psm %02x fbmsk %08x, ZBUF zbp %03x psm %02x zmsk %d, blend %d %d %d %d fix %d, box %d..%d %d..%d (%s); map: whole %d, box %d;",
          cur_vb, g->path, d->prim, (unsigned)tbp, (unsigned)tbw, psm, tw, th, d->tcc, d->tfx, (int)(cl & 3), (int)(cl >> 2 & 3),
          (unsigned)(texa & 0xff), (int)(texa >> 15 & 1), (unsigned)(texa >> 32 & 0xff), (unsigned)(d->fbp / 32), (unsigned)d->fbw, d->fpsm,
          (unsigned)d->fbmsk, (unsigned)(d->zbp / 32), d->zpsm, d->zmsk, d->ba, d->bb, d->bc, d->bd, d->fix, box[0], box[2], box[1], box[3],
          hb ? "valid" : "none", fw, fx);
  for (int y = 0; y < th; y += 64) for (int x = 0; x < tw; x += 64) {
    int f2 = 0; gu_area_id(map, psm, tbp, tbw, x, y, x + 63 < tw ? x + 63 : tw - 1, y + 63 < th ? y + 63 : th - 1, &f2);
    if ((f2 & GU_ID_DRAWN) && nd++ < 24) fprintf(stderr, " drawn %d,%d", x, y);
  }
  fprintf(stderr, " (%d squares drawn)\n", nd); rt_shown++;
}
static void on_prim(void *ctx, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n)
{
  int box[4]; (void)ctx; nprim++; xdg_prim(g, d, v, n);
  if (tx_on && d->tme) { ck.prims++; tex_use(g->gr, d->ctxt, prim_box(d, v, n, box) ? box : 0); }
  if (d->tme && cur_vb >= rt_from && cur_vb <= rt_to && rt_shown < 80) rt_log(g, d, v, n);
  if (gb_now) gl_prim(&glf, g, d, v, n);
}

static void setup(void);
void xlive_chk_dma(const uint8_t *mem, const uint8_t *spr, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int one)
{
  if (from == -2) setup();
  if (!on) return;
  if (lane_opt) { int m = lane_base & (((g_opt & OPT_XL_LANE) ? 1 : 0) | ((g_opt & OPT_XL_LANE2) ? 2 : 0) | ((g_opt & OPT_XL_LANE3) ? 4 : 0) |
                                     ((g_opt & OPT_XL_LANE4) ? 8 : 0));   /* gl11: as psp/xl_psp.c xlive_chk_dma's first lines */
    if (m != g_xl_lane) { xl_lane(m != 0); g_xl_lane = m; }
    { static int eg = 1, mk = 1; int e = (g_opt & OPT_XL_EAGER) != 0, k = (g_opt & OPT_XL_MARK) != 0;   /* sb8: as psp/xl_psp.c's */
      if (e != eg) { xl_ud_eager(e); eg = e; }
      if (k != mk) { xl_ud_mark(k); mk = k; } } }
  { static int um = -1, li = -1;   /* sb9: g_opt2's step 0 bits at each transfer, as psp/xl_psp.c's */
    int u = (g_opt2 & OPT2_TX_UMEMO) ? 1 + um_chk : 0, l = (g_opt2 & OPT2_GU_LDID) ? 1 + li_chk : 0;
    if (tkc && u != um) { tk_umemo(tkc, u); um = u; }
    { static int uf = -1; int f = (g_opt2 & OPT2_TX_FRONT) ? 1 + uf_chk : 0; if (tkc && f != uf) { tk_ufront(tkc, f); uf = f; } }   /* sb10 T1 */
    if (map && l != li) { gu_ld_ident(map, l); li = l; } }
  gx_seen = 1;                            /* MGS2_XLIVE_GX: frames begin from here on (the PSP's translator starts at its first transfer) */
  if (skip_now) { static long last = -1; sk_tr++; if (cur_vb != last) { sk_vb++; last = cur_vb; } return; }
  M.mem = mem; M.mem_size = GSZ; M.spr = spr; tp_mem = mem;
  glf.ram = mem;   /* sb6: the thinning's scan (gelive.c fx_thin_scan) reads the game's RAM through glf.ram, which a frame's start copies
                      from M.mem; with the translator started late (MGS2_XLIVE <from> > 0) the first frame begins before this first transfer,
                      M.mem still 0: the digest runs from vb 30,800 stopped there at the rain's first +25f0 call.  The same pointer otherwise */
  if (one_on && one) { one_tr++; g_xl_one = 1; clock_gettime(CLOCK_MONOTONIC, &one_t0); return; }   /* the exact walk feeds it */
  struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
  xlv_dma(&M, ch, chcr, madr, qwc, tadr);
  clock_gettime(CLOCK_MONOTONIC, &b); t_walk += (double)(b.tv_sec - a.tv_sec) * 1e3 + (double)(b.tv_nsec - a.tv_nsec) / 1e6;
}

void xlive_one_end(void)   /* runtime/ee_io.h: the end of a VIF1 transfer the exact walk fed to the translator */
{
  struct timespec b;
  xl_flush(); g_xl_one = 0;   /* the game may change this RAM before the next transfer */
  clock_gettime(CLOCK_MONOTONIC, &b); t_walk += (double)(b.tv_sec - one_t0.tv_sec) * 1e3 + (double)(b.tv_nsec - one_t0.tv_nsec) / 1e6;
}

/* sb9: Stage C step 0's counters (psp/xl_psp.c's line, with the check modes' comparisons) */
static void step0_line(const char *pre)
{
  const TkStats *k = tk_stats(tkc); const GuStats *u = gu_stats(map);
  fprintf(stderr, "%s texture look-ups by upload %ld, answered %ld; not served: TEX0 / CLAMP %ld, TEXA %ld, pages %ld %ld %ld %ld %ld %ld, no slot "
          "%ld, entry gone %ld; slots filled %ld, geometry slots made %ld; CLUT loads by identity %ld (joined %ld; no block list %ld, records "
          "not final %ld); check modes: textures %ld compared, %ld differing; CLUT loads %ld compared, %ld differing\n", pre, k->um_asks,
          k->um_hits, k->um_geo, k->um_texa, k->um_sig[1], k->um_sig[2], k->um_sig[3], k->um_sig[4], k->um_sig[5], k->um_sig[6], k->um_slot,
          k->um_gone, k->um_fill, k->um_gnew, u->ld_ident, u->ld_ident_put, u->ld_ident_nolist, u->ld_ident_nosig, k->um_chk, k->um_bad,
          u->ld_ident_chk, u->ld_ident_bad);
  fprintf(stderr, "%s the front answered %ld look-ups, entries put %ld; check mode: %ld compared, %ld differing; drawn areas dropped %ld\n",
          strncmp(pre, "picture", 7) ? "xlive tk: step 1:" : "picture: step 1:", k->uf_hits, k->uf_fill, k->uf_chk, k->uf_bad, u->dt_fail);   /* sb10 T1 */
}
static void totals(void)
{
  const XlStats *S = xl_stats(); const GfGs *F = xl_front(); const GuStats *U = gu_stats(map);
  if (g_xl_lane) fprintf(stderr, "sb lane: groups %ld (lane 1) %ld (lane 2); %ld df0 calls from the game's streams, checked %ld, differing %ld; "
                         "%ld P06 calls from the game's streams, %ld from the VU memory, checked %ld, differing %ld\n", xl_lane_groups[1],
                         xl_lane_groups[2], xc_rec_calls, xc_rec_n, xc_rec_diff, xc_rec6_calls, xc_rec6_vm, xc_rec6_n, xc_rec6_diff);
  if (g_xl_lane & 8) fprintf(stderr, "sb lane 4: slots %ld; %ld P07 calls from the game's streams, %ld from the VU memory, checked %ld, differing %ld; "
                             "STCYCL 7 / 1 tags not taken: room %ld, tag 0 %ld, tags 1-5 %ld, tags 6-19 %ld, data %ld, declined %ld\n", xl_lane_groups[4],
                             xc_rec7_calls, xc_rec7_vm, xc_rec7_n, xc_rec7_diff, lane7_miss[0], lane7_miss[1], lane7_miss[2], lane7_miss[3],
                             lane7_miss[4], lane7_miss[5]);
  if (g_xl_lane) fprintf(stderr, "sb lane 3: prims %ld; %ld 25f0 / 2870 calls from the game's streams, %ld from the VU memory, checked %ld, differing %ld; "
                         "prim tags not taken: room %ld, tag 0 %ld, tag 1 %ld, tags 2-5 %ld, data %ld, declined %ld; prim tags left to the generic walk: "
                         "lane 3 off %ld, not taken %ld, data left %ld, no TTE %ld, not the one walk %ld\n", xl_lane_groups[3], xc_rec3_calls, xc_rec3_vm,
                         xc_rec3_n, xc_rec3_diff, lane3_miss[0], lane3_miss[1], lane3_miss[2], lane3_miss[3], lane3_miss[4], lane3_miss[5], lane3_gen[0],
                         lane3_gen[1], lane3_gen[2], lane3_gen[3], lane3_gen[4]);
  if (gl_need) fprintf(stderr, "sb need: %ld converter calls, %ld descriptors decoded for them; checked %ld, differing %ld\n", xl_stats()->need_calls,
                       xl_stats()->need_runs, gl_need_n, gl_need_diff);
  fprintf(stderr, "xlive: vb %ld..%ld: %ld transfers, %ld tags, %ld quadwords (%ld chains cut at 1,000,000 tags); translator: %ld VIF1 words, "
          "%ld calls (%ld MSCNT, %ld without an entry), %ld callbacks, A+D packets %ld (%ld without an end), GIF tags path 1/2/3 %ld/%ld/%ld, "
          "IMAGE quadwords %ld, unknown VIF commands %ld, filling writes %ld, MSCNT after MSCNT %ld (the first after entry %06x)\n", from, to - 1,
          M.transfers, M.tags, M.qw, M.guard_stops, S->words, S->calls, S->calls_cont, S->calls_noentry, ncall, S->ad_kicks, S->ad_bad,
          S->gif_tags[1], S->gif_tags[2], S->gif_tags[3], S->gif_image_qw, S->unk, S->fills, S->cont2, (unsigned)S->cont2_after);
  fprintf(stderr, "xlive: front end primitives by PRIM %ld %ld %ld %ld %ld %ld %ld (%ld to the callback), zero-area %ld, PRIM 7 %ld, uncertain %ld\n",
          F->prims[0], F->prims[1], F->prims[2], F->prims[3], F->prims[4], F->prims[5], F->prims[6], nprim, F->zero_area, F->prim7, F->uncertain);
  fprintf(stderr, "xlive: texture sources: %ld transfers (%ld other layout, %ld not modelled, %ld in pieces, %ld past 2048, %ld local, %ld local "
          "not known), %ld IMAGE without a transfer, %ld CLUT loads (%ld wide), %ld block entries dropped; areas drawn %ld (%ld draws kept "
          "them); live %d transfers, %d loads\n", U->xfers, U->xfer_other, U->xfer_unmod, U->xfer_split, U->xfer_wrap, U->xfer_local,
          U->local_unknown, U->image_orphan, U->loads, U->load_wide, U->dropped, U->drawn, U->drawn_kept, U->live_xfers, U->live_loads);
  if (M.fast) fprintf(stderr, "xlive lazy: %ld UNPACKs (%ld vectors) left to decode later; quadwords decoded later: %ld when read, %ld before an "
                      "UNPACK wrote over them, %ld at %ld flushes (%ld for a full table of UNPACKs, %ld of ROW / COL copies)\n", S->lz_cmds,
                      S->lz_vec, S->dec_read, S->dec_write, S->dec_flush, S->flushes, S->full_ud, S->full_rc);
  fprintf(stderr, "xlive time: %.3f ms a vblank in the walk, the translator and the callbacks (%s%s%s)\n", t_walk / (to - from > 0 ? to - from : 1),
          M.fast ? "spans" : "words", M.seen ? ", packet hash" : "", dig_on ? ", digests" : "");
  if (skip_on) fprintf(stderr, "xlive skip: shown frames only: %ld vblanks after reduced calls only, %ld transfers not handed over\n", sk_vb, sk_tr);
  if (one_on) fprintf(stderr, "xlive one: %ld VIF1 transfers fed by the exact walk\n", one_tr);
  if (gb_on) { char w[64]; snprintf(w, sizeof w, "xlive gb: %ld frames:", glf.all.frames); gb_line(&glf.all, glf.all_dbytes, glf.all_tbytes, w);
    fprintf(stderr, "; memo answers that differed %ld\n", glf.memo_bad); gb_line(&glf.max, glf.max.dbytes, glf.max.tbytes, "xlive gb: the largest of each in one frame:"); fputc('\n', stderr);
    if (gx_on) { const TkStats *k = tk_stats(tkc);
      fprintf(stderr, "picture: %ld frames made (the first began at vb %ld); lists: %ld that did not fit, %ld batches left out, the largest %ld "
              "words, the largest aux %ld bytes, the largest list and aux together %ld bytes (of %u); digests %ld (vb %ld..%ld), all %016llx\n",
              gx_count, gx_first, gx_fail, gx_drop, gx_wmax, gx_amax, gx_bmax, gx_listn, gx_n, gx_from, gx_to, (unsigned long long)gx_all);
      fprintf(stderr, "picture: content digests %ld, all %016llx\n", gx_n, (unsigned long long)gx_call);
      if (gx_sem_on) fprintf(stderr, "picture: drawn digests (MGS2_GX_SEM) %ld, all %016llx; addresses outside the frame memory %ld\n", gx_sem_n,
                             (unsigned long long)gx_sem_all, gx_sem_bad);
      fprintf(stderr, "picture: transform sharing census (4t) over %ld frames: step 4 effect batches %ld %ld %ld, equal to the last one's %ld %ld %ld; "
              "all effect batches %ld, equal %ld; transform batches %ld, camera equal %ld; saved A %lld KB, B %lld KB, C %lld KB; the largest "
              "frame's data now %lld KB (vb %ld), after A %lld KB (vb %ld), B %lld KB (vb %ld), C %lld KB (vb %ld)\n", cx_frames, cx_n[0], cx_n[1],
              cx_n[2], cx_same[0], cx_same[1], cx_same[2], cx_fx_n, cx_fx_same, cx_cam_n, cx_cam_same, cx_s[0] >> 10, cx_s[1] >> 10, cx_s[2] >> 10,
              cx_max[0] >> 10, cx_maxvb[0], cx_max[1] >> 10, cx_maxvb[1], cx_max[2] >> 10, cx_maxvb[2], cx_max[3] >> 10, cx_maxvb[3]);
      fprintf(stderr, "picture: textures: %ld uses, textures %ld hits / %ld conversions, palettes %ld / %ld; render targets %ld, not known %ld, "
              "refused %ld (no room in the pool %ld), texels or entries the map could not give %ld; converted %lld KB, kept %ld (%lld KB, peak "
              "%lld KB), evicted %ld, the heaviest frame %lld KB; pool %u KB, budget %u KB\n", k->uses, k->hits, k->misses, k->pal_hits,
              k->pal_misses, k->rt, k->lost, k->unsup, k->nomem, k->unknown, (long long)(k->conv_bytes >> 10), k->live,
              (long long)(k->live_bytes >> 10), (long long)(k->peak_bytes >> 10), k->evicted, (long long)(k->peak_frame_bytes >> 10),
              gx_pool_n >> 10, gx_pool_n / 8 * 7 >> 10);
      fprintf(stderr, "picture: texture conversions: %ld a block at a time, %ld handed back to the texel path, %ld differing from it; GS blocks "
              "read %ld, %ld of them whole from one upload\n", k->blk, k->blk_back, k->blk_diff, gu_stats(map)->blocks, gu_stats(map)->blocks_one);
      step0_line("picture: step 0:"); }
    const XcStats *x = xc_stats(); fprintf(stderr, "xlive gb: converters: calls %ld / %ld / %ld, batches %ld / %ld / %ld, empty %ld / %ld / %ld, bad %ld / %ld / %ld; "
              "effect triangles by class %ld %ld %ld %ld %ld (2870 culled %ld); polygons %ld (%ld vertices), empty %ld, culled %ld, no room %ld; "
              "df0 calls left out off the scissor %ld (%ld vertices)\n",
              x->calls[0], x->calls[1], x->calls[2], x->batches[0], x->batches[1], x->batches[2], x->empty[0], x->empty[1], x->empty[2],
              x->bad[0], x->bad[1], x->bad[2], x->tris[0], x->tris[1], x->tris[2], x->tris[3], x->tris[4], x->tris[5], x->polys, x->poly_v,
              x->poly_empty, x->poly_culled, x->poly_full, x->cut[0], x->cut_v[0]);
    fprintf(stderr, "xlive gb: effect vertices %ld, %ld of them through the clamps with the fast path on\n", x->clip[0], x->clip[1]);
    if (glf.ft) { const FxThin *t = glf.ft;   /* task #114 */
      fprintf(stderr, "xlive gb: thinned effects (sheets 1 in %d, splash 1 in %d): scans %ld, actors walked %ld, names read %ld, cache emptied "
              "%ld; blocks found: sheets %ld, splash %ld; bad %ld, over %ld; calls thinned %ld, their strips %ld, left out %ld (%ld triangles)\n",
              t->keep[0], t->keep[1], t->scans, t->actors, t->named, t->flushes, t->found[0], t->found[1], t->bad, t->over, x->thin_calls,
              x->thin_strips, x->thin_drop, x->thin_tris);
      if (tc_n) fprintf(stderr, "xlive gb: thin check: %ld thinned calls converted again without thinning: differing %ld; kept strips' triangles %ld "
                "(the same classes), left-out strips' %ld (class 0), kept polygons %ld (the same); calls with nothing left to draw %ld\n", tc_n, tc_bad,
                tc_keep_tris, tc_drop_tris, tc_fans, tc_empty);
      if (t->keep[2] != 1 || t->keep_calls > 1)   /* task #154 (0: none, calls 1 in N: task #158) */
        fprintf(stderr, "xlive gb: thinned streaks (1 in %d, calls 1 in %d): actors %ld; their tables' blocks %ld, entries 0 %ld, blocks not right %ld; "
                "ranges %ld; calls thinned %ld, their strips %ld, left out %ld (%ld segments); calls left out whole %ld\n", t->keep[2], t->keep_calls, t->found[2], t->tab_blocks,
                t->tab_empty, t->tab_bad, t->tab_ranges, x->thin_lcalls, x->thin_lstrips, x->thin_ldrop, x->thin_lseg, t->line_none);
      if (tl_own[0][0] + tl_own[0][1] + tl_own[1][0] + tl_own[1][1] + tl_own[2][0] + tl_own[2][1] + tl_own[3][0] + tl_own[3][1])
        fprintf(stderr, "xlive gb: thin check streaks: %ld thinned calls converted again without thinning: differing %ld, nothing left to "
                "draw %ld; segments kept %ld, left out %ld; streak calls by the census's owner, thinned / not: rain.c or rain_slow.c %ld / "
                "%ld, another actor %ld / %ld, no block %ld / %ld, no owner %ld / %ld\n", tl_n, tl_bad, tl_empty, tl_kseg, tl_dseg,
                tl_own[0][1], tl_own[0][0], tl_own[1][1], tl_own[1][0], tl_own[2][1], tl_own[2][0], tl_own[3][1], tl_own[3][0]);
      if (!t->keep[3])   /* task #161 */
        fprintf(stderr, "xlive gb: the rain's sprites left out: actors %ld; blocks %ld (not right %ld, over %ld); ranges %ld; calls left out whole %ld\n",
                t->found[3], t->spr_blocks, t->spr_bad, t->spr_over, t->spr_ranges, t->spr_none);
      if (ts_own[0][0] + ts_own[0][1] + ts_own[1][0] + ts_own[1][1] + ts_own[2][0] + ts_own[2][1] + ts_own[3][0] + ts_own[3][1])
        fprintf(stderr, "xlive gb: thin check sprites: sprite and quad calls by the census's owner, left out / drawn: the rain's four %ld / %ld, "
                "another actor %ld / %ld, no block %ld / %ld, no owner %ld / %ld\n", ts_own[0][1], ts_own[0][0], ts_own[1][1], ts_own[1][0],
                ts_own[2][1], ts_own[2][0], ts_own[3][1], ts_own[3][0]);
      if (ts_nb_n) fprintf(stderr, "xlive gb: thin check sprites: left out with no census block %ld: on a header whose third word is not 0 %ld; "
                           "vb %ld..%ld\n", ts_nb_n, ts_nb_pool, ts_nb_vb0, ts_nb_vb1);
      { static const char *an[4] = { "body_sph", "charaspread", "line_sph", "rain_gas_pers" };
        for (int a = 0; a < 4; a++) if (ts_reg[a][0] + ts_reg[a][1] + ts_reg[a][2])
          fprintf(stderr, "xlive gb: thin check sprites: %s calls: the opening (vb < 16,000) %ld, the deck (16,000-31,499) %ld, inside (31,500 on) "
                  "%ld; vb %ld..%ld\n", an[a], ts_reg[a][0], ts_reg[a][1], ts_reg[a][2], ts_rvb[a][0], ts_rvb[a][1]); } }
    fprintf(stderr, "xlive gb: step c: %ld calls held (%ld of them P07); flushes at a transfer's end %ld, for a call's room %ld, records full "
            "%ld, P07 copies full %ld, at a frame's end %ld; faults: held calls not placed %ld, passes after their call ran %ld%s%s\n",
            glf.held_n, glf.held7_n, glf.hflush[0], glf.hflush[1], glf.hflush[2], glf.hflush_log, glf.hflush[3], glf.held_bad, glf.held_late,
            xc_held_force ? "; every held call through the rebuilt VU memory" : "", xc_held_poison ? "; the records' images and the scratch memory poisoned" : "");
    fprintf(stderr, "xlive gb: P07 held (sb10 T2): %ld calls; slots copied at the hold %ld, placed at the run %ld; vertices rebuilt %ld; "
            "placed memory checked %ld, differing %ld%s\n", xc_c7_held, xc_c7_snap, xc_c7_put, xc_c7_rb, xc_c7_n, xc_c7_diff,
            xc_c7_chk ? " (MGS2_C7_CHK)" : "");
    c1m_report(stderr);   /* sb11 (MGS2_C1_MODEL=1) */
    c2m_report(stderr);   /* sb12 (MGS2_C2_MODEL=1) */
    c3m_report(stderr);   /* sb13 (MGS2_C3_MODEL=1) */
    fprintf(stderr, "xlive gb: P06: calls %ld (+0 %ld, +358 %ld, +738 %ld, +ab0 %ld), batches %ld, empty %ld, bad %ld; vertices %ld, exact "
              "form %ld; passes 2 and 3 drawn %ld / %ld (pass 2 with its own vertices %ld), not drawn %ld (a UV2 z not 1.0) / %ld (from "
              "T+0x104); cull bit %ld; light ramps %ld\n", x->calls[3], x->lit_kind[0], x->lit_kind[1], x->lit_kind[2], x->lit_kind[3],
              x->batches[3], x->empty[3], x->bad[3], x->lit_v, x->lit_exact, x->lit_pass[2], x->lit_pass[3], x->lit_own2, x->lit_nopass[2],
              x->lit_nopass[3], x->lit_cull, x->lit_ramp);
    fprintf(stderr, "xlive gb: P07: calls %ld, batches %ld, empty %ld, bad %ld (paths not seen %ld); vertices %ld, calls in the exact form %ld; "
              "passes 2 and 3 drawn %ld / %ld (pass 2 with its own vertices %ld), not drawn %ld (a UV2 z not 1.0) / %ld (flags 0x100 or 0x400); "
              "fast form checked against the exact form (XC_P7CHK) %ld calls, differing %ld\n", x->calls[4], x->batches[4], x->empty[4], x->bad[4],
              x->face_unseen, x->face_v, x->face_exact, x->face_pass[2], x->face_pass[3], x->face_own2, x->face_nopass[2], x->face_nopass[3],
              x->face_chk, x->face_chkdiff);
    if (x->c1_calls || x->c1_fall[0] || x->c1_fall[1] || x->c1_fall[2] || x->c1_fall[3] || gx_c1p)   /* sb11 C1 (g_opt2 OPT2_C1) */
      fprintf(stderr, "xlive gb: P07 C1 (the GE's skinning and lighting): calls %ld, vertices %ld, by weights a vertex 1..8 %ld %ld %ld %ld %ld "
              "%ld %ld %ld, data %ld KB; to today's path: %ld (its fast form's conditions), %ld (the clamp, a light, the ambient), %ld (an "
              "offset, a slot's weights, a UV), %ld (a bone); in the GE lists: passes %ld, bone matrices sent %ld, their own words %ld; the depth range by the bound alone %ld calls, %ld vertices\n",
              x->c1_calls, x->c1_v, x->c1_nw[1], x->c1_nw[2], x->c1_nw[3], x->c1_nw[4], x->c1_nw[5], x->c1_nw[6], x->c1_nw[7], x->c1_nw[8],
              (long)(x->c1_bytes >> 10), x->c1_fall[0], x->c1_fall[1], x->c1_fall[2], x->c1_fall[3], gx_c1p, gx_c1b, gx_c1w, x->c1_zin, x->c1_zin_v);
    if (x->c2_calls || x->c2_fall[0] || x->c2_fall[1] || x->c2_fall[2] || x->c2_fall[3] || gx_c2p)   /* sb12 C2 (g_opt2 OPT2_C2) */
      fprintf(stderr, "xlive gb: P06 C2 (the GE's skinning and lighting): calls %ld (skinned %ld), vertices %ld, data %ld KB; to today's path: %ld (its "
              "fast form's conditions), %ld (the clamp, a light's colour, the ambient), %ld (a weight, a tint, a UV), %ld (room); in the GE lists: "
              "passes %ld, bone matrices sent %ld, their own words %ld; the depth range by the bound alone %ld calls, %ld vertices\n",
              x->c2_calls, x->c2_skin, x->c2_v, (long)(x->c2_bytes >> 10), x->c2_fall[0], x->c2_fall[1], x->c2_fall[2], x->c2_fall[3], gx_c2p,
              gx_c2b, gx_c2w, x->c2_zin, x->c2_zin_v);
    if (x->c3v_calls || x->c3z_in || x->c3_chk)   /* sb14 step 3 (g_opt2 OPT2_C3V, OPT2_C3Z, OPT2_C3W) */
      fprintf(stderr, "xlive gb: df0 C3 (two passes, the depth range by the box): lane calls in two passes %ld (straight into the frame %ld), cut "
              "before the second pass %ld calls, %ld vertices; the depth range by the box alone %ld calls, %ld vertices; checked %ld, different %ld\n",
              x->c3v_calls, x->c3w_calls, x->c3v_cut, x->c3v_cut_v, x->c3z_in, x->c3z_in_v, x->c3_chk, x->c3_chk_bad);
    if (x->c4_calls[0] || x->c4_calls[1] || x->c4_calls[2])   /* sb15 step 4 (g_opt2 OPT2_C4) */
      fprintf(stderr, "xlive gb: effects C4: calls sprites %ld, quads %ld, streaks %ld; bad %ld %ld %ld; batches %ld %ld %ld; nothing drawn %ld %ld %ld; "
              "sprites %ld (dropped %ld, corners moved %ld, drawn %ld); quads %ld (dropped %ld, clipped %ld, whole %ld; polygons %ld, their "
              "vertices %ld, empty %ld); streak segments %ld (dropped %ld, to the clipper %ld, in the strip %ld; lines %ld, the clipper's drops "
              "%ld); no room %ld; GE list: sprites %ld, line strips %ld, lines %ld\n",
              x->c4_calls[0], x->c4_calls[1], x->c4_calls[2], x->c4_bad[0], x->c4_bad[1], x->c4_bad[2], x->c4_batches[0], x->c4_batches[1],
              x->c4_batches[2], x->c4_empty[0], x->c4_empty[1], x->c4_empty[2], x->c4_n[0], x->c4_drop[0], x->c4_clip[0], x->c4_drawn[0],
              x->c4_n[1], x->c4_drop[1], x->c4_clip[1], x->c4_drawn[1], x->c4_poly, x->c4_poly_v, x->c4_poly_empty, x->c4_n[2], x->c4_drop[2],
              x->c4_clip[2], x->c4_drawn[2], x->c4_lines, x->c4_line_drop, x->c4_full, gx_c4s, gx_c4r, gx_c4l);
  }
}

/* the options, at the first transfer or vblank */
static void setup(void)
{
  const char *e = getenv("MGS2_XLIVE"); char fn[256] = ""; from = -1;
  if (e && sscanf(e, "%ld:%ld:%255s", &from, &to, fn) < 2) from = -1;
  if (from >= 0) {
    M.seen = seen;
    if (!(map = gu_new(ram, 0))) { fprintf(stderr, "xlive: no memory for the texture sources\n"); exit(1); }
    xl_on_call(on_call, 0); xl_on_prim(on_prim, 0); xl_uploads(map); xl_on_pass(on_pass, 0); xl_on_flush(on_flush, 0);
    M.fast = getenv("MGS2_XLIVE_FAST") && atoi(getenv("MGS2_XLIVE_FAST"));
    sync_on = getenv("MGS2_XLIVE_SYNC") && atoi(getenv("MGS2_XLIVE_SYNC"));
    if (getenv("MGS2_XLIVE_NOSEEN") && atoi(getenv("MGS2_XLIVE_NOSEEN"))) M.seen = 0;
    if (getenv("MGS2_XLIVE_TEX") && atoi(getenv("MGS2_XLIVE_TEX"))) {
      tx_on = 1; tx_detail = atoi(getenv("MGS2_XLIVE_TEX")) >= 2; up = calloc(UP_N, sizeof *up); srcs = calloc(SRC_N, sizeof *srcs);
      if (!up || !srcs) { fprintf(stderr, "xlive: no memory for the texture check\n"); exit(1); }
      gu_on_data(map, on_data, 0); gu_keep_sent(map, 1);
      if (getenv("MGS2_XLIVE_TK") && atoi(getenv("MGS2_XLIVE_TK"))) {   /* the PSP texture path on top of the map */
        long bud = atoi(getenv("MGS2_XLIVE_TK")); gu_keys(map, 1); if (from == 0) gu_zero_empty(map, 1);
        if (!(tkc = tk_new(map, bud > 1 ? (uint32_t)(bud * 1024) : 0))) { fprintf(stderr, "xlive: no memory for the texture cache\n"); exit(1); }
        tk_on = 1;
      }
    }
    const char *gxe = getenv("MGS2_XLIVE_GX");
    if ((getenv("MGS2_XLIVE_GB") && atoi(getenv("MGS2_XLIVE_GB"))) || gxe) {
      int m = getenv("MGS2_XLIVE_GB") && atoi(getenv("MGS2_XLIVE_GB")) ? atoi(getenv("MGS2_XLIVE_GB")) : 1;
      double zk = gxe ? 0 : getenv("MGS2_XLIVE_GB_ZK") ? atof(getenv("MGS2_XLIVE_GB_ZK")) : 0.2;
      if (gxe) {   /* the PSP harness's sizes and order (psp/xl_psp.c gl_setup) */
        long kb = getenv("MGS2_XLIVE_GX_TEXKB") ? atol(getenv("MGS2_XLIVE_GX_TEXKB")) : GL_PSP_TEXKB;
        if (sscanf(gxe, "%ld:%ld", &gx_from, &gx_to) != 2 || from != 0 || tkc || kb < 256) {
          fprintf(stderr, "xlive: MGS2_XLIVE_GX=<a>:<b> needs MGS2_XLIVE=0:<to>, no MGS2_XLIVE_TK and a pool of 256 KB or more\n"); exit(1); }
        gx_pool_n = (uint32_t)kb << 10;
        if (getenv("MGS2_XLIVE_GX_LIST")) gx_listn = (uint32_t)atol(getenv("MGS2_XLIVE_GX_LIST")) << 10;   /* measurement only: the lists then
                                                                                                             differ from the PSP's */
        if (posix_memalign((void **)&gx_pool, 64, gx_pool_n) || posix_memalign((void **)&gx_arena, 64, GL_PSP_ARENA)
            || posix_memalign((void **)&gx_lb, 64, gx_listn)) {
          fprintf(stderr, "xlive: no memory for the PSP frames\n"); exit(1); }
        memset(gx_pool, 0, gx_pool_n); memset(gx_arena, 0, GL_PSP_ARENA); memset(gx_lb, 0, gx_listn);
        gu_keys(map, 1); gu_zero_empty(map, 1);
        if (!(tkc = tk_new(map, gx_pool_n / 8 * 7)) || tk_pool(tkc, gx_pool, gx_pool_n)) { fprintf(stderr, "xlive: no texture cache\n"); exit(1); }
        if (gl_init(&glf, tkc, map, 0, GL_PSP_MAXTEX) || gb_arena(&glf.o, gx_arena, GL_PSP_ARENA, GL_PSP_NB, GL_PSP_NV)) {
          fprintf(stderr, "xlive: no memory for the live frame\n"); exit(1); }
        gx_init(&gxl, (uint32_t *)gx_lb, gx_listn / 4, gx_lb, gx_listn); gxl.ret = 1;   /* one block, as the PSP's */
        if (gx_listn != GL_PSP_LIST) fprintf(stderr, "xlive mode: a list block of %u KB (MGS2_XLIVE_GX_LIST: not the PSP's)\n", gx_listn >> 10);
        gx_sem_on = getenv("MGS2_GX_SEM") && atoi(getenv("MGS2_GX_SEM"));   /* sb15 */
        gxh.addr = gx_addr; gxh.ctx = 0; gxh.fb[0] = 0x000000; gxh.fb[1] = 0x088000; gxh.zb = 0x110000; gx_on = 1;
        if (getenv("MGS2_GL_DZ")) {   /* gl29: the 0x42 decals' depth arms by frame, with the PSP's parser (ge/gelist.c gx_dz_parse) */
          if (gx_dz_parse(getenv("MGS2_GL_DZ"), &gx_dz) <= 0) { fprintf(stderr, "xlive: MGS2_GL_DZ is not <vb0>:<vb1>:<arm>[,...]\n"); exit(1); }
          gx_dz_on = 1; fprintf(stderr, "xlive mode: gl29 dz (MGS2_GL_DZ): %d entries:", gx_dz.n);
          for (int j = 0; j < gx_dz.n; j++) fprintf(stderr, " %ld-%ld %d/%d", (long)gx_dz.e[j].vb0, (long)gx_dz.e[j].vb1, gx_dz.e[j].arm, gx_dz.e[j].n);
          fputc('\n', stderr); }
        fprintf(stderr, "xlive mode: the PSP harness's frames and GE lists (MGS2_XLIVE_GX), texture pool %ld KB (budget %u KB); digests of the "
                "frames whose last vblank is in %ld..%ld\n", kb, gx_pool_n / 8 * 7 >> 10, gx_from, gx_to);
      } else {
      if (!tkc) { gu_keys(map, 1); if (from == 0) gu_zero_empty(map, 1);
        if (!(tkc = tk_new(map, 0))) { fprintf(stderr, "xlive: no memory for the texture cache\n"); exit(1); } }
      if (gl_init(&glf, tkc, map, zk, 4096)) { fprintf(stderr, "xlive: no memory for the live frame\n"); exit(1); }
      }
      glf.memo_check = m >= 2; glf.zchk = m >= 3; gb_on = 1;
      if (getenv("MGS2_GL_HOLD") && atoi(getenv("MGS2_GL_HOLD")) >= 1 && atoi(getenv("MGS2_GL_HOLD")) <= GL_HOLD) {   /* sb4: as the PSP's cfg */
        glf.hold_max = atoi(getenv("MGS2_GL_HOLD")); fprintf(stderr, "xlive mode: step c holds up to %d P06 calls (MGS2_GL_HOLD)\n", glf.hold_max); }
      { static FxThin ft; fx_thin_init(&ft, getenv("MGS2_GL_RAIN") ? atoi(getenv("MGS2_GL_RAIN")) : 1,   /* task #114: as the PSP's cfg */
          getenv("MGS2_GL_SPLASH") ? atoi(getenv("MGS2_GL_SPLASH")) : 1, getenv("MGS2_GL_STREAKS") ? atoi(getenv("MGS2_GL_STREAKS")) : 1);
        if (getenv("MGS2_GL_STREAK_CALLS")) fx_thin_calls(&ft, atoi(getenv("MGS2_GL_STREAK_CALLS")));   /* task #158 */
        if (getenv("MGS2_GL_RAIN_SPRITES")) fx_thin_rainspr(&ft, atoi(getenv("MGS2_GL_RAIN_SPRITES")));   /* task #161 */
        if (fx_thin_on(&ft)) { glf.ft = &ft; fprintf(stderr, "xlive mode: the rain's drop sheets drawn 1 strip in %d, the foot splash 1 in %d "
          "(MGS2_GL_RAIN, MGS2_GL_SPLASH)\n", ft.keep[0], ft.keep[1]);
          if (ft.keep[2] != 1) fprintf(stderr, "xlive mode: the rain's streaks drawn 1 strip in %d with C4 (MGS2_GL_STREAKS; 0: none)\n", ft.keep[2]);
          if (ft.keep_calls > 1) fprintf(stderr, "xlive mode: the rain's streak calls drawn 1 in %d, whole calls by block, with C4 "
                                         "(MGS2_GL_STREAK_CALLS)\n", ft.keep_calls);
          if (!ft.keep[3]) fprintf(stderr, "xlive mode: the rain's sprites left out whole, with C4 (MGS2_GL_RAIN_SPRITES=0)\n"); } }   /* task #161 */
      if (getenv("MGS2_XLIVE_CUT") && !atoi(getenv("MGS2_XLIVE_CUT"))) { glf.h.cut = 0;   /* the PSP's OPT_GL_CUT off */
        fprintf(stderr, "xlive mode: every df0 call converted (MGS2_XLIVE_CUT=0)\n"); }
      const char *e2 = getenv("MGS2_XLIVE_GER"), *c2 = e2 ? strrchr(e2, ':') : 0;
      if (c2 && (size_t)(c2 - e2) < sizeof ger_pre) { memcpy(ger_pre, e2, (size_t)(c2 - e2)); ger_pre[c2 - e2] = 0;
        for (c2++; *c2 && ger_n < 512; ) { ger_vb[ger_n++] = strtol(c2, (char **)&c2, 10); if (*c2 == ',') c2++; else break; } }
      if (zk > 0) fprintf(stderr, "xlive mode: the live renderer's frame on shown vblanks (MGS2_XLIVE_GB=%d), GE depth = Z24 x %g%s\n", m, zk,
                          m >= 2 ? ", memo checked" : "");
      else fprintf(stderr, "xlive mode: the live renderer's frame on shown vblanks (MGS2_XLIVE_GB=%d), the PSP's depth (Z24, each frame's "
                   "range mapped)%s%s\n", m, m >= 2 ? ", memo checked" : "", m >= 3 ? ", map checked" : "");
    }
    { static const char *const sw[7] = { "MGS2_XLIVE_GUMEMO", "MGS2_XLIVE_TKFLAGS", "MGS2_XLIVE_GUCLUT", "MGS2_XLIVE_V48", "MGS2_XLIVE_T16",
                                         "MGS2_XL_EAGER", "MGS2_XL_MARK" };   /* sb8: the last two (xl_ud_eager, xl_ud_mark; default 1) */
      for (int i = 0; i < 7; i++) {
        const char *e = getenv(sw[i]); if (!e) continue;
        int v = atoi(e) != 0;
        if (i == 5) { xl_ud_eager(v); fprintf(stderr, "xlive mode: %s=%d\n", sw[i], v); continue; }
        if (i == 6) { xl_ud_mark(v); fprintf(stderr, "xlive mode: %s=%d\n", sw[i], v); continue; }
        if (i == 0) gu_area_memo(map, v); else if (i == 1) { if (tkc) tk_flags(tkc, v); } else if (i == 2) gu_clut_memo(map, v); else if (i == 3) xl_ud_v48(v); else if (tkc) tk_t16(tkc, v);
        fprintf(stderr, "xlive mode: %s=%d\n", sw[i], v); } }
    um_chk = getenv("MGS2_TX_UMEMO_CHK") && atoi(getenv("MGS2_TX_UMEMO_CHK"));   /* sb9 */
    uf_chk = getenv("MGS2_TX_FRONT_CHK") && atoi(getenv("MGS2_TX_FRONT_CHK"));   /* sb10 T1 */
    if (uf_chk) fprintf(stderr, "xlive mode: step 1 T1, the texture memo's front checked\n");
    li_chk = getenv("MGS2_GU_LDID_CHK") && atoi(getenv("MGS2_GU_LDID_CHK"));
    if (um_chk || li_chk || getenv("MGS2_OPT2") || getenv("MGS2_OPT2_CYCLE"))
      fprintf(stderr, "xlive mode: step 0 (g_opt2 %08x at the start%s%s%s)\n", getenv("MGS2_OPT2") ? (unsigned)strtoul(getenv("MGS2_OPT2"), 0, 16) : (unsigned)g_opt2, getenv("MGS2_OPT2_CYCLE") ? ", turns" : "",
              um_chk ? ", texture memo checked" : "", li_chk ? ", CLUT loads by identity checked" : "");
    if (tkc && getenv("MGS2_XLIVE_TKBLK")) { tk_blk(tkc, atoi(getenv("MGS2_XLIVE_TKBLK")));
      fprintf(stderr, "xlive mode: texture conversion %s (MGS2_XLIVE_TKBLK)\n", atoi(getenv("MGS2_XLIVE_TKBLK")) == 0 ? "texel by texel" :
              atoi(getenv("MGS2_XLIVE_TKBLK")) == 1 ? "a block at a time" : "both ways, compared"); }
    if (getenv("MGS2_XLIVE_WATCH") && sscanf(getenv("MGS2_XLIVE_WATCH"), "%x:%ld:%ld", &wbn, &wfrom, &wto) != 3) wbn = -1;
    if (getenv("MGS2_XLIVE_RTLOG") && sscanf(getenv("MGS2_XLIVE_RTLOG"), "%ld:%ld", &rt_from, &rt_to) != 2) rt_from = rt_to = -1;
    if (getenv("MGS2_XLIVE_TRX") && atoi(getenv("MGS2_XLIVE_TRX")) >= 1) { trx_all = atoi(getenv("MGS2_XLIVE_TRX")) >= 2; gu_on_trx(map, on_trx, 0); }
    for (const char *c = getenv("MGS2_XLIVE_CENSUS"); c && *c && census_n < 32; ) { census_at[census_n++] = strtol(c, (char **)&c, 10); if (*c == ',') c++; else break; }
    vc_on = getenv("MGS2_XLIVE_VCEN") && atoi(getenv("MGS2_XLIVE_VCEN"));
    if (getenv("MGS2_SB_NEED") && atoi(getenv("MGS2_SB_NEED"))) { gl_need = 1; xl_need(1); gl_need_chk = getenv("MGS2_SB_NCHK") && atoi(getenv("MGS2_SB_NCHK")); }
    if (getenv("MGS2_SB_LANE") && (atoi(getenv("MGS2_SB_LANE")) & 0xf)) { g_xl_lane = atoi(getenv("MGS2_SB_LANE")) & 0xf; xl_lane(1);
      xc_rec_chk = getenv("MGS2_SB_CHK") && atoi(getenv("MGS2_SB_CHK"));
      fprintf(stderr, "xlive mode: Stage B lanes%s%s%s%s (MGS2_SB_LANE)%s\n", (g_xl_lane & 1) ? " 1" : "", (g_xl_lane & 2) ? " 2" : "",
              (g_xl_lane & 4) ? " 3" : "", (g_xl_lane & 8) ? " 4" : "", xc_rec_chk ? ", their df0, P06, P07 and effect input checked (MGS2_SB_CHK)" : ""); }
    if (getenv("MGS2_C3_CHK") && atoi(getenv("MGS2_C3_CHK"))) { xc_c3_chk = 1;   /* sb14 */
      fprintf(stderr, "xlive mode: Stage C step 3's trims checked against the one-pass conversion and the depth range by vertices (MGS2_C3_CHK)\n"); }
    if (getenv("MGS2_SB_LANE_OPT") && atoi(getenv("MGS2_SB_LANE_OPT"))) { lane_opt = 1; lane_base = g_xl_lane;
      fprintf(stderr, "xlive mode: the lanes follow g_opt's lane bits at each transfer (MGS2_SB_LANE_OPT), within mask %d\n", lane_base); }
    xc_held_force = getenv("MGS2_SB_FORCE") && atoi(getenv("MGS2_SB_FORCE")); xc_held_poison = getenv("MGS2_SB_POISON") && atoi(getenv("MGS2_SB_POISON"));
    vc_p06_exact = getenv("MGS2_SB_P6EXACT") && atoi(getenv("MGS2_SB_P6EXACT"));
    c1m_on = getenv("MGS2_C1_MODEL") && atoi(getenv("MGS2_C1_MODEL"));   /* sb11 */
    c2m_on = getenv("MGS2_C2_MODEL") && atoi(getenv("MGS2_C2_MODEL"));   /* sb12 */
    c3m_on = getenv("MGS2_C3_MODEL") && atoi(getenv("MGS2_C3_MODEL"));   /* sb13 */
    xc_c7_chk = getenv("MGS2_C7_CHK") && atoi(getenv("MGS2_C7_CHK"));   /* sb10 T2: each held P07 call's placed memory against its own */
    if (xc_held_force || xc_held_poison || vc_p06_exact) fprintf(stderr, "xlive mode: step c checks:%s%s%s\n", xc_held_force ? " every held call through "
      "the rebuilt VU memory (MGS2_SB_FORCE)" : "", xc_held_poison ? " the records' images and the scratch memory poisoned (MGS2_SB_POISON)" : "", vc_p06_exact ? " P06 in "
      "the exact form (MGS2_SB_P6EXACT)" : "");
    if (getenv("MGS2_SB_PROBE")) { tp_vb = atol(getenv("MGS2_SB_PROBE")); const char *c = strchr(getenv("MGS2_SB_PROBE"), ':'); tp_to = c ? atol(c + 1) : tp_vb; }
    if (fn[0] && !(dig = fopen(fn, "w"))) { fprintf(stderr, "xlive: cannot write %s\n", fn); exit(1); }
    dig_on = dig != 0;
    if (getenv("MGS2_XLIVE_DIGUSED") && atoi(getenv("MGS2_XLIVE_DIGUSED"))) { xdg_used(1); fprintf(stderr, "xlive mode: digests of what is read (MGS2_XLIVE_DIGUSED)\n"); }
    fprintf(stderr, "xlive: the translator live from vb %ld to %ld%s%s\n", from, to - 1, dig ? ", digests to " : "", fn);
    if (M.fast || sync_on) fprintf(stderr, "xlive mode:%s%s\n", M.fast ? " spans (MGS2_XLIVE_FAST)" : "", sync_on ? " sync before each call (MGS2_XLIVE_SYNC)" : "");
    cg_on = getenv("MGS2_XLIVE_CG") && atoi(getenv("MGS2_XLIVE_CG"));
    skip_on = getenv("MGS2_XLIVE_SKIP") && atoi(getenv("MGS2_XLIVE_SKIP")); sk_c0 = g_dg_calls[0]; sk_c1 = g_dg_calls[1];
    if (skip_on) fprintf(stderr, "xlive mode: shown frames only (MGS2_XLIVE_SKIP)\n");
    one_on = M.fast && getenv("MGS2_XLIVE_ONE") && atoi(getenv("MGS2_XLIVE_ONE"));
    if (one_on) fprintf(stderr, "xlive mode: one walk (MGS2_XLIVE_ONE)\n");
    if (from == 0) { on = 1; xl_reset(); xdg_open(dig); vc_start(); if (cg_on) CALLGRIND_START_INSTRUMENTATION; }
  }
}
/* each vblank start (boot.c k_vblank_hook, after pkt_vblank): the line of the vblank that ended, then the window */
void xlive_chk_vblank(long vb)
{
  if (from == -2) setup();
  if (from < 0 || done) return;
  if (on) fprintf(stderr, "xlive: vb %ld %016llx\n", vb - 1, (unsigned long long)ph);
  if (gb_now) { gb_frame_end(vb - 1); gb_now = 0; }
  { int k = (g_dg_calls[0] != sk_c0) | (g_dg_calls[1] != sk_c1) << 1; sk_c0 = g_dg_calls[0]; sk_c1 = g_dg_calls[1];   /* the vblank that ended */
    skip_now = skip_on && k == 2;
    if (gb_on && (k & 1) && ((on && vb < to) || (!on && vb == from)) && (!gx_on || gx_seen)) { gb_now = 1; glf.h.clipfast = (g_opt & OPT_CLIP_FAST) != 0; glf.stepc = (g_opt & OPT_XL_STEPC) != 0; glf.stepc7 = (g_opt2 & OPT2_C7) != 0; glf.h.c1 = (g_opt2 & OPT2_C1) != 0; glf.h.c2 = (g_opt2 & OPT2_C2) != 0; glf.h.c3v = (g_opt2 & OPT2_C3V) != 0; glf.h.c3z = (g_opt2 & OPT2_C3Z) != 0; glf.h.c3w = (g_opt2 & OPT2_C3W) != 0; glf.h.c4 = (g_opt2 & OPT2_C4) != 0; glf.h.c4x = (g_opt2 & OPT2_C4X) != 0; glf.ram = M.mem; gl_begin(&glf);
      if (gx_on && gx_first < 0) { gx_first = vb; fprintf(stderr, "picture: the first frame begins at vb %ld\n", vb); } } }
  if (on && vc_on) vc_cline(vb - 1);
  if (on) for (int k = 0; k < census_n; k++) if (census_at[k] == vb - 1) ec_report(vb - 1);
  ph = 0xcbf29ce484222325ull;
  if (!on && vb == from) { on = 1; xl_reset(); xdg_open(dig); vc_start(); if (cg_on) CALLGRIND_START_INSTRUMENTATION; }
  if (tkc) tk_frame(tkc);
  if (map && wbn >= 0) gu_watch(map, vb >= wfrom && vb <= wto ? wbn : -1);
  if (on && tx_on) {   /* the vblank that ended: transfers whose EE bytes changed since they were sent; every 100 vblanks a line */
    ck.scanned += gu_sent_scan(map, scan_fn, 0);
    if (vb > from && ((vb - from) % 100 == 0 || vb >= to)) { tx_line(&ck, vb - 1 - (vb - 1 - from) % 100, vb - 1); tx_add(); }
  }
  cur_vb = vb; c2m_vb = vb; c3m_vb = vb; c3m_ram = M.mem;
  if (c3m_on) c3m_vblank();   /* sb13 (MGS2_C3_WATCH) */
  if (on && vb >= to) { on = 0; done = 1; if (cg_on) CALLGRIND_STOP_INSTRUMENTATION; xdg_close(); if (dig) fclose(dig); dig = 0; totals();
    ec_report(vb - 1);
    if (vc_on) { const XlStats *S = xl_stats();   /* the UNPACK kinds of the window */
      for (int i = 0; i < S->nuk; i++) { uint64_t k = S->uk[i];
        fprintf(stderr, "xlive vc kind: format %02x mode %u usn %u flg %u cl %u wl %u mask %08x: %ld commands, %ld vectors\n", (unsigned)(k & 0x1f),
                (unsigned)(k >> 5 & 3), (unsigned)(k >> 7 & 1), (unsigned)(k >> 8 & 1), (unsigned)(k >> 16 & 0xff), (unsigned)(k >> 24 & 0xff),
                (unsigned)(k >> 32), S->uk_cmds[i], S->uk_vec[i]); }
      for (int i = 0; i < S->nent; i++) fprintf(stderr, "xlive vc entry: %06x (%s): %ld calls, %ld vectors unpacked before them\n", (unsigned)S->ent[i],
                                                  S->ent[i] == XL_DF0 || S->ent[i] == XL_FX || S->ent[i] == XL_FX2 ? "converter" : xl_rule(S->ent[i]) ? "rule" : "no rule",
                                                  S->ent_n[i], S->ent_uq[i]); }
    if (tkc) { const TkStats *k = tk_stats(tkc);
      fprintf(stderr, "xlive tk: the texture path: %ld uses, textures %ld hits / %ld conversions, palettes %ld / %ld; render targets %ld, not known %ld, "
              "refused %ld, texels or entries the map could not give %ld; converted %lld KB, kept %ld (%lld KB, peak %lld KB), evicted %ld, "
              "the heaviest frame %lld KB\n", k->uses, k->hits, k->misses, k->pal_hits, k->pal_misses, k->rt, k->lost, k->unsup, k->unknown,
              (long long)(k->conv_bytes >> 10), k->live, (long long)(k->live_bytes >> 10), (long long)(k->peak_bytes >> 10), k->evicted,
              (long long)(k->peak_frame_bytes >> 10));
      fprintf(stderr, "xlive tk: conversions: %ld a block at a time, %ld handed back to the texel path, %ld differing from it; GS blocks read %ld, "
              "%ld of them whole from one upload\n", k->blk, k->blk_back, k->blk_diff, gu_stats(map)->blocks, gu_stats(map)->blocks_one);
      step0_line("xlive tk: step 0:"); }
    if (tx_on) { fprintf(stderr, "xlive tex: totals, "); tx_line(&ck_all, from, vb - 1); fprintf(stderr, "xlive tex: %ld distinct upload keys\n", up_n); src_report(vb - from); kc_report(); }
    return; }
  if (on) xdg_vblank(vb);
}
